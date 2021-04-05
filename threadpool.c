

#include <pthread.h>
#include <stdio.h>
#include <stdlibthreadpool.c.h>
#include "threadpool.h"

typedef struct My_pool{
  int dim;
  pthread_t * ar;
}My_pool;

My_pool *Create_pool(int dim){
  My_pool *new = malloc(sizeof(My_pool));
  new->dim = dim;
  new->ar= malloc(sizeof(pthread_t)*dim);

  for(int i=0; i<dim; i++){
  if(pthread_create(&new->ar[i], NULL, (void*)thtask, (void*) richieste)!= 0){
      fprintf(stderr, "allocazione pthread fallita\n");
      exit(EXIT_FAILURE);
    }
  }
}
