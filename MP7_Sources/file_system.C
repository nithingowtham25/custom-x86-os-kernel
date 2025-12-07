/*
     File        : file_system.C

     Author      : Nithin Gowtham Saravanan
     Modified    : 12/05/2025

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
#include "macros_config.H"
/*--------------------------------------------------------------------------*/
/* LOCAL CONSTANTS */
/*--------------------------------------------------------------------------*/

static const unsigned int INODES_BLOCK    = 0;
static const unsigned int FREELIST_BLOCK  = 1;
static const unsigned int METADATA_BLOCKS = 2;

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
		save_inodes();
		save_freelist();
	}

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
	/* Read inode list from disk into memory. */

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

	unsigned int fs_blocks = 0;
	fs_blocks |= static_cast<unsigned int>(buf[0]);
	fs_blocks |= static_cast<unsigned int>(buf[1]) << 8;
	fs_blocks |= static_cast<unsigned int>(buf[2]) << 16;
	fs_blocks |= static_cast<unsigned int>(buf[3]) << 24;

	if (fs_blocks == 0) {
		size = 0;
		return;
	}

	size = fs_blocks;

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

	buf[0] = static_cast<unsigned char>(size & 0xFF);
	buf[1] = static_cast<unsigned char>((size >> 8) & 0xFF);
	buf[2] = static_cast<unsigned char>((size >> 16) & 0xFF);
	buf[3] = static_cast<unsigned char>((size >> 24) & 0xFF);

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
			free_blocks[b] = 1;
			return static_cast<int>(b);
		}
	}
	return -1;
}

#ifdef LARGE_FILE_SUPPORT
unsigned int FileSystem::GetDataBlock(Inode* _inode,
                                      unsigned int _logical_block_index,
                                      bool _allocate) {
	/* Return physical block for logical index, optionally allocating it. */

	if (!_inode) {
		return 0;
	}
	if (_logical_block_index >= Inode::MAX_FILE_BLOCKS) {
		return 0;
	}
	if (_inode->index_block_no == 0) {
		return 0;
	}

	unsigned char index_buf[SimpleDisk::BLOCK_SIZE];
	disk->read(_inode->index_block_no, index_buf);

	unsigned int* entries = reinterpret_cast<unsigned int*>(index_buf);
	unsigned int phys = entries[_logical_block_index];

	if (!_allocate) {
		return phys;
	}

	if (phys != 0) {
		return phys;
	}

	int new_block = GetFreeBlock();
	if (new_block < 0) {
		return 0;
	}

	entries[_logical_block_index] = static_cast<unsigned int>(new_block);
	disk->write(_inode->index_block_no, index_buf);

	unsigned char zero_block[SimpleDisk::BLOCK_SIZE];
	for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; ++i) {
		zero_block[i] = 0;
	}
	disk->write(static_cast<unsigned int>(new_block), zero_block);

	return static_cast<unsigned int>(new_block);
}

void FileSystem::FreeAllBlocks(Inode* _inode) {
	/* Free all data blocks and the index block for the inode. */

	if (!_inode || !_inode->index_block_no || !free_blocks) {
		return;
	}

	unsigned char index_buf[SimpleDisk::BLOCK_SIZE];
	disk->read(_inode->index_block_no, index_buf);

	unsigned int* entries = reinterpret_cast<unsigned int*>(index_buf);
	for (unsigned int i = 0; i < Inode::MAX_FILE_BLOCKS; ++i) {
		unsigned int b = entries[i];
		if (b >= METADATA_BLOCKS && b < size) {
			free_blocks[b] = 0;
		}
	}

	if (_inode->index_block_no >= METADATA_BLOCKS && _inode->index_block_no < size) {
		free_blocks[_inode->index_block_no] = 0;
	}
}
#endif

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

	load_freelist();
	if (size == 0) {
		return false;
	}

	unsigned int disk_blocks = disk->NaiveSize() / SimpleDisk::BLOCK_SIZE;
	assert(size <= disk_blocks);

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

	unsigned int fs_blocks = _size / SimpleDisk::BLOCK_SIZE;
	if (fs_blocks < METADATA_BLOCKS) {
		return false;
	}

	unsigned int max_blocks_encodable = (SimpleDisk::BLOCK_SIZE - 4) * 8;
	assert(fs_blocks <= max_blocks_encodable);

	Inode temp_inodes[FileSystem::MAX_INODES];

	for (unsigned int i = 0; i < FileSystem::MAX_INODES; ++i) {
		temp_inodes[i].id = -1;
#ifdef LARGE_FILE_SUPPORT
		temp_inodes[i].index_block_no = 0;
#else
		temp_inodes[i].block_no = 0;
#endif
		temp_inodes[i].length = 0;
		temp_inodes[i].used   = false;
		temp_inodes[i].fs     = 0;
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
		inode_buf[i] = reinterpret_cast<unsigned char*>(temp_inodes)[i];
	}

	_disk->write(INODES_BLOCK, inode_buf);

	unsigned char freelist_buf[SimpleDisk::BLOCK_SIZE];
	for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; ++i) {
		freelist_buf[i] = 0;
	}

	freelist_buf[0] = static_cast<unsigned char>(fs_blocks & 0xFF);
	freelist_buf[1] = static_cast<unsigned char>((fs_blocks >> 8) & 0xFF);
	freelist_buf[2] = static_cast<unsigned char>((fs_blocks >> 16) & 0xFF);
	freelist_buf[3] = static_cast<unsigned char>((fs_blocks >> 24) & 0xFF);

	set_block_used(freelist_buf, INODES_BLOCK);
	set_block_used(freelist_buf, FREELIST_BLOCK);

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

	if (LookupFile(_file_id) != 0) {
		return false;
	}

	short inode_index = GetFreeInode();
	if (inode_index < 0) {
		return false;
	}

	Inode& inode = inodes[inode_index];

#ifdef LARGE_FILE_SUPPORT
	int index_block = GetFreeBlock();
	if (index_block < 0) {
		return false;
	}

	inode.id            = static_cast<long>(_file_id);
	inode.index_block_no = static_cast<unsigned int>(index_block);
	inode.length        = 0;
	inode.used          = true;
	inode.fs            = this;

	unsigned char index_buf[SimpleDisk::BLOCK_SIZE];
	for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; ++i) {
		index_buf[i] = 0;
	}
	disk->write(inode.index_block_no, index_buf);
#else
	int block_index = GetFreeBlock();
	if (block_index < 0) {
		return false;
	}

	inode.id       = static_cast<long>(_file_id);
	inode.block_no = static_cast<unsigned int>(block_index);
	inode.length   = 0;
	inode.used     = true;
	inode.fs       = this;

	unsigned char zero_block[SimpleDisk::BLOCK_SIZE];
	for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; ++i) {
		zero_block[i] = 0;
	}
	disk->write(inode.block_no, zero_block);
#endif

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

	Inode* inode = LookupFile(_file_id);
	if (!inode) {
		return false;
	}

#ifdef LARGE_FILE_SUPPORT
	FreeAllBlocks(inode);
	inode->index_block_no = 0;
#else
	if (inode->block_no < size && inode->block_no >= METADATA_BLOCKS && free_blocks) {
		free_blocks[inode->block_no] = 0;
	}
#endif

	inode->id       = -1;
	inode->length   = 0;
	inode->used     = false;

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
