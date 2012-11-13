#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <strings.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "utilities.h"
#include "tracker.h"
#include "packet.h"


int main(int argc, char **argv) {
    // ------------------------------------------------------------------------
    // Handle commandline arguments
    if (argc != 5) {
        printf("usage: requester -p <port> -o <file option>\n");
        exit(1);
    }

    char *portStr    = NULL;
    char *fileOption = NULL;

    int cmd;
    while ((cmd = getopt(argc, argv, "p:o:")) != -1) {
        switch(cmd) {
            case 'p': portStr    = optarg; break;
            case 'o': fileOption = optarg; break;
            case '?':
                if (optopt == 'p' || optopt == 'o')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option -%c.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
                exit(EXIT_FAILURE);
            break;
            default: 
                printf("Unhandled argument: %d\n", cmd);
                exit(EXIT_FAILURE); 
        }
    }

    // DEBUG
    printf("Port: %s\n", portStr);
    printf("File: %s\n", fileOption);

    // Convert program args to values
    int requesterPort = atoi(portStr);

    // Validate the argument values
    if (requesterPort <= 1024 || requesterPort >= 65536)
        ferrorExit("Invalid requester port");

    // ------------------------------------------------------------------------

    // Parse the tracker file for parts corresponding to the specified file
    struct file_info *fileParts = parseTracker(fileOption);
    assert(fileParts != NULL && "Invalid file_info struct");

    // ------------------------------------------------------------------------
    // Setup requester address info 
    struct addrinfo rhints;
    bzero(&rhints, sizeof(struct addrinfo));
    rhints.ai_family   = AF_INET;
    rhints.ai_socktype = SOCK_DGRAM;
    rhints.ai_flags    = AI_PASSIVE;

    // Get the requester's address info
    struct addrinfo *requesterinfo;
    int errcode = getaddrinfo(NULL, portStr, &rhints, &requesterinfo);
    if (errcode != 0) {
        fprintf(stderr, "requester getaddrinfo: %s\n", gai_strerror(errcode));
        exit(EXIT_FAILURE);
    }

    // Loop through all the results of getaddrinfo and try to create a socket for requester
    int sockfd;
    struct addrinfo *rp;
    for(rp = requesterinfo; rp != NULL; rp = rp->ai_next) {
        // Try to create a new socket
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) {
            perror("Socket error");
            continue;
        }

        // Try to bind the socket
        if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == -1) {
            perror("Bind error");
            close(sockfd);
            continue;
        }

        break;
    }
    if (rp == NULL) perrorExit("Request socket creation failed");
    else            { printf("Requester socket: "); printNameInfo(rp); }

    // ------------------------------------------------------------------------
    // Sender hints TODO: move into part loop
    struct addrinfo shints;
    bzero(&shints, sizeof(struct addrinfo));
    shints.ai_family   = AF_INET;
    shints.ai_socktype = SOCK_DGRAM;
    shints.ai_flags    = 0;
    
    FILE *file = fopen("recvd.txt", "at");
    if (file == NULL) perrorExit("File open error");
    
    struct file_part *part = fileParts->parts;
    while (part != NULL) {
        // Convert the sender's port # to a string
        char senderPortStr[6] = "\0\0\0\0\0\0";
        sprintf(senderPortStr, "%d", part->sender_port);
    
        // Get the sender's address info
        struct addrinfo *senderinfo;
        errcode = getaddrinfo(part->sender_hostname, senderPortStr, &shints, &senderinfo);
        if (errcode != 0) {
            fprintf(stderr, "sender getaddrinfo: %s\n", gai_strerror(errcode));
            exit(EXIT_FAILURE);
        }
    
        // Loop through all the results of getaddrinfo and try to create a socket for sender
        // NOTE: this is done so that we can find which of the getaddrinfo results is the sender
        int sendsockfd;
        struct addrinfo *sp;
        for(sp = senderinfo; sp != NULL; sp = sp->ai_next) {
            sendsockfd = socket(sp->ai_family, sp->ai_socktype, sp->ai_protocol);
            if (sendsockfd == -1) {
                perror("Socket error");
                continue;
            }
    
            break;
        }
        if (sp == NULL) perrorExit("Send socket creation failed");
        //else            printf("Sender socket created.\n\n");
        close(sendsockfd); // don't need this socket
    
        // ------------------------------------------------------------------------
    
        // Setup variables for statistics
        unsigned long numPacketsRecvd = 0;
        unsigned long numBytesRecvd = 0;
        time_t startTime = time(NULL);
    
        // ------------------------------------------------------------------------
        // Construct a REQUEST packet
        struct packet *pkt = NULL;
        pkt = malloc(sizeof(struct packet));
        bzero(pkt, sizeof(struct packet));
        pkt->type = 'R';
        pkt->seq  = 0;
        pkt->len  = strlen(fileOption) + 1;
        strcpy(pkt->payload, fileOption);
    
        sendPacketTo(sockfd, pkt, (struct sockaddr *)sp->ai_addr);
    
        free(pkt);
    
        // Create the file to write data to
        if (access(fileOption, F_OK) != -1) // if it already exists
            remove(fileOption);             // delete it

        // ------------------------------------------------------------------------
        // Connect to senders one at a time to get all parts
    
        // TODO: sometimes the requester stops receiving packets 
        //       even though the sender sends them properly... 
        //       requester sits blocked in recvfrom below, but doesn't recv 
        // NOTE: this isn't happening anymore with the rate limit betw 1-1000 pkt/sec
    
        struct sockaddr_storage senderAddr;
        bzero(&senderAddr, sizeof(struct sockaddr_storage));
        socklen_t len = sizeof(senderAddr);
    
        // Start a recv loop here to get all packets for the given part
        for (;;) {
            void *msg = malloc(sizeof(struct packet));
            bzero(msg, sizeof(struct packet));
    
            // Receive a message 
            size_t bytesRecvd = recvfrom(sockfd, msg, sizeof(struct packet), 0,
                (struct sockaddr *)&senderAddr, &len);
            if (bytesRecvd == -1) perrorExit("Receive error");
    
            // Deserialize the message into a packet
            pkt = malloc(sizeof(struct packet));
            bzero(pkt, sizeof(struct packet));
            deserializePacket(msg, pkt);
    
            // Handle DATA packet
            if (pkt->type == 'D') {
                // Update statistics
                ++numPacketsRecvd;
                numBytesRecvd += pkt->len;

                /* FOR DEBUG
                printf("[Packet Details]\n------------------\n");
                printf("type : %c\n", pkt->type);
                printf("seq  : %lu\n", pkt->seq);
                printf("len  : %lu\n", pkt->len);
                printf("payload: %s\n\n", pkt->payload);
                */
    
                // Print details about the received packet
                printf("<- [Received DATA packet] ");
                printPacketInfo(pkt, (struct sockaddr_storage *)&senderAddr);
    
                // Save the data so the file can be reassembled later
                size_t bytesWritten = fprintf(file, "%s", pkt->payload);
                if (bytesWritten != pkt->len) {
                    fprintf(stderr,
                        "Incomplete file write: %d bytes written, %lu pkt len",
                        (int)bytesWritten, pkt->len);
                } else {
                    fflush(file);
                }
            }
    
            // Handle END packet
            if (pkt->type == 'E') {
                printf("<- *** [Received END packet] ***");
                double dt = difftime(time(NULL), startTime);
                if (dt <= 1) dt = 1;
    
                // Print statistics
                printf("\n---------------------------------------\n");
                printf("Total packets recvd: %lu\n", numPacketsRecvd);
                printf("Total payload bytes recvd: %lu\n", numBytesRecvd);
                printf("Average packets/second: %d\n", (int)(numPacketsRecvd / dt));
                printf("Duration of test: %f sec\n\n", dt);
    
                break;
            }
    
        }
        part = part->next_part;
        free(pkt);
        freeaddrinfo(senderinfo);
    }

    fclose(file);

    // Got what we came for, shut it down
    if (close(sockfd) == -1) perrorExit("Close error");
    else                        puts("Connection closed.\n");

    // Cleanup address and file info data 
    freeaddrinfo(requesterinfo);
    freeFileInfo(fileParts);

    // All done!
    exit(EXIT_SUCCESS);
}

