/*
     File        : file_system.C

     Author      : Nithin Gowtham Saravanan
     Modified    : 11/27/2025

     Description : Implementation of simple File System class.
                   Has support for numerical file identifiers.
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file_system.H"

/*--------------------------------------------------------------------------*/
/* LOCAL CONSTANTS */
/*--------------------------------------------------------------------------*/

static const unsigned int INODES_BLOCK    = 0; /* Block holding inode list.    */
static const unsigned int FREELIST_BLOCK  = 1; /* Block holding free list.     */
static const unsigned int METADATA_BLOCKS = 2; /* Number of metadata blocks.   */

/*--------------------------------------------------------------------------*/
/* CLASS Inode */
/*--------------------------------------------------------------------------*/

/* You may need to add a few functions, for example to help read and store 
   inodes from and to disk. */

/*--------------------------------------------------------------------------*/
/* BITMAP HELPERS FOR FREE LIST */
/*--------------------------------------------------------------------------*/

void FileSystem::set_block_used(unsigned char* _buf, unsigned int _block_no) {
	/* Set corresponding bit for a block to 1 (used). */
	unsigned int byte_index = 4 + (_block_no / 8);
	unsigned int bit_index  = _block_no % 8;
	_buf[byte_index] |= (1u << bit_index);
}

void FileSystem::set_block_free(unsigned char* _buf, unsigned int _block_no) {
	/* Clear corresponding bit for a block to 0 (free). */
	unsigned int byte_index = 4 + (_block_no / 8);
	unsigned int bit_index  = _block_no % 8;
	_buf[byte_index] &= ~(1u << bit_index);
}

bool FileSystem::is_block_used(const unsigned char* _buf, unsigned int _block_no) {
	/* Test corresponding bit for a block. */
	unsigned int byte_index = 4 + (_block_no / 8);
	unsigned int bit_index  = _block_no % 8;
	return (_buf[byte_index] & (1u << bit_index)) != 0;
}

/*--------------------------------------------------------------------------*/
/* CLASS FileSystem */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

FileSystem::FileSystem() {
	Console::puts("In file system constructor.\n");

	/* Initialize members to safe defaults. */
	disk        = 0;
	size        = 0;
	inodes      = 0;
	free_blocks = 0;
	mounted     = false;
}

FileSystem::~FileSystem() {
	Console::puts("unmounting file system\n");
	/* Make sure that the inode list and the free list are saved. */

	if (mounted && disk != 0) {
		/* Persist metadata before tearing down in-memory state. */
		save_inodes();
		save_freelist();
	}

	/* Release dynamically allocated memory. */
	if (inodes) {
		delete[] inodes;
		inodes = 0;
	}
	if (free_blocks) {
		delete[] free_blocks;
		free_blocks = 0;
	}

	disk    = 0;
	size    = 0;
	mounted = false;
}

/*--------------------------------------------------------------------------*/
/* METADATA LOAD / SAVE HELPER FUNCTIONS */
/*--------------------------------------------------------------------------*/

void FileSystem::load_inodes() {
	/* Read inode list from disk into the in-memory array. */

	unsigned char buf[SimpleDisk::BLOCK_SIZE];
	disk->read(INODES_BLOCK, buf);

	if (!inodes) {
		inodes = new Inode[MAX_INODES];
	}

	unsigned int bytes_to_copy = MAX_INODES * sizeof(Inode);
	if (bytes_to_copy > SimpleDisk::BLOCK_SIZE) {
		bytes_to_copy = SimpleDisk::BLOCK_SIZE;
	}

	for (unsigned int i = 0; i < bytes_to_copy; ++i) {
		reinterpret_cast<unsigned char*>(inodes)[i] = buf[i];
	}

	/* Reattach FileSystem pointer for each inode. */
	for (unsigned int i = 0; i < MAX_INODES; ++i) {
		inodes[i].fs = this;
	}
}

void FileSystem::save_inodes() {
	/* Write in-memory inode list back to the INODES block on disk. */

	if (!disk || !inodes) {
		return;
	}

	unsigned char buf[SimpleDisk::BLOCK_SIZE];
	for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; ++i) {
		buf[i] = 0;
	}

	unsigned int bytes_to_copy = MAX_INODES * sizeof(Inode);
	if (bytes_to_copy > SimpleDisk::BLOCK_SIZE) {
		bytes_to_copy = SimpleDisk::BLOCK_SIZE;
	}

	for (unsigned int i = 0; i < bytes_to_copy; ++i) {
		buf[i] = reinterpret_cast<unsigned char*>(inodes)[i];
	}

	disk->write(INODES_BLOCK, buf);
}

void FileSystem::load_freelist() {
	/* Read and decode free-block list from the FREELIST block. */

	unsigned char buf[SimpleDisk::BLOCK_SIZE];
	disk->read(FREELIST_BLOCK, buf);

	/* First 4 bytes store number of FS blocks. */
	unsigned int fs_blocks = 0;
	fs_blocks |= static_cast<unsigned int>(buf[0]);
	fs_blocks |= static_cast<unsigned int>(buf[1]) << 8;
	fs_blocks |= static_cast<unsigned int>(buf[2]) << 16;
	fs_blocks |= static_cast<unsigned int>(buf[3]) << 24;

	if (fs_blocks == 0) {
		/* Disk does not seem to contain a formatted FS. */
		size = 0;
		return;
	}

	size = fs_blocks;

	/* Allocate in-memory free-block list. */
	if (free_blocks) {
		delete[] free_blocks;
	}
	free_blocks = new unsigned char[size];

	for (unsigned int b = 0; b < size; ++b) {
		free_blocks[b] = is_block_used(buf, b) ? 1 : 0;
	}
}

void FileSystem::save_freelist() {
	/* Encode and write free-block list into FREELIST block on disk. */

	if (!disk || !free_blocks || size == 0) {
		return;
	}

	unsigned char buf[SimpleDisk::BLOCK_SIZE];
	for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; ++i) {
		buf[i] = 0;
	}

	/* Store size (in blocks) of file system. */
	buf[0] = static_cast<unsigned char>(size & 0xFF);
	buf[1] = static_cast<unsigned char>((size >> 8) & 0xFF);
	buf[2] = static_cast<unsigned char>((size >> 16) & 0xFF);
	buf[3] = static_cast<unsigned char>((size >> 24) & 0xFF);

	/* Store free/used information as bitmap. */
	for (unsigned int b = 0; b < size; ++b) {
		if (free_blocks[b]) set_block_used(buf, b);
		else                set_block_free(buf, b);
	}

	disk->write(FREELIST_BLOCK, buf);
}

/*--------------------------------------------------------------------------*/
/* FREE INODE / FREE BLOCK HELPERS */
/*--------------------------------------------------------------------------*/

short FileSystem::GetFreeInode() {
	/* Scan inode list for an unused inode. */

	for (unsigned int i = 0; i < MAX_INODES; ++i) {
		if (!inodes[i].used) {
			return static_cast<short>(i);
		}
	}
	return -1;
}

int FileSystem::GetFreeBlock() {
	/* Scan for a free data block (skipping metadata blocks). */

	if (!free_blocks || size <= METADATA_BLOCKS) {
		return -1;
	}

	for (unsigned int b = METADATA_BLOCKS; b < size; ++b) {
		if (!free_blocks[b]) {
			free_blocks[b] = 1; /* Mark block as used. */
			return static_cast<int>(b);
		}
	}
	return -1;
}

/*--------------------------------------------------------------------------*/
/* FILE SYSTEM FUNCTIONS */
/*--------------------------------------------------------------------------*/

bool FileSystem::Mount(SimpleDisk * _disk) {
	Console::puts("mounting file system from disk\n");
	/* Here you read the inode list and the free list into memory */

	if (!_disk) {
		return false;
	}

	disk = _disk;

	/* Load free list first to know FS size. */
	load_freelist();
	if (size == 0) {
		return false;
	}

	/* Sanity-check against underlying disk size. */
	unsigned int disk_blocks = disk->NaiveSize() / SimpleDisk::BLOCK_SIZE;
	assert(size <= disk_blocks);

	/* Then load inode list. */
	load_inodes();

	mounted = true;
	return true;
}

bool FileSystem::Format(SimpleDisk * _disk, unsigned int _size) { // static!
	Console::puts("formatting disk\n");
	/* Here you populate the disk with an initialized (probably empty) inode list
	   and a free list. Make sure that blocks used for the inodes and for the free list
	   are marked as used, otherwise they may get overwritten. */

	if (!_disk) {
		return false;
	}

	/* Compute number of blocks that belong to this file system. */
	unsigned int fs_blocks = _size / SimpleDisk::BLOCK_SIZE;
	if (fs_blocks < METADATA_BLOCKS) {
		return false;
	}

	/* Check that single bitmap block can encode all blocks. */
	unsigned int max_blocks_encodable = (SimpleDisk::BLOCK_SIZE - 4) * 8;
	assert(fs_blocks <= max_blocks_encodable);

	/* ---- Initialize inode block ---- */

	Inode empty_inodes[FileSystem::MAX_INODES];
	for (unsigned int i = 0; i < FileSystem::MAX_INODES; ++i) {
		empty_inodes[i].id       = -1;
		empty_inodes[i].block_no = 0;
		empty_inodes[i].length   = 0;
		empty_inodes[i].used     = false;
		empty_inodes[i].fs       = 0;
	}

	unsigned char inode_buf[SimpleDisk::BLOCK_SIZE];
	for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; ++i) {
		inode_buf[i] = 0;
	}

	unsigned int bytes_to_copy = FileSystem::MAX_INODES * sizeof(Inode);
	if (bytes_to_copy > SimpleDisk::BLOCK_SIZE) {
		bytes_to_copy = SimpleDisk::BLOCK_SIZE;
	}

	for (unsigned int i = 0; i < bytes_to_copy; ++i) {
		inode_buf[i] = reinterpret_cast<unsigned char*>(empty_inodes)[i];
	}

	_disk->write(INODES_BLOCK, inode_buf);

	/* ---- Initialize FREELIST block ---- */

	unsigned char freelist_buf[SimpleDisk::BLOCK_SIZE];
	for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; ++i) {
		freelist_buf[i] = 0;
	}

	/* Store FS size. */
	freelist_buf[0] = static_cast<unsigned char>(fs_blocks & 0xFF);
	freelist_buf[1] = static_cast<unsigned char>((fs_blocks >> 8) & 0xFF);
	freelist_buf[2] = static_cast<unsigned char>((fs_blocks >> 16) & 0xFF);
	freelist_buf[3] = static_cast<unsigned char>((fs_blocks >> 24) & 0xFF);

	/* Mark metadata blocks as used. */
	set_block_used(freelist_buf, INODES_BLOCK);
	set_block_used(freelist_buf, FREELIST_BLOCK);

	/* Data blocks remain free (bits = 0). */

	_disk->write(FREELIST_BLOCK, freelist_buf);

	return true;
}

Inode * FileSystem::LookupFile(int _file_id) {
	Console::puts("looking up file with id = "); Console::puti(_file_id); Console::puts("\n");
	/* Here you go through the inode list to find the file. */

	if (!inodes) {
		return 0;
	}

	for (unsigned int i = 0; i < MAX_INODES; ++i) {
		if (inodes[i].used && inodes[i].id == _file_id) {
			return &inodes[i];
		}
	}
	return 0;
}

bool FileSystem::CreateFile(int _file_id) {
	Console::puts("creating file with id:"); Console::puti(_file_id); Console::puts("\n");
	/* Here you check if the file exists already. If so, throw an error.
	   Then get yourself a free inode and initialize all the data needed for the
	   new file. After this function there will be a new file on disk. */

	if (!mounted || !disk) {
		return false;
	}

	/* Fail if file already exists. */
	if (LookupFile(_file_id) != 0) {
		return false;
	}

	/* Get free inode and free data block. */
	short inode_index = GetFreeInode();
	if (inode_index < 0) {
		return false;
	}

	int block_index = GetFreeBlock();
	if (block_index < 0) {
		return false;
	}

	/* Initialize inode fields. */
	Inode& node = inodes[inode_index];
	node.id       = static_cast<long>(_file_id);
	node.block_no = static_cast<unsigned int>(block_index);
	node.length   = 0;
	node.used     = true;
	node.fs       = this;

	/* Zero-initialize the new data block on disk. */
	unsigned char zero_block[SimpleDisk::BLOCK_SIZE];
	for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; ++i) {
		zero_block[i] = 0;
	}
	disk->write(node.block_no, zero_block);

	/* Persist updated metadata. */
	save_inodes();
	save_freelist();

	return true;
}

bool FileSystem::DeleteFile(int _file_id) {
	Console::puts("deleting file with id:"); Console::puti(_file_id); Console::puts("\n");
	/* First, check if the file exists. If not, throw an error. 
	   Then free all blocks that belong to the file and delete/invalidate 
	   (depending on your implementation of the inode list) the inode. */

	if (!mounted || !disk) {
		return false;
	}

	Inode* node = LookupFile(_file_id);
	if (!node) {
		return false;
	}

	/* Mark the file's data block as free. */
	if (node->block_no < size && node->block_no >= METADATA_BLOCKS && free_blocks) {
		free_blocks[node->block_no] = 0;
	}

	/* Invalidate the inode. */
	node->id       = -1;
	node->block_no = 0;
	node->length   = 0;
	node->used     = false;

	/* Persist updated metadata. */
	save_inodes();
	save_freelist();

	return true;
}

void FileSystem::FlushMetadata() {
	/* Helper to flush inode and free-block metadata back to disk. */

	if (!disk) {
		return;
	}
	save_inodes();
	save_freelist();
}
