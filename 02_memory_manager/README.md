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

* **0MB – 1MB**: Reserved (system, devices, BIOS)
* **1MB – 2MB**: Kernel code and stack
* **2MB – 4MB**: Kernel frame pool
* **> 4MB**: Process frame pool

The frame manager operates primarily on the **kernel pool (2–4MB)** and **process pool (>4MB)** .

---

## Design Overview

### Frame Pools

Memory is divided into multiple **frame pools**, each managing a contiguous region of physical memory.

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

This design enables efficient tracking of contiguous memory regions.

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

* Allows identification of the correct pool during deallocation
* Supports multiple independent memory regions

This design abstracts the frame manager functionality through the `ContFramePool` class.

---

### Handling Inaccessible Memory

Certain physical memory regions may be unavailable (e.g., **15MB–16MB**).

These regions are explicitly marked using:

```cpp id="4k2v0k"
mark_inaccessible(base_frame, n_frames);
```

Marked frames are treated as allocated and excluded from future allocations.

---

## Implementation Details

### Key Files

* `cont_frame_pool.H` → Class definition and interface
* `cont_frame_pool.C` → Frame pool implementation
* `kernel.C` → Initialization and testing of memory pools

---

### Core Functions

#### `get_frames(n)`

Allocates a contiguous sequence of frames.

#### `release_frames(first_frame)`

Releases a previously allocated sequence.

#### `mark_inaccessible(base, n)`

Marks a region as unavailable for allocation.

#### `needed_info_frames(n)`

Calculates the number of frames required to store bitmap metadata.

---

### Initialization

* Bitmap is initialized with all frames marked `Free`
* Management data is stored either:

  * Internally within the pool, or
  * Externally (e.g., process pool uses kernel pool memory)

---

## Testing Strategy

Testing was performed using kernel-level test routines and console output.

### Test Cases

* Allocation and deallocation of varying frame sizes
* Verification of contiguous allocation correctness
* Handling of inaccessible memory regions
* Validation of correct frame numbering
* Testing both kernel and process frame pools

### Test Procedure

1. Initialize kernel frame pool (2MB–4MB)
2. Initialize process frame pool (>4MB)
3. Allocate frames for bitmap storage
4. Mark inaccessible region (15MB–16MB)
5. Perform multiple allocation and release operations
6. Validate behavior using debug output

---

## Key Learning Outcomes

* Implementation of a **bitmap-based memory allocator**
* Understanding of **physical memory management**
* Handling **contiguous memory allocation**
* Managing **multiple memory pools**
* Foundation for **paging and virtual memory systems**

---

## Notes

* The frame manager is implemented via the `ContFramePool` abstraction
* Efficient bitmap manipulation is critical for performance
* This module serves as the foundation for subsequent memory subsystems

---

## Reference

This implementation is based on the frame manager design and requirements described in the machine problem handout and the accompanying design document.
