#!/bin/bash
echo "Script $0"
echo "primo patametro, il path: $1"
echo "secondo parametro, t = $2"

if[$1 == "-help" || $2 == "-help" || $3 == "-help" ]; then
  echo "Script bash di eliminiazione di file in uso, inserire il path del file di configurazione e il tempo t"
fi

if[ ! -f $1 ]; then
  echo "$1 non è il file di configurazione"
  exit 1
fi

IN=$(grep '^DirName' $1 | cut -d "=" -f2 | cut -d " " -f2)
echo "il path è:$IN."

if[ $2 -lt 0];then
  echo "$2 non è un numero valido"
  exit 1
fi

#opzione t == 0
if[ $2 -eq 0 ]; then
  echo "t = 0 stampo tutti i file"
  echo $IN*/
  exit
fi

#altrimenti
find $IN -mmin +$2 -exec rm -rf {} \;
exit
