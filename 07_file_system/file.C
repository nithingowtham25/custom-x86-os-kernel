/*
     File        : file.C

     Author      : Nithin Gowtham Saravanan
     Modified    : 12/05/2025

     Description : Implementation of simple File class, with support for
                   sequential read/write operations.
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
#include "file.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR/DESTRUCTOR */
/*--------------------------------------------------------------------------*/

File::File(FileSystem *_fs, int _id) {
    Console::puts("Opening file.\n");

    fs = _fs;
    inode = 0;
    current_pos = 0;

    assert(fs != 0);
    inode = fs->LookupFile(_id);
    assert(inode != 0);

#ifdef LARGE_FILE_SUPPORT
    current_block_index = 0;
    cache_dirty         = false;

    if (inode->length > 0) {
        LoadBlock(0, false);
    } else {
        for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; ++i) {
            block_cache[i] = 0;
        }
    }
#else
    current_pos = 0;
    assert(inode->block_no < fs->GetSizeInBlocks());
    fs->GetDisk()->read(inode->block_no, block_cache);
#endif
}

File::~File() {
    Console::puts("Closing file.\n");
    /* Make sure that you write any cached data to disk. */
    /* Also make sure that the inode in the inode list is updated. */

    if (fs && inode) {
#ifdef LARGE_FILE_SUPPORT
        if (cache_dirty) {
            unsigned int phys = fs->GetDataBlock(inode, current_block_index, false);
            if (phys != 0) {
                fs->GetDisk()->write(phys, block_cache);
            }
        }
#else
        fs->GetDisk()->write(inode->block_no, block_cache);
#endif
        fs->FlushMetadata();
    }
}

#ifdef LARGE_FILE_SUPPORT
void File::LoadBlock(unsigned int _logical_block_index, bool _allocate) {
    /* Load the logical block into cache, allocating it if requested. */

    if (!inode) {
        return;
    }

    if (cache_dirty) {
        unsigned int old_phys = fs->GetDataBlock(inode, current_block_index, false);
        if (old_phys != 0) {
            fs->GetDisk()->write(old_phys, block_cache);
        }
        cache_dirty = false;
    }

    unsigned int phys = fs->GetDataBlock(inode, _logical_block_index, _allocate);

    if (phys != 0) {
        assert(phys < fs->GetSizeInBlocks());
        fs->GetDisk()->read(phys, block_cache);
    } else {
        for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; ++i) {
            block_cache[i] = 0;
        }
    }

    current_block_index = _logical_block_index;
}
#endif

/*--------------------------------------------------------------------------*/
/* FILE FUNCTIONS */
/*--------------------------------------------------------------------------*/

int File::Read(unsigned int _n, char *_buf) {
    Console::puts("reading from file\n");

    if (!inode) {
        return 0;
    }

#ifdef LARGE_FILE_SUPPORT
    if (current_pos >= inode->length) {
        return 0;
    }

    unsigned int end_pos = current_pos + _n;
    if (end_pos > inode->length) {
        end_pos = inode->length;
    }

    unsigned int total_copied = 0;

    while (current_pos < end_pos) {
        unsigned int logical_block_index = current_pos / SimpleDisk::BLOCK_SIZE;
        unsigned int offset_in_block     = current_pos % SimpleDisk::BLOCK_SIZE;
        unsigned int remaining           = end_pos - current_pos;
        unsigned int space_in_block      = SimpleDisk::BLOCK_SIZE - offset_in_block;
        unsigned int bytes_in_block      = (remaining < space_in_block) ? remaining : space_in_block;

        LoadBlock(logical_block_index, false);

        for (unsigned int i = 0; i < bytes_in_block; ++i) {
            _buf[total_copied + i] = block_cache[offset_in_block + i];
        }

        current_pos   += bytes_in_block;
        total_copied  += bytes_in_block;
    }

    return static_cast<int>(total_copied);
#else
    if (current_pos >= inode->length) {
        return 0;
    }

    unsigned int remaining = inode->length - current_pos;
    unsigned int to_read   = (_n < remaining) ? _n : remaining;

    for (unsigned int i = 0; i < to_read; ++i) {
        _buf[i] = block_cache[current_pos + i];
    }

    current_pos += to_read;
    return static_cast<int>(to_read);
#endif
}

int File::Write(unsigned int _n, const char *_buf) {
    Console::puts("writing to file\n");

    if (!inode) {
        return 0;
    }

#ifdef LARGE_FILE_SUPPORT
    const unsigned int MAX_FILE_SIZE = Inode::MAX_FILE_BLOCKS * SimpleDisk::BLOCK_SIZE;

    if (current_pos >= MAX_FILE_SIZE) {
        return 0;
    }

    unsigned int max_bytes      = MAX_FILE_SIZE - current_pos;
    unsigned int to_write_total = (_n < max_bytes) ? _n : max_bytes;
    unsigned int total_written  = 0;

    while (total_written < to_write_total) {
        unsigned int logical_block_index = current_pos / SimpleDisk::BLOCK_SIZE;
        unsigned int offset_in_block     = current_pos % SimpleDisk::BLOCK_SIZE;
        unsigned int remaining           = to_write_total - total_written;
        unsigned int space_in_block      = SimpleDisk::BLOCK_SIZE - offset_in_block;
        unsigned int bytes_in_block      = (remaining < space_in_block) ? remaining : space_in_block;

        LoadBlock(logical_block_index, true);

        for (unsigned int i = 0; i < bytes_in_block; ++i) {
            block_cache[offset_in_block + i] = _buf[total_written + i];
        }

        cache_dirty   = true;
        current_pos  += bytes_in_block;
        total_written += bytes_in_block;

        if (current_pos > inode->length) {
            inode->length = current_pos;
        }
    }

    return static_cast<int>(total_written);
#else
    if (current_pos >= SimpleDisk::BLOCK_SIZE) {
        return 0;
    }

    unsigned int remaining_space = SimpleDisk::BLOCK_SIZE - current_pos;
    unsigned int to_write        = (_n < remaining_space) ? _n : remaining_space;

    for (unsigned int i = 0; i < to_write; ++i) {
        block_cache[current_pos + i] = _buf[i];
    }

    current_pos += to_write;

    if (current_pos > inode->length) {
        inode->length = current_pos;
    }

    return static_cast<int>(to_write);
#endif
}

void File::Reset() {
    Console::puts("resetting file\n");
    /* Set the ’current position’ to be at the beginning of the file. */

    current_pos = 0;
#ifdef LARGE_FILE_SUPPORT
    current_block_index = 0;
#endif
}

bool File::EoF() {
    Console::puts("checking for EoF\n");
    /* Is the current position for the file at the end of the file? */

    if (!inode) {
        return true;
    }
    return (current_pos >= inode->length);
}
