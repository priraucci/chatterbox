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

void Pthread_mutex_lock(pthread_mutex_t *mtx);

void Pthread_mutex_unlock(pthread_mutex_t *mtx);

static inline int readn(long fd, void *buf, size_t size){
  size_t left = size;
  int r = -1;
  char *bufptr = (char*)buf;
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
  int r = -1;
  char *bufptr = (char*)buf;
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
