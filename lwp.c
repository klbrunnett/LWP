#include "lwp.h"
#include <stdlib.h>
#include <stdio.h>

void rr_remove(context *victim);
void rr_admit(context *newContext);
context *rr_next();

static tid_t tidCounter = 1;
static context *head = NULL;
static context *prev = NULL;
static context *current = NULL;
static struct scheduler rr_publish = {NULL, NULL, rr_admit, rr_remove, rr_next};
static scheduler schedulerState = &rr_publish;
static rfile *originalRegs = NULL;

static context *rrHead = NULL;
/*
 * Creates a new lightweight process which executes the given function
 * with the given argument. The new processes’s stack will be
 * stacksize words.
 * lwp_create() returns the (lightweight) thread id of the new thread
 * ir −1 if the thread cannot be created.
 */
tid_t lwp_create(lwpfun function, void *argument, size_t stacksize) {
   printf("\nCREATE\n");
   context *myThread = malloc(sizeof(context));
   if (head == NULL) {
      head = myThread;
   }
   myThread->tid = tidCounter++;
   myThread->stacksize = stacksize;
   myThread->stack = malloc(stacksize * sizeof(unsigned long));
   
   unsigned long *sp = myThread->stack + stacksize;

   printf("thread %d: %p\n", myThread->tid, myThread);
   printf("fuctionPtr: %p\n", function);
   printf("stack: %p\n", sp);




   *(sp - 1) = (unsigned long) lwp_exit;
   *(sp - 2) = (unsigned long) function;

   // printf("functionPtr:%p\n", *(sp - 2));

   myThread->state.rdi = (unsigned long) argument;
   myThread->state.rbp = (unsigned long) (sp - 3);
   myThread->state.rsp = (unsigned long) (sp - 3);

   myThread->state.fxsave=FPU_INIT;

   printf("%p\n", (unsigned long*)myThread->state.rsp);

   
   myThread->lib_one = prev;
   myThread->lib_two = NULL;

   if (prev) {
      prev->lib_two = myThread;
   }

   prev = myThread;

   schedulerState->admit(myThread);
   // rrPrintQueue();

   printStackFrame(myThread);
   rFilePrint(myThread);

   return myThread->tid;
}

void printStackFrame(context *currThread) {
   printf("\n\nprinting thread %d: context %p's stack frame\n", currThread->tid, currThread);
   unsigned long *sp = currThread->stack + currThread->stacksize;

   int i;
   for(i = 0; i < 10; i++) {
      printf("%p: %p\n", sp, *sp);
      sp--;
   }

   printf("end printing stack frame\n\n");
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
   printf("exit\n");
   //might be wrong...
   context *threadToFree = current;

   schedulerState->remove(threadToFree);

   free(threadToFree);

   context *newThread = schedulerState->next();
   if (newThread) {
      current = newThread;
      swap_rfiles(NULL, &newThread->state); //load
   }
   else {
      printf("done\n");
      swap_rfiles(NULL, originalRegs); //load
   }
}

/*
 * Yields control to another LWP. Which one depends on the scheduler.
 * Saves the current LWP’s context, picks the next one, restores
 * that thread’s context, and returns.
 */
void lwp_yield() {
   printf("yeild\n");
   context *threadToStart;

   threadToStart = schedulerState->next();

   swap_rfiles(&current->state, NULL); //save
   current = threadToStart;
   swap_rfiles(NULL, &current->state); //load

}

void rrPrintQueue() {
   printf("\n\nprinting rr queue\n");
   context *curr = rrHead;
   while (curr) {
      printf("%p\n", curr);
      curr = curr->sched_two;
   }
   printf("end rr queue\n\n");
}

void rFilePrint(context *currThread) {
   printf("\n\nRegisters\n");
   printf("thread %d: context %p's registers\n", currThread->tid, currThread);
   rfile *reg = &currThread->state;
   printf("rax: %lu\n", reg->rax);
   printf("rbx: %lu\n", reg->rbx);
   printf("rcx: %lu\n", reg->rcx);
   printf("rdx: %lu\n", reg->rdx);
   printf("rsi: %lu\n", reg->rsi);

   printf("rdi: %lu\n", reg->rdi);
   printf("rbp: %p\n", reg->rbp);
   printf("rsp: %p\n", reg->rsp);
   
   printf("r8: %lu\n", reg->r8);
   printf("r9: %lu\n", reg->r9);
   printf("r10: %lu\n", reg->r10);
   printf("r11: %lu\n", reg->r11);
   printf("r12: %lu\n", reg->r12);
   printf("r13: %lu\n", reg->r13);
   printf("r14: %lu\n", reg->r14);
   printf("r15: %lu\n", reg->r15);
}
/* 
 * Starts the LWP system. Saves the original context (for lwp stop()
 * to use later), picks a LWP and starts it running. If there are no
 * LWPs, returns immediately.
 */
void lwp_start() {
   printf("\nSTART\n");

   if (!head) {
      return;
   }
   context *threadToStart;
   threadToStart = schedulerState->next();


   current = threadToStart;
   printContext(current);

   printf("starting thread id: %d\n", current->tid);

   unsigned long *sp = current->state.rsp;
   lwpfun functionToCall = (lwpfun) *(sp);

   rFilePrint(current);
   printStackFrame(current);


   printf("before swap_rfiles\n");
   swap_rfiles(originalRegs, NULL); //save
   swap_rfiles(NULL, &threadToStart->state); //load
   printf("after swap_rfiles\n");


   functionToCall((void *)current->state.rdi);

   printf("end start\n");
}

/*
 * Stops the LWP system, 
 * restores the original stack pointer and returns
 * to that context. (Wherever lwp start() was called from.
 * lwp stop() does not destroy any existing contexts, and thread
 * processing will be restarted by a call to lwp start().
 */
void lwp_stop() {
   //destroy the schduler

   printf("stop\n");

   swap_rfiles(NULL, originalRegs); //load
   SetSP(originalRegs->rsp);
}

/*
 * Causes the LWP package to use the given scheduler to choose the
 * next process to run. Transfers all threads from the old scheduler to
 * the new one in next() order. If scheduler is NULL, or has never
 * been set, the scheduler should do round-robin scheduling.
 */
void lwp_set_scheduler(scheduler fun) {
   printf("lwp_set_scheduler\n");
   context *curr;
   fun->init();

   while (!(curr = schedulerState->next())) {
      schedulerState->remove(curr);
      fun->admit(curr);
   }
   schedulerState->shutdown();

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
   while (!curr) {
      if (curr->tid == tid) {
         return curr;
      }
   }
   return NULL;
}


void rr_admit(context *newContext) {
   printf("admit\n");
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
   printf("end admit\n");
}

void rr_remove(context *victim) {
   printf("remove\n");
   context *toRemove = rrHead;
   context *prev = NULL;
   while (toRemove && toRemove != victim) {
      prev = toRemove;
      toRemove = toRemove->sched_two;
   }

   if (prev && toRemove) {
      prev->sched_two = toRemove->sched_two;
      if (toRemove->sched_two) {
         toRemove->sched_two->sched_one = prev;
      }
   }
}

void printContext(context *thisThread) {
   printf("\n\nthread info\n");
   printf("thread id: %d\n", thisThread->tid);
   printf("end thread info\n\n");
}

context *rr_next() {
   printf("next\n");
   context *toReturn = rrHead;
   if (toReturn) {
      rrHead = rrHead->sched_two;
      rrHead->sched_one = NULL;

      toReturn->sched_one = NULL;
      toReturn->sched_two = NULL;

      rr_admit(toReturn);
   }
   printContext(toReturn);
   return toReturn;
}