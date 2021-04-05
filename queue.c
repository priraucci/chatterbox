/**
 *
 * @file queue.c
 * author Priscilla Raucci 517046
 * Si dichiara che il contenuto di questo file è in ogno sua parte opera
 * originale dell'autore
 * @brief implementazione di una coda concorrente con politica FIFO
 *
 **/

#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include "queue.h"

#include "message.h"

#define MAX_SLOTS 512
/**
 * @function initQueue
 * @brief funzione che crea la struttura della coda
 *
 * @return il puntatore alla coda
 */

Queue_t * initQueue(){
    size_t len = 102;
    Queue_t *new = (Queue_t*)malloc(sizeof(Queue_t));
    if(!new)
      return NULL;
    new->elements = (request**)malloc(len*sizeof(request*));
    if(!new->elements){
      free(new);
      return NULL;
    }
    pthread_mutex_init(&(new->qlock), NULL);
    pthread_cond_init(&(new->qempty), NULL);
    pthread_cond_init(&(new->qfull), NULL);
    new->maxlen = len;
    new->size = 0;
    new->head = 0;
    new->tail = 0;
    new->accoda = 1;
    return new;
}


/**
 * @function deleteQueue
 * @brief funzione che distrugge fa la free della struttura della coda
 *
 * @param q --- coda da liberare
 * @return il puntatore alla coda
 */

void deleteQueue(Queue_t **q){
    if(!q || !(*q)){
      errno = EINVAL;
      return;
    }
    //request* rqs= NULL;
    while((*q)->size!=0){
      request* rqs = (request*)(*q)->elements[(*q)->head];
      if(rqs->msg.data.buf) free(rqs->msg.data.buf);
      free(rqs);
      (*q)->head = ((*q)->head+1)%(*q)->maxlen;
      (*q)->size--;
    }
    pthread_mutex_destroy(&(*q)->qlock);
    pthread_cond_destroy(&(*q)->qempty);
    pthread_cond_destroy(&(*q)->qfull);
    free((*q)->elements);
    free(*q);
    return;
}

/**
 * @function push
 * @brief funzione che inserisce un elemento in coda
 *
 * @param q --- coda a cui aggiungere un elemento
 * @param data --- dato da inserire nell'elemento della coda
 *
 * @return 0 on success, -1 non puoi accodare
 */

int push(Queue_t *q, void *elem) {
  if(!q){
    errno = EINVAL;
    free(elem);
    return -1;
    }

    if (q->accoda == 0) {
      free(elem);
      return -1;
    }

    pthread_mutex_lock(&q->qlock);
    while(q->size >= q->maxlen){
      pthread_cond_wait(&q->qfull, &q->qlock);
    }

//puts("sono in push");
    q->elements[q->tail] = (void*)elem;
    q->tail = (q->tail+1)%q->maxlen;
    q->size++;

    pthread_mutex_unlock(&q->qlock);
    pthread_cond_signal(&q->qempty);
    return 0;
}


/**
 * @function pop
 * @brief funzione che prende il primo elemento della coda
 *
 * @param q --- coda da cui prendere l'elemento
 *
 * @return il dato dell'elemento prelevato, NULL in caso di errore
 */

void *pop(Queue_t *q){
if(!q){
    errno=EINVAL;
    return NULL;
  }
  pthread_mutex_lock(&q->qlock);
  while(q->size == 0){
    if(q->accoda == 0){
      pthread_mutex_unlock(&q->qlock);
      return NULL;
    }
	     pthread_cond_wait(&q->qempty, &q->qlock);
  }

  q->size--;
  void* result = q->elements[q->head];
  q->head = (q->head+1)%q->maxlen;

  pthread_mutex_unlock(&q->qlock);
  pthread_cond_signal(&q->qfull);
  return result;
}


/**
 * @function lenght
 * @brief funzione che restituisce la lunghezza della coda
 *
 * @param q --- coda
 * @return len, la lunghezza della coda
 */

unsigned long length(Queue_t *q) {
    unsigned long len = q->size;
    return len;
}

/**
 * @function stopPush
 * @brief funzione che blocca l'accodamento
 *
 * @param q --- coda
 */

void stopPush(Queue_t *q){
    pthread_mutex_lock(&q->qlock);
    q->accoda = 0;
    pthread_mutex_unlock(&q->qlock);
    pthread_cond_broadcast(&q->qempty);

}
