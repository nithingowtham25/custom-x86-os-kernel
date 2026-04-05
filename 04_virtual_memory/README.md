# Virtual Memory Management & Allocation

## Overview

This module extends the paging system to support **dynamic virtual memory allocation** through virtual memory pools. It builds upon the paging infrastructure and introduces a **lazy allocation model**, where physical memory is allocated only upon access.

The system enables flexible memory management similar to user-level programming (`new` / `delete`) while maintaining full control at the kernel level.

---

## Objectives

* Extend paging to support large virtual address spaces
* Implement recursive page table lookup
* Introduce virtual memory pools (VMPool)
* Enable dynamic memory allocation using lazy paging
* Integrate page table with memory allocation and deallocation

---

## System Design

The implementation is divided into three major components:

### 1. Paging in Virtual Memory

* Page directory and page tables are allocated in **mapped memory (> 4MB)**
* Managed using the **process frame pool**
* Enables support for large address spaces

---

### 2. Recursive Page Table Lookup

The last entry of the page directory (index 1023) points to itself:

```
PDE[1023] → Page Directory
```

This creates a **self-referential mapping**, allowing direct access to:

* Page Directory Entries (PDE)
* Page Table Entries (PTE)

This eliminates the need for manual page table traversal .

---

### 3. Virtual Memory Pools

A new abstraction, **VMPool**, is introduced:

* Manages regions of virtual memory
* Tracks allocated and free regions
* Allocates memory lazily (on page fault)
* Works with PageTable for mapping

---

## Paging Enhancements

### Page Table Updates

* Page directory and tables allocated from **process memory pool**
* Recursive mapping enabled for logical access
* New helper functions:

```cpp
unsigned long* PDE_address(unsigned long addr);
unsigned long* PTE_address(unsigned long addr);
```

These allow direct access to PDEs and PTEs via virtual addresses.

---

### Page Fault Handling

On a page fault:

1. Extract faulting address
2. Validate address using registered VMPools
3. Allocate page table (if missing)
4. Allocate frame for page
5. Update PTE
6. Flush TLB (`invlpg`)

Invalid accesses are treated as **segmentation faults**.

---

### TLB Management

* TLB is not coherent with memory updates
* Must flush entries when invalidating pages
* Implemented using:

  * `invlpg` (per-page flush)
  * CR3 reload (full flush)

---

## Virtual Memory Pool (VMPool)

### Design

Each VMPool:

* Has a base virtual address and size
* Uses page-level granularity
* Maintains:

  * `allocated_regions[]`
  * `free_regions[]`

This approach simplifies allocation and avoids complex memory structures.

---

### Allocation Strategy

* Uses **first-fit allocation**
* Allocates memory in page-sized units
* Does NOT immediately allocate frames

Instead:

* Pages are mapped only when accessed (lazy allocation)

---

### Key Functions

#### `allocate(size)`

* Finds free region
* Returns starting virtual address
* Updates region tables

#### `release(address)`

* Frees all pages in region
* Calls `PageTable::free_page()`
* Returns frames to frame pool

#### `is_legitimate(address)`

* Checks if address belongs to allocated region
* Used during page fault validation

---

## Integration with Page Table

* VMPools register with PageTable using:

```cpp
PageTable::register_pool(VMPool* pool);
```

* PageTable maintains a linked list of pools
* On page fault:

  * Checks legitimacy via `is_legitimate()`
  * Prevents illegal memory access

---

## Implementation Highlights

Based on the design:

* Recursive mapping implemented using logical address windows
* Page directory stored in process memory
* Linked list of VMPools added to PageTable
* Lazy allocation implemented via page faults
* Frame allocation unified through process frame pool
* Clean separation between:
  * allocation (VMPool)
  * mapping (PageTable)

Your implementation follows this design precisely .

---

## Codebase Overview

### Core Files

| File                  | Description                            |
| --------------------- | -------------------------------------- |
| `page_table.H/C`      | Extended page table with VM support    |
| `vm_pool.H/C`         | Virtual memory allocator               |
| `cont_frame_pool.H/C` | Physical frame allocator               |
| `kernel.C`            | Test harness and system initialization |

---

### Low-Level Support

| File                | Description                             |
| ------------------- | --------------------------------------- |
| `paging_low.H/asm`  | Control registers and paging operations |
| `machine_low.H/asm` | x86-specific low-level operations       |

---

## Testing

Two modes are supported:

### Page Table Test Mode

* Enabled via `_TEST_PAGE_TABLE_`
* Tests paging independently
* Verifies:

  * Page table creation
  * Frame allocation
  * Page fault handling

---

### Virtual Memory Pool Test Mode

* Disable `_TEST_PAGE_TABLE_`
* Tests full VM system

Includes:

* Allocation using `allocate()`
* Page faults on first access
* Data integrity validation
* Memory release using `release()`

As shown in logs, all scenarios pass successfully .

---

## Execution

```bash
make
make run
```

---

## 📂 Artifacts & Data

* 📜 [Page Table Validation Log](docs/logs/kernel_log_page_table.txt)
* 📜 [Virtual Memory Pool Log](docs/logs/kernel_log_vm_pool.txt)

---

## 📄 Documentation

* 📄 [Design Document](docs/design.pdf)
* 📄 [Handout](../docs/handout_04_virtual_memory.pdf)

---

## Key Learning Outcomes

* Advanced **virtual memory design**
* Recursive page table mapping
* Demand paging and lazy allocation
* Integration of allocation with paging
* Handling segmentation faults
* TLB management and coherence

---

## Notes

* Memory is allocated in page-sized units for simplicity
* No page replacement (no swapping)
* No coalescing of free regions (intentional simplification)

---

## Summary

This module completes the transition from basic paging to a **fully functional virtual memory system**, enabling dynamic allocation and safe memory access within the kernel.
