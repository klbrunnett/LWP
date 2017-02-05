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
   // printf("\nCREATE\n");
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

/*void printStackFrame(context *currThread) {
   unsigned long *sp = currThread->stack + currThread->stacksize;

   int i;
   for(i = 0; i < 10; i++) {
      printf("%p: %p\n", sp, *sp);
      sp--;
   }
}*/

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
   //might be wrong...
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

/*void rrPrintQueue() {
   printf("\n\nprinting rr queue\n");
   context *curr = rrHead;
   while (curr) {
      printf("%p\n", curr);
      curr = curr->sched_two;
   }
   printf("end rr queue\n\n");
}*/

/*void rFilePrint(context *currThread) {
   printf("\n\nRegisters\n");
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
}*/
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
   //destroy the schduler

   // SetSP((unsigned long *)originalRegs.rsp - 123);

   swap_rfiles(&current->state, &originalRegs); //load
   // asm("leaq 128(%rsi),%rax");
   // printf("fxrstor\n");
   // // asm("fxrstor (%rax)");
   // printf("rax\n");
   
   // asm("movq    (%rsi),%rax");
   // printf("rbx\n");
   // asm("movq   8(%rsi),%rbx");
   // printf("rcx\n");
   // asm("movq  16(%rsi),%rcx");
   // printf("rdx\n");
   // asm("movq  24(%rsi),%rdx");
   // printf("rdi\n");
   // asm("movq  40(%rsi),%rdi");
   // printf("rbp\n");
   // asm("movq  48(%rsi),%rbp");
   // printf("rsp\n");
   // asm("movq  56(%rsi),%rsp");
   // printf("r8\n");
   // asm("movq  64(%rsi),%r8 ");
   // printf("r9\n");
   // asm("movq  72(%rsi),%r9 ");
   // printf("r10\n");
   // asm("movq  80(%rsi),%r10");
   // printf("r11\n");
   // asm("movq  88(%rsi),%r11");
   // printf("r12\n");
   // asm("movq  96(%rsi),%r12");
   // printf("r13\n");
   // asm("movq 104(%rsi),%r13");
   // printf("r14\n");
   // asm("movq 112(%rsi),%r14");
   // printf("r15\n");
   // asm("movq 120(%rsi),%r15");
   // printf("rsi\n");
   // asm("movq  32(%rsi),%rsi");
   // printf("done\n");
}

/*
 * Causes the LWP package to use the given scheduler to choose the
 * next process to run. Transfers all threads from the old scheduler to
 * the new one in next() order. If scheduler is NULL, or has never
 * been set, the scheduler should do round-robin scheduling.
 */
void lwp_set_scheduler(scheduler fun) {
   // context *curr; 

   // while (curr = schedulerState->next()) {
   //    schedulerState->remove(curr);
   //    fun->admit(curr);
   // }

   // schedulerState = fun;
   context *curr;
   fun->init();

   while (curr = schedulerState->next()) {
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

// void printContext(context *thisThread) {
//    printf("\n\nthread info\n");
//    printf("thread id: %d\n", thisThread->tid);
//    printf("end thread info\n\n");
// }

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