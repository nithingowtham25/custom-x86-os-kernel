/*
     File        : file.C

     Author      : Nithin Gowtham Saravanan
     Modified    : 11/27/2025

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

    /* Store the file system pointer. */
    fs = _fs;
    inode = 0;
    current_pos = 0;

    /* Look up the inode for this file id. */
    assert(fs != 0);
    inode = fs->LookupFile(_id);
    assert(inode != 0);

    /* Start reading/writing at beginning of file. */
    current_pos = 0;

    /* Load the file's data block into the cache. */
    assert(inode->block_no < fs->GetSizeInBlocks());
    fs->GetDisk()->read(inode->block_no, block_cache);
}

File::~File() {
    Console::puts("Closing file.\n");
    /* Make sure that you write any cached data to disk. */
    /* Also make sure that the inode in the inode list is updated. */

    if (fs && inode) {
        /* Flush cached block to disk. */
        fs->GetDisk()->write(inode->block_no, block_cache);

        /* Persist updated length (and any other metadata). */
        fs->FlushMetadata();
    }
}

/*--------------------------------------------------------------------------*/
/* FILE FUNCTIONS */
/*--------------------------------------------------------------------------*/

int File::Read(unsigned int _n, char *_buf) {
    Console::puts("reading from file\n");

    if (!inode) {
        return 0;
    }

    /* Do not read beyond end-of-file. */
    if (current_pos >= inode->length) {
        return 0;
    }

    unsigned int remaining = inode->length - current_pos;
    unsigned int to_read   = (_n < remaining) ? _n : remaining;

    /* Copy requested bytes from cache into caller's buffer. */
    for (unsigned int i = 0; i < to_read; ++i) {
        _buf[i] = block_cache[current_pos + i];
    }

    current_pos += to_read;
    return static_cast<int>(to_read);
}

int File::Write(unsigned int _n, const char *_buf) {
    Console::puts("writing to file\n");

    if (!inode) {
        return 0;
    }

    /* Do not write beyond maximum file size (one block). */
    if (current_pos >= SimpleDisk::BLOCK_SIZE) {
        return 0;
    }

    unsigned int remaining_space = SimpleDisk::BLOCK_SIZE - current_pos;
    unsigned int to_write        = (_n < remaining_space) ? _n : remaining_space;

    /* Copy data from caller's buffer into cache. */
    for (unsigned int i = 0; i < to_write; ++i) {
        block_cache[current_pos + i] = _buf[i];
    }

    current_pos += to_write;

    /* Extend file length if we wrote past previous end. */
    if (current_pos > inode->length) {
        inode->length = current_pos;
    }

    return static_cast<int>(to_write);
}

void File::Reset() {
    Console::puts("resetting file\n");
    /* Set current file position back to beginning. */
    current_pos = 0;
}

bool File::EoF() {
    Console::puts("checking for EoF\n");
    /* Return true if current position has reached or passed file length. */
    if (!inode) {
        return true;
    }
    return (current_pos >= inode->length);
}
