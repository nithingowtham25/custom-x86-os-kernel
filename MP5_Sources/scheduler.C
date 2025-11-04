/*
 File: scheduler.C
 
  Author: Nithin Gowtham Saravanan
          Department of Electrical and Computer Engineering
          Texas A&M University
  Date  : 11/01/2025
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "machine.H"
#include "macros_config.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/
static Thread* idle_thread = nullptr;

static void idle_body() 
{
  for (;;) 
  {
    // keep interrupts enabled so timer/IRQs work. no enqueue/yield here
    Machine::enable_interrupts();
  }
}

/* remember the most recent thread that called yield() */
static Thread* last_yielder = nullptr;

/* minimal FIFO queue node so we don't touch Thread's private fields. */
struct ReadyNode {
  Thread*    th;
  ReadyNode* next;
  ReadyNode(Thread* t) : th(t), next(nullptr) {}
};

/* private state (file-local) for a simple FIFO ready queue. */
static ReadyNode* rq_head = nullptr;
static ReadyNode* rq_tail = nullptr;

/* enqueue helper (tail). */
static inline void rq_push_tail(Thread* t) {
  ReadyNode* n = new ReadyNode(t);
  if (!rq_tail) 
    rq_head = rq_tail = n;
  else 
  { 
    rq_tail->next = n; 
    rq_tail = n; 
  }
}

/* dequeue helper (head). */
static inline Thread* rq_pop_head() 
{
  if (!rq_head) 
    return nullptr;
  //Console::puts("[sched] pop head T"); Console::puti(rq_head->th->ThreadId()); Console::puts("\n"); //debug
  ReadyNode* n = rq_head;
  rq_head = rq_head->next;
  if (!rq_head) 
    rq_tail = nullptr;
  Thread* t = n->th;
  delete n;
  return t;
}

/* remove a specific thread from the ready queue (if present). */
static inline bool rq_remove(Thread* target) {
  ReadyNode* prev = nullptr;
  ReadyNode* cur  = rq_head;
  while (cur) 
  {
    if (cur->th == target) 
    {
      if (prev) 
        prev->next = cur->next;
      else      
        rq_head    = cur->next;
      if (rq_tail == cur) 
        rq_tail = prev;
      delete cur;
      return true;
    }
    prev = cur;
    cur  = cur->next;
  }
  return false;
}

/*--------------------------------------------------------------------------*/
/* ZOMBIE QUEUE (file-local helpers) */
/*--------------------------------------------------------------------------*/

/* zombie nodes for threads that terminated and must be reaped by other contexts */
struct ZombieNode {
  Thread* th;
  ZombieNode* next;
  ZombieNode(Thread* t) : th(t), next(nullptr) {}
};
static ZombieNode* zombie_head = nullptr;

/* enqueue thread to zombie list (caller must hold interrupts disabled if required) */
static inline void enqueue_zombie(Thread* t) {
  if (!t) 
    return;
  ZombieNode* n = new ZombieNode(t);
  n->next = zombie_head;
  zombie_head = n;
}

/* cleanup: free stacks and delete Thread objects */
static inline void cleanup_zombies() {
  ZombieNode* cur = zombie_head;
  while (cur) 
  {
    ZombieNode* nxt = cur->next;
    Thread* t = cur->th;
    /* free stack memory allocated by kernel (new char[]) */
    char* base = t->get_stack_base();
    if (base) 
      delete[] base;
    delete t;
    delete cur;
    cur = nxt;
  }
  zombie_head = nullptr;
}

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler::Scheduler() 
{  
  if (!idle_thread) 
  {
    char* idle_stack = new char[1024];
    idle_thread = new Thread(&idle_body, idle_stack, 1024);
  }
  Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield() 
{
  Thread* me_dbg = Thread::CurrentThread(); 
  assert(me_dbg != nullptr); /* sanity check we have a running thread */

  last_yielder = me_dbg;            /* remember who yielded */
  
  /* cooperative FIFO — current was already enqueued by pass_on_CPU()->resume() */
  Machine::disable_interrupts();    /* guard queue ops against timer IRQs */

  cleanup_zombies();                /* ensures no leak when only yielding */

  if (!rq_head) {                   /* nobody ready → keep running */
    Machine::enable_interrupts();
    return;
  }

  Thread* next = rq_pop_head();  /* pick next from head */
  Machine::enable_interrupts();  /* leave critical section */

  Thread::dispatch_to(next);     /* context switch; returns when we run again */
}

void Scheduler::resume(Thread * _thread) 
{
  /* Add the given thread to the ready queue of the scheduler. This is called
     for threads that were waiting for an event to happen, or that have 
     to give up the CPU in response to a preemption. */
  if (!_thread) 
   return;
  Machine::disable_interrupts();                 /* guard queue ops */

  /* reap any zombies (runs while interrupts disabled) */
  cleanup_zombies();

  if (_thread == idle_thread)    /* never enqueue idle */
  {                  
    Machine::enable_interrupts();
    return;
  }
//   Console::puts("[sched] resume T");             /* trace */
//   Console::puti(_thread->ThreadId());
//   Console::puts("\n");
  rq_push_tail(_thread);                         /* enqueue at tail */
  Machine::enable_interrupts();                  /* done */
}

void Scheduler::add(Thread * _thread) 
{
  /* Make the given thread runnable by the scheduler. This function is called
     after thread creation. Depending on implementation, this function may 
     just add the thread to the ready queue, using 'resume'. */
  if (!_thread) 
    return;
  Machine::disable_interrupts();                 /* guard queue ops */

  /* opportunistically reap any zombies */
  cleanup_zombies();

  if (_thread == idle_thread)       /* never enqueue idle */
  {
    Machine::enable_interrupts();
    return;
  }
  // Console::puts("[sched] add T");                /* trace */
  // Console::puti(_thread->ThreadId());
  // Console::puts("\n");
  rq_push_tail(_thread);                         /* enqueue at tail */
  Machine::enable_interrupts();                  /* done */
}

void Scheduler::terminate(Thread * _thread) 
{
  /* Remove the given thread from the scheduler in preparation for destruction
     of the thread. 
     Graciously handle the case where the thread wants to terminate itself.*/
  if (!_thread) 
    return;

  Thread* me = Thread::CurrentThread();

  if (_thread == me) {                           /* running thread wants to terminate */
    Machine::disable_interrupts();               /* atomic pick of successor */

    /* don't ever reap/queue idle thread as zombie */
    if (me != idle_thread)  /* protect idle thread from deletion */
    {                     
      rq_remove(me);
      enqueue_zombie(me);
    } 
    else 
    {
      rq_remove(me);        /* keep idle out of RQ if it ever gets here */
    }

    Thread* next = rq_pop_head();                /* choose next runnable (if any) */
    Machine::enable_interrupts();

    if (!next) 
    {
      if (last_yielder) 
      {                             
        Machine::disable_interrupts();                
        rq_remove(last_yielder);                      
        Machine::enable_interrupts();                 
        // Console::puts("[sched] terminate: switching to last_yielder T"); 
        // Console::puti(last_yielder->ThreadId()); Console::puts("\n");    
        Thread::dispatch_to(last_yielder);            
        return;                                       
      }
      next = idle_thread;   // fallback (existing)
    }

    /* DO NOT free your own stack here; another thread must reap it safely. */

    Thread::dispatch_to(next);                   /* never returns */
    return;                                      /* not reached */
  }

  /* terminating a non-running thread: best-effort remove from ready queue. */
  if (_thread == idle_thread)     /* never delete idle explicitly */
  {                  
    return;
  }

  Machine::disable_interrupts();                 /* guard queue ops */
  bool removed = rq_remove(_thread);             /* remove if enqueued */
  (void)removed;
  Machine::enable_interrupts();

  /* Reclaim resources now that we are not the thread itself. */
  char* base = _thread->get_stack_base();
  if (base) delete[] base;
  delete _thread;
}

/* helper used by thread_shutdown() for current thread */
void Scheduler::terminate_self()
{
  // Console::puts("[sched] terminate_self ENTER\n");
  // Console::puts("[sched] TERMINATE_SELF PATCH ACTIVE\n");
  Machine::disable_interrupts();

  /* reap zombies here as well to eliminate leaks when only terminations happen */
  cleanup_zombies();    /* reaper on self-termination path */

  Thread* me = Thread::CurrentThread();

  /* remove ourselves from ready queue if present */
  rq_remove(me);

  /* park current thread as zombie (reaped later), unless it's the idle thread */
  if (me != idle_thread) /* don't zombie the idle thread */
  {                        
    enqueue_zombie(me);
  }

  Thread* next = rq_pop_head();
  // Console::puts(next ? "[sched] terminate_self: have next\n"
  //                  : "[sched] terminate_self: rq empty\n");
  Machine::enable_interrupts();

  if (!next) 
  {
    //Console::puts("[sched] terminate_self: rq empty"); Console::puts("\n");  /* trace */

    if (last_yielder)   /* prefer the last thread that yielded */
    {                               
      Machine::disable_interrupts();                  
      rq_remove(last_yielder);          /* avoid duplicate if it was enqueued */
      Machine::enable_interrupts();                   
      // Console::puts("[sched] terminate_self: switching to last_yielder T"); 
      // Console::puti(last_yielder->ThreadId()); Console::puts("\n");         
      Thread::dispatch_to(last_yielder);  /* never returns */
      return;                                         
    }

    //Console::puts("[sched] terminate_self: switching to idle\n");            /* keep idle as last resort */
    next = idle_thread;
  } 
  else 
  {
    // Console::puts("[sched] terminate_self: next = ");
    // Console::puti(next->ThreadId());
    // Console::puts("\n");
  }

  Thread::dispatch_to(next);
}