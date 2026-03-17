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
#include "assert.H"
#include "machine.H"
#include "macros_config.H"

/*--------------------------------------------------------------------------*/
/* READY QUEUE: SIMPLE FIFO */
/*--------------------------------------------------------------------------*/

struct ReadyNode {
  Thread*    th;
  ReadyNode* next;
  ReadyNode(Thread* t) : th(t), next(nullptr) {}
};

static ReadyNode* rq_head = nullptr;
static ReadyNode* rq_tail = nullptr;

static inline void rq_push_tail(Thread* t) {
  ReadyNode* n = new ReadyNode(t);
  if (!rq_tail) {
    rq_head = rq_tail = n;
  } else {
    rq_tail->next = n;
    rq_tail       = n;
  }
}

static inline Thread* rq_pop_head() {
  if (!rq_head) return nullptr;
  ReadyNode* n = rq_head;
  rq_head      = rq_head->next;
  if (!rq_head) rq_tail = nullptr;
  Thread* t    = n->th;
  delete n;
  return t;
}

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler::Scheduler() 
{
  Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield() 
{
  Thread* me = Thread::CurrentThread();
  assert(me != nullptr);

  Machine::disable_interrupts();

  /* Enqueue the current thread at the tail. */
  rq_push_tail(me);

  /* Pick the next runnable thread from the head. */
  Thread* next = rq_pop_head();

  /* Check whether we are the only runnable thread. In that case,
     next == me and the queue is now empty, so just continue running. */
  bool alone = (next == me && rq_head == nullptr);

  Machine::enable_interrupts();

  if (!next || alone) {
    /* Nobody else runnable → keep executing this thread. */
    return;
  }

  /* Switch to the next thread. */
  Thread::dispatch_to(next);
}

void Scheduler::resume(Thread * _thread) 
{
  if (!_thread) return;

  Thread* me = Thread::CurrentThread();

  /* If resume() is called on the currently running thread (as in pass_on_CPU),
     we ignore it here, because yield() will enqueue 'me' explicitly. */
  if (_thread == me) {
    return;
  }

  Machine::disable_interrupts();
  rq_push_tail(_thread);
  Machine::enable_interrupts();
}

void Scheduler::add(Thread * _thread) 
{
  if (!_thread) return;

  Machine::disable_interrupts();
  rq_push_tail(_thread);
  Machine::enable_interrupts();
}

void Scheduler::terminate(Thread * _thread) 
{
  /* Not used in this MP: threads never terminate in kernel.C.
     If somehow called, just assert to catch it during debugging. */
  (void)_thread;
  assert(false && "Scheduler::terminate() not supported in this MP");
}

void Scheduler::terminate_self()
{
  /* Not used in this MP: thread functions never return in kernel.C.
     If somehow called, just spin to avoid corrupting context. */
  for (;;) { /* spin */ }
}

#ifdef _RR_SCHEDULER_
void Scheduler::eoq_preempt_isr() {
  /* Not used in this MP (we are not enabling RR). */
}
#endif
