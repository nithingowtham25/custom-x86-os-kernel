/*
     File        : nonblocking_disk.C

     Author      : Nithin Gowtham Saravanan

     Description : Non-blocking disk implementation.
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
#include "interrupts.H"   // InterruptHandler::register_handler

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
/* METHODS FOR CLASS   N o n B l o c k i n g D i s k                        */
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
