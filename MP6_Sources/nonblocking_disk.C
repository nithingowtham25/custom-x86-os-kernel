/*
 * File        : nonblocking_disk.C
 * Author      : Nithin Gowtham Saravanan
 * Description : Non-blocking and (optionally) thread-safe disk implementation.
 */

#include "nonblocking_disk.H"
#include "assert.H"
#include "console.H"
#include "machine.H"
#include "system.H"
#include "scheduler.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

NonBlockingDisk::NonBlockingDisk(unsigned int _size)
    : SimpleDisk(_size)
#ifdef _THREADSAFE_DISK_
    , lock_held(false)
#endif
#if defined(_USES_SCHEDULER_) && defined(_DISK_MASK_INTERRUPTS_)
    , irq_fired(false)
#endif
{
#ifdef _USES_SCHEDULER_
    /* For Options 1 & 2 we don't *use* the interrupt, we just silence
       the default "NO DEFAULT INTERRUPT HANDLER" messages.
       For Option 3, we also use it to set irq_fired.*/
    InterruptHandler::register_handler(14, this);
#endif
}

/*--------------------------------------------------------------------------*/
/* WAIT-WHILE-BUSY (NON-BLOCKING BEHAVIOR) */
/*--------------------------------------------------------------------------*/

void NonBlockingDisk::wait_while_busy()
{
#ifndef _USES_SCHEDULER_

    // No scheduler: fall back to original busy-wait from SimpleDisk.
    SimpleDisk::wait_while_busy();
    return;

#else // _USES_SCHEDULER_ is defined

# ifdef _DISK_MASK_INTERRUPTS_

    /*  - Loop while is_busy() is true.
        - Between checks, yield the CPU.
        - If IRQ14 fires, handle_interrupt() sets irq_fired; here we
          notice it, clear it, and immediately re-check is_busy(). */

    assert(System::SCHEDULER != nullptr);

    while (is_busy()) {
        // If an interrupt fired, the disk likely changed state; just clear
        // the flag and let the top of the loop re-check is_busy().
        if (irq_fired) {
            irq_fired = false;
            continue;
        }

        // Let other threads run while we wait for BSY to drop.
        System::SCHEDULER->yield();
    }

    return;

# else  // !_DISK_MASK_INTERRUPTS_

    /* Options 1 & 2: scheduler-based polling. */
    while (is_busy()) {
        if (System::SCHEDULER != nullptr) {
            System::SCHEDULER->yield();
        } else {
            // Safety fallback if scheduler isn't initialized for some reason.
            SimpleDisk::wait_while_busy();
            break;
        }
    }
    return;

# endif // _DISK_MASK_INTERRUPTS_

#endif  // _USES_SCHEDULER_
}

/*--------------------------------------------------------------------------*/
/* THREAD-SAFE READ/WRITE WRAPPERS (OPTION 2) */
/*--------------------------------------------------------------------------*/

#ifdef _THREADSAFE_DISK_

void NonBlockingDisk::lock_disk()
{
    // Simple spin-lock with scheduler-friendly yielding when available.
#ifdef _USES_SCHEDULER_
    assert(System::SCHEDULER != nullptr);
#endif

    for (;;) {
        Machine::disable_interrupts();
        if (!lock_held) {
            lock_held = true;
            Machine::enable_interrupts();
            return;
        }
        Machine::enable_interrupts();

#ifdef _USES_SCHEDULER_
        // Let the lock holder (and other threads) run.
        if (System::SCHEDULER != nullptr) {
            System::SCHEDULER->yield();
        }
#endif
        // If no scheduler, this is just a busy loop.
    }
}

void NonBlockingDisk::unlock_disk()
{
    Machine::disable_interrupts();
    lock_held = false;
    Machine::enable_interrupts();
}

void NonBlockingDisk::read(unsigned long _block_no, unsigned char* _buf)
{
    lock_disk();
    SimpleDisk::read(_block_no, _buf);
    unlock_disk();
}

void NonBlockingDisk::write(unsigned long _block_no, unsigned char* _buf)
{
    lock_disk();
    SimpleDisk::write(_block_no, _buf);
    unlock_disk();
}

#endif // _THREADSAFE_DISK_

/*--------------------------------------------------------------------------*/
/* IRQ14 HANDLER */
/*--------------------------------------------------------------------------*/

#ifdef _USES_SCHEDULER_
void NonBlockingDisk::handle_interrupt(REGS* /*regs*/)
{
#ifdef _DISK_MASK_INTERRUPTS_
    /* record the fact that an IDE interrupt occurred. The waiting
       thread in wait_while_busy() will see this and re-check the disk. */
    irq_fired = true;
#else
    // Options 1 & 2 - don't do anything 
    (void)0;
#endif
}
#endif
