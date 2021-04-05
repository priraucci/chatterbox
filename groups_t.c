/**
 *
 * @file groups_t.c
 * author Priscilla Raucci 517046
 * Si dichiara che il contenuto di questo file è in ogno sua parte opera
 * originale dell'autore
 * @brief struttura per la gestione di gruppi di utenti
 *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "groups_t.h"
#include "utils.h"

/*                          ------ REMAINDER ------
 * typedef member_t{
 *        char nickname[30];
 *        struct member_t * next;
 * }member_t;
 *
 * typedef struct group_t{
 *        char namegroup[30];     //mome del gruppo
 *        size_t len;
 *        struct node *head;      // testa della lista dei membri
 *        struct node *tail;      // coda della lista dei membri
 * }group_t;
 *
 * typedef struct grouplist_t{
 *        size_t len;
 *        struct node *head;
 *        struct node *tail;
 *        pthread_mutex_t mutex;
 *        pthread_cond_t not_empty;
 * }grouplist_t;
 *
 */

/**
 * @function groups_newlist
 * @brief Alloca e inizializza una struttura che elenca i gruppi
 *
 * @return il puntatore alla lista
 */

grouplist_t * groups_newlist(){
    grouplist_t * new = (grouplist_t * )malloc(sizeof(grouplist_t));
    if(!new) return NULL;
    new->len = 0;
    new->head = NULL;
    new->tail = NULL;

    int err;
    err = pthread_mutex_init(&new->mutex, NULL);
    if(err){
        errno = err;
        free(new);
        return NULL;
    }

    /* err=pthread_cond_init(&new->not_empty, NULL);
    if(err){
        errno = err;
        free(new);
        return NULL;
      } */
    return new;
}

/**
 * @function groups_destroy
 * @brief funzione che distrugge fa la free della struttura
 *
 * @param q --- struttura da liberare
 */


void groups_destroy(grouplist_t *q){
  grouplist_t * new = q;
  Pthread_mutex_lock(&(new->mutex));
  q = NULL;
  Pthread_mutex_unlock(&(new->mutex));

  group_t *gcurr;
  member_t *mcurr;

  while(new->head!=NULL){
    while(gcurr->head!=NULL){
        mcurr = gcurr->head;
        gcurr->head = mcurr->next;
        free(mcurr);
    }
    gcurr = new->head;
    new->head = gcurr->next;
    free(gcurr);
  }

  pthread_mutex_destroy(&new->mutex);
  //pthread_cond_destroy(&new->not_empty);
  free(new);

}

/**
 * @function groups_isinlist
 * @brief funzione per la ricerca di un gruppo nella struttura dati
 *
 * @param q --- struttura in cui ricercare il gruppo
 * @param gneame ---nome del gruppo
 * @return 0 se gname è nella struttura, -1 altrimenti
 */
int groups_isinlist(grouplist_t *q, char* gname){
    if(!q || !gname) return -1;

    group_t * tmp;
    tmp = q->head;
    Pthread_mutex_lock(&(q->mutex));
    while(tmp!=NULL){
        if(strcmp(gname, tmp->namegroup)==0){
            Pthread_mutex_unlock(&(q->mutex));
            return 0;
        }
        tmp = tmp->next;
    }
    Pthread_mutex_unlock(&(q->mutex));
    return -1;
}

// @brief controlla che l'utente nick appartenga a un dato gruppo
// returns -1 se non appartiene, 0 se appartiene

/*int groups_isingroup(grouplist_t *q, char* gname, char* nickname){
    if(!q || !gname) return -1;

    group_t * tmp;
    tmp = q->head;
    Pthread_mutex_lock(&(q->mutex));
    while(tmp!=NULL){
        if(strcmp(gname, tmp->namegroup)==0){
          member_t * mbrtmp = tmp->head;
         while(mbrtmp!=NULL) {
            if(strcmp(mbrtmp->nickname, member)==0){ // questo utente c'è già nel gruppo
              Pthread_mutex_unlock(&q->mutex);
              return 0;
          } else mbrtmp = mbrtmp->next;
        }
        tmp = tmp->next;
    }
    Pthread_mutex_unlock(&(q->mutex));
    return -1;
}*/

/**
 * @function groups_isingroup
 * @brief funzione per la ricerca di un utente all'interno di un gruppo nella struttura dati
 *
 * @param q --- struttura in cui ricercare il gruppo
 * @param gneame ---nome del gruppo
 * @param gneame ---nome del membro da cercare
 * @return 0 se member è in gname, -1 altrimenti
 */

int groups_isingroup(grouplist_t *q, char* gname, char* member){
    if(!q || !gname || !member) return -1;
    group_t * tmp;
    Pthread_mutex_lock(&(q->mutex));
    tmp = q->head;
    while(tmp!=NULL){
        if(strcmp(gname, tmp->namegroup) == 0){
            member_t *mbr = tmp->head;
            //member_t *prec = tmp->head;
            while(mbr!=NULL){
                if(strcmp(mbr->nickname, member) == 0){
                    Pthread_mutex_unlock(&(q->mutex));
                    return 0;
                }
                else {
                    //member_t *prec = mbr;
                    mbr = mbr->next;
                }
            }
            Pthread_mutex_unlock(&(q->mutex));
            return -1; // non ho trovato l'elemento
        }
        tmp = tmp->next;
    }
    Pthread_mutex_unlock(&(q->mutex));
    return -1; // non ho trovato il gruppo

}

/**
 * @function groups_addgroup
 * @brief aggiunge alla struttura un nuovo gruppo
 *
 * @param q --- struttura in cui ricercare il gruppo
 * @param gneame ---nome del gruppo
 * @param gneame ---nome del creatore
 * @return -1 on error, -2 on groupame already exists, 0 on success
 */

int groups_addgroup(grouplist_t *q, char* gname, char * creator){
    if(!q || !gname || !creator) return -1;
    int trovato = 0;

    member_t * mbr = (member_t*)malloc(sizeof(member_t));
    if(!mbr) return -1;
    strcpy(mbr->nickname, creator);
    mbr->next = NULL;
    group_t * tmp;
    Pthread_mutex_lock(&(q->mutex));
    tmp=q->head;
    while(tmp!=NULL && !trovato){
        if(strcmp(gname, tmp->namegroup)==0) trovato=1;
        tmp = tmp->next;
    }

    if(!trovato){   // se non esiste già un gruppo con quel nome
//printf("group_t sto creando %s\n", gname);
        group_t * gr = (group_t * )malloc(sizeof(group_t));
        if(!gr){
            free(mbr);
            free(gr);
            Pthread_mutex_unlock(&(q->mutex));
           return -1;
        }
        //setto tutte le info del gruppo
        strcpy(gr->namegroup, gname);
        gr->len = 1;
        gr->next = NULL;
        gr->head = mbr;
        gr->tail = mbr;

        //inserisco il gruppo nella struttura generale
        if(q->head ==NULL) q->head = gr;
        else q->tail->next = gr;
        q->tail = gr;

        q->len++;
        Pthread_mutex_unlock(&(q->mutex));
        return 0;
    }

    Pthread_mutex_unlock(&(q->mutex));
    return -2;

}


/**
 * @function groups_addmember
 * @brief aggiunge un utente ad un dato gruppo
 *
 * @param q --- struttura in cui ricercare il gruppo
 * @param gneame ---nome del gruppo
 * @param gneame ---nome del nuovo membro
 * @return -1 on error, 0 on success
 */
int groups_addmember(grouplist_t *q, char * gname, char * member){
printf("aggiungo %s in %s\n", member, gname);
    if(!q || !gname || !member) return -1;

    group_t * tmp;
    Pthread_mutex_lock(&(q->mutex));
    tmp = q->head;
    while(tmp!=NULL){
        if(strcmp(gname, tmp->namegroup) == 0){
            member_t * mbrtmp = tmp->head;
            //controllo se u gia esiste
           while(mbrtmp!=NULL) {
              if(strcmp(mbrtmp->nickname, member)==0){ // questo utente c'è già nel gruppo
                Pthread_mutex_unlock(&q->mutex);
                return -1;
            } else mbrtmp = mbrtmp->next;
          }

            //alloco lo spazio per il nuovo membro
            member_t * mbr = (member_t *)malloc(sizeof(member_t));
            if(!mbr){
                Pthread_mutex_unlock(&(q->mutex));
                return -1;
            }
            strcpy(mbr->nickname, member);
            mbr->next = NULL;
            if(tmp->head == NULL) tmp->head = mbr;
            else tmp->tail->next = mbr;
            tmp->tail = mbr;
            tmp->len++;

            Pthread_mutex_unlock(&(q->mutex));
            return 0;
        }
         else tmp = tmp->next;
    }
    Pthread_mutex_unlock(&(q->mutex));
    return -1;

}

/**
 * @function groups_delmember
 * @brief elimina un utente ad un dato gruppo
 *
 * @param q --- struttura in cui ricercare il gruppo
 * @param gneame ---nome del gruppo
 * @param gneame ---nome del membro da eliminare
 * @return -1 on error, 0 on success
 */
int groups_delmember(grouplist_t *q, char * gname, char *member){
    if(!q || !gname || !member) return -1;
printf("elimino %s da %s\n", member, gname);
    group_t * tmp;
    Pthread_mutex_lock(&(q->mutex));
    tmp = q->head;
    while(tmp!=NULL){
        if(strcmp(gname, tmp->namegroup) == 0){
            member_t *mbr = tmp->head;
            member_t *prec = tmp->head;
            while(mbr!=NULL){
                if(strcmp(mbr->nickname, member) == 0){
                    prec->next = mbr->next;
                    if( tmp->head == mbr ) tmp->head = mbr->next;
                    if( tmp->tail == mbr ) tmp->tail = prec;
                    tmp->len--;
                    free(mbr);
                    Pthread_mutex_unlock(&(q->mutex));
                    return 0;
                }
                else {
                    prec = mbr;
                    mbr = mbr->next;
                }
            }
            Pthread_mutex_unlock(&(q->mutex));
            return -1; // non ho trovato l'elemento
        }
        tmp = tmp->next;
    }
    Pthread_mutex_unlock(&(q->mutex));
    return -1; // non ho trovato il gruppo

}


/**
 *@function force_freequeue
 *@brief Dealloca un gruppo eliminando anche tutta la lista di componenti
 *@param **q il puntatore al puntatore di una coda
 *@return -1 in caso di errore (setta errno)
 *@return 0 in caso di successo con gruppo vuoto
 *@return 1 in caso di successo con gruppo vuoto

int groups_force_freequeue(grouplist_t **q){
  return 0;
} */


/**
 * @function groups_components
 * @brief funzione per elencare gli utenti di un dato gruppo
 *
 * @param q --- struttura in cui ricercare il gruppo
 * @param gneame ---nome del gruppo
 * @param buf ---buffer in cui inserire i componenti
 * @param ncomponents ---puntstore alla variabile incu inserire il numro dei componenti
 */

void groups_components(grouplist_t *q, char * gname, char ** buf, int *ncomponents){
//puts("groups_components");
  if(!q || !gname) return;

  char* str = (char*)malloc(sizeof(char)*MAX_NAME_LENGTH+1);
  group_t * tmp= q->head;
  Pthread_mutex_lock(&(q->mutex));
//puts("loccato");
  while(tmp!=NULL){
      if(strcmp(gname, tmp->namegroup) == 0){// ho trovato il gruppo

        *ncomponents = tmp->len;
        int dimbuf = (*ncomponents)*(MAX_NAME_LENGTH+1);
        *buf = (char*)malloc(dimbuf*sizeof(char));
        if(!buf){
          free(str);
          Pthread_mutex_unlock(&(q->mutex));
          return;
        }

        char* thehelp = *buf;
        //ciclo sugli utenti del grouppo
        member_t *ut = tmp->head;
        for(int j=0; j<dimbuf && ut!=NULL; j = j+MAX_NAME_LENGTH+1){
          strncpy( str, ut->nickname, MAX_NAME_LENGTH+1);
          strcpy(&thehelp[j], str);
//printf("groups_components: ut: %s\n", &thehelp[j] );
          ut = ut->next;
        }
        free(str);
        Pthread_mutex_unlock(&(q->mutex));
        return;

      } else tmp = tmp->next;
//puts("groups_components: tmp = tmp->next;");
    }
    free(str);
    Pthread_mutex_unlock(&(q->mutex));
    return;
}
/*
int main(int argc, char** argv){

  puts("_______________________INIZIO_________________________");

  char* nomeg1 =(char*)malloc(sizeof(char)*32);
  strncpy(nomeg1, argv[1], 31*sizeof(char));
  char* nomeu1 =(char*)malloc(sizeof(char)*32);
  strncpy(nomeu1, argv[2], 31*sizeof(char));
  char* nomeu2 =(char*)malloc(sizeof(char)*32);
  strncpy(nomeu2, argv[3], 31*sizeof(char));
  char* nomeu3 =(char*)malloc(sizeof(char)*32);
  strncpy(nomeu3, argv[4], 31*sizeof(char));

  char* nomeg2 =(char*)malloc(sizeof(char)*32);
  strncpy(nomeg2, argv[5], 31*sizeof(char));
  char* nomeu4 =(char*)malloc(sizeof(char)*32);
  strncpy(nomeu4, argv[6], 31*sizeof(char));

  char* nomeg3 =(char*)malloc(sizeof(char)*32);
  strncpy(nomeg3, argv[7], 31*sizeof(char));
  char* nomeu5 =(char*)malloc(sizeof(char)*32);
  strncpy(nomeu5, argv[8], 31*sizeof(char));
  char* nomeu6 =(char*)malloc(sizeof(char)*32);
  strncpy(nomeu6, argv[9], 31*sizeof(char));

  grouplist_t * list = newlist();
  //puts("2");
  addgroup(list, nomeg1, nomeu1);
  if(addgroup(list, nomeg2, nomeu4)== -1) puts("g2 gia esiste");
  if(addgroup(list, nomeg3, nomeu5)==-1) puts("g3 già esiste");
  //puts("add g");
  if(addmember(list, nomeg1, nomeu2)==-1) puts("u2 gia esiste in g1");
  puts("add m");
  addmember(list, nomeg1, nomeu3);
  puts("add m2");
  addmember(list, nomeg3, nomeu6);

puts("inizio elenco");
  member_t* elenco = components(list,  nomeg1);
  puts("componenti g1");
  while(elenco!=NULL){
    printf("%s\n", elenco->nickname);
    elenco = elenco->next;
  }

  elenco = components(list,  nomeg2);
  puts("componenti g2");
  while(elenco!=NULL){
    printf("%s\n", elenco->nickname);
    elenco = elenco->next;
  }

  elenco = components(list,  nomeg3);
  puts("componenti g3");
  while(elenco!=NULL){
    printf("%s\n", elenco->nickname);
    elenco = elenco->next;
  }
  printf("END\n");

  return 0;
}*/
