# Disk Device Driver (Non-Blocking I/O)

## Overview

This module implements a **primitive disk device driver** for an x86 operating system. It extends a basic programmed-I/O disk (`SimpleDisk`) into a **scheduler-aware non-blocking disk driver (`NonBlockingDisk`)**, eliminating CPU-intensive busy waiting.

The design preserves **blocking semantics at the API level** while ensuring that the CPU remains available for other threads during disk operations.

---

## Objectives

* Understand low-level disk I/O using the ATA (LBA28) protocol
* Identify and eliminate busy waiting in device drivers
* Implement scheduler-aware blocking I/O
* Support safe concurrent access to disk (thread-safe design)
* Explore interrupt-assisted I/O handling

---

## System Architecture

The disk subsystem consists of:

* **SimpleDisk** → Low-level ATA-based disk driver
* **NonBlockingDisk** → Scheduler-aware wrapper
* **Scheduler (MP5)** → Enables cooperative multitasking
* **Threads** → Issue disk read/write requests
* **Interrupt System (IRQ14)** → Optional responsiveness improvement

The goal is to maintain synchronous read/write semantics while avoiding CPU waste .

---

## SimpleDisk (Baseline)

### Features

* Implements LBA28 disk access
* Uses programmed I/O (PIO)
* Provides:

  * `read(block, buf)`
  * `write(block, buf)`

### Problem: Busy Waiting

```cpp
while (is_busy()) { /* busy loop */ }
```

* CPU is blocked during disk operations
* Inefficient due to slow mechanical disk latency

---

## NonBlockingDisk Design

### Key Idea

Replace busy waiting with:

```cpp
while (is_busy()) {
    System::SCHEDULER->yield();
}
```

This allows:

* Calling thread → logically blocked
* CPU → free to run other threads

---

## Implementation Strategy

### 1. Override Waiting Behavior

* Inherit from `SimpleDisk`
* Override `wait_while_busy()`
* Keep all ATA logic unchanged

---

### 2. Scheduler-Aware Polling

When `_USES_SCHEDULER_` is enabled:

* Replace tight polling with cooperative yielding
* Periodically re-check disk status

This significantly reduces CPU wastage.

---

### 3. Thread-Safe Disk Access (Bonus)

To handle multiple threads:

* Introduced **per-disk lock**
* Implemented using:

  * `lock_disk()`
  * `unlock_disk()`

Features:

* Interrupt-safe locking (disable/enable interrupts)
* Scheduler-friendly spin (uses `yield()`)

Ensures:

* No interleaved ATA commands
* No data corruption

---

### 4. Interrupt Hint (IRQ14)

Optional improvement:

* Register disk interrupt handler (IRQ14)
* Use flag `irq_fired`

Behavior:

* Interrupt signals disk state change
* Driver re-checks disk status immediately

This reduces unnecessary polling cycles without full interrupt-driven design.

---

## Disk Driver Workflow

1. Thread calls `read()` / `write()`
2. Acquire disk lock (if enabled)
3. Issue ATA command (SimpleDisk)
4. Wait for disk readiness:

   * Yield CPU if busy
5. Transfer data
6. Release lock

This preserves synchronous semantics while enabling concurrency.

---

## Codebase Overview

### Core Files

| File                   | Description                          |
| ---------------------- | ------------------------------------ |
| `simple_disk.H/C`      | Base ATA disk driver                 |
| `nonblocking_disk.H/C` | Non-blocking disk implementation     |
| `kernel.C`             | Disk initialization and test threads |

---

### Supporting Components

| File              | Description                 |
| ----------------- | --------------------------- |
| `scheduler.H/C`   | Reused FIFO scheduler (MP5) |
| `thread.H/C`      | Thread management           |
| `interrupts.H/C`  | Interrupt handling          |
| `macros_config.H` | Feature configuration       |

---

## Configuration Flags

Defined in `macros_config.H`:

* `_USES_SCHEDULER_` → Enable non-blocking behavior
* `_THREADSAFE_DISK_` → Enable per-disk locking
* `_DISK_MASK_INTERRUPTS_` → Enable IRQ14 hint mechanism

---

## Testing

### A. Non-Blocking Disk (Baseline)

* Disk operations do not freeze CPU
* Other threads continue execution

Observed behavior (page 17):

* Disk I/O interleaves with CPU threads
* No busy looping observed 

---

### B. Thread-Safe Disk

* Multiple threads perform disk I/O
* Access serialized via lock

Results:

* No data corruption
* Clean 512-byte block transfers
* Proper synchronization

---

### C. Interrupt-Assisted Disk

* IRQ14 handler installed
* Faster responsiveness to disk readiness

Behavior:

* Same correctness as thread-safe mode
* Reduced polling overhead

---

## Implementation Highlights

Based on your design:

* Clean separation:

  * Hardware layer → `SimpleDisk`
  * Policy layer → `NonBlockingDisk`
* Minimal modification strategy (only override waiting)
* Scheduler integration for cooperative multitasking
* Optional interrupt hint without full redesign
* Thread-safe extension with minimal complexity

Your implementation carefully balances **performance, simplicity, and correctness** .

---

## Execution

```bash
make
make run
```

---

## 📂 Artifacts & Data

* 💾 `c.img` → Disk image used for I/O testing

* 📜 [Non-Blocking Disk Log](docs/logs/kernel_log_baseline_nonblocking.txt)

* 📜 [Interrupt-Assisted Disk Log](docs/logs/kernel_log_disk_mask_interrupts.txt)

* 📜 [Thread-Safe Disk Log](docs/logs/kernel_log_threadsafe.txt)

---

## 📄 Documentation

* 📄 [Design Document](docs/design.pdf)
* 📄 [Handout](../docs/handout_06_disk_driver.pdf)

---

## Key Learning Outcomes

* Device driver design in operating systems
* Eliminating busy waiting using scheduling
* Synchronization for shared hardware access
* Interaction between CPU, I/O, and interrupts
* Trade-offs between polling and interrupts

---

## Notes

* Full interrupt-driven disk (bottom-half processing) not implemented
* No request queue or I/O scheduling
* Design prioritizes simplicity and correctness

---

## Summary

This module introduces **efficient disk I/O handling** by replacing CPU-intensive polling with scheduler-driven waiting, marking a key step toward realistic operating system behavior.
