#include <stdlib.h>
#include <stdio.h>  
#include <cstring>  
#include <math.h>
#include <errno.h>
#include <arpa/inet.h>  
#include <sys/types.h> 		
#include <sys/socket.h>	
#include <unistd.h>	 
#include <netinet/in.h>	
#include <pthread.h>
#include <netdb.h>
#include <signal.h>
#include <sys/time.h>

// Included to get the support library
#include <calcLib.h>
#include "protocol.h"

#define BUFFER_SIZE 256
#define CALC_MESSAGE 12		// sizeof(calcMessage_t);
#define CALC_PROTOCOL 50	// sizeof(calcProtocol_t);
#define TRUE 1
#define FALSE 0

typedef struct calcMessage calcMessage_t;
typedef struct calcProtocol calcProtocol_t;

typedef struct client {
  calcProtocol_t * pcalcProt;
  struct sockaddr_in addr;
  struct timeval lastMessage;
} client_t;

// Global variables.
client_t ** g_clients = NULL;
int g_num_clients = 0;
int g_arr_size = 10;
static uint32_t ID = 0;


// Server related methods.
void add_client(client_t * pclient);
client_t * find_client(uint32_t id);
int handle_calcMessage(calcMessage_t * pcalcMsg);
void host_byte_order(calcProtocol_t * pcalcProt);
void network_byte_order(calcProtocol_t * pcalcProt);
calcProtocol_t * create_calcProtocol();
void precalculate_calcProtocol(calcProtocol_t * pcalcProt);
calcMessage_t * validate_assignment(client_t * pclient, calcProtocol_t * result, const char * fromAddress);



/* Call back function, will be called when the SIGALRM is raised when the timer expires. */
void checkJobbList(int signum)
{ // As anybody can call the handler, its good coding to check the signal number that called it.

	if(signum == SIGALRM)
	{
    	struct timeval currentTime;

		for(int i = 0; i < g_arr_size; i++)
		{
			if(g_clients[i] != NULL)
			{
				gettimeofday(&currentTime, NULL);
        		int deltaTime = (int)((currentTime.tv_sec - g_clients[i]->lastMessage.tv_sec));
        		if(deltaTime >= 10) 
				{
					printf("[Info] connection timeout, client with id %d considered lost.\n", g_clients[i]->pcalcProt->id);
					free(g_clients[i]);
					g_clients[i] = NULL;
					g_num_clients--;
        		}
			}
    	}
	}	
}




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
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_addr.s_addr = inet_addr(pHost);
    serverAddr.sin_port = htons(iPort);
    serverAddr.sin_family = AF_INET;


    /* 
    * Bind: associate the parent socket with a port. 
    */
    int status = bind(sockfd, (struct sockaddr * ) &serverAddr, sizeof(serverAddr));
	if(status < 0) {
	    perror("bind()");
        exit(EXIT_FAILURE);
    }

    /* 
    * SIGALRM: configure alarm time. 
    */
	struct itimerval alarmTime;
	/* Configure the timer to expire after 1 sec... */
	alarmTime.it_interval.tv_sec = 1;
	alarmTime.it_interval.tv_usec = 0;
	/* ... and every 1 sec after that. */
	alarmTime.it_value.tv_sec = 1;
	alarmTime.it_value.tv_usec = 0;

	signal(SIGALRM, checkJobbList);
	setitimer(ITIMER_REAL, &alarmTime, NULL);
    
	int byteSent, byteRcvd;
    void * pflexible = (void *) malloc(sizeof(calcProtocol_t)); // Flexible pointer.
    struct sockaddr_in cliAddr;
    socklen_t cliLen = sizeof(cliAddr);

	g_clients = (client_t**)malloc(sizeof(client_t*) * g_arr_size);


    /* 
    * Main loop: wait for and handle datagram.
    */
    while(1)
    { 

        /*
        * Recvfrom: receive UDP datagram from client(s).
        */
		memset(&cliAddr, 0, sizeof(cliAddr));
        byteRcvd = recvfrom(sockfd, pflexible, sizeof(calcProtocol_t), 0, (struct sockaddr*) &cliAddr, &cliLen);
        if (byteRcvd <= 0) {
            perror("recvfrom()\n");
        } 
		printf("\n[Info] received datagram [%d bytes] from %s:%d\n", byteRcvd, inet_ntoa(cliAddr.sin_addr), htons(cliAddr.sin_port));


        /*
        * Handle datagram by size.
        */
		if(byteRcvd == CALC_MESSAGE)
   		{
			calcMessage_t * pcalcMsgHolder = (calcMessage_t*)pflexible;	   
			if(handle_calcMessage(pcalcMsgHolder))
			{
				// Create new client.
				client_t * client = (client_t*)malloc(sizeof(client_t));
				client->addr = cliAddr;

				// Create new protocol.
				client->pcalcProt = create_calcProtocol();

				// Always send data on the network in big-endian...
				network_byte_order(client->pcalcProt); 

				// Send protocol to client.
				byteSent = sendto(sockfd, client->pcalcProt, sizeof(calcProtocol_t), 0, (const struct sockaddr *) &cliAddr, sizeof(cliAddr));
				if (byteRcvd <= 0) {
            		perror("sendto()\n");
        		}
				printf("[Info] sent datagram [%d bytes] to %s:%d\n", byteSent, inet_ntoa(cliAddr.sin_addr), htons(cliAddr.sin_port));		

				// Pre-calculate the protocol for easier comparison.
				host_byte_order(client->pcalcProt);
				precalculate_calcProtocol(client->pcalcProt);
				
				// Timestamp client.
				gettimeofday(&client->lastMessage, NULL);

				// Add client to list.
				add_client(client);

			}
			else // Protocol not supported.
			{
				byteSent = sendto(sockfd, pcalcMsgHolder, sizeof(calcMessage_t), 0, (const struct sockaddr *) &cliAddr, sizeof(cliAddr));
				if (byteRcvd <= 0) {
            		perror("sendto()\n");
        		} 
			  	printf("[Info] protocol not supported. Datagram [%d bytes] were sent back to %s:%d\n", byteSent, inet_ntoa(cliAddr.sin_addr), htons(cliAddr.sin_port));	
			}
		}
		else if(byteRcvd == CALC_PROTOCOL)
    	{
			calcProtocol_t * pcalcProtHolder = (calcProtocol_t*)pflexible;
			host_byte_order(pcalcProtHolder);

      		char comprAddress[BUFFER_SIZE];
      		memset(&comprAddress, 0, sizeof(comprAddress));
      		sprintf(comprAddress, "%s:%d", inet_ntoa(cliAddr.sin_addr), htons(cliAddr.sin_port));
		  
      		// Find node / client.
      		client_t * pclient = find_client(pcalcProtHolder->id);

			// Compare result if node was found.
			if(pclient != NULL)
			{
				calcMessage_t * pcalcMsgHolder = validate_assignment(pclient, pcalcProtHolder, comprAddress);

	    		byteSent = sendto(sockfd, pcalcMsgHolder, sizeof(calcMessage_t), 0, (const struct sockaddr *)&cliAddr, sizeof(cliAddr));
				if (byteRcvd <= 0) {
            		perror("sendto()\n");
        		} 
   				printf("[Info] sent datagram [%d bytes] to %s:%d\n", byteSent, inet_ntoa(cliAddr.sin_addr), htons(cliAddr.sin_port));

				// Client has completed the assignment, remove it from list.
				for(int i = 0; i < g_arr_size; i++)
				{
					if(g_clients[i] == pclient)
					{
						free(g_clients[i]);
						g_clients[i] = NULL;
						g_num_clients--;
						break;
					}
				}
			}
			else // Client were not in the list (probably a lost client).
			{
				byteSent = sendto(sockfd, "ERROR TO\n", sizeof("ERROR TO\n"), 0, (const struct sockaddr *)&cliAddr, sizeof(cliAddr));
				if (byteRcvd <= 0) {
            		perror("sendto()\n");
        		} 
			}
		}


    } // End of main loop.



    /*
    * Cleanup.
    */
	if(pflexible != NULL)
		free(pflexible);

	for(int i = 0; i < g_arr_size; i++)
	{
		if(g_clients[i] != NULL)
			free(g_clients[i]);
	}

    close(sockfd);

    return EXIT_SUCCESS;
}



/*
* Method for adding a new client to the array. If the array is full, it will be expanded.
*/
void add_client(client_t * pclient)
{
	// Check if the array needs to be expanded (double size).
	if(g_num_clients == g_arr_size)
	{
		g_arr_size = g_arr_size * 2;
		g_clients = (client_t**) realloc(g_clients, sizeof(client_t*) * g_arr_size);
	}

	// Find an empty spot for the new client.
	for(int i = 0; i < g_arr_size; i++)
	{
		if(g_clients[i] == NULL)
		{
			g_clients[i] = pclient;
			g_num_clients++;
			break;
		}
	}
}


/*
* Method for finding a client with specific id.
*/
client_t * find_client(uint32_t id)
{
	client_t * pclient = NULL;
	for(int i = 0; i < g_arr_size; i++)
	{
		if(g_clients[i]->pcalcProt->id == id)
		{
			pclient =  g_clients[i];
			break;
		}
	}

	return pclient;
}


/*
* Method for validating the assignment and create a calcMessage based on result.
*/
calcMessage_t * validate_assignment(client_t * pclient, calcProtocol_t * result, const char * fromAddress)
{
	calcMessage_t * pcalcMsg = (calcMessage_t*) malloc(sizeof(calcMessage_t*)); 

	char refAddress[BUFFER_SIZE];
    memset(&refAddress, 0, sizeof(refAddress));
    sprintf(refAddress, "%s:%d", inet_ntoa(pclient->addr.sin_addr), htons(pclient->addr.sin_port));
			
	if(strcmp(refAddress, fromAddress) == 0) 
	{
    	calcProtocol_t * ref = pclient->pcalcProt;
		
		if(result->arith < 5) // Int values.  
  		{
	  		if(result->inResult == ref->inResult)
				pcalcMsg->message = 1;	// OK
			else
				pcalcMsg->message = 2;	// NOT OK
  		}
		else // Float values.
  		{
			double dDelta = fabs(result->flResult - ref->flResult);
			if (dDelta <= 0.0001)
				pcalcMsg->message = 1; // OK
			else
				pcalcMsg->message = 2; // NOT OK
  		}
		pcalcMsg->type = 2;
    }
	else 
	{
		pcalcMsg->type = 3;
		pcalcMsg->message = 2; 
	}

	pcalcMsg->type 			= htons(pcalcMsg->type);    		// uint16_t
  	pcalcMsg->message 		= htonl(pcalcMsg->message); 		// uint32_t
  	pcalcMsg->protocol 		= htons(pcalcMsg->protocol); 		// uint16_t
  	pcalcMsg->major_version = htons(pcalcMsg->major_version); 	// uint16_t
  	pcalcMsg->minor_version = htons(pcalcMsg->minor_version); 	// uint16_t

	return pcalcMsg;
}


/*
* Method for pre-calculating the calcProtocol.
*/
void precalculate_calcProtocol(calcProtocol_t * pcalcProt)
{
	switch(pcalcProt->arith)
  	{
	  	case 0:
			printf("Error: calcProtocol-->arith = 0.");
		break;
		case 1: 
			pcalcProt->inResult = pcalcProt->inValue1 + pcalcProt->inValue2;
		break;
		case 2:
			pcalcProt->inResult = pcalcProt->inValue1 - pcalcProt->inValue2;
		break;
		case 3:
			pcalcProt->inResult = pcalcProt->inValue1 * pcalcProt->inValue2;
		break;
		case 4:
			pcalcProt->inResult = pcalcProt->inValue1 / pcalcProt->inValue2;
		break;
		case 5:
			pcalcProt->flResult = pcalcProt->flValue1 + pcalcProt->flValue2;
		break;
		case 6:
		  pcalcProt->flResult = pcalcProt->flValue1 - pcalcProt->flValue2;
		break;
		case 7:
			pcalcProt->flResult = pcalcProt->flValue1 * pcalcProt->flValue2;
		break;
		case 8:
		  pcalcProt->flResult = pcalcProt->flValue1 / pcalcProt->flValue2;
		break;
  	}
}


/*
* Method for checking if calcMessage is correct: type = 22, message = 0, protocol = 17, major_version = 1, minor_version = 0.
* If the calcMessage is incorrect, it will be changed: type = 2, message = 2, major_version = 1, minor_version = 0.
*/
int handle_calcMessage(calcMessage_t * pcalcMsg)
{
	int isCorrect = FALSE;
	if(pcalcMsg->type 			== ntohs(22)  &&
		pcalcMsg->message 		== ntohs(0)   &&
		pcalcMsg->protocol 	   	== ntohs(17)  &&
		pcalcMsg->major_version == ntohs(1)   &&
		pcalcMsg->minor_version == ntohs(0) )
	{
		isCorrect = TRUE;
	}
	else {
		pcalcMsg->type 			= htons(2);
		pcalcMsg->message 		= htons(2);
		pcalcMsg->major_version = htons(1);
		pcalcMsg->minor_version = htons(0);
	}

	return isCorrect;
}


/*
* Method for creating a new protocol.
*/
calcProtocol_t * create_calcProtocol()
{
  	calcProtocol_t * new_calcProtocol = (calcProtocol_t*) malloc(sizeof(calcProtocol_t));

  	new_calcProtocol->arith = rand() % 7 + 1;
	if(new_calcProtocol->arith < 5) 
	{
		new_calcProtocol->inValue1 = randomInt();
		new_calcProtocol->inValue2 = randomInt();
	}
	else 
	{
		new_calcProtocol->flValue1 = randomFloat();
		new_calcProtocol->flValue2 = randomFloat();
	}
  	new_calcProtocol->id = ++ID;
  
	return new_calcProtocol;
}


/*
* Method for converting calcProtocol from network byte order to host byte order.
*/
void host_byte_order(calcProtocol_t * pcalcProt)
{
    pcalcProt->type             = ntohs(pcalcProt->type);           // uint16_t
    pcalcProt->major_version    = ntohs(pcalcProt->major_version);  // uint16_t         
    pcalcProt->minor_version    = ntohs(pcalcProt->minor_version);  // uint16_t
    pcalcProt->id               = ntohl(pcalcProt->id);             // uint32_t
    pcalcProt->arith            = ntohl(pcalcProt->arith);          // uint32_t
    pcalcProt->inValue1         = ntohl(pcalcProt->inValue1);       // int32_t
    pcalcProt->inValue2         = ntohl(pcalcProt->inValue2);       // int32_t
    pcalcProt->inResult         = ntohl(pcalcProt->inResult);       // int32_t

    // double-types can be ignored.
}


/*
* Method for converting calcProtocol to from host byte order to network byte order.
*/
void network_byte_order(calcProtocol_t * pcalcProt)
{
    pcalcProt->type             = htons(pcalcProt->type);           // uint16_t
    pcalcProt->major_version    = htons(pcalcProt->major_version);  // uint16_t         
    pcalcProt->minor_version    = htons(pcalcProt->minor_version);  // uint16_t
    pcalcProt->id               = htonl(pcalcProt->id);             // uint32_t
    pcalcProt->arith            = htonl(pcalcProt->arith);          // uint32_t
    pcalcProt->inValue1         = htonl(pcalcProt->inValue1);       // int32_t
    pcalcProt->inValue2         = htonl(pcalcProt->inValue2);       // int32_t
    pcalcProt->inResult         = htonl(pcalcProt->inResult);       // int32_t

    // double-types can be ignored.
}
