# Initialization Environment

## Overview

This module sets up the **initial execution environment** for the x86 kernel. It includes the bootstrapping process, minimal runtime support, and basic console output to verify that the kernel is successfully loaded and executed.

The primary goal of this stage is to ensure that the kernel can be compiled, booted, and executed within an emulated environment using QEMU.

---

## Objectives

* Set up the development and execution environment for kernel development
* Understand the kernel boot process
* Compile and run a minimal kernel binary
* Verify kernel execution through console output

---

## System Components

The initialization environment consists of the following core components:

### Kernel Entry (`kernel.C`)

* Contains the main entry point of the kernel
* Responsible for printing the welcome message
* Modified to display user-specific output

---

### Boot Assembly (`start.asm`)

* Contains the multiboot header
* Performs low-level system initialization
* Transfers control to the kernel entry point

---

### Console Interface (`simple_console.C / .H`)

* Provides primitive access to screen output
* Used to print text directly to video memory

---

### Utility Functions (`utils.C / .H`)

* Basic helper functions such as:

  * Memory operations
  * String handling
  * Program termination

---

### Build System (`Makefile`)

* Automates compilation and linking
* Generates the kernel binary (`kernel.bin`)
* Provides convenient commands:

  * `make` → build kernel
  * `make clean` → remove old builds
  * `make run` → run kernel in QEMU
  * `make debug` → run kernel in debug mode

---

## Execution Flow

```
Bootloader (QEMU)
        ↓
start.asm (low-level setup)
        ↓
kernel.C (main entry point)
        ↓
Console Output (video memory)
```

---

## Building the Kernel

```bash
make clean
make
```

---

## Running the Kernel

```bash
make run
```

This launches the kernel using QEMU in system emulation mode.

---

## Debugging

```bash
make debug
```

Then connect using GDB:

```bash
gdb
target remote localhost:1234
continue
```

---

## Sample Output

```
Initialized console.

WELCOME TO MY KERNEL!
Nithin Gowtham Saravanan
```

This output confirms that:

* The kernel successfully boots
* Console output is functional
* The execution flow reaches the kernel entry point

---

## Key Learning Outcomes

* Understanding the basic **kernel boot process**
* Setting up a **low-level development environment**
* Working with **x86 assembly and C integration**
* Using **QEMU for OS-level emulation**
* Building and running a **bare-metal kernel binary**

---

## Notes

* The kernel runs in a minimal environment without an operating system
* Output is written directly to video memory
* This module serves as the foundation for all subsequent kernel subsystems

---

## Reference

This implementation is based on the provided machine problem handout , which outlines the setup of a minimal kernel, toolchain configuration, and execution using QEMU.
