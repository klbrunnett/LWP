#include "lwp.h"
#include <stdlib.h>
#include <stdio.h>

int main() {
// 	rfile *regs = malloc(sizeof(rfile));
// 	save_context(regs);
// 	regs->r14 = 1;
// 	load_context(regs);

// 	// swap_rfiles(NULL, regs);

// 	printf("%lu\n", regs->r14);
}

// void rr_admit(context *newContext);
// void rr_remove(context *victim);
context *rr_next();

static tid_t tidCounter = 1;
static context *head = NULL;
static context *prev = NULL;
static context *current = NULL;
static struct scheduler rr_publish = {NULL, NULL, NULL, NULL, rr_next};
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
	context *myThread = malloc(sizeof(context) + stacksize + 2 * sizeof(unsigned long));
	if (!head) {
		head = myThread;
	}
	myThread->tid = tidCounter++;
	myThread->stacksize = stacksize;
	myThread->stack = (unsigned long*)(myThread + 1);

	*(myThread->stack) = (unsigned long) lwp_exit;
	*(myThread->stack + 1) = (unsigned long) function;

	myThread->lib_one = myThread->sched_one = prev;
	myThread->lib_two = myThread->sched_two = NULL;

	if (!prev) {
		prev->lib_two = prev->sched_two = myThread;
	}

	prev = myThread;

	schedulerState->admit(myThread);

	myThread->state.rdi = *(unsigned long *) argument;
	myThread->state.rbp = (unsigned long) myThread->stack - 1;
	myThread->state.rsp = (unsigned long) myThread->stack + 1;

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
	context *threadToFree = current;

}

/*
 * Yields control to another LWP. Which one depends on the scheduler.
 * Saves the current LWP’s context, picks the next one, restores
 * that thread’s context, and returns.
 */
void lwp_yield() {
	context *threadToStart;

	threadToStart = schedulerState->next();

	save_context(&current->state);
	current = threadToStart;
	load_context(&current->state);

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

	save_context(originalRegs);
	load_context(&threadToStart->state);

	current = threadToStart;

	SetSP(current->stack + 1);

	lwpfun functionToCall = (lwpfun) current->state.rsp;
	functionToCall((void *)current->state.rdi);
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
}

/*
 * Causes the LWP package to use the given scheduler to choose the
 * next process to run. Transfers all threads from the old scheduler to
 * the new one in next() order. If scheduler is NULL, or has never
 * been set, the scheduler should do round-robin scheduling.
 */
void lwp_set_scheduler(scheduler fun) {
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

   if (prev && toRemove) {
      prev->sched_two = toRemove->sched_two;
      if (toRemove->sched_two) {
         toRemove->sched_two->sched_one = prev;
      }
   }
}

context *rr_next() {
   context *toReturn = rrHead;
   if (rrHead) {
      rr_admit(toReturn);
      rrHead = rrHead->sched_two;
   }
   return toReturn;
}