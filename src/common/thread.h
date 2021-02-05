#pragma once
#if defined(__STDC_NO_THREADS__) && __STDC_NO_THREADS__
  #include "thread_pthread.h"
#else
  #include <threads.h>
#endif

typedef enum ThreadStatus {
  ThreadSuccess  = thrd_success,
  ThreadNomem    = thrd_nomem,
  ThreadTimedout = thrd_timedout,
  ThreadBusy     = thrd_busy,
  ThreadError    = thrd_error,
} ThreadStatus;

typedef thrd_t Thread;

ThreadStatus    ThreadStart(Thread* nonull t, thrd_start_t nonull fn, void* nullable arg);
Thread nullable ThreadSpawn(thrd_start_t nonull fn, void* nullable arg); // null on error
int             ThreadAwait(Thread t);
