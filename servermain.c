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

#define BUFFER_SIZE 256
#define CALC_MESSAGE 12
#define CALC_PROTOCOL 50
#define TRUE 1
#define FALSE 0

typedef struct calcMessage calcMessage_t;
typedef struct calcProtocol calcProtocol_t;

typedef struct client {
  calcProtocol_t * pcalcProt;
  struct sockaddr_in addr;
  struct timeval lastMessage;
} client_t;

typedef struct node {
	client_t * pclient;
	node* next;
	node* prev;
} node_t;

// Global variables.
node_t* head = NULL;
static uint32_t ID = 0;

// Linked list related methods.
node_t * create_node(client_t * pclient);
void add_node(node_t * node);
node_t * find_node(uint32_t id);
void remove_node_by(uint32_t id);
void print_list();
void deallocate_node(node_t * node);
void deallocate_client(client_t * client);
void free_list();

// Server related methods.
int handle_calcMessage(calcMessage_t * pcalcMsg);
void host_byte_order(calcProtocol_t * pcalcProt);
void network_byte_order(calcProtocol_t * pcalcProt);
calcProtocol_t * create_calcProtocol();
void precalculate_calcProtocol(calcProtocol_t * pcalcProt);
calcMessage_t * validate_assignment(node_t * ref_node, calcProtocol_t * result, const char * fromAddress);



/* Call back function, will be called when the SIGALRM is raised when the timer expires. */
void checkJobbList(int signum)
{ // As anybody can call the handler, its good coding to check the signal number that called it.

	if(signum == SIGALRM)
	{
		free_list();
		if(head != NULL)
  		{
			node_t * current = head;
    		struct timeval currentTime;

			while (current)
    		{
				gettimeofday(&currentTime, NULL);
        		int deltaTime = (int)(currentTime.tv_sec - current->pclient->lastMessage.tv_sec);
        		if(fabs(deltaTime) > 5) 
				{
					printf("\nremove id: %d\n", current->pclient->pcalcProt->id);
          			remove_node_by(current->pclient->pcalcProt->id);
        		} 
        		current = current->next;
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
    if(bind(sockfd, (struct sockaddr * ) &serverAddr, sizeof(serverAddr)) < 0) {
	    perror("bind()");
        exit(EXIT_FAILURE);
    }

    /* 
    * SIGALRM: configure alarm time. 
    */
	struct itimerval alarmTime;
	/* Configure the timer to expire after 5 sec... */
	alarmTime.it_interval.tv_sec = 5;
	alarmTime.it_interval.tv_usec = 0;
	/* ... and every 5 sec after that. */
	alarmTime.it_value.tv_sec = 5;
	alarmTime.it_value.tv_usec = 0;

	signal(SIGALRM, checkJobbList);
	setitimer(ITIMER_REAL, &alarmTime, NULL);
    
	int byteSent, byteRcvd;
    void * pflexible = (void *) malloc(sizeof(calcProtocol_t)); // Flexible pointer.
    struct sockaddr_in cliAddr;
    socklen_t cliLen = sizeof(cliAddr);


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
        if (byteRcvd < 0) {
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
				if (byteRcvd < 0) {
            		perror("sendto()\n");
        		}
				printf("[Info] sent datagram [%d bytes] to %s:%d\n", byteSent, inet_ntoa(cliAddr.sin_addr), htons(cliAddr.sin_port));		

				// Pre-calculate the protocol for easier comparison.
				host_byte_order(client->pcalcProt);
				precalculate_calcProtocol(client->pcalcProt);
				
				// Create and add new node to the list.
				node_t * node = create_node(client);
				add_node(node);

				// Timestamp client.
				gettimeofday(&client->lastMessage, NULL);
			}
			else // Protocol not supported.
			{
				byteSent = sendto(sockfd, pcalcMsgHolder, sizeof(calcMessage_t), 0, (const struct sockaddr *) &cliAddr, sizeof(cliAddr));
				if (byteRcvd < 0) {
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
      		node_t * pnode = find_node(pcalcProtHolder->id);

			// Compare result if node was found.
			if(pnode != NULL)
			{
				calcMessage_t * pcalcMsgHolder = validate_assignment(pnode, pcalcProtHolder, comprAddress);

	    		byteSent = sendto(sockfd, pcalcMsgHolder, sizeof(calcMessage_t), 0, (const struct sockaddr *)&cliAddr, sizeof(cliAddr));
				if (byteRcvd < 0) {
            		perror("sendto()\n");
        		} 
   				printf("[Info] sent datagram [%d bytes] to %s:%d\n", byteSent, inet_ntoa(cliAddr.sin_addr), htons(cliAddr.sin_port));
			}
		}


    } // End of main loop.



    /*
    * Cleanup.
    */
	if(pflexible != NULL)
		free(pflexible);
	free_list();

    close(sockfd);

    return EXIT_SUCCESS;
}




/*
* Method for validating the assignment and create a calcMessage based on result.
*/
calcMessage_t * validate_assignment(node_t * ref_node, calcProtocol_t * result, const char * fromAddress)
{
	calcMessage_t * pcalcMsg = (calcMessage_t*) malloc(sizeof(calcMessage_t*)); 

	char refAddress[BUFFER_SIZE];
    memset(&refAddress, 0, sizeof(refAddress));
    sprintf(refAddress, "%s:%d", inet_ntoa(ref_node->pclient->addr.sin_addr), htons(ref_node->pclient->addr.sin_port));
			
	if(strcmp(refAddress, fromAddress) == 0) 
	{
    	calcProtocol_t * ref = ref_node->pclient->pcalcProt;
		
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


/*
 * Method to create a new node.
 */
node_t * create_node(client_t * pclient)
{
	node_t * newnode = (node_t*)malloc(sizeof(node_t));
	newnode->pclient = pclient;
	newnode->next = NULL;
	newnode->prev = NULL;
	return newnode;
} // validated.


/*
 * Method for inserting a node (at the end).
 */
void add_node(node_t * node)
{
	if (head != NULL)
	{
		node_t* it = head;
		while (it->next != NULL)
			it = it->next;
		
		it->next = node;
		node->prev = it;
	}
	else
		head = node;
} // validated.


/*
 * Method for finding a specific node. Returns NULL if node wasn't found.
 */
node_t * find_node(uint32_t id)
{
	node_t * it = head;
	
	if (head == NULL || it->pclient->pcalcProt->id == id) return it;
	
	while (it->next != NULL)
	{
		it = it->next;
		if (it->pclient->pcalcProt->id == id) 
			return it;
	}
	
	return NULL;
} // validated.


/*
 * Method for removing a node by data.
 */
void remove_node_by(uint32_t id)
{
	// Check if list is empty.
	if (head == NULL) return;

	// Find node.
	node_t* node = find_node(id);
	
	if (node != NULL)
	{
		if(node == head) // if node is head.
			head = node->next;

		if (node->next != NULL)
			node->next->prev = node->prev;

		if (node->prev != NULL)
			node->prev->next = node->next;

		deallocate_node(node);
	}
} // validated.


/*
 * Method for printing list.
 */
void print_list()
{
	if (head != NULL)
	{
		printf("\n");
		node_t* temp = head;
		while (temp->next != NULL) {
			printf("%d - ", temp->pclient->pcalcProt->id);
			temp = temp->next;
		}
		printf("%d - ", temp->pclient->pcalcProt->id);
	}
	else
		printf("\n -empty list- ");
} // validated.


/*
 * Method for deallocating a single node.
 */
void deallocate_node(node_t * node)
{
    if(node != NULL)
    {
        deallocate_client(node->pclient);
        free(node);
    }
}


/*
 * Method for deallocating a single client.
 */
void deallocate_client(client_t * client)
{
	if(client != NULL)
    {
        if(client->pcalcProt != NULL)
            free(client->pcalcProt);

        free(client);
	}
}


/*
 * Method for deallocating the entire list.
 */
void free_list()
{
	if (head != NULL)
	{
		node_t* pcalcProt = head;

		while(pcalcProt->next != NULL)
		{
			pcalcProt = pcalcProt->next;
			deallocate_node(pcalcProt->prev);
		}
		deallocate_node(pcalcProt);
		head = NULL;
	}
}