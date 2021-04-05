/**
 *
 * @file myicl_hash.c
 * author Priscilla Raucci 517046
 * Si dichiara che il contenuto di questo file è in ogno sua parte opera
 * originale dell'autore
 * @brief hashtable sviluppata a partire dal codice di Jakub Kurzak
 *
 **/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <string.h>

#include "myicl_hash.h"
#include "message.h"
#include "connections.h"
#include "ops.h"

#include <limits.h>


#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))


/* --------------- funzioni di utilità ---------------- */

static void freeMessageList(message_t_list *elem){
    if(!elem) return;
    if(elem->msg.data.buf) free(elem->msg.data.buf);
    }
static void LockHash(pthread_mutex_t *lock)            { pthread_mutex_lock(lock);}
static void UnlockHash(pthread_mutex_t *lock)          { pthread_mutex_unlock(lock);}

/**
 * A simple string hash.
 *
 * An adaptation of Peter Weinberger's (PJW) generic hashing
 * algorithm based on Allen Holub's version. Accepts a pointer
 * to a datum to be hashed and returns an unsigned integer.
 * From: Keith Seymour's proxy library code
 *
 * @param[in] key -- the string to be hashed
 *
 * @returns the hash index
 */
static unsigned int hash_pjw(void* key){
    char *datum = (char *)key;
    unsigned int hash_value, i;

    if(!datum) return 0;

    for (hash_value = 0; *datum; ++datum) {
        hash_value = (hash_value << ONE_EIGHTH) + *datum;
        if ((i = hash_value & HIGH_BITS) != 0)
            hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hash_value);
}

static int string_compare(void* a, void* b){
    return (strcmp( (char*)a, (char*)b ) == 0);
}

static void free_key(void* key){ free(key); }

/**
 *
 * @brief Cerca un utente all'interno della hashtable
 * funzione a basso livello che utilizzo solo allinterno delle funzioni della struttura
 * pericolosa perchè non utilizzo i lock per gestire l'accesso in mutua esclusione
 *
 * @param ht -- Hashtable
 * @param key -- la chiave dell'elemento
 *
 * @returns un puntatore al dato indicato dalla chave.
 *   se la chiave non viene ritrovata restituisce NULL.
 */

static utente * icl_hash_find(icl_hash_t *ht, void* key){
//printf("icl_hash_find: key %s\n", (char*)key);
    icl_entry_t* curr;
    unsigned int hash_val;

    if(!ht || !key) return NULL;

    hash_val = (* ht->hash_function)(key) % ht->nbuckets;
    for (curr=ht->buckets[hash_val]; curr != NULL; curr=curr->next)
        if ( ht->hash_key_compare(curr->key, key))
          return(curr->data);
    return NULL;
}

/* --------------- funzioni hash table ---------------- */

/**
 * @brief cerca un elemento nella tabella hash
 * funzione che indica se un utente è presente o meno nella hashtable
 * analoga alla icl_hash_find ma la ricerca è regolarizzata dall'uso delle mutex
 *
 * @param ht -- hashtable
 * @param key -- chiave dell'elemento
 *
 * @returns 0 se la chiave è in ht, altrimenti -1.
 */
int icl_hash_isin(icl_hash_t *ht, void* key){
//puts("icl_hash_isin");
    icl_entry_t* curr;
    unsigned int hash_val;

    if(!ht || !key) return -1;

    hash_val = (* ht->hash_function)(key) % ht->nbuckets;

    LockHash(&ht->lock);
    for (curr=ht->buckets[hash_val]; curr != NULL; curr=curr->next)
        if ( ht->hash_key_compare(curr->key, key)){
            UnlockHash(&ht->lock);
            return 0;
          }

    UnlockHash(&ht->lock);
    return -1;
}
/**
 * @brief Crea una nuova hash table
 *
 * @param[in] nbuckets -- numero di Buckets
 * @param[in] hash_function -- puntatore alla funzione di hash
 * @param[in] hash_key_compare -- puntatore alla funzione per comparare la chiavi da utilizzare
 *
 * @returns puntatore alla nuova hashtable
 */

icl_hash_t *icl_hash_create(int nbuckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) ){
    icl_hash_t *ht;
    int i;

    ht = (icl_hash_t*) malloc(sizeof(icl_hash_t));
    if(!ht) return NULL;

    ht->nentries = 0;
    ht->nconnected = 0;

    pthread_mutex_init(&(ht->lock), NULL);

    ht->buckets = (icl_entry_t**)malloc(nbuckets * sizeof(icl_entry_t*));
    if(!ht->buckets) return NULL;

    ht->nbuckets = nbuckets;
    for(i=0;i<ht->nbuckets;i++) ht->buckets[i] = NULL;

    ht->hash_function = hash_function ? hash_function : hash_pjw;
    ht->hash_key_compare = hash_key_compare ? hash_key_compare : string_compare;

    return ht;
}

/**
 * @brief inserisce un elemento nella hashtable
 *
 * @param ht -- la hashtable
 * @param key -- il nickname dell'elemento
 * @param data -- puntatore ai dati dell'elemento
 *
 * @returns 0 in caso di successo, -2 se la chiave è già presente, -1 in caso di errore.
 */

int icl_hash_insert(icl_hash_t *ht, char* nick){
////printf("icl_hash_insert : inserendo....%s\n", nick);
    if(!ht || !nick) return -1;
    if( strlen(nick) > MAX_NAME_LENGTH ) return -1; //se il nome è troppo lungo

    icl_entry_t *curr;
    unsigned int hash_val = (* ht->hash_function)(nick) % ht->nbuckets;

    LockHash(&(ht->lock));

    for (curr=ht->buckets[hash_val]; curr != NULL; curr=curr->next){

        if ( ht->hash_key_compare(curr->key, nick)){ /* key already exists */
            UnlockHash(&ht->lock);
            return(-2);
        }
    }
    utente *ut = (utente*)malloc(sizeof(utente));
    if(ut == NULL){
        return -1;
        }
    ut->segreteria = NULL;
    ut->nmsg = 0;
    ut->connesso = 0;
    ut->fd = -1;
    strcpy(ut->nickname, nick);
    pthread_mutex_init(&(ut->lock), NULL);
    curr = (icl_entry_t*)malloc(sizeof(icl_entry_t));
    if(!curr) {
        UnlockHash(&ht->lock);
        free(ut);
        pthread_mutex_destroy(&ut->lock);
        return -1;
    }
    curr->key = (char*)malloc(sizeof(char)*(MAX_NAME_LENGTH+1));
    strcpy(curr->key, nick);
    curr->data = ut;
    curr->next = ht->buckets[hash_val]; /* add at start */

    ht->buckets[hash_val] = curr;

    ht->nentries++;

    UnlockHash(&ht->lock);
    return 0;
}

/**
 *  @brief libera l'elemento con chiave key
 *
 * @param ht -- the hash table
 * @param key -- chiave dell'elemento
 *
 * @returns 0 in caso di successo, -1 altrimenti.
 */

int icl_hash_delete(icl_hash_t *ht, void* key){
//puts("icl_hash_delete");
    icl_entry_t *curr, *prev;
    unsigned int hash_val;

    if(!ht || !key) return -1;

    hash_val = (* ht->hash_function)(key) % ht->nbuckets;
    prev = NULL;
    curr=ht->buckets[hash_val];
    int trovato = 0;
    LockHash(&ht->lock);
    while(curr != NULL && !trovato){
        if ( strcmp(curr->key, key) == 0) { // ho trovato il mio elemento
            LockHash(&curr->data->lock);
            trovato = 1;

            if (prev == NULL) { // il mio elemento è la testa della coda
                ht->buckets[hash_val] = curr->next;
            } else { // caso comune
                prev->next = curr->next;
            }
        } else{ //cerco ancora
        prev = curr;
        curr = curr->next;
      }
    }

    if(trovato == 1) { // in curr ho l'elemento che cercavo
        ht->nentries--;

        if(((curr->data)->connesso)==1){
            ht->nconnected--;
        }
        UnlockHash(&ht->lock);
        UnlockHash(&curr->data->lock);

        free_key(curr->key);
        return 0;
    }

    UnlockHash(&ht->lock);
    return -1;
}

/**
 * @brief libera l'intera hashtable.
 *
 * @param ht -- la hashtable
 * @param free_key -- puntatore alla funzione per liberare le chiavi
 * @param free_data -- punatore alla funzione per liberare i dati
 *
 * @returns 0 in caso di successo, -1 altrimenti.
 */
int icl_hash_destroy(icl_hash_t *ht, void (*free_key)(void*), void (*free_data)(void*)){
//puts("icl_hash_destroy");
    icl_entry_t *bucket, *curr, *next;
    icl_hash_t *tmp;
    int i;

    if(!ht) return -1;

    LockHash(&ht->lock);
    tmp = ht;
    ht = NULL;
    UnlockHash(&tmp->lock);
    for(i=0; i<(tmp->nbuckets); i++){
        bucket = tmp->buckets[i];
        for (curr=bucket; curr!=NULL; ) {
            next=curr->next;
            free(curr->key);

            message_t_list *c ;
            while((c = curr->data->segreteria)!=NULL && (curr->data->nmsg)>0){
                curr->data->segreteria = curr->data->segreteria->next;
                freeMessageList(c);
                free(c);
                curr->data->nmsg--;
            }
            pthread_mutex_destroy(&(curr->data->lock));
            free(curr->data);
            free(curr);
            curr=next;
        }
    }

  if(tmp->buckets) free(tmp->buckets);

    pthread_mutex_destroy(&tmp->lock);

    if(tmp) free(tmp);
    return 0;
}

/*
 * @brief setta l'utente come connesso all'interno della hash e registra l'indirizzo della socket di connessione
 *
 * @param ht -- la hash table
 * @param key -- la chiave dell'utente da connettere
 * @param connfd -- indirizzo di connessione
 *
 * @returns 0 in caso di successo, -1 in caso di errore, -2 in caso di eccessivo numero di connessioni presenti;
 */
int icl_hash_connect(icl_hash_t* ht, void* key, long connfd, int MaxConnections){
//puts("myicl_hash_connect");
    if(!ht ||  !key || connfd<0 || MaxConnections==0) return -1;

    LockHash(&ht->lock);
    utente *ut = icl_hash_find(ht, key);
    if(ut==NULL){
        UnlockHash(&ht->lock);
        return -1;
    }

    LockHash(&ut->lock);

    if(ut->connesso != 1) {
      ut->connesso=1;

      if( ++(ht->nconnected) > MaxConnections ){
        ht->nconnected--;
        return -2;
      }

    }

    ut->fd = connfd;
    UnlockHash(&ht->lock);
    UnlockHash(&ut->lock);

    return 0;
}

/**
 * @brief setta l'utente come sconnesso all'interno della hash
 *
 * @param ht -- the hash table
 * @param key -- the key of the item to connect
 * @param connfd -- indirizzo di connessione
 *
 * @returns 0 in caso di successo, -1 in caso di errore.
 */
int icl_hash_deconnect(icl_hash_t* ht, long connfd){
//printf("icl_hash_deconnect\n");
    int  nbuck = ht->nbuckets;
    icl_entry_t *curr;

    LockHash(&ht->lock);
    for(int i=0; i<nbuck; i++){
      curr = ht->buckets[i];
      while(curr!=NULL){
          if( curr->data->fd == connfd){
            LockHash(&curr->data->lock);
            ht->nconnected--;

            UnlockHash(&ht->lock);

            curr->data->connesso = 0;
            curr->data->fd = -1;

            UnlockHash(&curr->data->lock);

            return 0;
          }
          curr = curr->next;
      }
    }
    UnlockHash(&ht->lock);
    return -1;
}

/**
 * @brief funzione per elencare gli utenti connessi
 *
 * @param ht -- la hash table
 * @param buf -- punatore al buffer in cui inserire i nickname connessi
 * @param connessi -- puntatoore in cui inserire il numero di utenti connessi
 *
*/

void icl_hash_listonline(icl_hash_t* ht, char** buf, int *connessi){
    icl_entry_t *curr;
    char* str = (char*)malloc(sizeof(char)*MAX_NAME_LENGTH+1);
    char* lista;
    int j = 0;

    LockHash(&ht->lock);

    (*connessi) = ht-> nconnected;

    int dimbuf = (*connessi)*(MAX_NAME_LENGTH+1);
    lista = (char*)malloc(sizeof(char)*dimbuf);
    if(!lista){
      UnlockHash(&ht->lock);
      free(str);
      return;
    }
    memset(lista, '0', sizeof(char)*dimbuf);

    for(int i = 0; i < (ht->nbuckets) && j < dimbuf ; i++){
        curr = ht->buckets[i];
        while(curr!=NULL){
            if( curr->data->connesso == 1 && j < dimbuf ){
                strncpy( str, curr->data->nickname, MAX_NAME_LENGTH+1);
                strcpy(&lista[j], str);

                j = j+MAX_NAME_LENGTH+1;
            }
            curr = curr->next;
        }
    }

    *buf = lista;
    UnlockHash(&ht->lock);
    free(str);
}

/**
 * @brief funzione per elencare gli utenti registrati
 *
 * @param ht -- la hashtable
 * @param buf -- puntatore al buffer in cui inserire gli utnti registrati
 * @param utenti -- puntatore all'intero in cui inserire il numero degli utenti registrati
 *
*/

void icl_hash_list(icl_hash_t* ht, char** buf, int *utenti){
//puts("icl_hash_list");
    icl_entry_t *curr;
    char* str = (char*)malloc(sizeof(char)*MAX_NAME_LENGTH+1); //stringa di appoggio

    LockHash(&ht->lock);

    *utenti = ht->nentries;

    int dimbuf = (*utenti)*(MAX_NAME_LENGTH+1);
    *buf = (char*)malloc(sizeof(char)*dimbuf);
    if (!buf){
      free(str);
      UnlockHash(&ht->lock);
      return;
    }
    char* help = *buf;

    int j = 0;
    for(int i = 0; i < (ht->nbuckets) && j<dimbuf; i++){
      curr = ht->buckets[i];
      while(curr!=NULL && j<dimbuf){
        strncpy( str, curr->data->nickname, MAX_NAME_LENGTH+1);
        strncpy(&help[j], str, MAX_NAME_LENGTH+1);
        j = j+MAX_NAME_LENGTH+1;
        curr = curr->next;
      }
    }
    UnlockHash(&ht->lock);

    free(str);
    return;

}


/**
 * @brief aggiunge un messaggio nella coda dei messaggi della segreteria di un utente
 *
 * @param ht -- the hash table
 * @param key -- the key of the item to connect
 * @param msg -- messaggio da inserire
 *
 * @returns 0 on success, -1 on failure.
 */

int icl_hash_addmsg(icl_hash_t* ht, void* key, message_t *msg, int MaxHistMsgs){
//puts("icl_hash_addmsg");
    message_t_list * elem = malloc(sizeof(message_t_list));
    if(!elem) {
        fprintf(stderr, "errore di allocazione\n" );
        return -1;
    }
    elem->next = NULL;
    memset(&elem->msg, '\0', sizeof(message_t));
    elem->msg.hdr.op = msg->hdr.op;
    strcpy(elem->msg.hdr.sender, msg->hdr.sender);
    strcpy(elem->msg.data.hdr.receiver, msg->data.hdr.receiver);
    elem->msg.data.hdr.len = msg->data.hdr.len;
    elem->msg.data.buf= (char*)malloc((elem->msg.data.hdr.len)*sizeof(char));
    if(!elem->msg.data.buf){
        fprintf(stderr, "errore di allocazione\n" );
        return -1;
    }
    memset(elem->msg.data.buf, '\0', (elem->msg.data.hdr.len)*sizeof(char));
    strcpy(elem->msg.data.buf, msg->data.buf);

    message_t_list *ptr;

    LockHash(&ht->lock);
    utente * ut = icl_hash_find(ht, key);
    if(ut==NULL){
        UnlockHash(&ht->lock);
        return -1;
    }

    LockHash(&ut->lock);
    UnlockHash(&ht->lock);

    ut->nmsg++;
    if( (ut->nmsg)> MaxHistMsgs ){
    //in caso di messaggi in eccesso, elimino il più vecchio per far spazio al nuovo
    ut->nmsg--;
    message_t_list *old = ut->segreteria;
    ut->segreteria = old->next;
    free(old->msg.data.buf);
    free(old);
    }
    // aggiungo in coda il messaggio
    ptr = ut->segreteria;
    if(ptr==NULL){
      ut->segreteria=elem;
    }else {
      while(ptr->next != NULL){
        ptr = ptr->next;
      }
      ptr->next = elem;
    }
    elem->next = NULL;

    UnlockHash(&ut->lock);

    return 0;
}

/**
 * @brief funzione che invia un messaggio a un utente se online
 *
 * @param ht -- hashtable
 * @param key -- chiave del receiver
 * @param msg -- message
 * @param connfd -- indirizzo del destinatario, se -1 la funzione setta il connfd di key
 *
 * @returns 0 in caso di successo, -1 in caso di fallimento, -2 se il nickname non è connesso.
 */

int icl_hash_sendmsg(icl_hash_t* ht, void* key, message_t *msg, long connfd, op_t op){
    long fd;
    utente * ut;
    if( !ht || !msg ) return -1;

    LockHash(&ht->lock);
    if (key!=NULL){
      ut = icl_hash_find(ht, key);
    if(ut==NULL){
        UnlockHash(&ht->lock);
        return -1;
    }
    LockHash(&ut->lock);
    UnlockHash(&ht->lock);
  } else {
      icl_entry_t *curr;
      for(int i=0; i<(ht->nbuckets); i++){
        curr = ht->buckets[i];
        while(curr!=NULL){
            if( curr->data->fd == connfd){
              LockHash(&curr->data->lock);
              UnlockHash(&ht->lock);
              ut = curr->data;
            }
            curr = curr->next;
        }
      }

  }

    if((ut->connesso) == 0){ // se l'utente non è connesso aggiungo il messaggio nella segreteria
        UnlockHash(&ut->lock);
        return -2;
        }
    if(connfd == -1){ //ricerco l'indirizzo di connessione del receiver
      fd = ut->fd;
    } else fd = connfd;
    if(op == POSTTXT_OP || op == POSTTXTALL_OP) msg->hdr.op = TXT_MESSAGE;
    else {
      if(op == POSTFILE_OP) msg->hdr.op = FILE_MESSAGE;
      else msg->hdr.op = op;
    }
    if(sendRequest(fd, msg)!= 1){
      UnlockHash(&ut->lock);
      return -1;
    }

    UnlockHash(&ut->lock);

    return 0;
}

/**
 * @brief invia dati a un utente se online
 * pensato per le risposte ai client
 *
 * @param ht -- la hashtable
 * @param key -- la chiave del receiver del messaggio
 * @param msg -- messaggio
 * @param connfd -- indirizzo del destinatario, se -1 la funzione setta il connfd di key
 *
 * @returns 0 in caso di successo, -1 in caso di fallimento, -2 se il nickname non è connesso.
 */
int icl_hash_senddata(icl_hash_t* ht, void* key, message_data_t *data, long connfd){
    long fd;
    utente * ut;
    if( !ht || !data ) return -1;

    LockHash(&ht->lock);
    if (key!=NULL){
      ut = icl_hash_find(ht, key);
      if(ut==NULL){
          UnlockHash(&ht->lock);
          return -1;
        }
      LockHash(&ut->lock);
      UnlockHash(&ht->lock);
    } else {
        icl_entry_t *curr;
        for(int i=0; i<(ht->nbuckets); i++){
          curr = ht->buckets[i];
          while(curr!=NULL){
              if( curr->data->fd == connfd){
                LockHash(&curr->data->lock);
                UnlockHash(&ht->lock);
                ut = curr->data;
              }
              curr = curr->next;
            }
          }
      }

    if((ut->connesso) == 0){ // se l'utente non è connesso errore
        ut->nmsg--;
        UnlockHash(&ut->lock);
        return -1;

    } else { // se l'utente e connesso invio il messaggio sul suo socket
    if(connfd==-1){
      fd = ut->fd;
    } else fd = connfd;

        sendData(fd, data);
    }
        UnlockHash(&ut->lock);
        return 0;
}

/**
 * @brief invia un messaggio di Ack
 *
 * @param ht -- hash table
 * @param key -- la chiave del receiver dell'Ack
 * @param op -- operazione da inviare(ack o error message)
 * @param connfd -- indirizzo a cui inviare Ack, se -1 la funzione setta il connfd di key
 *
 * @returns 0 on success, -1 on nickanme unregistred.
 */

int icl_hash_sendAck(icl_hash_t* ht, void* key, op_t op, long connfd){
//printf("icl_hash_sendAck, key: %s op: %d\n", (char*)key, op);
    long fd;
    message_hdr_t *risposta = (message_hdr_t*) malloc(sizeof(message_hdr_t));
    if(!risposta){
      return -1;
    }
    LockHash(&ht->lock);
    utente * ut = icl_hash_find(ht, key);
    if(ut!=NULL) LockHash(&ut->lock);
    UnlockHash(&ht->lock);
    if(connfd==-1){
      fd = ut->fd;
    } else fd = connfd;

    setHeader(risposta, op, (char*)key);
    sendHeader(fd, risposta);

    if(ut!=NULL) UnlockHash(&ut->lock);
    free(risposta);
    return 0;
    }

/**
 * @brief operazione per prendere i messaggi dalla coda della segreteria
 *
 * @param ht -- la hash table
 * @param key -- la chiave dell'utente
 * @param list -- il puntatore in cui verraà inserita la lista di messaggi
 *
 * @returns la lunghezza della coda di messaggio.
 */

int icl_hash_getmsg(icl_hash_t* ht, void* key, message_t_list **list){
    int lenght;
    LockHash(&ht->lock);
    utente *ut = icl_hash_find(ht, key);
    if(ut==NULL){
        UnlockHash(&ht->lock);
        return -1;
    }
    LockHash(&ut->lock);
    UnlockHash(&ht->lock);

    lenght = ut->nmsg;
    // prendo la lista di messagggi
    *list = ut->segreteria;
    ut->segreteria = NULL;
    ut->nmsg = 0;
    UnlockHash(&ut->lock);

    return lenght;
}
