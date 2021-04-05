/**
 *
 * @file parser.h
 * author Priscilla Raucci 517046
 * Si dichiara che il contenuto di questo file è in ogno sua parte opera
 * originale dell'autore
 * @brief header di parser.c
 *
 **/

#if !defined(PARSER_H_)
#define PARSER_H_

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "parametri.h"

/* trova nel file filename i valori da inserire nella struct */
void parser(char* filename, configurazione* conf);

char* funzione(char* buffer);

#endif
