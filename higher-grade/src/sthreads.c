/* On Mac OS (aka OS X) the ucontext.h functions are deprecated and requires the
   following define.
*/
#define _XOPEN_SOURCE 700

/* On Mac OS when compiling with gcc (clang) the -Wno-deprecated-declarations
   flag must also be used to suppress compiler warnings.
*/

#include <signal.h>   /* SIGSTKSZ (default stack size), MINDIGSTKSZ (minimal
                         stack size) */
#include <stdio.h>    /* puts(), printf(), fprintf(), perror(), setvbuf(), _IOLBF,
                         stdout, stderr */
#include <stdlib.h>   /* exit(), EXIT_SUCCESS, EXIT_FAILURE, malloc(), free() */
#include <ucontext.h> /* ucontext_t, getcontext(), makecontext(),
                         setcontext(), swapcontext() */
#include <stdbool.h>  /* true, false */

#include "sthreads.h"

#include <ucontext.h>
#include <sys/time.h>
#include <string.h>

/* Stack size for each context. */
#define STACK_SIZE SIGSTKSZ*100
#define TIMEOUT 50 		// ms
#define TIMER_TYPE ITIMER_REAL 	// type of timer

/*******************************************************************************
                             Global data structures

                Add data structures to manage the threads here.
********************************************************************************/
thread_t * threads;
int ind = 0;
int t_num = 0; // thread number
tid_t termin = -1; // the thread id that terminated last


/*******************************************************************************
                             Auxiliary functions

                      Add internal helper functions here.
********************************************************************************/

void init_context(ucontext_t *ctx, void(*func)(), ucontext_t *next);
void init_thread(thread_t * t, void (*start)());
void schedule();
int timer_signal(int timer_type);
void set_timer(int type, void (*handler)(int), int ms);
void stop_timer(int type, void (*handler)(int));
void timer_handler(int signum);


void init_context(ucontext_t *ctx, void(*func)(), ucontext_t *next){
	void *stack = malloc(STACK_SIZE);

	if(stack == NULL){
		perror("Allocating stack");
		exit(EXIT_FAILURE);
	}
	
	if(getcontext(ctx) < 0){
		perror("getcontext");
		exit(EXIT_FAILURE);
	}

	ctx->uc_link = next;
	ctx->uc_stack.ss_sp = stack;
	ctx->uc_stack.ss_size = STACK_SIZE;
	ctx->uc_stack.ss_flags = 0;

	makecontext(ctx, func, 0);
}

void init_thread(thread_t * t, void (*start)()){
	t->tid = t_num;
	t->state = ready;
	init_context(&(t->ctx), start, NULL);
	t->next = NULL;
}

void schedule(){
	/*for(int i=0; i<t_num; i++){
		printf("threads[%d].state: %u\n", i, threads[i].state);
	}*/
	if(t_num == 1) { // there is no ready state thread
		threads[0].state = running;
		return;
	}
	int ready_t = -1;
	int cur = -1;
	for(int i=0; i<t_num; i++){
		ind = (ind+1) % t_num;
		if(threads[ind].state == ready){
			ready_t = ind;
			break;
		}
	}
	
	for(int i=0; i<t_num; i++){
		if(threads[i].state == running){
			cur = i;
			break;
		}
	}

	set_timer(TIMER_TYPE, timer_handler, TIMEOUT);
	if(cur == -1){ // if there is no thread running
		threads[ready_t].state = running;
		setcontext(&(threads[ready_t].ctx));
	}
	else if(ready_t == -1){
		// if there is no other thread to run, run the current thread
	}
	else{ // there is thread running
		threads[cur].state = ready;
		threads[ready_t].state = running;
		
		if (swapcontext(&(threads[cur].ctx), &(threads[ready_t].ctx)) < 0) {
			perror("swapcontext");
			exit(EXIT_FAILURE);
		}
	}
}

/*		------------------ Timer Functions ------------------		*/

int timer_signal(int timer_type){
	int sig;

	switch(timer_type){
		case ITIMER_REAL:
			sig = SIGALRM;
			break;
		case ITIMER_VIRTUAL:
			sig = SIGVTALRM;
			break;
		case ITIMER_PROF:
			sig = SIGPROF;
			break;
		default:
			fprintf(stderr, "[ERROR] unknown timer type - %d\n", timer_type);
			exit(EXIT_FAILURE);
	}

	return sig;
}

void set_timer(int type, void (*handler)(int), int ms){
	struct itimerval timer;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));			// make all sa memory space to 0
	sa.sa_handler = handler;			// assign handler
	sigaction(timer_signal(type), &sa, NULL);	// install signal handler
	
	// after which second the timer will alarm the program
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = ms*1000;
	// the gap between alarms (0 means don't repeat timer)
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	
	if(setitimer(type, &timer, NULL) < 0){
		perror("Setting timer");
		exit(EXIT_FAILURE);
	}
}

void stop_timer(int type, void (*handler)(int)){
	struct itimerval timer;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));			// make all sa memory space to 0
	sa.sa_handler = handler;			// assign handler
	sigaction(timer_signal(type), &sa, NULL);	// install signal handler
	
	// after which second the timer will alarm the program
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 0;
	// the gap between alarms (0 means don't repeat timer)
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	
	if(setitimer(type, &timer, NULL) < 0){
		perror("Setting timer");
		exit(EXIT_FAILURE);
	}
}

void timer_handler(int signum){
	stop_timer(TIMER_TYPE, timer_handler);
	schedule();
}

/*******************************************************************************
                    Implementation of the Simple Threads API
********************************************************************************/


int  init(){
	t_num++;
	threads = (thread_t *) malloc(sizeof(thread_t)*t_num);
	if(threads == NULL){
		return -1;
	}

	// thread for main
	threads[t_num-1].tid = t_num;
	threads[t_num-1].state = ready;
	threads[t_num-1].next = NULL;
	// get main info and initialize it in the thread structure
	if(getcontext(&(threads[t_num-1].ctx)) < 0){
		perror("getcontext");
		exit(EXIT_FAILURE);
	}

	schedule();

	ind = 0;
	return 1;
}


tid_t spawn(void (*start)()){
	// make space for new thread
	t_num++;
	threads = (thread_t *) realloc(threads, sizeof(thread_t)*t_num);
	
	if(threads == NULL){
		perror("realloc");
		exit(EXIT_FAILURE);
	}

	// set thread structure
	init_thread(&(threads[t_num-1]), start);
	
	/*for(int i=0; i<t_num; i++){
		if(threads[i].state == running){ // if another thread is running, just return the thread id
			return threads[t_num-1].tid;
		}
	}*/

	schedule();

	return threads[t_num-1].tid;
}

void yield(){
	stop_timer(TIMER_TYPE, timer_handler);
	schedule();

}

void  done(){
	// running -> terminated & save thread id of the terminated thread
	for(int i=0; i<t_num; i++){
		if(threads[i].state == running){
			threads[i].state = terminated;
			termin = threads[i].tid;
			break;
		}
	}

	// make all waiting threads to ready
	for(int i=0; i<t_num; i++){
		if(threads[i].state == waiting){
			threads[i].state = ready;
		}
	}

	// schedule another thread
	schedule();
}

tid_t join() {
	for(int i=0; i<t_num; i++){
		if(threads[i].state == running){
			threads[i].state = waiting;
			stop_timer(TIMER_TYPE, timer_handler);
			
			// save the context
			if(getcontext(&(threads[i].ctx)) < 0){
				perror("getcontext");
				exit(EXIT_FAILURE);
			}

			schedule();
			break;
		}
	}

	return termin;
}







