/*
     File        : nonblocking_disk.c

     Author      : 
     Modified    : 

     Description : 

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

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

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

NonBlockingDisk::NonBlockingDisk(unsigned int _size) 
  : SimpleDisk(_size) {
}

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   N o n B l o c k i n g D i s k                        */
/*--------------------------------------------------------------------------*/

void NonBlockingDisk::wait_while_busy()
{
   /* 
      Baseline non-blocking behavior:

      - When a scheduler is enabled and initialized, avoid tying up the CPU
        in a tight busy loop while the disk is busy.
        Instead, repeatedly:
          * check if the disk is still busy, and
          * yield the CPU to let other threads run.

      - If the scheduler is not compiled in (_USES_SCHEDULER_ not defined) or
        not yet initialized (System::SCHEDULER == nullptr), we fall back to
        the original SimpleDisk::wait_while_busy() behavior so that the
        system remains functional in those configurations.
   */

#ifdef _USES_SCHEDULER_
   while (is_busy()) {
      if (System::SCHEDULER != nullptr) {
         /* Cooperative waiting: give the CPU to another ready thread. */
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
