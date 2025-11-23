/*
     File        : nonblocking_disk.H

     Author      : Nithin Gowtham Saravanan

     Description : Non-blocking disk implementation derived from SimpleDisk.
                   Optional thread-safe mode (Options 1 & 2) can be enabled
                   via _THREADSAFE_DISK_ in macros_config.H.
*/

#ifndef _NONBLOCKING_DISK_H_
#define _NONBLOCKING_DISK_H_

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "simple_disk.H"
#include "macros_config.H"

#ifdef _USES_SCHEDULER_
#include "interrupts.H"   // InterruptHandler, REGS
#endif

/*--------------------------------------------------------------------------*/
/* N o n B l o c k i n g   D i s k */
/*--------------------------------------------------------------------------*/

class NonBlockingDisk : public SimpleDisk
#ifdef _USES_SCHEDULER_
                      , public InterruptHandler
#endif
{
public:
    NonBlockingDisk(unsigned int _size);
    /* Creates a NonBlockingDisk device with the given size connected to the 
       MASTER slot of the primary ATA controller.
       NOTE: We are passing the _size argument out of laziness. 
       In a real system, we would infer this information from the 
       disk controller. */

#if defined(_USES_SCHEDULER_) && defined(_THREADSAFE_DISK_)
    /*---------------------------------------------------------------------*/
    /* THREAD-SAFE, NON-BLOCKING READ/WRITE (Options 1 & 2)                */
    /*---------------------------------------------------------------------*/

    virtual void read(unsigned long _block_no, unsigned char* _buf) override;
    /* Thread-safe wrapper around SimpleDisk::read(). Serializes access to
       the disk so that only one thread issues a command at a time. */

    virtual void write(unsigned long _block_no, unsigned char* _buf) override;
    /* Thread-safe wrapper around SimpleDisk::write(). */
#endif

protected:
    virtual void wait_while_busy() override;
    /* Override of SimpleDisk::wait_while_busy().
       In this implementation, we avoid busy-waiting when a scheduler is
       available by yielding the CPU while the disk is busy. If no scheduler
       is compiled in or initialized, we fall back to the original behavior. */

#ifdef _USES_SCHEDULER_
    virtual void handle_interrupt(REGS* _regs) override;
    /* IRQ 14 handler. We register this object as the handler for the disk
       interrupt to suppress the default "NO DEFAULT INTERRUPT HANDLER"
       message. The actual PIO protocol is still handled synchronously. */
#endif

private:
#if defined(_USES_SCHEDULER_) && defined(_THREADSAFE_DISK_)
    /* Simple per-disk mutex for thread-safe access (Options 1 and 2). */
    volatile bool disk_locked = false;

    void lock_disk();
    void unlock_disk();
#endif
};

#endif /* _NONBLOCKING_DISK_H_ */
