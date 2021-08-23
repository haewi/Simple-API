#include <stdlib.h>   // exit(), EXIT_FAILURE, EXIT_SUCCESS
#include <stdio.h>    // printf(), fprintf(), stdout, stderr, perror(), _IOLBF
#include <stdbool.h>  // true, false
#include <limits.h>   // INT_MAX

#include "sthreads.h" // init(), spawn(), yield(), done()

/*******************************************************************************
                   Functions to be used together with spawn()

    You may add your own functions or change these functions to your liking.
********************************************************************************/

/* Prints the sequence 0, 1, 2, .... INT_MAX over and over again.
 */
void numbers() {
  int n = 0;
  while (true) {
    printf(" n = %d\n", n);
    n = (n + 1) % (INT_MAX);
    //if (n > 3) done();
    //yield();
    if(n == 4) printf("\t\tdone by %u!\n", join());
    else if(n > 6) done();
    else yield();
  }
}

/* Prints the sequence a, b, c, ..., z over and over again.
 */
void letters() {
  char c = 'a';

  while (true) {
      printf(" c = %c\n", c);
      if (c == 'f') done();
      yield();
      c = (c == 'z') ? 'a' : c + 1;
    }
}

/* Calculates the nth Fibonacci number using recursion.
 */
int fib(int n) {
  switch (n) {
  case 0:
    return 0;
  case 1:
    return 1;
  default:
    return fib(n-1) + fib(n-2);
  }
}

/* Print the Fibonacci number sequence over and over again.

   https://en.wikipedia.org/wiki/Fibonacci_number

   This is deliberately an unnecessary slow and CPU intensive
   implementation where each number in the sequence is calculated recursively
   from scratch.
*/

void fibonacci_slow() {
  int n = 0;
  int f;
  while (true) {
    f = fib(n);
    if (f < 0) {
      // Restart on overflow.
      n = 0;
    }
    printf("slow: fib(%02d) = %d\n", n, fib(n));
    n = (n + 1) % INT_MAX;
  }
}

/* Print the Fibonacci number sequence over and over again.

   https://en.wikipedia.org/wiki/Fibonacci_number

   This implementation is much faster than fibonacci().
*/
void fibonacci_fast() {
  int a = 0;
  int b = 1;
  int n = 0;
  int next = a + b;

  while(true) {
    printf("fast: fib(%02d) = %d\n", n, a);
    next = a + b;
    a = b;
    b = next;
    n++;
    if (a < 0) {
      // Restart on overflow.
      a = 0;
      b = 1;
      n = 0;
    }
  }
}

/* Prints the sequence of magic constants over and over again.

   https://en.wikipedia.org/wiki/Magic_square
*/
void magic_numbers() {
  int n = 3;
  int m;
  while (true) {
    m = (n*(n*n+1)/2);
    if (m > 0) {
      printf(" magic(%d) = %d\n", n, m);
      n = (n+1) % INT_MAX;
    } else {
      // Start over when m overflows.
      n = 3;
    }
    yield();
  }
}

static volatile int counter1 = 0;
static volatile int counter2 = 0;
mutex_t m;

void add(){
	for(int i=0; i<5000; i++){
		counter1 = counter1 + 1;
		printf("counter1: %d\n", counter1);
	}
	done();
}

void add_lock(){
	for(int i=0; i<10000; i++){
		lock(&m);
		counter2 = counter2 + 1;
		printf("counter2: %d\n", counter2);
		unlock(&m);
	}
	done();
}

#define MAX_c 100
int buffer_c[MAX_c];
int fill_ptr = 0;
int use_ptr = 0;
int count = 0;

cond_t empty_c, fill_c;
mutex_t mutex_c;

void put_c(int value){
	buffer_c[fill_ptr] = value;
	fill_ptr = (fill_ptr + 1) % MAX_c;
	count++;
}

int get_c(){
	int tmp = buffer_c[use_ptr];
	use_ptr = (use_ptr + 1) % MAX_c;
	count--;
	return tmp;
}

void producer_c(){
	int i=0;
	while(1){
		printf("producer\n");
		lock(&mutex_c);
		while(count == MAX_c){
			printf("producer - cond_wait\n");
			cond_wait(&empty_c, &mutex_c);
		}
		printf("\tput_c %i\n", i);
		put_c(i);
		cond_signal(&fill_c);
		unlock(&mutex_c);
		i++;
	}
}

void consumer_c(){
	while(1){
		printf("consumer\n");
		lock(&mutex_c);
		while(count == 0){
			printf("consumer - cond_wait\n");
			cond_wait(&fill_c, &mutex_c);
		}
		int tmp = get_c();
		printf("\tget_c %i\n", tmp);
		cond_signal(&empty_c);
		unlock(&mutex_c);
	}
}

#define MAX_s 200
int buffer_s[MAX_s];
int fill_s = 0;
int use_s = 0;
sem_t empty_s, full_s, mutex_s;

void put_s(int value){
	buffer_s[fill_s] = value;
	fill_s = (fill_s + 1) % MAX_s;
}

int get_s(){
	int tmp = buffer_s[use_s];
	use_s = (use_s + 1) % MAX_s;
	return tmp;
}

void producer_s(){
	int i=0;
	while(1){
		printf("\n");
		sem_wait(&empty_s);
		sem_wait(&mutex_s);
		put_s(i);
		printf("producer put_s %d\n", i);
		i++;
		sem_post(&mutex_s);
		sem_post(&full_s);
	}
}

void consumer_s(){
	while(1){
		printf("\n");
		sem_wait(&full_s);
		sem_wait(&mutex_s);
		int tmp = get_s();
		printf("consumer got_s %d\n", tmp);
		sem_post(&mutex_s);
		sem_post(&empty_s);
	}
}


/*******************************************************************************
                                     main()

            Here you should add code to test the Simple Threads API.
********************************************************************************/


int main(){
	puts("\n==== Test program for the Simple Threads API ====\n");

	init(); // Initialization
	
	lock_init(&m);

	cond_init(&empty_c);
	cond_init(&fill_c);

	sem_init(&empty_s, 0, MAX_s);
	sem_init(&full_s, 0, 0);
	sem_init(&mutex_s, 0, 1);

	//spawn(numbers);
	//spawn(letters);
	//spawn(magic_numbers);
	//spawn(fibonacci_slow);
	//spawn(fibonacci_fast);
	
	// lock test
	//printf("counter: %d\n", counter);
	// spawn(add);
	// spawn(add);
	// spawn(add_lock);
	// spawn(add_lock);
	// spawn(add_lock);
	//spawn(add_lock);


	// conditional variable test
	// spawn(consumer_c);
	// spawn(producer_c);
	// spawn(consumer_c);
	// spawn(producer_c);

	// semaphore test
	// spawn(producer_s);
	spawn(consumer_s);
	spawn(producer_s);
	spawn(consumer_s);

	int count = 0;
	// while(count < 30000) {
	// 	printf("main - %d\n", count);
	// 	count++;
	// 	yield();
	// }
	// printf("counter1 = %d\n", counter1);
	// printf("counter2 = %d\n", counter2);

	while(count <100){
		printf("main = %d\n", count);
		count++;
		yield();
	}

	printf("main done\n");
}








