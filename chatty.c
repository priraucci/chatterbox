/**
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 *
 * @file chatty.c
 * author Priscilla Raucci 517046
 * Si dichiara che il contenuto di questo file è in ogno sua parte opera
 * originale dell'autore
 * @brief File principale del server chatterbox
 *
 **/

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

/* inserire gli altri include che servono */
#include "stats.h"
#include "config.h"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include "ops.h"
#include "message.h"
#include "queue.h"
#include "parser.h"
#include "myicl_hash.h"
#include "utils.h"
#include "parametri.h"
#include "groups_t.h"
#include "connections.h"


#define SYSCALL(r,c,e) \
  if((r=c)==-1){       \
    if (errno!=EINTR)  \
    perror(e);         \
    exit(errno);       \
  }

#define ECC_NULL(c, e) \
  if(c==NULL){          \
    perror(e);          \
    exit(errno);        \
  }

/* struttura che memorizza le statistiche del server, struct statistics
 * e' definita in stats.h.
 */
struct statistics  chattyStats = { 0,0,0,0,0,0,0 };

pthread_mutex_t mtxStats = PTHREAD_MUTEX_INITIALIZER;

static icl_hash_t *anagrafica;
static configurazione * conf;
Queue_t *richieste;
static grouplist_t * grouplist;
pthread_t * th;

int valoresegnale = 0;

/* ______________________________ PROTOTIPI  ______________________________*/

void handle_signal(int signal);

static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

int request_handler(long connfd, Queue_t *richieste); //funzione di lettura e impacchettamento richieste
void * thtask(void* arg); //task per i thread
void mask_signals(void);
void set_signal_hnd(struct sigaction*);
int sendMSG(message_t * msg, long connfd, void* receiver, op_t op);
void inc_stats(unsigned long *n);
void dec_stats(unsigned long *n);
void* gestore_segnali(void* args);

/* ______________________________  THREAD PRINCIPALE ______________________________*/

int main(int argc, char *argv[]) {

    if (argc == 1) {
        usage(argv[0]);
        return -1;
    }

    // setto il thread di gestione dei Segnali
    sigset_t sets;
    pthread_t gestore;
    //pthread_attr_t attr;
    int r;

    SYSCALL(r, sigemptyset(&sets), "sigemptyset fallita");
    SYSCALL(r, sigaddset(&sets, SIGPIPE), "sigadset SIGPIPE");
    SYSCALL(r, sigaddset(&sets, SIGUSR1), "sigadset SIGUSR1");
    SYSCALL(r, sigaddset(&sets, SIGINT), "sigadset SIGINT");
    SYSCALL(r, sigaddset(&sets, SIGTERM), "sigadset SIGTERM");
    SYSCALL(r, sigaddset(&sets, SIGQUIT), "sigadset SIGQUIT");
    SYSCALL(r, pthread_sigmask(SIG_SETMASK, &sets, NULL), "pthread_sigmask");

    pthread_create(&gestore, NULL, gestore_segnali, NULL);


    //passo il parser per prendere dati dal file di conf
    conf = (configurazione*)malloc(sizeof(configurazione));
    parser(argv[2], (configurazione*)conf);

    //creo hashtable utenti
    anagrafica = icl_hash_create(60, NULL, NULL);

    //instanzio coda per le richieste da passare ai thread
    richieste = initQueue();

    //creo struttura per ricordare i gruppi
    grouplist = groups_newlist();

    //creo array di thread
    th = malloc(conf->ThreadsInPool*sizeof(pthread_t));
    if(!th){
        fprintf(stderr, "allocazione pthread fallita\n");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < (conf->ThreadsInPool); ++i){
        if(pthread_create(&th[i], NULL, thtask, (void*)richieste)!= 0){
            fprintf(stderr, "allocazione pthread fallita\n");
            exit(EXIT_FAILURE);
        }
    }

    //metto su il socket
    int listenfd;
    SYSCALL(listenfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket");

    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, conf->UnixPath, strlen(conf->UnixPath)+1); // setto l'indirizzo

    int notused;
    SYSCALL(notused, bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr)), "bind"); // assegno l'indirizzo al socket
    SYSCALL(notused, listen(listenfd, conf->MaxConnections), "listen");

    // impostazioni per utilizzare la select
    fd_set set, rdset;
    FD_ZERO(&set);
    FD_ZERO(&rdset);

    FD_SET(listenfd, &set);
    int fdmax = listenfd;

    struct timeval  timeout;

    timeout.tv_sec  = 0;
    timeout.tv_usec = 500000;
    while(!valoresegnale){
        rdset = set;
        int notused;

        SYSCALL(notused, select(fdmax+1, &rdset, NULL, NULL, &timeout), "select");


        for(int i = 0; i <= fdmax; i++){

            if (FD_ISSET(i, &rdset)){
                long connfd;
                if(i == listenfd){
                    SYSCALL(connfd, accept(listenfd, (struct sockaddr*)NULL, NULL), "accept");
                    FD_SET(connfd, &set);
                    if(connfd > fdmax) fdmax = connfd; //la select richiede di memorizzare il massimo
                }else{
                    connfd = i;
                }
                //printf("prima della request handler\n");
                r = 0;
                if((r = request_handler(connfd, richieste)) == 0 ){ // per l'impacchettamento dei dati e l'inserimento chiamo request_handler
                    icl_hash_deconnect( anagrafica, connfd);
                    dec_stats(&chattyStats.nonline);
                    close(connfd);
                    FD_CLR(connfd, &set);
                    if(connfd == fdmax){ //reimposto il massimo
                        for(int i = fdmax; i >= 0; i--) if(FD_ISSET(i, &set)){
                          fdmax = i;
                          break;
                          }
                    }else if(r == -1){printf("errore nella requesthanler\n");}
                    continue;
                }
            }
        }
    }

    //libero tutto
    close(listenfd);

    stopPush(richieste);

    //faccio la join e libero l'array dei thread
    for(int i = 0; i < (conf->ThreadsInPool); ++i){
        if(pthread_join(th[i], NULL)== -1){
            fprintf(stderr, "join pthread fallita\n");
            exit(EXIT_FAILURE);
        }
    }
    pthread_join(gestore, NULL);

    free(th);
    // Dealloco la struttura che elenca i gruppi
    groups_destroy(grouplist);

    //dealloco la struttura della coda di richieste
    deleteQueue(&richieste);

    //dealloco l'hashtable
    icl_hash_destroy(anagrafica, NULL, NULL);
   free(conf);

    printf("cleanup completata\n");

    return 0;
}

void* gestore_segnali(void* args){
  int r;
  int sig;
   //setto maschere
   sigset_t mask;
   SYSCALL(r, sigemptyset(&mask), "sigemptyset fallita");
   SYSCALL(r, sigaddset(&mask, SIGUSR1), "sigadset SIGUSR1");
   SYSCALL(r, sigaddset(&mask, SIGINT), "sigadset SIGINT");
   SYSCALL(r, sigaddset(&mask, SIGTERM), "sigadset SIGTERM");
   SYSCALL(r, sigaddset(&mask, SIGQUIT), "sigadset SIGQUIT");

   while(!valoresegnale){
     if((r=sigwait(&mask, &sig))==0){

       if (sig==SIGUSR1){ //richiesta di stampa delle statistiche
         FILE *statsFile;
         Pthread_mutex_lock(&mtxStats);
         statsFile = fopen(conf->StatFileName, "w");
         if (statsFile != NULL) {
           if (!printStats(statsFile)) fclose(statsFile);
             else printf("Errore scrittura in %s per salvataggio stats.\n", conf->StatFileName);
         } else  printf("Errore apertura file %s per salvataggio stats.\n", conf->StatFileName);
         Pthread_mutex_unlock(&mtxStats);
         continue;
      } else if(sig==SIGINT || sig==SIGTERM || sig==SIGQUIT){ //segnali di chiusura
        valoresegnale = 1;
        return NULL;
      } else continue;

     }
   }

   return NULL;
}


// funzione che legge dal soket i dati, li "impacchetta" in una richiesta da mettere nella coda richieste
//restituisce -1 se la connessione è chiusa
// 0 altrimenti
int request_handler(long connfd, Queue_t *richieste){
//puts("request_handler");

    int n;
    request *rqs = (request*)malloc(sizeof(request));
    memset(&rqs->msg, '0', sizeof(message_t));

    if(!rqs) return -1;
    if((n = readHeader(connfd, &rqs->msg.hdr)) != 1) {
      free(rqs);
      return n;
    }
    if((n = readData(connfd, &rqs->msg.data)) != 1) {
      free(rqs);
      return n;
    }
    //aggiungo gli ultimi dati a rqs
    rqs->connfd = connfd;

    //metto i dati in coda
    if (push(richieste, rqs)==-1) {
      free(rqs);
      perror("errore push coda");
      return -1;
    }

    if (errno != 0)
    return -1;
    else return 1;
}


/* ______________________________ TREADS  ______________________________*/

void * thtask(void* richieste){
    request *corrente = NULL;
    message_t msg;
    long connfd;
    message_data_t file;

    //preparo il messaggio di risposta
    message_data_t risposta;

    while(!valoresegnale){
printf("thread in attesa...\n");
      corrente = pop(richieste);
        if(corrente == NULL){
          if(valoresegnale){
            printf("thread: esco...\n");
            return(NULL);}
          else{
            perror("errore nel prelevare dalla coda");
            exit(EXIT_FAILURE);}
        }

        msg = (corrente->msg);
        connfd = corrente->connfd;
        free(corrente);
        corrente = NULL;

        // "ripulisco" il messaggio di risposta
        memset(risposta.hdr.receiver, '\0', MAX_NAME_LENGTH+1);
        risposta.hdr.len = 0;

        switch((msg.hdr).op){
            case REGISTER_OP:{
            //printf("REGISTER_OP: sender %s\n", msg.hdr.sender);
                int val;
                //controllo che il nickname non esiste già
                if((val = icl_hash_insert( anagrafica, msg.hdr.sender)) == -2 || groups_isinlist( grouplist, msg.hdr.sender) == 0){
                    // il nickname esiste già o esiste un gruppo con stesso nome
                    inc_stats(&chattyStats.nerrors);
                    icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_NICK_ALREADY, connfd);
                    break;
                  } if (val == -1){ // se c'è stato un errore di altro genere
                      inc_stats(&chattyStats.nerrors);
                      icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                      break;
                } else { // se tutto va bene

                    // lo setto come connesso e mando l'elenco di tutti gli utenti online
                    if(icl_hash_connect( anagrafica, &(msg.hdr.sender), connfd, conf->MaxConnections) < 0 ) {
                        inc_stats(&chattyStats.nerrors);
                        icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                        break;
                    }
                    //ora devo mandargli la lista di tutti gli utenti connessi
                    int nconnect;
                    char* buf = NULL;

                    inc_stats(&chattyStats.nusers);

                    icl_hash_listonline(anagrafica, &buf, &nconnect);
                    if(!buf || nconnect == 0){
                        inc_stats(&chattyStats.nerrors);
                        icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                        break;
                     }

                    inc_stats(&chattyStats.nonline);
                    icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_OK, connfd);

                    int len = (MAX_NAME_LENGTH+1)*nconnect;
                    setData( &risposta, msg.hdr.sender, buf, len );
                    icl_hash_senddata(anagrafica, &(msg.hdr.sender), &risposta, connfd);
                    free(buf);
                    break;

                }
            }

            case CONNECT_OP:{
//printf("CONNECT_OP: rischiesta di connessione da: %s\n", msg.hdr.sender);
                int val;
                if( (val = icl_hash_connect( anagrafica, &(msg.hdr.sender), connfd, conf->MaxConnections)) == -1 ){
                    //invio il msg di errore utente sconosciuto
                    inc_stats(&chattyStats.nerrors);
                    icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_NICK_UNKNOWN, connfd);
                    break;
                }if(val == -2){ // invio messaggio di errore
                    inc_stats(&chattyStats.nerrors);
                    icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                    break;
                }else { // se va tutto bene
                    int nconnect;
                    char* buf = NULL;
                    inc_stats(&chattyStats.nonline);

                    // mando l'elenco di tutti gli utenti online(come in REGISTER_OP)
                    icl_hash_listonline(anagrafica, &buf, &nconnect);
                    if(!buf || nconnect == 0){
                        inc_stats(&chattyStats.nerrors);
                      icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                      break;
                    }

                    icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_OK, connfd);
                    int len = (MAX_NAME_LENGTH+1)*nconnect;
                    setData(&risposta, msg.hdr.sender, buf, len);
                    icl_hash_senddata(anagrafica, &(msg.hdr.sender), &risposta, connfd);
                    free(buf);
                    break;

                }
            }

            case POSTTXT_OP:{
              int val;
              //printf("len msg:%d, len conf:%d\n", msg.data.hdr.len, conf->MaxMsgSize);
              if((msg.data.hdr.len) > (conf->MaxMsgSize) ){
                if(msg.data.buf) free(msg.data.buf);
                  inc_stats(&chattyStats.nerrors);
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_MSG_TOOLONG, connfd);
                  break;
                }
//printf("POSTTXT_OP: send: %s  rec: %s   buf:%s len %d\n", msg.hdr.sender, msg.data.hdr.receiver, msg.data.buf, msg.data.hdr.len);
              if(icl_hash_isin(anagrafica, msg.data.hdr.receiver) == 0){ //se devo mandare msg a un utente singolo

                  if((val = sendMSG(&msg, connfd, &(msg.data.hdr.receiver), POSTTXT_OP))==-1) {
                      if(msg.data.buf) free(msg.data.buf);
                      inc_stats(&chattyStats.nerrors);
                      icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                      break;
                  } else if (val==0) {
                    if(msg.data.buf) free(msg.data.buf);
                    icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_OK, connfd);
                    break;
                  } //else break;

              } else if(groups_isinlist( grouplist , msg.data.hdr.receiver) == 0){
                // se devo mandare msg a un gruppo
                //posso mandare un messaggio a un gruppo solo se io sono in quel gruppo
                if(groups_isingroup( grouplist, msg.data.hdr.receiver, msg.hdr.sender)==-1){
                  if(msg.data.buf) free(msg.data.buf);
                  inc_stats(&chattyStats.nerrors);
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_NICK_UNKNOWN, connfd);
                  break;
                }
                char *buf = NULL;
                int nmember;

                groups_components( grouplist, msg.data.hdr.receiver, &buf, &nmember);
                  if(buf==NULL || nmember==0){
                      if(msg.data.buf) free(msg.data.buf);
                      inc_stats(&chattyStats.nerrors);
					            icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                      break;
                  }
                  for(int i=0,p=0; i<nmember; ++i, p+=(MAX_NAME_LENGTH+1)){
                      sendMSG(&msg, connfd, &buf[p], 2);
                  }
                  if(msg.data.buf) free(msg.data.buf);
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_OK, connfd);
                  free(buf);
                  break;

              } else { // se non trovo il nome del receiver
                  inc_stats(&chattyStats.nerrors);
                  if(msg.data.buf) free(msg.data.buf);
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_NICK_UNKNOWN, connfd);
                  break;
              }
            }

			case POSTTXTALL_OP:{
//printf("POSTTXTALL_OP\n");

            if((msg.data.hdr.len) > (conf->MaxMsgSize) ){
              if(msg.data.buf) free(msg.data.buf);
                inc_stats(&chattyStats.nerrors);
                icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_MSG_TOOLONG, connfd);
                break;
              }
              int nregistered;
              char* buf = NULL;

              icl_hash_list(anagrafica, &buf, &nregistered);
              if(!buf || nregistered == 0){
                  if(msg.data.buf) free(msg.data.buf);
                  inc_stats(&chattyStats.nerrors);
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                  break;
              }

            	for(int i=0,p=0; i<nregistered; ++i, p+=(MAX_NAME_LENGTH+1)) {
                        sendMSG(&msg, connfd, &buf[p], 3);

            	}
                if(msg.data.buf) free(msg.data.buf);
                icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_OK, connfd);
                free(buf);
                break;
              }

            case POSTFILE_OP:{
//printf("POSTFILE_OP\n");

              if(readData(connfd, &file)<=0){
                if(msg.data.buf) free(msg.data.buf);
                if(file.buf) free(file.buf);
                inc_stats(&chattyStats.nerrors);
                icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                break;
              }
              if((file.hdr.len)/1024 > (conf->MaxFileSize) || (file.hdr.len) == 0){
                if(msg.data.buf) free(msg.data.buf);
                if(file.buf) free(file.buf);
                  inc_stats(&chattyStats.nerrors);
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_MSG_TOOLONG, connfd);
                  break;
              }

                //devo scrivere file nella directory conf->DirName
                //prendo la stringa e gli tolgo il ./ da davanti e la concateno al nome della directory
                  char* help;
                  char* SubStr = (char*)malloc(sizeof(char)*128);
                  char* StrPath = (char*)malloc(sizeof(char)*128);
                  if(!StrPath|| !SubStr){
                    perror("errore di allocazione");
                  }
                  memset(StrPath, '\0', sizeof(char)*128);
                  memset(SubStr, '\0', sizeof(char)*128);
                  int fdnew;
                  if((help = strchr(msg.data.buf, '/'))!=NULL){
                    strcpy(SubStr, help+1);
                  }else strcpy(SubStr, msg.data.buf);
                  strcpy(StrPath, conf->DirName);
                  int lenght = strlen(StrPath);
                  StrPath[lenght-1] = '\0';
                  strcat(StrPath, "/");
                  strcat(StrPath, SubStr); // così ho ottenuto il path dove memorizzare il file
                  free(SubStr);
                  char* mappedfile;
                  if((fdnew = open(StrPath, O_RDWR | O_CREAT, 0777))==-1){
                    if(msg.data.buf) free(msg.data.buf);
                    if(file.buf) free(file.buf);
                    free(StrPath);
                    inc_stats(&chattyStats.nerrors);
                    icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                    break;
                  }
                  ftruncate(fdnew, file.hdr.len);

                  mappedfile = mmap(NULL, file.hdr.len, PROT_READ | PROT_WRITE, MAP_SHARED, fdnew,  0);
                  if(mappedfile == MAP_FAILED){
                    if(msg.data.buf) free(msg.data.buf);
                    if(file.buf) free(file.buf);
                    free(StrPath);
                    close(fdnew);
                    inc_stats(&chattyStats.nerrors);
                    icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                    break;
                  }
                    //copio
                  if(memcpy(mappedfile, file.buf, file.hdr.len)==NULL){
                    if(msg.data.buf) free(msg.data.buf);
                    if(file.buf) free(file.buf);
                    free(StrPath);
                    close(fdnew);
                    inc_stats(&chattyStats.nerrors);
                    icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                    break;
                  }
                free(msg.data.buf);
                msg.data.buf = NULL;
                msg.data.buf = malloc(sizeof(char)*strlen(StrPath)+1);
                strncpy(msg.data.buf, StrPath, strlen(StrPath)+1);
                msg.data.hdr.len = strlen(StrPath)+1;
                free(StrPath);
                close(fdnew);

                if(icl_hash_isin(anagrafica, msg.data.hdr.receiver) == 0){ //se il destinatario è un singolo utente
                  if(sendMSG(&msg, connfd, &(msg.data.hdr.receiver), 4)==0){ //mando la notfica di file
                  icl_hash_sendAck( anagrafica, msg.hdr.sender, OP_OK, connfd);
                  if(msg.data.buf) free(msg.data.buf);
                  if(file.buf) free(file.buf);
                  break;
                }//se c'è stato un errore nell'invio
                inc_stats(&chattyStats.nerrors);
                icl_hash_sendAck( anagrafica, msg.hdr.sender, OP_FAIL, connfd);
                if(msg.data.buf) free(msg.data.buf);
                if(file.buf) free(file.buf);
                break;
              }else if(groups_isinlist( grouplist , msg.data.hdr.receiver) == 0){//se il destinatario è un gruppo
                //posso mandare un messaggio a un gruppo solo se io sono in quel gruppo
                if(groups_isingroup( grouplist, msg.data.hdr.receiver, msg.hdr.sender)==-1){
                  inc_stats(&chattyStats.nerrors);
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_NICK_UNKNOWN, connfd);
                  if(msg.data.buf) free(msg.data.buf);
                  if(file.buf) free(file.buf);
                  break;
                }
                char *buf = NULL;
                int nmember;

                groups_components( grouplist, msg.data.hdr.receiver, &buf, &nmember);
                if(buf==NULL || nmember==0){
                  inc_stats(&chattyStats.nerrors);
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                  if(msg.data.buf) free(msg.data.buf);
                  if(file.buf) free(file.buf);
                  if(buf) free(buf);
                  break;
                }
                for(int i=0,p=0; i<nmember; ++i, p+=(MAX_NAME_LENGTH+1)){
                  sendMSG(&msg, connfd, &buf[p], 4);
                }

                icl_hash_sendAck( anagrafica, msg.hdr.sender, OP_OK, connfd);
                if(msg.data.buf) free(msg.data.buf);
                if(file.buf) free(file.buf);
                free(buf);
                break;
              } else {//se non trovo il destinatario
                inc_stats(&chattyStats.nerrors);
                icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_NICK_UNKNOWN, connfd);
                if(msg.data.buf) free(msg.data.buf);
                if(file.buf) free(file.buf);
                break;
              }
            }

            case GETFILE_OP:{
//printf("GETFILE_OP:");

                //recupero il path da cui prendere il file
                  char* StrPath = (char*)malloc(sizeof(char)*(msg.data.hdr.len));
                  strncpy(StrPath, msg.data.buf, sizeof(char)*(msg.data.hdr.len));
                  int fd;

                  struct stat st;
                  if(stat(StrPath, &st)==-1){
                    if(msg.data.buf) free(msg.data.buf);
                  	free(StrPath);
                    icl_hash_sendAck( anagrafica, msg.hdr.sender, OP_FAIL, connfd);
                    break;
                  }
                  char* buf = (char*)malloc(st.st_size+1);
                  if(!buf){
                    if(msg.data.buf) free(msg.data.buf);
                  	free(StrPath);
                    icl_hash_sendAck( anagrafica, msg.hdr.sender, OP_FAIL, connfd);
                    break;
                  }
                  memset(buf, '\0', st.st_size+1);
                  if((fd = open(StrPath, O_RDONLY))==-1){
                      if(msg.data.buf) free(msg.data.buf);
                  		free(StrPath);
                  		free(buf);
                      inc_stats(&chattyStats.nerrors);
                      icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                      break;
                  }
                  int left = (int)st.st_size;
                  int r = 0;
                  int totBytes = 0;
                  while(left>0){
                    if((r=read((int)fd, buf+totBytes, left))==-1){
                      if(msg.data.buf) free(msg.data.buf);
                  		free(StrPath);
                  		free(buf);
                        close(fd);
                        free(StrPath);
                        inc_stats(&chattyStats.nerrors);
                        icl_hash_sendAck( anagrafica, msg.hdr.sender, OP_FAIL, connfd);
                      break;
                    }
                    left-=r;
                    totBytes+=r;
                  }
                  if(msg.data.buf) free(msg.data.buf);
                  setData(&risposta, "", buf, st.st_size+1);

                icl_hash_sendAck(anagrafica, msg.hdr.sender, OP_OK, connfd);
                icl_hash_senddata(anagrafica, msg.hdr.sender, &risposta, connfd);

                  close(fd);
                  free(StrPath);
                  if(buf) free(buf);
                  inc_stats(&chattyStats.nfiledelivered);
                  dec_stats(&chattyStats.nfilenotdelivered);
                  break;
            }

            case GETPREVMSGS_OP:{
//printf("GETPREVMSGS_OP:");
              message_t_list *list = NULL;
              message_t_list *tmp = NULL;
              size_t n;

              if((n = icl_hash_getmsg( anagrafica, &(msg.hdr.sender), &list)) < 0){ //caso di errore

                  inc_stats(&chattyStats.nerrors);
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                  break;
              }
              icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_OK, connfd);
              size_t buf = n;
              setData(&risposta, (char*)msg.hdr.sender, (const char*)&buf , (unsigned int)sizeof(size_t*));
              icl_hash_senddata( anagrafica, &(msg.hdr.sender), &risposta, connfd);
              while( list != NULL  && n > 0){

                icl_hash_sendmsg(anagrafica, &(msg.hdr.sender), &list->msg, connfd, list->msg.hdr.op);
                tmp = list;
                list = list->next;
                free(tmp->msg.data.buf);
                free(tmp);
                  inc_stats(&chattyStats.ndelivered);
                  dec_stats(&chattyStats.nnotdelivered);
                n--;
              }
              break;

            }

            case USRLIST_OP:{
//printf("USRLIST_OP");
              int nconnect;
              char* buf = NULL;

              icl_hash_listonline(anagrafica, &buf, &nconnect);
              if(!buf || nconnect==0){
                  inc_stats(&chattyStats.nerrors);
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                  break;
              }

              icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_OK, connfd);
              int len = (MAX_NAME_LENGTH+1)*nconnect;
              setData(&risposta, msg.hdr.sender, buf, len);
              icl_hash_senddata(anagrafica, &(msg.hdr.sender), &risposta, connfd);
              free(buf);
              break;
            }

            case UNREGISTER_OP:{
//printf("UNREGISTER_OP");
                if(icl_hash_delete( anagrafica, msg.data.hdr.receiver) != 0){ // se c'è qualche errore
                    inc_stats(&chattyStats.nerrors);
                    icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_NICK_UNKNOWN, connfd);
                    break;
                } else { // se sono riuscita ad eliminarlo correttamente
                    dec_stats(&chattyStats.nusers);
                    icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_OK, connfd);
                    break;
                }
            }

            case DISCONNECT_OP:{
//printf("DISCONNECT_OP\n");
                if(icl_hash_deconnect( anagrafica, connfd) != 0){ // se c'è stato un errore
                    inc_stats(&chattyStats.nerrors);
                    icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                    break;
                } else { // se va tutto bene
                    dec_stats(&chattyStats.nonline);

                    icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_OK, connfd);
                    break;
                }
              }


            case CREATEGROUP_OP:{
//printf("CREATEGROUP_OP");
              int val;
              if(icl_hash_isin( anagrafica, msg.data.hdr.receiver) == 0){
                //controllo se esiste un utente con stesso nome
                  inc_stats(&chattyStats.nerrors);
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_NICK_ALREADY, connfd);
                  break;
              }
              if((val = groups_addgroup( grouplist, msg.data.hdr.receiver, msg.hdr.sender)) == -1){ // se c'è stato un errore
                  //se c'è stato un errore generico
                  inc_stats(&chattyStats.nerrors);
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                  break;
                } if(val == -2){ //se esiste già un gruppo con stesso nome
                    inc_stats(&chattyStats.nerrors);
                    icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_NICK_ALREADY, connfd);
                    break;
                }else { // se va tutto bene
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_OK, connfd);
                  break;
              }
            }

            case ADDGROUP_OP:{
//printf("ADDGROUP_OP");
              if(groups_addmember( grouplist, msg.data.hdr.receiver, msg.hdr.sender) != 0){ // se c'è stato un errore
                  inc_stats(&chattyStats.nerrors);
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                  break;
              } else { // se va tutto bene
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_OK, connfd);
                  break;
              }
            }

            case DELGROUP_OP:{
//printf("DELGROUP_OP");
              if(groups_delmember( grouplist, msg.data.hdr.receiver, msg.hdr.sender) != 0){ // se c'è stato un errore
                  inc_stats(&chattyStats.nerrors);
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                  break;
              } else { // se va tutto bene
                  icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_OK, connfd);
                  break;
              }
            }

            default: {
              inc_stats(&chattyStats.nerrors);
              icl_hash_sendAck( anagrafica, &(msg.hdr.sender), OP_FAIL, connfd);
                break;
            }
        }

        //corrente = NULL;
      }

    fprintf(stdout, "thread: esco...");
    fflush(stdout);
    pthread_exit(NULL);
}


/**
 * funzione che invia messaggio :
 * se il receiver è online lo invia via socket altrimenti lo inserisce nella sua lista
 **/

int sendMSG(message_t * msg, long connfd, void* receiver, op_t op){
//printf("sendMSG a %s\n", (char*)receiver);
//printf("sendMSG: %d\n", op);
//printf("sendMSG sender: %s |receiver %s\n", msg->hdr.sender, msg->data.hdr.receiver);
  int val;

  if((val = icl_hash_sendmsg(anagrafica, receiver, msg, -1, op)) == 0){

    if(op == POSTTXT_OP || op == POSTTXTALL_OP) inc_stats(&chattyStats.ndelivered);
    if(op == POSTFILE_OP) inc_stats(&chattyStats.nfilenotdelivered);

    return 0;
  } else if(val == -2) { // utente non conneso
    if(icl_hash_addmsg(anagrafica, receiver, msg, conf->MaxHistMsgs) == 0){

        if(op == POSTTXT_OP || op == POSTTXTALL_OP) inc_stats(&chattyStats.nnotdelivered);
        if(op == POSTFILE_OP) inc_stats(&chattyStats.nfilenotdelivered);

      return 0;
    } else return -1;
  } else
  //puts("sendMSG: errore in icl_hash_sendmsg");
    return -1;

}

void inc_stats(unsigned long *n){
    Pthread_mutex_lock(&mtxStats);
    (*n)++;
    Pthread_mutex_unlock(&mtxStats);
}

void dec_stats(unsigned long *n){
    Pthread_mutex_lock(&mtxStats);
    (*n)--;
    Pthread_mutex_unlock(&mtxStats);
}
