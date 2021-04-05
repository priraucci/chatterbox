#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <pthread.h>

typedef struct My_pool{
  int dim;
  pthread_t * ar;
}My_pool;

My_pool *Create_pool(int dim);

#endif
