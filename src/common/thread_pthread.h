#pragma once
#include <pthread.h>

#define ONCE_FLAG_INIT  PTHREAD_ONCE_INIT

typedef pthread_t       thrd_t;
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t  cnd_t;
typedef pthread_key_t   tss_t;
typedef pthread_once_t  once_flag;

typedef int  (*thrd_start_t)(void*);
typedef void (*tss_dtor_t)(void*);

enum {
  mtx_plain     = 0,
  mtx_recursive = 1,
  mtx_timed     = 2,
};

enum {
  thrd_success,
  thrd_timedout,
  thrd_busy,
  thrd_error,
  thrd_nomem
};
