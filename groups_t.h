/**
 *
 * @file groups_t.h
 * author Priscilla Raucci 517046
 * Si dichiara che il contenuto di questo file è in ogno sua parte opera
 * originale dell'autore
 * @brief header di groups_t.c
 *
 **/

#ifndef GROUP_H
#define GROUP_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include "config.h"

/**
 @file groups.h
 @brief Interfaccia di una coda concorrente che raggruppo i gruppi di utenti
 @author Priscilla Raucci
 */

typedef struct member_t{
    char nickname[MAX_NAME_LENGTH+1];
    struct member_t * next;
}member_t;

typedef struct group_t{
    char namegroup[30];     //mome del gruppo
    size_t len;
    struct member_t *head;      // testa della lista dei membri
    struct member_t *tail;      // coda della lista dei membri
    struct group_t *next;
}group_t;

typedef struct grouplist_t{
    size_t len;
    struct group_t *head;
    struct group_t *tail;
    pthread_mutex_t mutex;
    //pthread_cond_t not_empty;
}grouplist_t;


/**
 @function newlist
 @brief Alloca e inizializza una struttura che elenca i gruppi
 @return il puntatore ad un nuovo oggetto di tipo grouplist_t o NULL in caso di errore (setta errno)
 */
grouplist_t * groups_newlist();


int groups_isinlist(grouplist_t *q, char* gname);

int groups_isingroup(grouplist_t *q, char* gname, char* nickname);
/**
 @function addgroup
 @brief aggiunge alla struttura un nuovo gruppo
 @param q nome della lista
 @param gname nome del gruppo
 @param creator cretore e primo utente del gruppo
 @return 0 in caso di successo, -1 altrimenti(setta errno)
 */
int groups_addgroup(grouplist_t *q, char* gname, char * creator);

/**
 @function addmember
 @brief aggiunge ad un dato gruppo un nuovo membro
 @param q nome della lista
 @param gname nome del gruppo
 @param member utente da inserire
 @return 0 in caso di successo, -1 altrimenti(setta errno)
 */
int groups_addmember(grouplist_t *q, char * gname, char * member);

/**
 @function addmember
 @brief aggiunge ad un dato gruppo un nuovo membro
 @param q nome della lista
 @param gname nome del gruppo
 @param member utente da inserire
 @return 0 in caso di successo, -1 altrimenti(setta errno)
 */
int groups_delmember(grouplist_t *q, char * gname, char *member);

/**
 @function force_freequeue
 @brief Dealloca un gruppo eliminando anche tutta la lista di componenti
 @param **q il puntatore al puntatore di una coda
 @return -1 in caso di errore (setta errno)
 @return 0 in caso di successo con gruppo vuoto
 @return 1 in caso di successo con gruppo vuoto
 */
// int groups_force_freequeue(grouplist_t **q);

/**
 @function components
 @brief Dealloca un gruppo eliminando anche tutta la lista di componenti
 @param *q il puntatore al gruppo
 @return head in caso di successo
 @return NULL altrimenti
 */
void groups_components(grouplist_t *q, char * gname, char ** buf, int *ncomponents);

void groups_destroy(grouplist_t *q);


#endif
