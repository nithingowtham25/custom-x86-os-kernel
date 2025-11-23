/*
     File        : nonblocking_disk.C

     Author      : Nithin Gowtham Saravanan

     Description : Non-blocking disk implementation.
                   Optional thread-safe mode (Options 1 & 2) can be enabled
                   via _THREADSAFE_DISK_.
*/

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "console.H"
#include "nonblocking_disk.H"
#include "scheduler.H"
#include "system.H"
#include "macros_config.H"
#include "interrupts.H"
#include "thread.H"
#include "machine.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

NonBlockingDisk::NonBlockingDisk(unsigned int _size) 
    : SimpleDisk(_size)
#ifdef _USES_SCHEDULER_
    , InterruptHandler()
#endif
{
#ifdef _USES_SCHEDULER_
    /* Register this disk as handler for IRQ 14.
       This avoids the "NO DEFAULT INTERRUPT HANDLER REGISTERED" spam when the
       disk raises an interrupt. */
    InterruptHandler::register_handler(14, this);
#endif
}

/*--------------------------------------------------------------------------*/
/* THREAD-SAFE READ/WRITE (Options 1 and 2, flag-controlled)                */
/*--------------------------------------------------------------------------*/

#if defined(_USES_SCHEDULER_) && defined(_THREADSAFE_DISK_)

void NonBlockingDisk::lock_disk()
{
    Thread* me = Thread::CurrentThread();
    assert(me != nullptr);
    assert(System::SCHEDULER != nullptr);

    for (;;) {
        Machine::disable_interrupts();
        if (!disk_locked) {
            /* Acquire the lock. */
            disk_locked = true;
            Machine::enable_interrupts();
            return;
        }
        /* Lock is held by some other thread; back off cooperatively. */
        Machine::enable_interrupts();

        /* Give some other ready thread a chance to run and possibly
           release the lock. This avoids tight spinning. */
        System::SCHEDULER->yield();
    }
}

void NonBlockingDisk::unlock_disk()
{
    Machine::disable_interrupts();
    disk_locked = false;
    Machine::enable_interrupts();
}

void NonBlockingDisk::read(unsigned long _block_no, unsigned char* _buf)
{
    /* Thread-safe: serialize full disk operations. */
    lock_disk();
    SimpleDisk::read(_block_no, _buf);
    unlock_disk();
}

void NonBlockingDisk::write(unsigned long _block_no, unsigned char* _buf)
{
    /* Thread-safe: serialize full disk operations. */
    lock_disk();
    SimpleDisk::write(_block_no, _buf);
    unlock_disk();
}

#endif /* _USES_SCHEDULER_ && _THREADSAFE_DISK_ */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   N o n B l o c k i n g   D i s k                      */
/*--------------------------------------------------------------------------*/

void NonBlockingDisk::wait_while_busy()
{
#ifdef _USES_SCHEDULER_
    /* 
       Non-blocking behavior with scheduler:

       - While the disk controller reports "busy", yield the CPU so other
         threads can run instead of spinning.
       - If the scheduler isn't ready yet, fall back to the base behavior.
    */
    while (is_busy()) {
        if (System::SCHEDULER != nullptr) {
            System::SCHEDULER->yield();
        } else {
            /* Scheduler not initialized yet; safest is to use base behavior. */
            SimpleDisk::wait_while_busy();
            break;
        }
    }
#else
    /* No scheduler in this build – use the original busy-wait behavior. */
    SimpleDisk::wait_while_busy();
#endif
}

#ifdef _USES_SCHEDULER_
void NonBlockingDisk::handle_interrupt(REGS* /*_regs*/)
{
    /* 
       We are only installing this handler to consume IRQ 14 so the
       generic dispatcher doesn't print the "NO DEFAULT INTERRUPT
       HANDLER REGISTERED" message.

       The actual PIO protocol (status register reads, etc.) is still
       handled in the polling code (SimpleDisk + wait_while_busy()).
       The PIC EOI is sent by the generic IRQ dispatcher after this
       handler returns, so we don't need to do anything here.
    */
    /* intentionally empty */
}
#endif
