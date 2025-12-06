/*
	File: file_system.H

	Author: Nithin Gowtham Saravanan
			Department of Electrical and Computer Engineering
			Texas A&M University
	Date  : 12/05/2025

	Description: Simple File System.
*/

#ifndef _FILE_SYSTEM_H_ // include file only once
#define _FILE_SYSTEM_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "simple_disk.H"
#include "macros_config.H"

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

class FileSystem;

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

class Inode
{
	friend class FileSystem; // The inode is in an uncomfortable position between
	friend class File;       // File System and File. We give both full access
	// to the Inode.

private:
	long id; // File "name"

	/* You will need additional information in the inode, such as allocation
	   information. */

#ifdef LARGE_FILE_SUPPORT
	static const unsigned int MAX_FILE_BLOCKS = SimpleDisk::BLOCK_SIZE / sizeof(unsigned int);
	unsigned int index_block_no;           /* Block holding the list of data blocks. */
#else
	unsigned int block_no;                 /* Single data block.                     */
#endif

	unsigned int length;                   /* File length in Byte.                   */
	bool         used;                     /* Indicates whether this inode is used.  */

	FileSystem* fs; // It may be handy to have a pointer to the File system.
	// For example when you need a new block or when you want
	// to load or save the inode list. (Depends on your
	// implementation.)

	/* You may need a few additional functions to help read and store the
	   inodes from and to disk. */
};

/*--------------------------------------------------------------------------*/
/* FORWARD DECLARATIONS */
/*--------------------------------------------------------------------------*/

/* (none) */

/*--------------------------------------------------------------------------*/
/* F i l e S y s t e m  */
/*--------------------------------------------------------------------------*/

class FileSystem
{

	friend class Inode;
	friend class File;

private:
	/* -- DEFINE YOUR FILE SYSTEM DATA STRUCTURES HERE. */

	SimpleDisk*   disk;
	unsigned int  size;  /* Size of the file system in blocks. */

	static constexpr unsigned int MAX_INODES = SimpleDisk::BLOCK_SIZE / sizeof(Inode);
	/* Just as an example; you can store MAX_INODES in a single INODES block */

	Inode*        inodes; 
	/* The inode list */

	unsigned char* free_blocks;
	/* The free-block list. You may want to implement the "list" as a bitmap.
	   Feel free to use an unsigned char to represent whether a block is free or not;
	   no need to go to bits if you don't want to.
	   If you reserve one block to store the "free list", you can handle a file system up to
	   256kB. (Large enough for a proof of concept.) */

	bool mounted;
	/* Tracks whether this file system is currently mounted on a disk. */

	// short GetFreeInode();
	// int GetFreeBlock();
	/* It may be helpful to two functions to hand out free inodes in the inode list and free
	   blocks. These functions also come useful to class Inode and File. */

	short GetFreeInode();
	/* Find a free inode index, or -1 if none is available. */

	int   GetFreeBlock();
	/* Find a free data block index, or -1 if none is available. */

	void load_inodes();
	/* Read inode list from disk into memory. */

	void save_inodes();
	/* Write inode list from memory back to disk. */

	void load_freelist();
	/* Read free-block information from disk into memory. */

	void save_freelist();
	/* Write free-block information from memory back to disk. */

	static void set_block_used(unsigned char* _buf, unsigned int _block_no);
	/* Mark a block as used in the on-disk bitmap. */

	static void set_block_free(unsigned char* _buf, unsigned int _block_no);
	/* Mark a block as free in the on-disk bitmap. */

	static bool is_block_used(const unsigned char* _buf, unsigned int _block_no);
	/* Check whether a block is used in the on-disk bitmap. */

#ifdef LARGE_FILE_SUPPORT
	unsigned int GetDataBlock(Inode* _inode,
	                          unsigned int _logical_block_index,
	                          bool _allocate);
	/* Return physical block for logical index, optionally allocating it. */

	void FreeAllBlocks(Inode* _inode);
	/* Free all data blocks and the index block for the inode. */
#endif

public:
	FileSystem();
	/* Just initializes local data structures. Does not connect to disk yet. */

	~FileSystem();
	/* Unmount file system if it has been mounted. */

	bool Mount(SimpleDisk* _disk);
	/* Associates this file system with a disk. Limit to at most one file system per disk.
	   Returns true if operation successful (i.e. there is indeed a file system on the disk.) */

	static bool Format(SimpleDisk* _disk, unsigned int _size);
	/* Wipes any file system from the disk and installs an empty file system of given size. */

	Inode* LookupFile(int _file_id);
	/* Find file with given id in file system. If found, return its inode.
	   Otherwise, return null. */

	bool CreateFile(int _file_id);
	/* Create file with given id in the file system. If file exists already,
	   abort and return false. Otherwise, return true. */

	bool DeleteFile(int _file_id);
	/* Delete file with given id in the file system; free any disk block occupied by the file. */

	void FlushMetadata();
	/* Flush inode list and free-block list back to disk. */

	SimpleDisk* GetDisk() { return disk; }
	/* Return pointer to the underlying disk. */

	unsigned int GetSizeInBlocks() const { return size; }
	/* Return the size of the file system in blocks. */
};
#endif
