/**
 *
 * @file myicl_hash.h
 * author Priscilla Raucci 517046
 * Si dichiara che il contenuto di questo file è in ogno sua parte opera
 * originale dell'autore
 * @brief header file di  myicl_hash.c
 *
 **/

#ifndef icl_hash_h
#define icl_hash_h

#include <stdio.h>
#include <pthread.h>
#include "message.h"
#include "connections.h"
#include "ops.h"
#include "config.h"
// #include "parametri.h"

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

/* elemento della lista di messaggi */
typedef struct message_t_list{
    message_t msg;
    struct message_t_list *next;
}message_t_list;

/* struct per mantenere dati utente */
typedef struct utente{
    char nickname[MAX_NAME_LENGTH+1];
    message_t_list *segreteria; //array che dovrà avere dim massima MaxHistMsgs
    int nmsg;            //lunghezza della lista di messaggi
    int connesso;        // bit di connessione
    long fd;
    pthread_mutex_t lock;
}utente;

typedef struct icl_entry_s {
    void* key;
    utente *data;
    struct icl_entry_s* next;
} icl_entry_t;

typedef struct icl_hash_s {
    int nbuckets;
    int nentries;   //numero di utenti registrati
    int nconnected;  //numero di utenti connessi
    icl_entry_t **buckets;
    pthread_mutex_t lock;
    unsigned int (*hash_function)(void*);
    int (*hash_key_compare)(void*, void*);
} icl_hash_t;


int icl_hash_isin(icl_hash_t *ht, void* key);

icl_hash_t *icl_hash_create(int nbuckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*));

int icl_hash_insert(icl_hash_t *ht, char* nick);

int icl_hash_delete(icl_hash_t *ht, void* key);

int icl_hash_destroy(icl_hash_t *ht, void (*free_key)(void*), void (*free_data)(void*));

//int icl_hash_dump(FILE* stream, icl_hash_t* ht);

int icl_hash_connect(icl_hash_t* ht, void* key, long connfd, int MaxConnections);

int icl_hash_deconnect(icl_hash_t* ht, long connfd);

//long icl_hash_connfd(icl_hash_t* ht, void* key);

//int icl_hash_nconn(icl_hash_t* ht);

void icl_hash_listonline(icl_hash_t* ht, char** buf, int *connessi);

void icl_hash_list(icl_hash_t* ht, char** buf, int *utenti);

int icl_hash_addmsg(icl_hash_t* ht, void* key, message_t *msg, int MaxHistMsgs);

int icl_hash_sendmsg(icl_hash_t* ht, void* key, message_t *msg, long connfd, op_t op);

int icl_hash_senddata(icl_hash_t* ht, void* key, message_data_t *data, long connfd);

int icl_hash_sendAck(icl_hash_t* ht, void* key, op_t op, long connfd);

int icl_hash_getmsg(icl_hash_t* ht, void* key, message_t_list **list);


#define icl_hash_foreach(ht, tmpint, tmpent, kp, dp)    \
    for (tmpint=0;tmpint<ht->nbuckets; tmpint++)        \
        for (tmpent=ht->buckets[tmpint];                                \
             tmpent!=NULL&&((kp=tmpent->key)!=NULL)&&((dp=tmpent->data)!=NULL); \
             tmpent=tmpent->next)


#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

#endif /* icl_hash_h */
