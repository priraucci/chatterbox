/**\file connections.c
    \author Priscilla Raucci 517046
    Si dichiara che il contenuto di questo file è in ogno sua parte opera
    originale dell'autore
    */

#if !defined(UTILS_H)
#define UTILS_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "utils.h"

void Pthread_mutex_lock(pthread_mutex_t *mtx){
  int err=1;
  if((err=pthread_mutex_lock(mtx))!=0){
    perror("Pthrad_mutex_lock");
    exit(EXIT_FAILURE);
  }
//puts("loccato");
}

void Pthread_mutex_unlock(pthread_mutex_t *mtx){
  int err=1;
  if((err=pthread_mutex_unlock(mtx))!=0){
    perror("Pthread_mutex_unlock");
    exit(EXIT_FAILURE);
  }
//puts("unlockato");
}

static inline int readn(long fd, void *buf, size_t size){
  size_t left=size;
  int r = -1;
  char *bufptr=(char*)buf;
  while(left>0 && bufptr!=NULL){
    if((r=read((int)fd, bufptr, left))==-1){
      if (errno==EINTR) continue;
      return -1;
    }
    if(r==0) return 0;
    left-=r;
    bufptr+=r;
  }
  return r;
}

static inline int writen(long fd, void *buf, size_t size){
  size_t left=size;
  int r= -1;
  char *bufptr=(char*)buf;
  while(left>0 && bufptr!=NULL){
    if((r=write((int)fd, bufptr, left))==-1){
      if (errno==EINTR) continue;
      return -1;
    }
    if(r==0) return 0;
    left-=r;
    bufptr+=r;
  }
  return r;
}

#endif /*quello iniziale*/
