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

typedef struct calcMessage calcMessage_t;
typedef struct calcProtocol calcProtocol_t;

#define MAXLINE 1024
#define TRUE 1
#define FALSE 0
#define UNCOMPLETED 0
#define COMPLETED 1

void host_byte_order(calcProtocol_t * pcalcProt);
void network_byte_order(calcProtocol_t * pcalcProt);
void compute_assignment(calcProtocol_t * pcalcProt);




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
    struct sockaddr_in servAddr;
    socklen_t servLen = sizeof(servAddr);
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_addr.s_addr = inet_addr(pHost);
    servAddr.sin_port = htons(iPort);
    servAddr.sin_family = AF_INET;

    calcMessage_t message {
        htons(22),  // type
        htons(0),   // message
        htons(17),  // protocol
        htons(1),   // major_version
        htons(0)    // minor version
    };

    calcProtocol_t * pcalcProt = (calcProtocol_t*)malloc(sizeof(calcProtocol_t));

    int byteSent, byteRcvd;

    int recieve_calcProtocol    = UNCOMPLETED;
    int calculate_calcProtocol  = UNCOMPLETED;
    int send_calcProtocol       = UNCOMPLETED;
    int all_jobs_completed      = FALSE;


    /* 
    * Main loop: complete all jobs.
    */
    while(!all_jobs_completed)
    {
        /*
        * Job #1: recieve calcProtocol (assignment) from server. 
        */
        if(!recieve_calcProtocol)
        {
            /* 
            * Sendto: send calcMessage to the server.
            */
            byteSent = sendto(sockfd, (const calcMessage_t *) &message, sizeof(calcMessage_t), 0, (const struct sockaddr*)&servAddr, sizeof(servAddr));
            if(byteSent < 0) {
                perror("sendto()");
            } 
            printf("[+] calcMessage [%i bytes] was sent to the server.\n", byteSent);

            /* 
            * Recvfrom: recieve calcProtocol from the server.
            */
            byteRcvd = recvfrom(sockfd, pcalcProt, sizeof(calcProtocol_t), 0, (struct sockaddr*) &servAddr, &servLen);
            if (byteRcvd < 0) {
                perror("recvfrom error");
            } 
            else if (byteRcvd == 16) {
                printf("[-] Server did not support the protocol.\n");
                exit(EXIT_FAILURE);
            }           
            printf("[+] calcProtocol [%d bytes] was received from the server:\n\n", byteRcvd);
      
            printf("[calcProtocol]\nArith:%d\nFloat1:%lf\nFloat2:%lf\nInt1:%d\nInt2:%d\n\n", 
                ntohl(pcalcProt->arith), pcalcProt->flValue1, pcalcProt->flValue2, 
                ntohl(pcalcProt->inValue1), ntohl(pcalcProt->inValue2));

            recieve_calcProtocol = COMPLETED;
        }

        /*
        * Job #2: calculate assignment. 
        */
        if(!calculate_calcProtocol)
        {
            // Convert calcProtocol to host byte order.
            host_byte_order(pcalcProt);

            // Compute assignment.
            compute_assignment(pcalcProt);

            // Convert calcProtocol to network byte order.
            network_byte_order(pcalcProt);

            calculate_calcProtocol = COMPLETED;
        }

        /*
        * Job #3: calculate assignment. 
        */
        if(!send_calcProtocol)
        {
            /* 
            * Sendto: send calcProtocol back to the server.
            */
            byteSent = sendto(sockfd, pcalcProt, sizeof(calcProtocol_t), 0, (const struct sockaddr *) &servAddr, sizeof(servAddr));
            if(byteSent < 0) {
                perror("sendto()");
            } 
            printf("[+] calcProtocol [%d bytes] was sent to the server.\n", byteSent);    

            /* 
            * Sendto: send calcProtocol back to the server.
            */
            byteRcvd = recvfrom(sockfd, &message, sizeof(calcMessage_t), 0, (struct sockaddr*) &servAddr, &servLen);
            if (byteRcvd < 0) {
                perror("recvfrom error");
            }           
            printf("[+] calcMessage [%d bytes] was received from the server: calcMessage type = %d, message = %d.\n", byteRcvd, 
                ntohs(message.type), ntohl(message.message));

            send_calcProtocol = COMPLETED;
            all_jobs_completed = TRUE;
        }


    } // End of main loop.



    // Cleaup.
    if(pcalcProt != NULL)
        free(pcalcProt);
    close(sockfd);

    return EXIT_SUCCESS;
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
* Method for computing the assignment.
*/
void compute_assignment(calcProtocol_t * pcalcProt)
{
    switch(pcalcProt->arith) {
        case 0:
          printf("Error: calcProtocol->arith = 0.\n");
          break;
        case 1: 
          pcalcProt->inResult =  pcalcProt->inValue1 + pcalcProt->inValue2;
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

    if(pcalcProt->arith < 5)
        printf("[+] Calculation complete: %d\n", pcalcProt->inResult);
    else
        printf("[+] Calculation complete: %lf\n", pcalcProt->flResult); 
}