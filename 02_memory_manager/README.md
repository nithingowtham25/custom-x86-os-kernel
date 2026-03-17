# Physical Memory Manager (Frame Manager)

## Overview

This module implements a **frame manager** responsible for managing physical memory frames (pages) in the system. It provides mechanisms to allocate and release contiguous blocks of physical memory and forms the foundation for virtual memory and paging in later stages.

The system organizes memory into **frame pools**, allowing separate management of kernel and process memory regions.

---

## Objectives

* Manage physical memory using fixed-size frames
* Support allocation and deallocation of contiguous frame sequences
* Maintain efficient tracking of free and used memory
* Enable separation of kernel and user memory pools

---

## Memory Layout

The system assumes a **32MB physical memory** with the following layout:

* **0MB – 1MB** → Reserved (system, devices, BIOS)
* **1MB – 2MB** → Kernel code and stack
* **2MB – 4MB** → Kernel frame pool
* **> 4MB** → Process frame pool

The frame manager primarily operates on:

* Kernel pool (2–4MB)
* Process pool (>4MB)

---

## Design Overview

### Frame Pools

Memory is divided into multiple **frame pools**, each managing a contiguous region of physical memory:

* Kernel Pool → Used by kernel components
* Process Pool → Used by user-level memory

Each pool supports:

* Allocation (`get_frames`)
* Deallocation (`release_frames`)
* Region marking (`mark_inaccessible`)

---

### Bitmap-Based Allocation

A **bitmap** is used to track the state of each frame:

* Each frame uses **2 bits**
* 4 frames are stored per byte

Frame states:

* `Free` → Available for allocation
* `Used` → Allocated frame
* `HoS (Head of Sequence)` → Start of a contiguous allocation

This enables efficient tracking of contiguous memory regions.

---

### Contiguous Allocation Strategy

To allocate memory:

1. Scan bitmap for a contiguous sequence of free frames
2. Mark:

   * First frame → `HoS`
   * Remaining frames → `Used`
3. Return the starting frame number

If no suitable block is found, allocation fails.

---

### Frame Deallocation

To release memory:

1. Identify the owning frame pool using a global pool list
2. Verify the starting frame is marked `HoS`
3. Free all frames in the sequence until:

   * Next `HoS`, or
   * A `Free` frame is encountered

This ensures correct reconstruction of contiguous free regions.

---

### Global Frame Pool Management

A global linked list of frame pools is maintained:

* Enables correct pool identification during deallocation
* Supports multiple independent memory regions

This abstraction is implemented through the `ContFramePool` class.

---

### Handling Inaccessible Memory

Certain physical memory regions (e.g., **15MB–16MB**) are unavailable.

These are explicitly marked:

```cpp id="xg1m3v"
mark_inaccessible(base_frame, n_frames);
```

Marked frames are treated as allocated and excluded from future allocations.

---

## Implementation Details

### Key Files

* `cont_frame_pool.H` → Class definition and interface
* `cont_frame_pool.C` → Frame pool implementation
* `kernel.C` → Initialization and testing

---

### Core Functions

#### `get_frames(n)`

Allocates a contiguous sequence of frames.

#### `release_frames(first_frame)`

Releases a previously allocated sequence.

#### `mark_inaccessible(base, n)`

Marks a region as unavailable for allocation.

#### `needed_info_frames(n)`

Calculates frames required for bitmap metadata.

---

### Initialization

* Bitmap initialized with all frames marked `Free`
* Management data stored:

  * Internally within the pool, or
  * Externally (process pool uses kernel pool memory)

---

## Codebase Overview

### Build System

| File        | Description                  |
| ----------- | ---------------------------- |
| `Makefile`  | Builds kernel (`kernel.bin`) |
| `linker.ld` | Defines memory layout        |

---

### Core Kernel Components

| File          | Description                        |
| ------------- | ---------------------------------- |
| `start.asm`   | Entry point after bootloader       |
| `kernel.C`    | Initializes memory pools and tests |
| `console.H/C` | Console output                     |
| `utils.H/C`   | Utility functions                  |
| `assert.H/C`  | Debug assertions                   |

---

### Machine-Level Support

| File                | Description                               |
| ------------------- | ----------------------------------------- |
| `machine.H/C`       | System constants and hardware abstraction |
| `machine_low.H/asm` | Low-level operations                      |

---

### Memory Management Components

| File                    | Description                         |
| ----------------------- | ----------------------------------- |
| `cont_frame_pool.H/C`   | Contiguous frame allocator          |
| `simple_frame_pool.H/C` | Reference implementation (not used) |

> Note: `simple_frame_pool` is provided for reference only.

---

## Running the Kernel

```bash id="y2o6br"
qemu-system-x86_64 -kernel kernel.bin
```

---

## Testing Strategy

### Test Cases

* Allocation and deallocation of varying frame sizes
* Verification of contiguous allocation
* Handling inaccessible memory regions
* Validation of correct frame numbering
* Testing both kernel and process pools

---

### Test Procedure

1. Initialize kernel frame pool (2MB–4MB)
2. Initialize process frame pool (>4MB)
3. Allocate frames for bitmap storage
4. Mark inaccessible region (15MB–16MB)
5. Perform allocation and release operations
6. Validate using console output

---

## Key Learning Outcomes

* Bitmap-based memory allocation
* Physical memory management fundamentals
* Contiguous memory allocation strategies
* Multi-pool memory management
* Foundation for paging and virtual memory

---

## Notes

* Implemented using `ContFramePool` abstraction
* Efficient bitmap operations are critical
* Serves as the foundation for later memory subsystems

---

## 📄 Documentation

* 📄 [Design Document](docs/design.pdf)
* 📄 [Handout](../docs/handout_02_memory_manager.pdf)
