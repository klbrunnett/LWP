#include "lwp.h"
#include <stdlib.h>
#include <stdio.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>

void rr_remove(thread victim);
void rr_admit(thread newContext);
thread rr_next();

static tid_t tidCounter = 1;
static thread head = NULL;
static thread prev = NULL;
static thread current = NULL;
static struct scheduler rr_publish = 
{NULL, NULL, rr_admit, rr_remove, rr_next};
static scheduler schedulerState = &rr_publish;
static rfile originalRegs;

static thread rrHead = NULL;

/*
 * Creates a new lightweight process which executes the given function
 * with the given argument. The new processes’s stack will be
 * stacksize words.
 * lwp_create() returns the (lightweight) thread id of the new thread
 * ir −1 if the thread cannot be created.
 */
tid_t lwp_create(lwpfun function, void *argument, size_t stacksize) {
   thread myThread = malloc(sizeof(context));
   
   // if the thread linked list is empty, initialize it
   if (head == NULL) {
      head = myThread;
   }
   myThread->tid = tidCounter++;    // number new threads in order
   myThread->stacksize = stacksize;
   myThread->stack = malloc(stacksize * sizeof(unsigned long));
   
   // write to the stack top-down (decreasing address)
   unsigned long *sp = myThread->stack + stacksize;

   *(--sp) = (unsigned long) lwp_exit;    // push return address
   *(--sp) = (unsigned long) function;    // push the thread's function

   // gcc calling convention (rdi = arg, align rbp, rsp)
   myThread->state.rdi = (unsigned long) argument;
   myThread->state.rbp = (unsigned long) --sp;
   myThread->state.rsp = (unsigned long) sp;

   myThread->state.fxsave=FPU_INIT;    // initialize floating point state
   
   // lib_one: prev, lib_two: next
   myThread->lib_one = prev;
   myThread->lib_two = NULL;

   // maintain the linked list of threads
   if (prev) {
      prev->lib_two = myThread;
   }

   prev = myThread;
   schedulerState->admit(myThread);    // add the new thread to the scheduler

   return myThread->tid;
}


/*
 * Returns the tid of the calling LWP or NO THREAD if not called by a
 * LWP.
 */
tid_t lwp_gettid(void) {
   return current->tid;
}

/*
 * Terminates the current LWP and frees its resources. Calls
 * sched->next() to get the next thread. If there are no other
 * threads, restores the original system thread.
 */
void lwp_exit() {
   thread threadToFree = current;

   // if the last thread is exiting, move the prev pointer
   if (threadToFree == prev) {
      prev = threadToFree->lib_one;
   }

   // remove the thread to the scheduler
   schedulerState->remove(threadToFree);

   // get the new thread from the scheduler
   thread newThread = schedulerState->next();

   if (newThread) {     // if there are more threads, start the next one
      current = newThread;
      swap_rfiles(NULL, &newThread->state);  // load the new thread's context
      
      // free the old thread's resources
      free(threadToFree->stack);
      free(threadToFree);
   }
   else {               // otherwise stop lwp
      lwp_stop();
   }
}

/*
 * Yields control to another LWP. Which one depends on the scheduler.
 * Saves the current LWP’s context, picks the next one, restores
 * that thread’s context, and returns.
 */
void lwp_yield() {
   thread threadToStart = schedulerState->next();  // get the next thread

   if (threadToStart) {    // if there is a thread, start it
      swap_rfiles(&current->state, NULL);    // save the last thread's state
      current = threadToStart;
      swap_rfiles(NULL, &threadToStart->state); // load the new thread's state
   }
   else {   // otherwise stop lwp
      lwp_stop();  
   }
}

/* 
 * Starts the LWP system. Saves the original context (for lwp stop()
 * to use later), picks a LWP and starts it running. If there are no
 * LWPs, returns immediately.
 */
void lwp_start() {
   if (!head) {   // if there are no threads, return
      return;
   }

   current = schedulerState->next();   // start the first thread

   if (current) {    // if there is a thread, load its context
      swap_rfiles(&originalRegs,  &current->state);  
   }
}

/*
 * Stops the LWP system, 
 * restores the original stack pointer and returns
 * to that context. (Wherever lwp start() was called from.
 * lwp stop() does not destroy any existing contexts, and thread
 * processing will be restarted by a call to lwp start().
 */
void lwp_stop() {
   // load the original caller's context
   swap_rfiles(&current->state, &originalRegs);
}

/*
 * Causes the LWP package to use the given scheduler to choose the
 * next process to run. Transfers all threads from the old scheduler to
 * the new one in next() order. If scheduler is NULL, or has never
 * been set, the scheduler should do round-robin scheduling.
 */
void lwp_set_scheduler(scheduler fun) {
   thread curr;

   // if the scheduler has an init function, call it
   if (fun->init) {
      fun->init();
   }

   // move threads from old scheduler to the new one
   while (curr = schedulerState->next()) {
      schedulerState->remove(curr);
      fun->admit(curr);
   }

   // if the scheduler has a shutdown, call it
   if (schedulerState->shutdown) {
      schedulerState->shutdown();
   }

   schedulerState = fun;
}

/*
 * Returns the pointer to the current scheduler.
 */
scheduler lwp_get_scheduler() {
   return schedulerState;
}

/*
 * Returns the thread corresponding to the given thread ID, or NULL
 * if the ID is invalid
 */

thread tid2thread(tid_t tid) {
   thread curr = head;

   // look for the thread in the linked list
   while (curr) {
      if (curr->tid == tid) {
         return curr;            // found it!
      }
      curr = curr->lib_two;
   }

   // we didn't find it :(
   return NULL;
}

// --- Round Robin Scheduler functions ---

/*
 *  Adds a new thread to the end of the list of threads
 */
void rr_admit(thread newContext) {
   thread curr = rrHead;

   while (curr && curr->sched_two) {
      curr = curr->sched_two;
   }

   if (curr) {
      curr->sched_two = newContext;
   }
   else {
      rrHead = newContext;
   }
}

/*
 *  Removes a thread from the list of threads by its address.
 */
void rr_remove(context *victim) {
   context *toRemove = rrHead;
   context *prev = NULL;

   // traverse the list, looking for the victim
   while (toRemove && toRemove != victim) {
      prev = toRemove;
      toRemove = toRemove->sched_two;
   }

   // if it wasn't the first one, restore the previous thread's 'next'
   if (prev && toRemove) {

      // we found the thread, fix up the linked list
      prev->sched_two = toRemove->sched_two;
      if (toRemove->sched_two) {
         toRemove->sched_two->sched_one = prev;
      }
   }
   else if (rrHead == victim && rrHead->sched_two) {
      // the thread is the first of multiple threads
      rrHead = rrHead->sched_two;
      rrHead->sched_one = NULL;
   }
   else {
      // didn't find the thread
      rrHead = NULL;
   }
}


/*
 *  Gets the next thread in the list
 */
thread rr_next() {
   thread toReturn = rrHead;
   if (toReturn) {      // if there is a thread to run
      // get the new head, and set up it's pointers
      rrHead = rrHead->sched_two;
      if (rrHead) {
         rrHead->sched_one = NULL;
      }

      // remove the thread from the list and re-admit it
      toReturn->sched_one = NULL;
      toReturn->sched_two = NULL;

      rr_admit(toReturn);
   }
   return toReturn;
}