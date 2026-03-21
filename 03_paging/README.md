# Paging and Page Table Management

## Overview

This module implements a **demand-paging based virtual memory system** for the x86 architecture. It introduces a two-level page table structure and enables the transition from physical to logical addressing.

The system supports a **single address space** and lays the foundation for future extensions to multi-process virtual memory systems.

---

## Objectives

* Implement x86 paging infrastructure
* Set up and initialize page tables
* Enable paging through CPU control registers
* Handle page faults dynamically (demand paging)
* Integrate with physical memory frame pools

---

## System Architecture

The paging system divides memory into:

* **Shared Region (0 – 4MB)**

  * Direct-mapped (virtual = physical)
  * Used by kernel code and data

* **Process Region (> 4MB)**

  * Dynamically mapped
  * Uses demand paging

This design ensures efficient memory usage and flexibility for process memory allocation.

---

## Paging Structure

The system uses a **two-level paging mechanism**:

```text
Page Directory (1024 entries)
        ↓
Page Tables (1024 entries each)
        ↓
Physical Frames (4KB each)
```

* Each Page Directory Entry (PDE) points to a Page Table
* Each Page Table Entry (PTE) points to a physical frame

---

## Design Overview

### Page Directory and Page Tables

* One frame is allocated for the **Page Directory**
* Additional frames are allocated for **Page Tables**
* First Page Table maps the **shared 4MB region directly**

---

### Direct Mapping (Kernel Region)

* Virtual addresses map directly to physical addresses
* All entries are marked:

  * Present
  * Writable

---

### Dynamic Mapping (Process Region)

* Pages are initially marked **not present**
* Frames are allocated only when accessed (**lazy allocation**)

---

## Initialization Flow

```text
Initialize Frame Pools
        ↓
init_paging()
        ↓
Create PageTable Object
        ↓
Setup Page Directory + Page Tables
        ↓
load() → Load CR3
        ↓
enable_paging() → Set CR0
```

---

## Core Components

### PageTable Class

Represents the paging subsystem and manages address translation.

#### Key Members

* `current_page_table` → Active page table
* `kernel_mem_pool` → Kernel frame pool
* `process_mem_pool` → Process frame pool
* `shared_size` → Size of direct-mapped region

---

## Key Functions

### `init_paging()`

Initializes global paging parameters and memory pools.

### `PageTable() (Constructor)`

* Allocates frame for page directory
* Allocates initial page table
* Sets up direct mapping for first 4MB

### `load()`

* Loads page directory base address into **CR3 register**
* Activates the page table

### `enable_paging()`

* Enables paging by setting **bit 31 of CR0**

### `handle_fault()`

Handles **page fault exceptions (Exception 14)**:

1. Extract fault address from **CR2**
2. Determine PDE and PTE indices
3. Allocate:

   * New page table (if needed)
   * New frame for the page
4. Update page table entry
5. Flush TLB entry (`invlpg`)
6. Resume execution

---

## Memory Management Integration

* Uses **contiguous frame pools** from previous module
* Kernel pool → stores page tables and directory
* Process pool → stores dynamically allocated frames

---

## Codebase Overview

### Core Files

| File                  | Description                         |
| --------------------- | ----------------------------------- |
| `page_table.H/C`      | Page table implementation           |
| `cont_frame_pool.H/C` | Frame allocator used by paging      |
| `kernel.C`            | Initializes and tests paging system |

---

### Low-Level Support

| File               | Description                                 |
| ------------------ | ------------------------------------------- |
| `paging_low.H/asm` | Control register operations (CR0, CR2, CR3) |
| `machine.H/C`      | System constants and hardware abstraction   |

---

## Page Fault Handling

A page fault occurs when:

* Accessing unmapped memory (> 4MB)
* Accessing invalid or restricted regions

### Handling Strategy

* Allocate frame from process pool
* Create page table if missing
* Update page table entry
* Resume execution

This implements **demand paging**, where memory is allocated only when needed.

---

## Testing and Validation

### Test Scenarios

* Initialization of frame pools
* Page table creation
* Paging enablement
* Access to shared (direct-mapped) memory
* Access to process memory (triggers page faults)

### Observations

* First page fault creates a new page table
* Subsequent faults reuse existing page tables
* Efficient mapping of multiple pages within the same table

Execution logs confirm that accessing process memory correctly triggers page faults and results in dynamic frame allocation.

---

## Execution

```bash
make
make run
```

---

## Key Learning Outcomes

* Understanding of **x86 paging architecture**
* Implementation of **two-level page tables**
* Working with **CPU control registers (CR0, CR2, CR3)**
* Designing a **page fault handler**
* Integration of **virtual memory with physical memory management**

---

## Notes

* The first 4MB must always be correctly mapped before enabling paging
* Page faults must handle both:

  * Missing page tables
  * Missing page entries
* TLB flushing ensures correctness during updates

---

## 📂 Artifacts & Data

* 📜 [Paging Execution Log](docs/logs/kernel_log_paging.txt)

---

## 📄 Documentation

* 📄 [Design Document](docs/design.pdf)
* 📄 [Handout](../docs/handout_03_paging.pdf)
