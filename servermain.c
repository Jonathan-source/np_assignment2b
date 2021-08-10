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

// Global variable for linked list.
node_t* head = NULL;

// Linked list methods.
node_t * create_node(client_t * pclient);
void insert_node(node_t * node);
node_t * find_node(uint32_t id);
void remove_node_by(uint32_t id);
void print_list();
void deallocate_node(node_t * node);
void free_list();









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
void insert_node(node_t * node)
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
		printf("\n -empty- ");
} // validated.


/*
 * Method for deallocating a single node.
 */
void deallocate_node(node_t * node)
{
    if(node != NULL)
    {
        if(node->pclient->pcalcProt != NULL)
            free(node->pclient->pcalcProt);

        if(node->pclient != NULL)
            free(node->pclient);

        free(node);
    }
}


/*
 * Method for deallocating the entire list.
 */
void free_list()
{
	if (head != NULL)
	{
		node_t* current = head;

		while(current->next != NULL)
		{
			current = current->next;
			deallocate_node(current->prev);
		}
		deallocate_node(current);
		head = NULL;
	}
}