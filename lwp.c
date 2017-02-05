#include "lwp.h"
#include <stdlib.h>
#include <stdio.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>

void rr_remove(context *victim);
void rr_admit(context *newContext);
context *rr_next();

static tid_t tidCounter = 1;
static context *head = NULL;
static context *prev = NULL;
static context *current = NULL;
static struct scheduler rr_publish = 
{NULL, NULL, rr_admit, rr_remove, rr_next};
static scheduler schedulerState = &rr_publish;
static rfile originalRegs;

static context *rrHead = NULL;

/*
 * Creates a new lightweight process which executes the given function
 * with the given argument. The new processes’s stack will be
 * stacksize words.
 * lwp_create() returns the (lightweight) thread id of the new thread
 * ir −1 if the thread cannot be created.
 */
tid_t lwp_create(lwpfun function, void *argument, size_t stacksize) {
   context *myThread = malloc(sizeof(context));
   if (head == NULL) {
      head = myThread;
   }
   myThread->tid = tidCounter++;
   myThread->stacksize = stacksize;
   myThread->stack = malloc(stacksize * sizeof(unsigned long));
   
   unsigned long *sp = myThread->stack + stacksize;

   *(sp - 1) = (unsigned long) lwp_exit;
   *(sp - 2) = (unsigned long) function;

   myThread->state.rdi = (unsigned long) argument;
   myThread->state.rbp = (unsigned long) (sp - 3);
   myThread->state.rsp = (unsigned long) (sp - 3);

   myThread->state.fxsave=FPU_INIT;
   
   myThread->lib_one = prev;
   myThread->lib_two = NULL;

   if (prev) {
      prev->lib_two = myThread;
   }

   prev = myThread;
   schedulerState->admit(myThread);

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
   printf("create");
 * threads, restores the original system thread.
 */
void lwp_exit() {
   context *threadToFree = current;

   schedulerState->remove(threadToFree);
   context *newThread = schedulerState->next();

   if (newThread) {
      current = newThread;
      swap_rfiles(NULL, &newThread->state); //load
      free(threadToFree->stack);
      free(threadToFree);
   }
   else {
      lwp_stop();
   }
}

/*
 * Yields control to another LWP. Which one depends on the scheduler.
 * Saves the current LWP’s context, picks the next one, restores
 * that thread’s context, and returns.
 */
void lwp_yield() {
   context *threadToStart;

   threadToStart = schedulerState->next();

   if (threadToStart) {
      swap_rfiles(&current->state, NULL); //save
      current = threadToStart;
      swap_rfiles(NULL, &threadToStart->state); //load
   }
   else {
      lwp_stop();  
   }
}

/* 
 * Starts the LWP system. Saves the original context (for lwp stop()
 * to use later), picks a LWP and starts it running. If there are no
 * LWPs, returns immediately.
 */
void lwp_start() {
   if (!head) {
      return;
   }
   context *threadToStart;
   threadToStart = schedulerState->next();

   current = threadToStart;

   if (current) {
      swap_rfiles(&originalRegs,  &current->state); //load  
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
   swap_rfiles(&current->state, &originalRegs); //load
}

/*
 * Causes the LWP package to use the given scheduler to choose the
 * next process to run. Transfers all threads from the old scheduler to
 * the new one in next() order. If scheduler is NULL, or has never
 * been set, the scheduler should do round-robin scheduling.
 */
void lwp_set_scheduler(scheduler fun) {
   context *curr;

   if (fun->init) {
      fun->init();
   }

   while (curr = schedulerState->next()) {
      schedulerState->remove(curr);
      fun->admit(curr);
   }

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
   context *curr = head;
   while (curr) {
      if (curr->tid == tid) {
         return curr;
      }
      curr = curr->lib_two;
   }
   return NULL;
}


void rr_admit(context *newContext) {
   context *curr = rrHead;

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

void rr_remove(context *victim) {
   context *toRemove = rrHead;
   context *prev = NULL;

   while (toRemove && toRemove != victim) {
      prev = toRemove;
      toRemove = toRemove->sched_two;
   }

   if (prev) {
      if (toRemove) {
         prev->sched_two = toRemove->sched_two;
         if (toRemove->sched_two) {
            toRemove->sched_two->sched_one = prev;
         }
      }
   }
   else if (rrHead == victim) {
      if (rrHead->sched_two) {
         rrHead = rrHead->sched_two;
         rrHead->sched_one = NULL;
      }
      else {
         rrHead = NULL;
      }
   }
   else {
      rrHead = NULL;
   }
}

context *rr_next() {
   context *toReturn = rrHead;
   if (toReturn) {
      rrHead = rrHead->sched_two;
      if (rrHead) {
         rrHead->sched_one = NULL;
      }

      toReturn->sched_one = NULL;
      toReturn->sched_two = NULL;

      rr_admit(toReturn);
   }
   return toReturn;
}