#include "defs.h"
#include "thread.h"

#if defined(__STDC_NO_THREADS__) && __STDC_NO_THREADS__
  // pthread layer
  #include "thread_pthread.c.h"
#endif


ThreadStatus ThreadStart(Thread* nonull t, thrd_start_t nonull fn, void* nullable arg) {
  return (ThreadStatus)thrd_create(t, fn, arg);
}


int ThreadAwait(Thread t) {
  int result = 0;
  thrd_join(t, &result); // ignore ThreadStatus
  return result;
}


Thread ThreadSpawn(thrd_start_t nonull fn, void* nullable arg) nonull_return {
  Thread t;
  if (ThreadStart(&t, fn, arg) != ThreadSuccess) {
    return NULL;
  }
  return t;
}
