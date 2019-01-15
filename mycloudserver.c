/*
Program:    mycloudserver.c
Author:     Jared Rigdon
Date:       11/29/2018
Class:      CS 270 Section 001
Purpose:    accepts requests from Mycloud Client 
            and responds based on the requests
            from the client
 */
#include "csapp.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

/*--------------------------- Struct ---------------------------*/
//Structs to recieve and send data to the client
//stores the header from the client
struct {
    int seqNum;     //0-3 bytes
    int status;     //4-7 bytes
} header;

//holds data to be sent to the server
struct store {
    char file[40]; //file name 8-47 bytes
    int byteSize;  //byte size 48-51
};



void echo(int connfd, int secretKey);
int msgType(int type, int connfd, int secretKey);
void copyFiles(char *file1, char *file2);
/*--------------------------- Main ---------------------------*/
//Main func and loop to listen and connect to via socket
int main(int argc, char **argv) 
{
    int listenfd, connfd, msgCnt;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    
    int secretKey;

    //check for arg count
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> <secretkey>\n", argv[0]);
        exit(0);
    }
//check if the secret key is and int
    if(!(secretKey = atoi(argv[2]))){
        fprintf(stderr, "Error: Secret Key must be an integer!\n");
        exit(0);
    }

    //listen on a port
    listenfd = Open_listenfd(argv[1]); 
    while (1) {
        //get client info
        clientlen = sizeof(struct sockaddr_storage); 
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                        client_port, MAXLINE, 0);   
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        
        echo(connfd, secretKey);   //reads the the amount of bytes recieved
        
        //close connection to current client but continue to listen for more requests 
        Close(connfd);
    }
    exit(0);
}

/*--------------------------- Echo ---------------------------*/
//contains the function that reads and writes data to the client
void echo(int connfd, int secretKey) 
{
    size_t n; 
    rio_t rio;
    int seqNum, type;

    //cont to wait for response 
    while((n = Rio_readn(connfd, &seqNum, 4)) != 0) {
        //get the first 8 bytes
        header.seqNum = seqNum;
        Rio_readn(connfd, &type, 4);

        printf("Sequence Num: %d, Type: %d\n", seqNum, type);   //testing
        if (!msgType(type, connfd, secretKey)){
            //error
            header.status = -1; //failure
            //send error msg to client before closing the connection
            Rio_writen(connfd, &header, 8);
        }
        else if (type != 3 && type != 6){
            //send the success status
            header.status = 0;  //success
            Rio_writen(connfd, &header, 8);
        }
    }
}

/*--------------------------- msgType ---------------------------*/
//checks for the type that was sent in and executes the func
//return -1 if error
int msgType(int type, int connfd, int secretKey){

    //--------------------------------- Hello ---------------------------------//
    //hello
    if (type == 1){
        int clientKey;
        Rio_readn(connfd, &clientKey, 4);
        if (clientKey != secretKey){
            printf("Error: The keys do not match.\n");
           
            header.status = -1; //failure
            //send error msg to client before closing the connection
            Rio_writen(connfd, &header, 8);

            Close(connfd);
            return 2;
        }
        return 1;
    }
    //--------------------------------- Store ---------------------------------//
    //store
    else if (type == 2){
        int size; 
        char filename[40]; 
        FILE *sfp;
        char buf[MAXLINE]; 

        //get the filename and the size of the file
        Rio_readn(connfd, filename, 40); 
        Rio_readn(connfd, &size, 4);
        sfp = fopen(filename, "w+"); 
        if (sfp == NULL){
            printf("Cannot open %s\n", filename);
            return 0;
        }  
        else { 
            Rio_readn(connfd, buf, size); 
            fwrite(buf, 1, size, sfp); 
            fclose(sfp); 
        }
        return 1; 
    }
    //--------------------------------- Retrieve ---------------------------------//
    //retrieve
    else if (type == 3){
        //set var to hold info
        struct store s;
        FILE *fp; 
        int n; 
        char filecontent[1000];
        
        char filename2[40];
        Rio_readn(connfd, filename2, 40);

        //open the file
        fp = fopen(filename2, "r"); 
        //check if it exists(close connection?)
        errno = 0;
        if (fp == NULL){
            printf("Error %d \n", errno);
            printf("Cannot open %s\n", filename2); 
            return 0;
        }
        else { 
            //get the byte size of the file(or first 1000 bytes)
            n = fread(filecontent, 1, 1000, fp); 

            //pass in the header
            header.status = 0;
            Rio_writen(connfd, &header, 8);
            //send over the size (4 bytes for both)
            Rio_writen(connfd, &n, 4); 

            //send over the data
            Rio_writen(connfd, filecontent, n); 
            fclose(fp); 
        }
        return 1;
    }
    //--------------------------------- Copy ---------------------------------//
    //copy
    else if (type == 4){
        char file1[40];
        char file2[40];

        //get and store the files to be copied
        Rio_readn(connfd, &file1, 40);
        Rio_readn(connfd, &file2, 40);
        FILE *f1, *f2;

        //check they can be opened
        f1=fopen(file1, "r");
        if (f1 == NULL){ 
            printf("Cannot open %s\n", file1);
            return 0;
        }
        else{fclose(f1);}
        f2=fopen(file2, "r");
        if (f2 == NULL){             
            printf("Cannot open %s\n", file2);
            return 0;
        }
        else{fclose(f2);}

        copyFiles(file1, file2);
        return 1;
    }
    //--------------------------------- List ---------------------------------//
    //list
    else if (type == 6){
        DIR *dir;   
        int fileNum = 0;   //number of files
        char buf[MAXLINE];  //buff to send
        char tempFile[40];  //tempFile for individual files
        dir = opendir("."); 
        struct dirent *dp;  
        while ((dp = readdir(dir)) != NULL){
            if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")){
                //do nothing
            }
            else{
                //reset the tempFile to be filled with \0 everytime
                memset(tempFile, '\0', sizeof(tempFile));
                //fill the temp array with the file name and have the rest still be filled with \0
                strncpy(tempFile, dp->d_name, 40);
                //append the temp file to the buff with an offset 
                strncpy(buf+(fileNum*sizeof(tempFile)), tempFile, (sizeof(tempFile)));
                fileNum++;
            }
        }
        closedir(dir);

        //go ahead and send the header, size of list, and the buf of file names
        header.status = 0;  //success
        Rio_writen(connfd, &header, 8);
        Rio_writen(connfd, &fileNum, 4);
        Rio_writen(connfd, buf, (fileNum*40));
        return 1;
    }
    //--------------------------------- Delete ---------------------------------//
    //delete
    else if (type == 5){       
        char file[40];

        Rio_readn(connfd, &file, 40);
        //remove the file and check if successful
        if(remove(file) == 0){
            return 1;
        }
        else{
            return 0;
        }
    } 
    //shouldnt reach this but just in case
    else{
        return 0;
    }
}

//--------------------------------- Copy Files ---------------------------------//
void copyFiles(char *file1, char *file2){
    //allocate memory
    char filename1[40];
    memset(filename1, '\0', sizeof(filename1));
    strncpy(filename1, file1,40);

    char filename2[40];
    memset(filename2, '\0', sizeof(filename2));
    strncpy(filename2, file2, 40);

    //set the char and file ptrs
    char ch;
    FILE *f1, *f2;

    f1 = fopen(filename1, "r");
    f2 = fopen(filename2, "w");

    //cpy from file 1 to file 2 char by char
    while((ch = fgetc(f1)) != EOF ){
        fputc(ch, f2);  
    }

    //close the files
    fclose(f1);
    fclose(f2);
}