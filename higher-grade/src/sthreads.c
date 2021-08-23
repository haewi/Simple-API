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
#define TIMEOUT 20		// us
#define TIMER_TYPE ITIMER_REAL 	// type of timer

void init_context(ucontext_t *ctx, void(*func)(), ucontext_t *next);
void init_thread(thread_t * t, void (*start)());
void schedule();
void delete_t(int index);
int get_index(); // returns running thread index, -1 when failed

int timer_signal(int timer_type);
void set_timer(int type, void (*handler)(int), int us);
int stop_timer(int type, void (*handler)(int));
void timer_handler(int signum);


/*******************************************************************************
                             Global data structures

                Add data structures to manage the threads here.
********************************************************************************/
thread_t * threads;
int t_ind = 0; // thread cursor
int t_num = 0; // thread number
int m_ind = 0; // mutex cursor
int m_num = 0; // mutex number
int c_num = 0; // cond_t number
int s_num = 0; // sem_t number
int s_ind = 0; // semaphore cursor
tid_t termin = -1; // the thread id that terminated last



/*******************************************************************************
                             Auxiliary functions

                      Add internal helper functions here.
********************************************************************************/

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
	t->mid = -1;
	t->cid = -1;
	t->sid = -1;
}

void schedule(){
	for(int i=0; i<t_num; i++){
		printf("threads[%d].state: %u\n", i, threads[i].state);
	}
	printf("schedule\n");
	if(t_num == 1) { // there is no ready state thread
		threads[0].state = running;
		return;
	}
	int ready_t = -1;
	int cur = -1;
	for(int i=0; i<t_num; i++){
		t_ind = (t_ind+1) % t_num;
		if(threads[t_ind].state == ready){
			ready_t = t_ind;
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
	if(cur == -1){
		threads[ready_t].state = running;
		setcontext(&(threads[ready_t].ctx));
	}
	else if(ready_t == -1){
		// if there is no other thread to run, run the current thread
	}
	else{ // there is thread running
		threads[cur].state = ready;
		threads[ready_t].state = running;
		//printf("til now: %d from now: %d\n", cur, ready_t);
		
		if (swapcontext(&(threads[cur].ctx), &(threads[ready_t].ctx)) < 0) {
			perror("swapcontext");
			exit(EXIT_FAILURE);
		}
	}
}

void delete_t(int index){
	//printf("delete_t\n");
	free(threads[index].ctx.uc_stack.ss_sp);

	for(int i=index; i<t_num-1; i++){
		threads[i] = threads[i+1];
	}


	t_num--;
	threads = (thread_t*) realloc(threads, sizeof(thread_t)*t_num);
	if(threads == 0x0){
		perror("delete thread");
		exit(EXIT_FAILURE);
	}
}

int get_index(){
	for(int i=0; i<t_num; i++){
		if(threads[i].state == running){
			return i;
		}
	}
	return -1;
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

void set_timer(int type, void (*handler)(int), int us){
	struct itimerval timer;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));			// make all sa memory space to 0
	sa.sa_handler = handler;			// assign handler
	sigaction(timer_signal(type), &sa, NULL);	// install signal handler
	
	// after which second the timer will alarm the program
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = us;
	// the gap between alarms (0 means don't repeat timer)
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	
	if(setitimer(type, &timer, NULL) < 0){
		perror("Setting timer");
		exit(EXIT_FAILURE);
	}
}

int stop_timer(int type, void (*handler)(int)){
	struct itimerval timer;
	struct itimerval remain;
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
	
	if(setitimer(type, &timer, &remain) < 0){
		perror("Setting timer");
		exit(EXIT_FAILURE);
	}

	return remain.it_value.tv_usec;
}

void timer_handler(int signum){
	printf("timer\n");
	// stop timer and schedule a new thread
	stop_timer(TIMER_TYPE, timer_handler);
	schedule();
}



/*		------------------ Queue Functions ------------------		*/

/*void queue_init(queue_t * q){

}*/



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
	threads[t_num-1].mid = -1;
	threads[t_num-1].cid = -1;
	threads[t_num-1].sid = -1;
	// get main info and initialize it in the thread structure
	if(getcontext(&(threads[t_num-1].ctx)) < 0){
		perror("getcontext");
		exit(EXIT_FAILURE);
	}

	schedule();

	return 1;
}


tid_t spawn(void (*start)()){
	// printf("spawn\n");

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
		if(threads[i].state == waiting && threads[i].mid < 0 && threads[i].cid < 0){
			threads[i].state = ready;
		}
	}

	// schedule another thread
	stop_timer(TIMER_TYPE, timer_handler);
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

	for(int i=0; i<t_num; i++){
		if(threads[i].state == terminated){
			delete_t(i);
			break;
		}
	}

	return termin;
}

void lock_init(mutex_t *m){
	m_num++;
	m->mid = m_num;
	m->flag = 0;
}

void lock(mutex_t * m){
	int usec = stop_timer(TIMER_TYPE, timer_handler);
	//printf("usec: %d\n", usec);
	if(m->flag == 0){
		printf("hold lock\n");
		m->flag = 1;
		if(usec == 0){
			schedule();
		}
		else{
			set_timer(TIMER_TYPE, timer_handler, usec);
		}
	}
	else {
		printf("\tlock held sleep\n");
		int index = get_index();
		threads[index].mid = m->mid;
		threads[index].state = waiting;

		// save the context
		if(getcontext(&(threads[index].ctx)) < 0 ){
			perror("getcontext");
			exit(EXIT_FAILURE);
		}

		schedule();
	}
}

void unlock(mutex_t * m){
	int usec = stop_timer(TIMER_TYPE, timer_handler);
	printf("free lock\n");

	// if there is a thread who wants this mutex
	for(int i=0; i<t_num; i++){
		if(threads[i].mid == m->mid){
			printf("%d wanted this mutex\n", i);
			threads[i].mid = -1;
			threads[i].state = ready;
			// restart timer and resume timer
			if(usec ==0){
				schedule();
			}
			else{
				set_timer(TIMER_TYPE, timer_handler, usec);
			}
			return;
		}
	}
	printf("no thread wanted\n");
	m->flag = 0;
	if(usec ==0){
		schedule();
	}
	else{
		set_timer(TIMER_TYPE, timer_handler, usec);
	}
}


void cond_init(cond_t * c){
	c_num++;
	c->cid = c_num;

}

void cond_wait(cond_t * c, mutex_t * m){
	int usec = stop_timer(TIMER_TYPE, timer_handler);

	if(m->flag == 0){
		perror("[ERROR] cond_wait mutex not held");
		exit(EXIT_FAILURE);
	}
	
	if(usec != 0){
		set_timer(TIMER_TYPE, timer_handler, TIMEOUT); // for unlock function
	}
	unlock(m);
	usec = stop_timer(TIMER_TYPE, timer_handler);
	
	int index = get_index();
	threads[index].cid = c->cid;
	threads[index].state = waiting;
	c->m = m;
	
	// save the context
	if(getcontext(&(threads[index].ctx)) < 0 ){
		perror("getcontext");
		exit(EXIT_FAILURE);
	}

	schedule();

	lock(m);
}

void cond_signal(cond_t *c){
	int usec = stop_timer(TIMER_TYPE, timer_handler);
	printf("cond_signal\n");
	
	if(c->m == 0x0){ // if there is no thread to wake up
		if(usec == 0x0){
			schedule();
		}
		else{
			set_timer(TIMER_TYPE, timer_handler, usec);
		}
		return;
	}

	for(int i=0; i<t_num; i++){
		if(threads[i].cid == c->cid){
			// there can be more than 1 threads waiting for this signal
			printf("%d was signaled\n", i);

			threads[i].cid = -1;
			threads[i].state = ready;

			if(usec == 0){
				schedule();
			}
			else{
				set_timer(TIMER_TYPE, timer_handler, usec);
			}
			return;
		}
	}
	
	printf("no thread was signaled\n");
	c->m = 0x0;
	if(usec == 0){
		schedule();
	}
	else{
		set_timer(TIMER_TYPE, timer_handler, usec);
	}
}

void sem_init(sem_t * s, int pshared, int value){
	s_num++;
	s->sid = s_num;
	s->value = value;
}

void sem_wait(sem_t * s){
	int usec = stop_timer(TIMER_TYPE, timer_handler);

	s->value--;
	// printf("sem_wait -> %d (sid: %d)\n", s->value, s->sid);

	// if the value is negative, wait
	if(s->value < 0){
		int index = get_index();
		threads[index].state = waiting;
		threads[index].sid = s->sid;

		if(getcontext(&(threads[index].ctx)) < 0){
			perror("getcontext");
			exit(EXIT_FAILURE);
		}

		schedule();
	}
	else{ // else continue execution
		if(usec == 0){
			schedule();
		}
		else{
			set_timer(TIMER_TYPE, timer_handler, TIMEOUT);
		}
	}
}
void sem_post(sem_t *s){
	int usec = stop_timer(TIMER_TYPE, timer_handler);

	s->value++;
	// printf("sem_post -> %d (sid: %d)\n", s->value, s->sid);

	// if there is a thread waiting
	if(s->value <= 0){
		for(int i=0; i<t_num; i++){
			s_ind = (s_ind+1) % t_num; // set the finding semaphore index of threads
			// printf("s_ind: %d\n", s_ind);

			if(threads[s_ind].sid == s->sid){
				printf("%d wanted this semaphore\n", s_ind);
				threads[s_ind].state = ready;
				threads[s_ind].sid = -1;
				break;
			}
		}
	}
	
	// continue execution
	if(usec == 0){
		schedule();
	}
	else{
		set_timer(TIMER_TYPE, timer_handler, usec);
	}
}





















