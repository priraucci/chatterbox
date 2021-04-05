/**
 *
 * @file parser.c
 * author Priscilla Raucci 517046
 * Si dichiara che il contenuto di questo file è in ogno sua parte opera
 * originale dell'autore
 * @brief parser per memorizzare i dati del file di configurazione
 *
 **/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "parametri.h"


void parser(char* filename, configurazione* conf){
  size_t bufsize = 128;

  char *buffer=(char*)malloc(bufsize*sizeof(char));
  if(buffer==NULL){
    fprintf(stderr, "problemi con la malloc\n");
    exit(EXIT_FAILURE);
  }

  FILE* fp = fopen(filename, "r");
  if(fp == NULL){
    free(buffer);
    fprintf(stderr, "problemi con la fopen\n");
    exit(EXIT_FAILURE);
  }

  int i;
  char subString[128];
  while((i = getline(&buffer, &bufsize, fp)) != EOF){
    //printf("letto: %s\n",  buffer);
    if(buffer[0]=='#'){continue;}
      else if(strncmp(buffer, "UnixPath", strlen("UnixPath"))==0){
        strcpy(subString, funzione(buffer));

        memset(&conf->UnixPath, '\0', 128*sizeof(char));
        strcpy(conf->UnixPath, subString);
      } else {
        if(strncmp(buffer, "MaxConnections", strlen("MaxConnections"))==0){
          strcpy(subString, funzione(buffer));
          conf->MaxConnections = atoi(subString);
        }  else {
        if(strncmp(buffer, "ThreadsInPool", strlen("ThreadsInPool"))==0){
            strcpy(subString, funzione(buffer));
            conf->ThreadsInPool = atoi(subString);
          } else {
          if(strncmp(buffer, "MaxMsgSize", strlen("MaxMsgSize"))==0){
              strcpy(subString, funzione(buffer));
              conf->MaxMsgSize = atoi(subString);
            } else {
            if(strncmp(buffer, "MaxFileSize", strlen("MaxFileSize"))==0){
                strcpy(subString, funzione(buffer));
                conf->MaxFileSize = atoi(subString);
              } else {
                if(strncmp(buffer, "MaxHistMsgs", strlen("MaxHistMsgs"))==0){
                  strcpy(subString, funzione(buffer));
                  conf->MaxHistMsgs = atoi(subString);
                } else {
                  if(strncmp(buffer, "DirName", strlen("DirName"))==0){
                    strcpy(subString, funzione(buffer));

                    memset(&conf->DirName, '\0', 128*sizeof(char));
                    strcpy(conf->DirName, subString);
                  } else {
                    if(strncmp(buffer, "StatFileName", strlen("StatFileName"))==0){
                      strcpy(subString, funzione(buffer));

                      memset(&conf->StatFileName, '\0', 128*sizeof(char));
                      strcpy(conf->StatFileName, subString);
                      }
                    }
                  }
                }
              }
            }
          }
        }
  }
//  printf("%s %d %d %d %d %d %s %s\n",  conf->UnixPath, conf->MaxConnections, conf->ThreadsInPool,
//      conf->MaxMsgSize, conf->MaxFileSize, conf->MaxHistMsgs, conf->DirName, conf->StatFileName);
  free(buffer);
  fclose(fp);
}

char* funzione(char* buffer){
  char* subString;
  char tmp[128]; const char u = '=';
  const char s = '\n';
  strcpy(tmp, buffer);
//  subString = strchr(tmp, u) + 2;
  subString = strchr(buffer, u) + 2;
//  strcpy(subString, subString+2);
  char* ptr = strchr(subString, s);
  if(ptr!=NULL) *ptr = '\0';
// printf("buffer: |%s|    tmp: |%s|     subString: |%s|\n",  buffer, tmp, subString);
  return subString;
}
