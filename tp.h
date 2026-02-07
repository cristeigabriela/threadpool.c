#ifndef _TP_H
#define _TP_H

#ifdef _MSC_VER
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <stdint.h>
#include "stack.h"
#define LOGARITHMIC_CAPACITY 1

	typedef void(*worker_t)(void*);
	typedef struct work {
		uint32_t worker;
		ptrdiff_t arg;
	} WORK, * PWORK;

#define CREATE_WORK(a, b) ((WORK){.worker = (uint32_t)(a),\
	.arg = (ptrdiff_t)( (uint32_t)(a) - (ptrdiff_t)(b) )})
#define WORK_WORKER(a) (worker_t)((a).worker)
#define WORK_ARG(a) (void*)(((a).worker)-((a).arg))

	STACK(WORK)
	typedef struct {
		HANDLE* h_threads;
		int n_threads;
		Stack_WORK workload;
	} THREADPOOL, * PTHREADPOOL;

	typedef enum {
		TP_SUCCESS,
		TP_FAIL
	} THREADPOOL_CREATE_STATE;

	THREADPOOL_CREATE_STATE
		tp_prepare(PTHREADPOOL threadpool, int n_threads_per_core, int* n_created);
	void tp_push(PTHREADPOOL threadpool, WORK work);
	void tp_done(PTHREADPOOL threadpool);

#ifdef __cplusplus
}
#endif

#endif // _TP_H