/*
Program:    mycloudclient.c
Author:     Jared Rigdon
Date:       11/29/2018
Class:      CS 270 Section 001
Purpose:    makes requests to store, 
            retrieve, copy, delete, and list 
            files on the Mycloud Server
 */

#include "csapp.h"
#include <stdint.h>
#include <errno.h>
#define MAXARGS   128

void eval(char* cmdLine, int clientfd);
void parser(char *bufParse, char **argList);
int Cmd_List(char **argList, int clientfd);
void printResponse();
void copyFiles(char *file1, char *file2);

/*--------------------------- Struct ---------------------------*/
//Structs used to send and recieve data from the server

//used as the general header, for the hello, and list cmd 
struct header {
    int seqNo;  //sequence num to keep track of current packet 0-3 bytes
    int type;  //type of cmd being sent 4-7 bytes
} head;

//used for the retrieve of delete cmd 8-47 bytes
struct  {
    char file[40];     //filename 8-47 bytes
} retDel;

//used for the stor cmd  8-N bytes
struct store {
    char file[40]; //file name 8-47 bytes
    int byteSize;  //byte size 48-51
};

//used for copy cmd 8-87 bytes
struct copy {
    char file1[40];    //src filename 8-47
    char file2[40];    //dest filename 48-87
};

//return header with seqNo and status
struct {
    int seqNo;
    int status;
} rtnHead;

/*--------------------------- Main ---------------------------*/
//main program, continuously loops for user input until quit or Ctrl+D
int main(int argc, char **argv) 
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;
    int secretKey;
    
    //check for correct arguements
    if (argc != 4) {
        fprintf(stderr, "usage: %s <host> <port> <secretkey>\n", argv[0]);
        exit(0);
    }
    host = argv[1];
    port = argv[2];
    //check if the secret key is and int
    if(!(secretKey = atoi(argv[3]))){
        fprintf(stderr, "Error: Secret Key must be an integer!\n");
        exit(0);
    }

    //confirm connection
    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd); 

    //send the first seqNo, hello msg type, and the secret key
    head.seqNo = 0;
    head.type = 1;
    Rio_writen(clientfd, &head, 8);
    Rio_writen(clientfd, &secretKey, 4);

    //recieve and print the status from the server
    Rio_readn(clientfd, &rtnHead, 8);
    printResponse();
    //if the keys do not match close the connection
    if(rtnHead.status == -1){
        fprintf(stderr, "Error: Secret Key must match!\n");
        Close(clientfd);
        exit(0);
    }
    printf("> ");    

    //continuously read input until Ctrl-D
    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        eval(buf, clientfd);  //parses the buf
        printf("> ");
    }
    Close(clientfd); //close the connection
    exit(0);
}


/*--------------------------- Eval ---------------------------*/
//evaluate a cmd line and parse for exec
void eval(char *cmdLine, int clientfd){
	//initialize vars
	char *argList[MAXARGS];	//used to hold the arg list
	char bufPars[MAXLINE];		//holds the curr parsed cmd line

	strcpy(bufPars, cmdLine);
	parser(bufPars, argList);	//parses the cmdLine to get the argList
	if(argList[0] == NULL){	//used to ignore empty lines
		return;	
	}

    /*
    check var return meanings: 
        -1=local error
        0=invalid cmd
        1=success
        2=local
    */
    int check = Cmd_List(argList, clientfd);
	if(!check){ //0
        //return error msg
        printf("Error: Not a Valid Command!\n");
	}
    else if (check == 1){   //1
        //read and print response
        Rio_readn(clientfd, &rtnHead, 8);
        printResponse();
        //only expect additional data if retrieve or list
        if(head.type == 3 || head.type == 6){
            if(head.type == 6){ //list
                //set local vars to hold returned data
                int numFiles;
                char buff[MAXLINE];
                memset(buff, '\0', MAXLINE);    //not nessecary but to be safe

                Rio_readn(clientfd, &numFiles, 4);
                Rio_readn(clientfd, buff, (numFiles*40));
                printf("\nResult of List CMD:\n");
                for (int i = 0; i < numFiles; i++){
                    //print a line every 40 bytes
                    printf("%.*s \n", 40, buff+(i*40));
                }
            }
            if(head.type == 3){ //retrieve
                int size; 
                FILE *sfp;
                char bufRet[MAXLINE]; 
                //get the filename and the size of the file
                Rio_readn(clientfd, &size, 4);
                //this will create a new file if it doesnt exist
                sfp = fopen(retDel.file, "w+"); 
                if (sfp == NULL){ printf("Cannot open %s\n", retDel.file);}
                else { 
                    Rio_readn(clientfd, bufRet, size); 
                    fwrite(bufRet, 1, size, sfp); 
                    fclose(sfp); 
                }
            }
        }

    }
}

/*--------------------------- CmdLine Parser ---------------------------*/
//parses the line and builds an array for the args in argList
//returns and int if to run in foreground or background
void parser(char *bufParse, char **argList){
	char *delim;
	int argc;

	bufParse[strlen(bufParse)-1] = ' ';	//replace trailing '\n' with space
	while (*bufParse && (*bufParse == ' ')){
		bufParse++;
	}

	//build the argList list
	//add a condition that ignores all chars after the first % and until the second %
	argc = 0;
	while ((delim = strchr(bufParse, ' '))){
        argList[argc++] = bufParse;
		
		*delim = '\0';
		bufParse = delim + 1;
		while (*bufParse && (*bufParse == ' ')){	//ignore whitespace
			bufParse++;
		}
	}
	argList[argc] = NULL;

}

//--------------------------------- Cmd_List ---------------------------------//
//check the first arg in the array and convert to binary accordingly
int Cmd_List(char **argList, int clientfd){
    if(!strcmp(argList[0], "cp")){
        //check if there are 2 args
        if(argList[2] == NULL || argList[3] != NULL){
            printf(" Expected 2 arguement to copy a file \n");
            return -1;
        }
        else{
            //--------------------------------- Copy ---------------------------------//
            //copy 4
            if((argList[1][0] == 'c' && argList[1][1] == ':') && (argList[2][0] == 'c' && argList[2][1] == ':')){
                struct copy c;
                head.type = 4;
                head.seqNo++;
                
                //take off the extra c:
                char filename1[40];
                memset(filename1, '\0', sizeof(filename1));
                int tempLen = (strlen(argList[1]));
                strncpy(filename1, argList[1]+2,(tempLen-2));
                
                //take off the extra c:
                char filename2[40];
                memset(filename2, '\0', sizeof(filename2));
                int tempLen2 = (strlen(argList[2]));
                strncpy(filename2, argList[2]+2,(tempLen2-2));

                //copy file names to struct
                strcpy(c.file1, filename1);
                strcpy(c.file2, filename2);

                //send it (40 bytes each so 80 bytes total)
                Rio_writen(clientfd, &head, 8);
                Rio_writen(clientfd, &c, 80);

            }
            //--------------------------------- Store ---------------------------------//
            //store 2
            else if(!(argList[1][0] == 'c' && argList[1][1] == ':') && (argList[2][0] == 'c' && argList[2][1] == ':')){
                struct store s;
                FILE *fp; 
                int n; 
                char filecontent[1000];
                
                //take off the extra c:
                char filename2[40];
                memset(filename2, '\0', sizeof(filename2));
                int tempLen = (strlen(argList[2]));
                strncpy(filename2, argList[2]+2,(tempLen-2));

                head.type = 2;
                head.seqNo++;

                //open the file
                fp = fopen(argList[1], "r"); 
                //check if it exists(close connection?)
                errno = 0;
                if (fp == NULL){
                    printf("Error %d \n", errno);
                    printf("Cannot open %s\n", argList[1]); 
                    //return and error with which nothing is done
                    return -1;
                }
                else { 
                    //get the byte size of the file(or first 1000 bytes)
                    n = fread(filecontent, 1, 1000, fp); 

                    //assign the file name to the struct
                    strcpy(s.file, filename2);     //use the 2nd filename 

                    //assign the byte size to the struct
                    s.byteSize = n; 

                    //pass in the header
                    Rio_writen(clientfd, &head, 8);

                    //send over the name and size (44 bytes for both)
                    Rio_writen(clientfd, &(s), 44); 

                    //send over the data
                    Rio_writen(clientfd, filecontent, n); 
                    fclose(fp); 
                }

            }
            //--------------------------------- Retrieve ---------------------------------//
            //retrieve 3
            else if((argList[1][0] == 'c' && argList[1][1] == ':') && !(argList[2][0] == 'c' && argList[2][1] == ':')){
                head.type = 3;
                head.seqNo++;

                //take off c:
                char filename1[40];
                memset(filename1, '\0', sizeof(filename1));
                int tempLen = (strlen(argList[1]));
                strncpy(filename1, argList[1]+2,(tempLen-2));
                
                //store locally 
                char filename2[40];
                memset(filename2, '\0', sizeof(filename2));
                strncpy(filename2, argList[2], 40);

                //store the dest file in the struct
                strcpy(retDel.file, filename2);

                //send data
                Rio_writen(clientfd, &head, 8);
                Rio_writen(clientfd, filename1, 40);
                return 1;
            }
            //--------------------------------- Local Copy ---------------------------------//
            //local copy
            else{
                //purform local cmd
                char file1[40];
                char file2[40];

                //get and store the files to be copied
                strncpy(file1, argList[1], 40);
                strncpy(file2, argList[2], 40);
                FILE *f1, *f2;

                //check if it can be opened
                f1=fopen(file1, "r");
                if (f1 == NULL){ 
                    printf("Cannot open %s\n", file1);
                    return -1;    
                }
                else{fclose(f1);}
                f2=fopen(file2, "r");
                if (f2 == NULL){ 
                    printf("Cannot open %s\n", file1);
                    return -1;    
                }
                else{fclose(f2);}

                //call func to copy them
                copyFiles(file1, file2);
                return 2;
            }
        }
        return 1;
    }
    //--------------------------------- Delete ---------------------------------//
    //5     Either send a msg to server to delete the file or delete local
    if(!strcmp(argList[0], "rm")){
        //check if 2nd arg isnt null and no more than 2 args in total
        if(argList[1] == NULL || argList[2] != NULL){
            printf(" Expected 1 arguement to Remove a file \n");
            return -1;
        }
        else{
            //remove c: and send to server
            if(argList[1][0] == 'c' && argList[1][1] == ':'){
                char filename[40];
                memset(filename, '\0', sizeof(filename));
                int tempLen = (strlen(argList[1]));
                strncpy(filename, argList[1]+2,(tempLen-2));
                printf("Filename: %s\n", filename);

                head.type = 5;
                head.seqNo++;

                //send response
                Rio_writen(clientfd, &head, 8);
                Rio_writen(clientfd, filename, 40);                
            }
            else{
                //delete locally
                if(remove(argList[1]) == 0){
                    return 2;   //indicate it was done locally
                }
                else{
                    printf("Error: File does not exist!\n");
                    return -1;
                }
            }
        }
        return 1;
    }
    //--------------------------------- List ---------------------------------//
    //6 send header to searver and expect extra data
    if(!strcmp(argList[0], "list")){
        //check for extra args in the cmd
        if(argList[1] != NULL){
            printf(" Expected 0 arguement to list the current files in the server.\n");
            return -1;
        }
        else{
            //add to struct
            head.type = 6;
            head.seqNo++;
            //send to to server
            Rio_writen(clientfd, &head, 8);
            return 1;
        }
    }

    //exit the program
    ///**************Note: Per requirements, was not stated to also close the server program
    if(!strcmp(argList[0], "quit")){
        Close(clientfd);
        exit(0);
    }
    //not a vaalid cmd
    return 0;
}

//--------------------------------- Print Response ---------------------------------//
//print the response from the server
void printResponse(){
    printf("Response from server:\n");
    printf("Sequence Number: %d\n", rtnHead.seqNo);
    printf("Status: %d\n", rtnHead.status);
}

//--------------------------------- Copy Files ---------------------------------//
//performs a local copy of 2 files
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