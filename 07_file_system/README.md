# File System Implementation (Sequential & Indexed Allocation)

## Overview

This module implements a **simple file system** on top of a block device (`SimpleDisk`). It supports **sequential file access**, block allocation, and persistent metadata management.

The design follows a **minimal yet functional architecture**, with clear separation between:

* **Storage layer** → Disk (blocks)
* **Metadata layer** → FileSystem + Inodes
* **Access layer** → File abstraction

In addition to the baseline implementation, the system is extended to support **multi-block files (up to 64KB)** using **indexed allocation**.

---

## Objectives

* Implement a basic file system with sequential access
* Manage disk blocks using a free-block bitmap
* Design inode-based file representation
* Support file creation, deletion, read, and write
* Extend system to support large files using indexed allocation

---

## File System Architecture

The system consists of four major components:

### 1. SimpleDisk (Storage Layer)

* Provides raw block-level access (`read/write`)
* Operates on fixed-size blocks (512 bytes)
* No knowledge of files or metadata

---

### 2. FileSystem (Metadata Manager)

Responsible for:

* Managing inodes (file metadata)
* Free-block allocation using bitmap
* File creation and deletion
* Mounting and formatting

Disk layout:

```text
Block 0 → INODES
Block 1 → FREELIST
Block 2+ → DATA BLOCKS
```

This layout is simple and avoids complex structures like superblocks .

---

### 3. Inode (File Descriptor)

Each file is represented by an inode:

* File ID (acts as filename)
* File length
* Allocation information
* Used/free flag

Baseline:

* Single data block pointer

Extended (large-file mode):

* Index block pointer

---

### 4. File (Access Layer)

Represents an open file:

* Maintains current position
* Uses a 512-byte cache
* Provides:

  * `Read()`
  * `Write()`
  * `Reset()`
  * `EoF()`

All operations are **sequential**, not random access.

---

## Baseline File System

### Key Characteristics

* Single-level namespace (no directories)
* File size limited to **1 block (512B)**
* One inode per file
* Simple allocation strategy

### Operation Flow

1. Open file → load block into cache
2. Read/write from cache
3. Close file → flush cache to disk

This design minimizes disk access and simplifies implementation.

---

## Free Block Management

* Implemented as a **bitmap stored on disk**
* Managed in memory as `free_blocks[]`
* Persisted in the FREELIST block

### Allocation

* Linear scan for free block
* Mark block as used

### Deallocation

* Mark block as free
* Update metadata

---

## Metadata Management

* INODES and FREELIST stored on disk
* Loaded during `Mount()`
* Persisted using:

```cpp
save_inodes();
save_freelist();
```

* Unified flush operation:

```cpp
FlushMetadata();
```

Ensures consistency across executions .

---

## File Operations

### CreateFile()

* Allocates inode
* Allocates data block
* Initializes metadata

---

### DeleteFile()

* Frees data block
* Clears inode
* Updates free list

---

### Read()

* Reads from cache
* Stops at EOF

---

### Write()

* Writes into cache
* Updates file length
* Enforces max file size

---

## Large File Support (Bonus)

### Motivation

Baseline limitation:

```
Max file size = 512 bytes
```

Extended to:

```
Max file size = 64 KB
```

---

## Indexed Allocation Design

### Index Block

* Each file gets an **index block**
* Stores array of data block pointers

```text
Index Block → [block1, block2, ..., block128]
```

* Supports up to 128 blocks
* Total size = 128 × 512B = 64KB

---

## FileSystem Enhancements

* `index_block_no` replaces `block_no`
* New helper:

```cpp
GetDataBlock(inode, logical_index, allocate);
```

* Allocates blocks on demand
* Maintains logical → physical mapping

---

### Block Reclamation

```cpp
FreeAllBlocks(inode);
```

* Frees all data blocks
* Frees index block
* Updates bitmap

---

## File Class Enhancements

New features:

* `current_block_index`
* `cache_dirty` flag
* `LoadBlock()` for block switching

### Multi-Block Read/Write

* Compute logical block index
* Load appropriate block
* Continue across boundaries

This enables seamless sequential access across multiple blocks .

---

## Testing Strategy

### Baseline Testing

* Create small files
* Write strings
* Read and verify data
* Delete files
* Repeat multiple iterations

---

### Large File Testing

* Enabled via:

```cpp
#define LARGE_FILE_SUPPORT
```

* Create 64KB files
* Write patterned data
* Read and verify byte-by-byte
* Delete files and reclaim blocks

Results confirm:

* Correct allocation
* Accurate data persistence
* Proper cleanup of all blocks

---

## Codebase Overview

### Core Files

| File              | Description                   |
| ----------------- | ----------------------------- |
| `file.H/C`        | File abstraction              |
| `file_system.H/C` | Metadata & allocation manager |
| `kernel.C`        | Test harness                  |

---

### Supporting Components

| File              | Description    |
| ----------------- | -------------- |
| `simple_disk.H/C` | Disk interface |
| `macros_config.H` | Feature flags  |
| `console.H/C`     | Debug output   |

---

## Execution

```bash
make
make run
```

---

## 📂 Artifacts & Data

* 💾 `c.img` → Primary disk image (MASTER, ATA-0)

* 💾 `d.img` → Secondary disk image (SLAVE, ATA-0)

* 📜 [Baseline Execution Log](docs/logs/kernel_log_baseline.txt)

* 📜 [Large File Execution Log](docs/logs/kernel_log_large_files.txt)

---

## 📄 Documentation

* 📄 [Design Document](docs/design.pdf)
* 📄 [Handout](../docs/handout_07_file_system.pdf)

---

## Key Learning Outcomes

* File system design fundamentals
* Inode-based metadata management
* Block allocation and free space tracking
* Sequential file access implementation
* Indexed allocation for large files
* Persistence and disk consistency

---

## Notes

* No directories (flat namespace)
* No journaling or crash recovery
* No concurrency control
* Designed for clarity and learning, not production use

---

## Summary

This module builds a complete **file system layer** on top of the disk driver, transforming raw block storage into structured, persistent file storage. The indexed allocation extension demonstrates how real systems scale beyond simple designs while maintaining clean abstractions.
