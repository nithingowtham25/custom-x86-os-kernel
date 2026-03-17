/*
 * File        : nonblocking_disk.H
 * Author      : Nithin Gowtham Saravanan
 * Description : Non-blocking disk built on top of SimpleDisk.
 *
 *  Supported configurations:
 *
 *   - _USES_SCHEDULER_ only:
 *        * wait_while_busy() yields to scheduler instead of busy looping.
 *        * read()/write() are the SimpleDisk versions (not thread-safe).
 *
 *   - _USES_SCHEDULER_ + _THREADSAFE_DISK_:
 *        * Above + a simple per-disk lock so only one thread uses the disk
 *          at a time (thread-safe).
 *
 *   - _USES_SCHEDULER_ + _THREADSAFE_DISK_ + _DISK_MASK_INTERRUPTS_:
 *     OR _USES_SCHEDULER_ + _DISK_MASK_INTERRUPTS_:
 *        * Same semantics as above, but wait_while_busy() is "interrupt-hinted":
 *          IRQ14 sets a flag, and the waiting thread reacts to it, instead of
 *          using a pure tight polling loop.
 */

#ifndef _NONBLOCKING_DISK_H_
#define _NONBLOCKING_DISK_H_

#include "simple_disk.H"
#include "macros_config.H"

#ifdef _USES_SCHEDULER_
#include "interrupts.H"   // InterruptHandler
#include "thread.H"
#endif

class NonBlockingDisk
#ifdef _USES_SCHEDULER_
    : public SimpleDisk, public InterruptHandler
#else
    : public SimpleDisk
#endif
{
public:
    explicit NonBlockingDisk(unsigned int _size);

    // Core non-blocking behavior: override SimpleDisk's busy wait.
    virtual void wait_while_busy() override;

#ifdef _THREADSAFE_DISK_
    // Thread-safe wrappers around SimpleDisk's read/write.
    virtual void read(unsigned long _block_no, unsigned char* _buf) override;
    virtual void write(unsigned long _block_no, unsigned char* _buf) override;
#endif

#ifdef _USES_SCHEDULER_
    // IRQ14 handler.
    //  - Options 1 & 2: just silences the default handler.
    //  - Option 3 (_DISK_MASK_INTERRUPTS_): sets a flag observed by
    //    wait_while_busy().
    virtual void handle_interrupt(REGS* regs) override;
#endif

private:
#ifdef _THREADSAFE_DISK_
    volatile bool lock_held;
    void lock_disk();
    void unlock_disk();
#endif

#if defined(_USES_SCHEDULER_) && defined(_DISK_MASK_INTERRUPTS_)
    // Set by IRQ14 to tell the waiting thread that the disk changed state.
    volatile bool irq_fired;
#endif
};

#endif // _NONBLOCKING_DISK_H_
