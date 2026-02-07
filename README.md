# threadpool.c
Massively concurrent threadpool for Windows on C, using native `CreateThread` threads and [perfect_stack.h](https://github.com/cristeigabriel/perfect_stack.h) for atomic lockless work polling.

# Technique
- threadpool.c checks how many cores you have, takes in how many threads you want per core, then it goes over every core, assigning the process to it exclusively while the thread is created, and then re-assigns the process to whichever cores it was previously assigned to. As a result, the threads are created in place, and the `HANDLE` is assigned into a vector.
- When you push a task, the threads must be prepared, and as a result, they will be polling constantly for work. If they have nothing to poll for, they sleep for about 500ms (currently, this should change!). When they get work, they execute it and continue polling again. Basically, pushing is concurrent, lockless, and safe.
- Threads constantly check if they've been told to terminate themselves (atomically, CAS), and if that is so, then they only continue working until the workload is finished, then they terminate.
- When the threadpool should terminate, it does a blocking operation waiting for the threads to finish, by using `WaitForMultipleObjects` and explicitly waiting for them to `SIGNAL`, then it closes and `NULL`-s the handles, frees the vector and stack of workload.

# Results
I don't have the time to conduct proper benchmarking and great tests at the moment, because I only allowed myself the weekend at the time of writing for this (preparing for technical interviews with a big company), [perfect_stack.h](https://github.com/cristeigabriel/perfect_stack.h) and an education async engine for C on Windows, but my tests were the following:

On a setup with **8** cores, I was able to confidently run **200** threads per core `(8 * 200 = 1600)` and up to **200** workers per thread `(200 * 1600 = 32000)` (NOTE: the workers were not *assigned* to a thread, the number of threads was merely used as a metric for how many workers to push). On this setup, there were no polling task repeats, or skips. All tasks completed correctly. The curious may play around with [tp_test.c](./tp_test.c) and see what's what on their end!

The setup is made as such that the tasks were pushed while the prepared threads were polling. The threads are created ahead of time to make any push operation practically free. There has been no concurrency error in this setup (besides when using non-threadsafe code.)

# Usage as a library
I would not recommend it necessarily, although I do plan to use it as such, however solely for the fact that *I* am in control. This is more suited for the educational purposes of the reader.
