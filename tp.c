#include "tp.h"
#include <windows.h>
#include <intrin.h>
#include <stdio.h>

#define TP_DEBUG 0

volatile LONG g_finished = 0;

static
DWORD _stdcall poll_work(PTHREADPOOL ctx)
{
Lrestart:
	WORK work = {};
	while (0 == (Stack_pop_WORK(&ctx->workload, &work))) {
		// Check if threadpool should perish
		if (_InterlockedCompareExchange(&g_finished, 1, 1) != 0) {
			return 0;
		}
		Sleep(500);
	}

	worker_t worker = WORK_WORKER(work);
	void *arg = WORK_ARG(work);
	worker(arg);
	goto Lrestart;
}

static
DWORD_PTR get_current_process_affinity()
{
	HANDLE process = {};
	DWORD_PTR mask = {};
	DWORD_PTR sys_mask = {};
	BOOL ret = {};

	process = GetCurrentProcess();
	ret = GetProcessAffinityMask(process, &mask, &sys_mask);
	if (0 == ret || ERROR_SUCCESS != GetLastError())
	{
		return (DWORD)-1;
	}

	return mask;
}

static
BOOL set_process_affinity_mask(DWORD_PTR mask) {
	HANDLE handle = {};

	handle = GetCurrentProcess();
	return SetProcessAffinityMask(handle, mask);
}

THREADPOOL_CREATE_STATE tp_prepare(PTHREADPOOL threadpool, int n_threads_per_core, int* n_created)
{
	if (NULL == threadpool || n_threads_per_core < 1 || n_threads_per_core > 250) {
		goto Lfail;
	}

	SYSTEM_INFO system_info = {};
	HANDLE* handles = {};
	int cores = {};
	int threads = {};

	GetSystemInfo(&system_info);
	if (ERROR_SUCCESS != GetLastError()) {
		goto Lfail;
	}

	cores = system_info.dwNumberOfProcessors;
	if (cores < 1)
	{
		goto Lfail;
	}
	threads = cores * n_threads_per_core;

	handles = malloc(sizeof(*handles) * threads);
	if (NULL == handles)
	{
		goto Lfail;
	}

	for (int i = 0; i < cores; i++) {
		DWORD_PTR process_affinity = {};
		DWORD_PTR old_process_affinity = {};

		// Create process affinity to allocate on the i-th core
		// by making the process only execute on that momentarily
		process_affinity = (1 << i); // core

		old_process_affinity = get_current_process_affinity();
		if ((DWORD)-1 == old_process_affinity) {
			goto Lfail;
		}
#if TP_DEBUG
		__debugbreak();
#endif
		if (0 == set_process_affinity_mask(process_affinity)) {
			goto Lfail;
		}
#if TP_DEBUG
		__debugbreak();
#endif
		for (int j = 0; j < n_threads_per_core; j++) {
			int n_handle = {};
			HANDLE* curr_handle = {};

			// Get index in handles array for handle
			n_handle = j + (i * n_threads_per_core);
			curr_handle = &handles[n_handle];

			*curr_handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&poll_work, threadpool, 0, NULL);
			if (NULL == (*curr_handle)) {
				set_process_affinity_mask(old_process_affinity);
				goto Lfail;
			}
		}
		if (0 == set_process_affinity_mask(old_process_affinity)) {
			goto Lfail;
		}
#if TP_DEBUG
		__debugbreak();
#endif
	}

	*n_created = threads;
	threadpool->h_threads = handles;
	threadpool->n_threads = threads;
	Stack_new_WORK(&threadpool->workload);

	return TP_SUCCESS;

Lfail:
	if (NULL != handles) {
		free(handles);
		handles = NULL;
	}

	*n_created = 0;
	return TP_FAIL;
}

void tp_push(PTHREADPOOL threadpool, WORK work)
{
	if (NULL == threadpool || NULL == WORK_WORKER(work)) {
		goto Lfail;
	}

	if (InterlockedCompareExchange(&g_finished, 1, 1) == 1) {
		goto Lfail;
	}

	Stack_push_WORK(&threadpool->workload, work);

Lfail:
	return;
}

void tp_done(PTHREADPOOL threadpool)
{
	if (NULL == threadpool) {
		goto Lfail;
	}

	if (0 == threadpool->n_threads) {
		return;
	}

	if (NULL == threadpool->h_threads) {
		return;
	}

	// Give sign that threads should stop, they will
	// wait until they're done working
	InterlockedExchange(&g_finished, 1);

	// Wait for threads to signal finished workload
	for (int i = 0; i < threadpool->n_threads; i += 64) {
		int n_threads = {};
		const HANDLE* h_threads;
		DWORD result = {};
		
		n_threads = min(64, threadpool->n_threads - i);
		h_threads = &threadpool->h_threads[i];
		result = WaitForMultipleObjects(n_threads, h_threads, TRUE, INFINITE);
#if TP_DEBUG
		if (WAIT_FAILED == result) {
			__debugbreak();
		}
		else {
			printf("succeeded! %d %d\n", result, n_threads);
		}
#else
		UNREFERENCED_PARAMETER(result);
#endif
	}

	// Close handles
	for (int i = 0; i < threadpool->n_threads; i++) {
		if (NULL != threadpool->h_threads[i]) {
			CloseHandle(threadpool->h_threads[i]);
			threadpool->h_threads[i] = NULL;
		}
	}

	threadpool->n_threads = 0;
	free(threadpool->h_threads);

	// Threads don't finish until they finish their workload, but it stops
	// accepting new workloads at the same time
	Stack_free_WORK(&threadpool->workload);

Lfail:
	return;
}
