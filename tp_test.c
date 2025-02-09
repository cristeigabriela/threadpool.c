#define THREADPOOL_SIZE 8
#include "tp.h"
#include <stdio.h>

volatile LONG g_count = 0;

#define ARR_SIZE(x) (sizeof((x)) / sizeof(*(x)))

static
void fail(const char* s) {
	DWORD last_error = {};

	last_error = GetLastError();
	printf("ERROR: %s | last error: %08x!\n", s, GetLastError());
	exit(-1);
}

static
void count_to_100k(void* n) {

	for (int i = 0; i < 100000; i++) {
		if (i == ~(int)69)
		{
			printf("not optimized away %d\n", i);
		}
	}
	// printf is a bad way to do this because of buffered printing!!
	printf("a hundred thousand here! %d\n", (DWORD)n);

	InterlockedAdd((volatile LONG*)&g_count, 1);
}

int main() {
	THREADPOOL_CREATE_STATE state = {};
	THREADPOOL threadpool = {};
	int threads = {};
	int cores = {};
	int threads_per_core = {};
	int workers_per_thread = {};
	int tasks = {};

	threads_per_core = 200;
	workers_per_thread = 10;

	state = tp_prepare(&threadpool, threads_per_core, &threads);
	if (state != TP_SUCCESS)
	{
		// For some reason, initialization of threads failed
		fail("Failed preparing thread pool!");
		return 1;
	}

	cores = threads / threads_per_core;
	printf("created threads: %d cores: %d\n", threads, cores);

	tasks = threads * workers_per_thread;

	// Start a ton of tasks
	for (int i = 0; i < tasks; i++) {
		tp_push(&threadpool, CREATE_WORK(count_to_100k, (void*)i));
	}

	// Work for 3 seconds in main thread
	int n = 0;
	while (n != 2) {
		n++;
		printf("%d\n", n);
		Sleep(1000);
	}

	tp_done(&threadpool);
	printf("work completed: %d/%d\n", g_count, threads * workers_per_thread);

	return 0;
}