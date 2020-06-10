#include "defs.h"
#include "thread.h"

Thread ThreadSpawn(thrd_start_t nonull fn, void* nullable arg) nonull_return {
  Thread t;
  if (ThreadStart(&t, fn, arg) != ThreadSuccess) {
    return NULL;
  }
  return t;
}
