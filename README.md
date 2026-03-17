# Custom x86 Operating System Kernel

An educational **x86 operating system kernel** developed in **C/C++ and x86 Assembly**.  
This project implements several fundamental operating system subsystems including memory management, virtual memory, thread scheduling, device drivers, and a simple file system.

The kernel is built and executed using the **QEMU emulator**, enabling experimentation with low-level OS components without requiring physical hardware.

---

## Overview

This repository demonstrates the implementation of core operating system concepts such as:

- Kernel bootstrapping
- Physical memory management
- Virtual memory and paging
- Kernel-level thread scheduling
- Disk device drivers
- File system implementation

The project incrementally builds a minimal but functional OS kernel capable of managing memory, scheduling threads, interacting with disk devices, and storing files.

---

## Kernel Architecture

The system architecture evolves as additional subsystems are introduced:
    Bootloader
        ↓
Kernel Initialization
        ↓
Physical Memory Manager
        ↓
Paging & Virtual Memory
        ↓
Thread Scheduler
        ↓
Disk Device Driver
        ↓
    File System


Each component is implemented as part of the kernel and interacts with other subsystems to provide basic operating system functionality.

---

## Implemented Subsystems

### Kernel Initialization

The kernel boot process initializes the execution environment and prepares the system for running kernel code.

Responsibilities include:

- Kernel entry point initialization
- Basic runtime setup
- Console output utilities

---

### Physical Memory Management

A **frame manager** is implemented to manage physical memory frames.

Features include:

- Contiguous frame allocation
- Kernel and process frame pools
- Bitmap-based frame tracking
- Frame allocation and release

This subsystem manages the allocation of physical memory used by the kernel and user processes.

---

### Paging and Virtual Memory

The kernel implements **x86 paging** to support virtual memory.

Key components include:

- Page directory and page tables
- Page fault handler
- Dynamic frame allocation
- Logical-to-physical address translation

Virtual memory is lazily allocated when page faults occur.

---

### Virtual Memory Allocation

A virtual memory allocator enables dynamic memory allocation within the kernel.

Features:

- Virtual memory pools
- Lazy allocation through page faults
- Integration with the paging subsystem

This provides functionality similar to dynamic memory allocation mechanisms such as `new` and `delete`.

---

### Kernel Thread Scheduling

The kernel supports **kernel-level threads** and implements a basic **FIFO scheduling policy**.

Capabilities include:

- Thread creation
- Context switching
- Ready queue management
- Cooperative scheduling

Threads yield the CPU voluntarily and are scheduled by the kernel.

---

### Disk Device Driver

A simple block device driver is implemented for disk interaction.

Features:

- Programmed I/O disk operations
- Non-blocking disk access
- Thread-aware disk operations

This subsystem demonstrates how operating systems interact with hardware devices.

---

### File System

A minimal file system is implemented on top of the disk driver.

Features include:

- File creation and deletion
- Sequential file read/write
- Inode-based file management
- Block allocation and free block tracking

This file system demonstrates the core principles of persistent storage management.

---

## Technology Stack

- **Languages:** C, C++, x86 Assembly  
- **Architecture:** x86  
- **Build System:** Make  
- **Emulator:** QEMU  
- **Debugger:** GDB  

---

## Building the Kernel

Clean previous builds:

```bash
make clean
```

Build the kernel:
```bash
make
```

## Running the Kernel

Run the kernel using QEMU:
```bash
make run
```

## Debugging
Run the kernel in debug mode:
```bash
make debug
```

Connect using GDB:
```bash
gdb
target remote localhost:1234
continue
```
## Repository Structure
boot/           Boot and startup code
kernel/         Kernel initialization
memory/         Physical memory management
paging/         Paging and virtual memory
scheduler/      Thread scheduling
drivers/        Device drivers
filesystem/     File system implementation
utils/          Kernel utility functions
Makefile        Build configuration

---

## Learning Outcomes

Through this project, the following systems programming concepts are explored:

- Operating system architecture
- Memory management and paging
- Kernel thread scheduling
- Device driver design
- File system fundamentals
- Low-level systems programming

---

## Author

**Nithin Gowtham Saravanan**  
M.S. Computer Engineering  
Texas A&M University