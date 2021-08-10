#include <stdlib.h>
#include <stdio.h>  
#include <cstring>     
#include <arpa/inet.h>  
#include <sys/types.h> 		
#include <sys/socket.h>	
#include <unistd.h>	 
#include <netinet/in.h>	
#include <pthread.h>
#include <netdb.h>
#include <math.h>
#include <signal.h>
#include <sys/time.h>

// Included to get the support library
#include <calcLib.h>
#include "protocol.h"


//================================================
// Entry point of the client application.
//================================================
int main(int argc, char * argv[])
{
    /* 
    * Check command line arguments. 
    */
    if(argc != 2) 
    {
		printf("Syntax: %s <IP>:<PORT>\n", argv[0]);
		return EXIT_FAILURE;
	}

    char delim[] = ":";
    char * pHost = strtok(argv[1], delim);
    char * pPort = strtok(NULL, delim);
    int iPort = atoi(pPort);
    printf("Host %s, and port %d.\n", pHost, iPort);


    /* 
    * Socket: create the parent UDP socket. 
    */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0) {
	    perror("socket()");
        exit(EXIT_FAILURE);
    }


    /*
    * Fill in the server's address and data.
    */
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_addr.s_addr = inet_addr(pHost);
    serveraddr.sin_port = htons(iPort);
    serveraddr.sin_family = AF_INET;


    /* 
    * Bind: associate the parent socket with a port. 
    */
    if(bind(sockfd, (struct sockaddr * ) &serveraddr, sizeof(serveraddr)) < 0) {
	    perror("bind()");
        exit(EXIT_FAILURE);
    }




    /* 
    * Main loop: wait for a datagram.
    */
    while(1)
    { 
        /*
        * Recvfrom: receive a UDP datagram from a client.
        */



        /* 
        * Sendto: echo the input back to the client. 
        */



    } // End of main loop.



    /*
    * Cleanup.
    */
    close(sockfd);

    return 0;
}
