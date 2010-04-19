#include <inc/stdlib.h>
#include <inc/stdio.h>
#include <inc/unistd.h>
#include <inc/string.h>
#include <inc/file.h>
#include <inc/errno.h>
#include <inc/assert.h>
#include <inc/syscall.h>
#include <inc/vm.h>
#include <inc/pthread.h>


#define NUM_CHILDREN 4

static int array[10];  
static pthread_barrier_t barrier;


void print_array() {

	int i;
	for (i = 0; i < 10; i++) 
		printf("%d ", array[i]);
	printf("\n");
}


void * func(void * args_ptr) {

	int s = pthread_self();
	array[s] = s;
	cprintf("Thread %d in func before barrier.  array[4]: %d\n", s, array[4]);
	pthread_barrier_wait(&barrier);
       	array[s + NUM_CHILDREN] = s;
	cprintf("Thread %d in func after barrier.  array[4]: %d\n", s, array[4]);
	return (void *)EXIT_SUCCESS;
}

int main(int argc, char ** argv) {

	int i, status, ret;
	pthread_t threads[NUM_CHILDREN];

	ret = pthread_barrier_init(&barrier, NULL, NUM_CHILDREN);
	if (ret != 0)
		fprintf(stderr, "*** pthread_barrier_init  %d\n", ret);


	fprintf(stderr, "In parent.\n");

	print_array();

	for (i = 0; i < NUM_CHILDREN; i++)
		pthread_create(&threads[i], NULL, &func, NULL);
	fprintf(stderr, "In parent again.\n");
	for (i = 0; i < NUM_CHILDREN; i++)
		pthread_join(threads[i], (void **)&status);
		
	ret = pthread_barrier_destroy(&barrier);
	if (ret != 0)
		fprintf(stderr, "*** pthread_barrier_destroy  %d\n", ret);

	print_array();

	return EXIT_SUCCESS;
}
