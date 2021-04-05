/**
 *
 * @file parametri.h
 * author Priscilla Raucci 517046
 * Si dichiara che il contenuto di questo file è in ogno sua parte opera
 * originale dell'autore
 * @brief struttura per la gestione e memorizzazione dei parametri passati dal file di configurazione
 *
 **/

#if !defined(PARAMETRI_H_)
#define PARAMETRI_H_

typedef struct configurazione{
  int ThreadsInPool;
  int MaxConnections;
  int MaxMsgSize;
  int MaxFileSize;
  int MaxHistMsgs;
  char UnixPath[128]; //path utilizzato per la creazione del socket AF_UNIX
  char DirName[128]; // directory dove memorizzare i files da inviare agli utenti
  char StatFileName[128]; //file nel quale verranno scritte le statistiche del server
}configurazione;

#endif
