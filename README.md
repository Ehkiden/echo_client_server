# echo_client_server
Programs:   mycloudclient.c mycloudserver
Author:     Jared Rigdon
Date:       11/29/2018

To Execute: 
    make mycloudclient
    make mycloudserver
    mycloudserver TCPport SecretKey
    mycloudclient ServerName TCPport SecretKey

    Where ServerName is the location of the server program
    TCPport is the port to connect/listen on
    SecretKey is an unsigned 4 byte integer

mycloudclient Description:
    Makes requests to store, retrieve, copy, delete, and list 
    files on the Mycloud Server.

    
    Supported Commands:
    cp <filename1> <filename2>
        -Copy file “filename1” to file “filename2”. 
        Assuming neither filename1 and filename2 starts with c:
        4 Different use cases
        1) cp FileName1 FileName2
            This is to copy a file locally at the client site.
        2) cp FileName1 c:FileName2
            The client will copy file FileName1 from the local storage to the cloud site (server) and store in
            the cloud with name “FileName2”. The client sends a STORE message to the server.
        3) cp c:FileName1 FileName2
            The client will copy file FileName1 from the cloud storage (server) to the local site and name it
            as FileName2. The client sends a RETRIEVE message to the server.
        4) cp c:FileName1 c:FileName2
            The client will request the server to copy FileName1 in the cloud to another file named FileName2,
            which is also stored at the cloud site. The client sends a COPY message to the server.
    rm <filename>
        -Removes the file from the respective location. 
        c: before the filename denotes if the file is on the server
    list
        -Sends a request to the server for a list of files in the servers current working directory
    quit or Ctrl+D 
        -exits the client program

mycloudserver Description:
    Accepts requests from Mycloud Client and responds based 
    on the requests from the client.

Limitations:
    Most limitations are based on the assumptions listed in the requiremnets
        Filenames will never be more than 39 characters long.
        Filenames can contain any ASCII characters except \0.
        Files can contain binary or textual data.
        Files will not be longer than 100 KB.
        The SecretKey will be an unsigned integer value in the range 0 to 2^(32) − 1.
    As the requirements did not state for the server program to also close when the client
    program closes or when the connection is closed
