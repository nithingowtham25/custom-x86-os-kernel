/*
    File: thread.H

    Author: Nithin Gowtham Saravanan
            Department of Electrical and Computer Engineering
            Texas A&M University
    Date  : 11/01/2025

    Description: Thread Management. 
    
                 Defines the Thread Control Block data structure, and 
                 functions to create threads and to dispatch the 
                 execution of threads.
                 
*/

#ifndef _thread_H_                   // include file only once
#define _thread_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "machine.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- THREAD FUNCTION (CALLED WHEN THREAD STARTS RUNNING) */
typedef void (*Thread_Function)();

/*--------------------------------------------------------------------------*/
/* THREAD CONTROL BLOCK */
/*--------------------------------------------------------------------------*/

class Thread {

private: 
    char     * esp;         /* The current stack pointer for the thread.*/
                            /* Keep it at offset 0, since the thread 
                               dispatcher relies on this  location! */
    int        thread_id;   /* thread identifier. Assigned upon creation. */
    char     * stack;       /* pointer to the stack of the thread.*/
    unsigned int stack_size;/* size of the stack (in byte) */
    int        priority;    /* Maybe the scheduler wants to use priorities. */
    char     * cargo;       /* pointer to additional data that 
                               may need to be stored, typically by schedulers.
                               (for future use) */

    static int nextFreePid; /* Used to assign unique id's to threads. */

    void push(unsigned long _val);
    /* Push the given value on the stack of the thread. */

    void setup_context(Thread_Function _tfunction);
    /* Sets up the initial context for the given kernel-only thread. 
       The thread is supposed the call the function _tfunction upon start.
    */
 
public: 
    Thread(Thread_Function _tf, char * _stack, unsigned int _stack_size);
    /* Create a thread that is set up to execute the given thread function. 
       The thread is given a pointer to the stack to use. 
       NOTE: _stack points to the beginning of the stack area, 
       i.e., to the bottom of the stack.
    */

    int ThreadId();
    /* Returns the thread id of the thread. */

    static void dispatch_to(Thread * _thread);
    /* This is the low-level dispatch function that invokes the context switch
       code. This function is used by the scheduler.
       NOTE: dispatch_to does not return until the scheduler context-switches back
             to the calling thread.
    */

    static Thread * CurrentThread();
    /* Returns the currently running thread. nullptr if no thread has started 
       yet. */

    /* Accessors used by Scheduler for safe cleanup (zombie reaper) */
    /* These provide minimal, controlled access to stack base/size so the scheduler
       can free resources when reaping zombies. */
    char * get_stack_base() { return stack; }
    unsigned int get_stack_size() { return stack_size; }
};

/*--------------------------------------------------------------------------*/
/* SUPPORT FUNCTIONS */
/*--------------------------------------------------------------------------*/

/* Called automatically when a thread function returns.
   The scheduler uses it to safely terminate and context-switch away. */
extern "C" void thread_shutdown();

/* Entry stub for new threads */
extern "C" void thread_start();

#endif
