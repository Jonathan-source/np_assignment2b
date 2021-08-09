#include <stdlib.h>
#include <stdio.h>   
#include <cstring>  
#include <arpa/inet.h>  
#include <sys/types.h> 		
#include <sys/socket.h>	
#include <unistd.h>	 
#include <netinet/in.h>	
#include <errno.h>

// Included to get the support library
#include "calcLib.h"
#include "protocol.h"


//================================================
// Entry point of the client application.
//================================================
int main(int argc, char * argv[])
{
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

    return 0;
}