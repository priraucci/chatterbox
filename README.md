# Chatterbox
*Progetto di Sistemi Operativi e Laboratorio @Unipi. Anno Accademico 2016/2017.*

Operating Systems and Laboratory Project @ Unipi. Academic year 2016/2017.

The project involves the development of a competing server that implements
a chat: chatterbox. The name given to the server is chatty.

You can find the project presentation here 
http://didawiki.di.unipi.it/doku.php/informatica/sol/laboratorio17/progetto

--->sorry! The presentation as well the final report is written in Italian

## How to use it

To test the project: 

``` 
git clone https://github.com/priraucci/chatterbox.git
cd Chatterbox
make
```

Then you can launch the server:
```
./chatty -f ./DATA/chatty.conf1
```
You can run the client using the command:
```
./client -l tmp/chatty_socket -c pri
```
There are multiple flags that can be used, please refer to the <`client.c`> file for the full list. 


For more information about possible settings and operations please refer to the presentation of the project. 

## Folders

In the folder **DATA** you will find some configuration files.

In the folder **consegna** you can find the project presentation.

In the folder **tests** you can find some tests that can be used to try the Project.

In the folder **relazione** you can find my final report.

### Disclaimer
This code is highly sensitive to the coding environment. It was developed and tested using Ubuntu17.10.

