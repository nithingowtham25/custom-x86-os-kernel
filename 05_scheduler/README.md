# Kernel Thread Scheduling

## Overview

This module implements **kernel-level threading and scheduling**, enabling multiple execution contexts within the operating system. It introduces both **cooperative (FIFO)** and **preemptive (Round-Robin)** scheduling, driven by hardware timer interrupts.

The scheduler manages thread execution, context switching, and lifecycle management, forming the foundation for multitasking in the kernel.

---

## Objectives

* Implement kernel-level threads with independent execution contexts
* Design a FIFO scheduler using a ready queue
* Support thread lifecycle (creation, execution, termination)
* Enable interrupt-driven preemptive scheduling (Round-Robin)
* Ensure safe synchronization with interrupt handling

---

## System Architecture

The scheduling system is composed of:

* **Thread** → Execution context (stack + registers)
* **Scheduler** → Manages ready and zombie queues
* **Interrupt System** → Dispatches hardware interrupts
* **Timer (PIT)** → Drives scheduling events
* **Kernel Integration** → Configurable via compile-time flags

A scheduler is required to fairly allocate CPU time among threads, rather than relying on threads to voluntarily yield control .

---

## Scheduling Modes

### FIFO Scheduling (Cooperative)

* Threads explicitly call `yield()`
* Scheduler selects next thread from ready queue
* No preemption

### Round-Robin Scheduling (Preemptive)

* Timer interrupts trigger context switches
* Each thread gets a fixed time quantum
* Implemented using **EOQTimer**

---

## Thread Model

Each thread:

* Has its own **stack** and **CPU context**
* Starts execution via `thread_start()`
* Ends via `thread_shutdown()`

### Context Switching

* Implemented using low-level assembly (`threads_low_switch_to`)
* Saves/restores stack pointer and registers
* Does not return until switched back

---

## Scheduler Design

### Ready Queue

* FIFO queue (head → next thread)
* Enqueue → `add()` / `resume()`
* Dequeue → `yield()`

### Zombie Queue

* Stores terminated threads
* Prevents freeing stack while still executing
* Cleaned using `cleanup_zombies()`

This design ensures safe memory reclamation and avoids stack corruption.

---

## Core Scheduler Functions

### `yield()`

* Voluntary context switch
* Picks next thread from ready queue

### `add(Thread* t)`

* Adds newly created thread to ready queue

### `resume(Thread* t)`

* Re-enqueues a runnable thread

### `terminate(Thread* t)`

* Removes thread and frees resources
* Handles both running and non-running threads

### `terminate_self()`

* Called when a thread finishes execution
* Moves thread to zombie list
* Dispatches next thread

---

## Idle Thread

* Always available fallback thread
* Runs when no other threads are ready
* Prevents CPU from stalling

---

## Interrupt Integration

### Timer Interrupt (IRQ0)

* Drives scheduling events
* Handled via `SimpleTimer` or `EOQTimer`

### Interrupt Handling Design

* Interrupts disabled during critical sections
* Re-enabled before context switching
* Ensures race-free queue operations

Correct interrupt handling is critical for stable scheduling .

---

## Round-Robin Implementation

### EOQTimer

* Extends `SimpleTimer`
* Generates periodic interrupts
* Calls:

```cpp
Scheduler::eoq_preempt_isr();
```

---

### Preemption Flow

1. Timer interrupt occurs
2. Current thread moved to ready queue
3. Next thread selected
4. Context switch performed inside ISR

Special handling ensures:

* No double EOI
* Safe interrupt-driven switching
* Stable execution flow

---

## Implementation Highlights

Based on the design:

* Thread bootstrap with controlled entry/exit
* Ready queue implemented as FIFO linked list
* Zombie reaping for safe resource cleanup
* Interrupt-safe queue operations (CLI/STI)
* Preemption implemented inside ISR context
* Configurable scheduling modes via macros

Your implementation successfully integrates all components into a unified scheduler .

---

## Codebase Overview

### Core Files

| File            | Description                             |
| --------------- | --------------------------------------- |
| `thread.H/C`    | Thread abstraction and lifecycle        |
| `scheduler.H/C` | Scheduling logic and queues             |
| `kernel.C`      | Scheduler initialization and test setup |

---

### Timer & Interrupts

| File               | Description                     |
| ------------------ | ------------------------------- |
| `simple_timer.H/C` | Base timer implementation       |
| `interrupts.C`     | Interrupt dispatch and handling |
| `macros_config.H`  | Compile-time configuration      |

---

## Configuration Flags

Defined in `macros_config.H`:

* `_USES_SCHEDULER_` → Enable scheduler
* `_TERMINATING_FUNCTIONS_` → Enable thread termination
* `_RR_SCHEDULER_` → Enable Round-Robin scheduling

---

## Testing

Multiple scenarios were validated:

### 1. No Scheduling

* Threads run sequentially
* No context switching

### 2. FIFO Scheduling

* Threads execute in strict order
* No mid-execution preemption

### 3. FIFO with Termination

* Threads terminate correctly
* Zombie cleanup verified

### 4. Round-Robin Scheduling

* Preemptive switching observed
* Threads interleave execution
* Timer-driven scheduling validated

As shown in logs (pages 18–21 of the design document), Round-Robin scheduling demonstrates proper time-sliced execution and clean thread termination .

---

## Execution

```bash
make
make run
```

---

## 📂 Artifacts & Data

* 📜 [No Scheduling Log](docs/logs/kernel_log_no_scheduling.txt)
* 📜 [FIFO Scheduling (Without Termination)](docs/logs/kernel_log_fifo_without_terminate.txt)
* 📜 [FIFO Scheduling (With Termination)](docs/logs/kernel_log_fifo_with_terminate.txt)
* 📜 [Round Robin Scheduling](docs/logs/kernel_log_RR_with_terminate.txt)

---

## 📄 Documentation

* 📄 [Design Document](docs/design.pdf)
* 📄 [Handout](../docs/handout_05_scheduler.pdf)

---

## Key Learning Outcomes

* Kernel-level threading and context switching
* FIFO vs Round-Robin scheduling
* Interrupt-driven preemption
* Safe synchronization with interrupts
* Resource management using zombie queues

---

## Notes

* Idle thread is never scheduled in ready queue
* Context switching must remain interrupt-safe
* Preemption logic must execute within ISR carefully

---

## Summary

This module introduces **true multitasking** into the kernel by combining thread management, scheduling policies, and interrupt-driven execution, forming a critical step toward a fully functional operating system.
