/**
 *
 * @file queue.h
 * author Priscilla Raucci 517046
 * Si dichiara che il contenuto di questo file è in ogno sua parte opera
 * originale dell'autore
 * @brief header file di  queue.c
 *
 **/

#ifndef QUEUE_H_
#define QUEUE_H_

#include <pthread.h>
#include "message.h"
typedef struct request{
    //message_t *msg;
    message_t msg;      /* messaggio */
    //message_data_t data;   /* puntatore ai dati, nel caso di invio di file */
    long connfd;            /* connfd della connessione */
}request;
/** Elemento della coda.
 *
 */
typedef struct Node {
    request        * data;
    struct Node * next;
} Node_t;

/** Struttura dati coda.
 *
 */
typedef struct Queue {
  /*  Node_t        *head;
    Node_t        *tail;
    unsigned long  qlen;*/
    request ** elements;
    size_t maxlen; //considero che sia 32...
    size_t size;
    unsigned int head;
    unsigned int tail;
    pthread_mutex_t qlock;
    pthread_cond_t qfull;
    pthread_cond_t qempty;
    int           accoda; //campo che indica se posso accodare o meno elementi(necessario per chiusura)
} Queue_t;


Queue_t *initQueue();

void deleteQueue(Queue_t **q);

int    push(Queue_t *q, void *elem);

void  *pop(Queue_t *q);

unsigned long length(Queue_t *q);

void stopPush(Queue_t *q);


#endif /* QUEUE_H_ */
