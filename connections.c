/**
 * file connections.c
 *
 * Si dichiara che il contenuto di questo file è in ogno sua parte opera
 * originale dell'autore
 *
 * @file connections.c
 * author Priscilla Raucci 517046
 * Si dichiara che il contenuto di questo file è in ogno sua parte opera
 * originale dell'autore
 * @brief File per la gestione delle connessioni
 *
 */


#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <time.h>
#include "connections.h"
#include "ops.h"
#include "utils.h"


#define SYSCALL(r,c,e) \
  if((r=c)==-1){  \
    if (errno!=EINTR)    \
    perror(e);         \
  }

/**
 * @function openConnection
 * @brief Apre una connessione AF_UNIX dal client verso il server
 *
 * @param path Path del socket AF_UNIX
 * @param ntimes numero massimo di tentativi di retry
 * @param secs tempo di attesa in secondi tra due retry consecutive
 *
 * @return il descrittore associato alla connessione in caso di successo
 *         -1 in caso di errore
 */
int openConnection(char* path, unsigned int ntimes, unsigned int secs){
  struct sockaddr_un sa;
  int sockfd;

  if((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
    fprintf(stderr, "errore nella copia del path");
    exit(EXIT_FAILURE);
  }

  memset(&sa, '0', sizeof(sa));
  sa.sun_family = AF_UNIX;
  strncpy(sa.sun_path, path, strlen(path)+1);

  for(int i=0; i < ntimes; i++){
    if (connect(sockfd, (struct sockaddr*) &sa, sizeof(sa)) == 0) {
      return sockfd;
    } else sleep(secs);
  }
  return -1;
}

/**
 * @function readHeader
 * @brief Legge l'header del messaggio
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere(mette qui i dati ricevuti dal socket)
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */

int readHeader(long connfd, message_hdr_t *hdr){
//printf("readHeader\n");
    int n;
    op_t op;
    char* sender = (char*)malloc(sizeof(char)*(MAX_NAME_LENGTH+1));
    if(!sender) return -1;
    memset(sender, '\0',(MAX_NAME_LENGTH+1));

    if(connfd < 0 || hdr == NULL){
      if(sender) free(sender);
      return -1;
    }

    SYSCALL(n, readn(connfd, &op, sizeof(op_t)), "readOp");
    //printf("readHeader: readop siz: %d\n", n);
    if(n==0|| n==-1) {
      if(sender) free(sender);
      return n;
    }
    hdr->op = op;

    SYSCALL(n, readn(connfd, sender, (MAX_NAME_LENGTH+1)), "readSender");
    //printf("readHeader: readsender siz: %d\n", n);
    if(n==0|| n==-1) {
      if(sender) free(sender);
      return n;
    }
    strncpy(hdr->sender, sender, (MAX_NAME_LENGTH+1));
    free(sender);
    return 1;

}

/**
 * @function readData
 * @brief Legge il body del messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al body del messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readData(long fd, message_data_t *data){
//puts("readData");
    int n;
    char* rec = (char*)malloc(sizeof(char)*(MAX_NAME_LENGTH+1));
    if(!rec){return -1;}
    memset(rec, '\0',(MAX_NAME_LENGTH+1));
    unsigned int len;
    if(fd < 0 || data == NULL){
        return -1;
    }

    //estraggo il receiver e lo copio
    SYSCALL(n, readn(fd, rec, (MAX_NAME_LENGTH+1)), "readReciver");
    //printf("readData: readrec siz: %d rec %s\n", n, rec);
    if(n==0|| n==-1) return n;
    strncpy(data->hdr.receiver, rec, (MAX_NAME_LENGTH+1));

    //estraggo la lunghezza del messaggio
    SYSCALL(n, readn(fd, &len, sizeof(unsigned int)), "readLen");
    //printf("readData: readlen siz: %d len %d\n", n, len);
    if(n==0|| n==-1) return n;
    data->hdr.len = len;

    //estraggo il messaggio
    if(len==0){
      free(rec);
      return 1;
    }
    data->buf = (char*)malloc((len)*sizeof(char));
    if(!data->buf) return -1;
    memset(data->buf, '\0', (len)*sizeof(char));

    SYSCALL(n, readn(fd, data->buf, (len)), "readBuf");
    //printf("readData: readbuf siz: %d\n", n);
    if(n==0|| n==-1) return n;
//printf("readData; receiver %s,    len %d,   buf %s\n", data->hdr.receiver, data->hdr.len, data->buf);
    free(rec);
    return 1;

}

/**
 * @function readMsg
 * @brief Legge l'intero messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readMsg(long fd, message_t *msg){
//puts("connections: readMsg");
  if (readHeader(fd, &msg->hdr)>0){
    if (readData(fd, &msg->data)>0)
      return 1;
    else return 0;
  }

  return 0;

}

/* da completare da parte dello studente con altri metodi di interfaccia */


// ------- client side ------
/**
 * @function sendRequest
 * @brief Invia un messaggio(message_t) di richiesta dal client al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendRequest(long fd, message_t *msg){
    int n;
    int len = (((msg->data).hdr).len);

    if(fd < 0 || msg == NULL){
        return -1;
    }

    //invia l'header
    // invio prima l'operazione
    SYSCALL(n, writen(fd, &((msg->hdr).op), sizeof(op_t)), "writeOp in send Request"); //mando l'Operazione
    //printf("sendRequest: writeop siz: %d op: %d\n", n, (msg->hdr).op);
    if(n==0|| n==-1) return n;

    SYSCALL(n, writen(fd, &(msg->hdr).sender, (MAX_NAME_LENGTH+1)), "writeSender");
    //printf("sendRequest: writesend siz: %d send: %s\n", n, (msg->hdr).sender);
    if(n==0|| n==-1) return n;

    // invio il receiver
    SYSCALL(n, writen(fd, &(((msg->data).hdr).receiver), (MAX_NAME_LENGTH+1)), "writeReciver");
    //printf("sendRequest: writerec siz: %d\n", n);
    if(n==0|| n==-1) return n;

    SYSCALL(n, writen(fd, &(((msg->data).hdr).len), sizeof(unsigned int)),"writeLenMSG");
    //printf("sendRequest: writelen siz: %d\n", n);
    if(n==0|| n==-1) return n;

    len = (((msg->data).hdr).len);
    if(len == 0) return 1;

    char  buffer[len];
    memset(&buffer[0], '\0', len);
    strncpy(&buffer[0], msg->data.buf , len);
    SYSCALL(n, write(fd, &buffer, (len)), "writeData");
    //printf("sendRequest: writebuf siz: %d\n", n);
    if(n==0|| n==-1) return n;

    if (errno != 0)
        return -1;
    else return 1;

}

/**
 * @function sendData
 * @brief Invia il body(il contenuto del file) del messaggio dal client al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */

int sendData(long fd, message_data_t *msg){
//puts("sendData");
    int n;
    int len = ((msg->hdr).len);

    if(fd < 0 || msg == NULL){
        return -1;
    }
//printf("sendData: receiver %s   LenMSG %d   buf %s\n", msg->hdr.receiver, msg->hdr.len, msg->buf);
    SYSCALL(n, writen(fd, &(msg->hdr.receiver), (MAX_NAME_LENGTH+1)), "writeReceiverContent");
    //printf("sendData: writerec siz: %d\n", n);
    if(n==0|| n==-1) return n;

    SYSCALL(n, writen(fd, &(msg->hdr.len), sizeof(unsigned int)),"writeLenContent");
    //printf("sendData: writelen siz: %d\n", n);
    if(n==0|| n==-1) return n;

    if(len == 0) return 1;
    SYSCALL(n, writen(fd, (msg->buf), len), "writeContent");
    //printf("sendData: writebuf siz: %d\n", n);
    if(n==0|| n==-1) return n;

    return 1;
}

/**
 * @function sendHeader
 * @brief Invia l'header del messaggio dal client al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */

int sendHeader(long fd, message_hdr_t *msg){
    int n;

    if(fd < 0 || msg == NULL){
        return -1;
    }

    //invia l'header
    SYSCALL(n, writen(fd, &(msg->op), sizeof(op_t)), "writeOp"); //mando l'Operazione
    //printf("sendheader: writeop siz: %d, op: %d\n", n, msg->op);
    if(n==0|| n==-1) return n;

    // invio il sender
    SYSCALL(n, writen(fd, &(msg->sender), (MAX_NAME_LENGTH+1)), "writeSender");
    //printf("sendData: writesend siz: %d, send: %s\n", n, msg->sender);
    if(n==0|| n==-1) return n;

    return 1;


}
