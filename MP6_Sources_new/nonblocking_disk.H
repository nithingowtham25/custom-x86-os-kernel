/*
     File        : nonblocking_disk.H

     Author      : Nithin Gowtham Saravanan

     Description : Non-blocking disk built on top of SimpleDisk.
*/

#ifndef _NONBLOCKING_DISK_H_
#define _NONBLOCKING_DISK_H_

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "simple_disk.H"
#include "macros_config.H"
#include "interrupts.H"     // for InterruptHandler / REGS

/*--------------------------------------------------------------------------*/
/* N o n B l o c k i n g D i s k  */
/*--------------------------------------------------------------------------*/

class NonBlockingDisk 
  : public SimpleDisk
#ifdef _USES_SCHEDULER_
  , public InterruptHandler   // only meaningful when scheduler build is on
#endif
{
public:
   NonBlockingDisk(unsigned int _size); 
   /* Creates a NonBlockingDisk device with the given size connected to the 
      MASTER slot of the primary ATA controller.
      NOTE: We are passing the _size argument out of laziness. 
      In a real system, we would infer this information from the 
      disk controller. */

protected:
   virtual void wait_while_busy() override;
   /* Override of SimpleDisk::wait_while_busy().
      In this implementation, we avoid tight busy-waiting when a scheduler is
      available by yielding the CPU while the disk is busy. If no scheduler
      is compiled in or initialized, we fall back to the original behavior. */

#ifdef _USES_SCHEDULER_
   virtual void handle_interrupt(REGS* _regs) override;
   /* IRQ 14 handler. For now we just consume the interrupt to avoid the
      "NO DEFAULT INTERRUPT HANDLER" messages. The actual disk completion
      still uses polling in wait_while_busy(). */
#endif
};

#endif
