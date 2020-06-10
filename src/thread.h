#pragma once
#include "memory.h"

#if defined(__STDC_NO_THREADS__) && __STDC_NO_THREADS__
  #include "c11threads.h"
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

static ThreadStatus ThreadStart(Thread* nonull t, thrd_start_t nonull fn, void* nullable arg);
Thread nullable     ThreadSpawn(thrd_start_t nonull fn, void* nullable arg); // null on error
static int          ThreadAwait(Thread t);

inline static ThreadStatus ThreadStart(
  Thread* nonull t,
  thrd_start_t nonull fn,
  void* nullable arg
) {
  return (ThreadStatus)thrd_create(t, fn, arg);
}

inline static int ThreadAwait(Thread t) {
  int result = 0;
  thrd_join(t, &result); // ignore ThreadStatus
  return result;
}
