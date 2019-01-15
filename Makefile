mycloudclient: mycloudclient.c csapp.c csapp.h 
	gcc -g -o mycloudclient mycloudclient.c csapp.c -lpthread
mycloudserver: mycloudserver.c csapp.c csapp.h 
	gcc -g -o mycloudserver mycloudserver.c csapp.c -lpthread