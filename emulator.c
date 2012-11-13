#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <strings.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "utilities.h"
#include "packet.h"


int main(int argc, char **argv) {
    // ------------------------------------------------------------------------
    // Handle commandline arguments
    if (argc < 9) {
        printf("usage: emulator -p <port> -q <queue_size> ");
        printf("-f <filename> -f <log>\n");
        exit(1);
    }

    char *portStr     = NULL;
    char *queueSizeStr= NULL;
    char *filename    = NULL;
    char *log         = NULL;

    int cmd;
    while ((cmd = getopt(argc, argv, "p:q:f:l:")) != -1) {
        switch(cmd) {
            case 'p': portStr      = optarg; break;
            case 'q': queueSizeStr = optarg; break;
            case 'f': filename     = optarg; break;
            case 'l': log          = optarg; break;
            case '?':
                if (optopt == 'p' || optopt == 'f' || optopt == 'q' || optopt == 'l')
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

    printf("Port           : %s\n", portStr);
    printf("Queue Size     : %s\n", queueSizeStr);
    printf("Filename       : %s\n", filename);
    printf("Log File       : %s\n", log);

    // Convert program args to values
    int port         = atoi(portStr);
    int queueSize    = atoi(queueSizeStr);

    // Validate the argument values
    if (port <= 1024 || port >= 65536)
        ferrorExit("Invalid port");
    puts("");

    // ------------------------------------------------------------------------
    // Setup sender address info 
    struct addrinfo shints;
    bzero(&shints, sizeof(struct addrinfo));
    shints.ai_family   = AF_INET;
    shints.ai_socktype = SOCK_DGRAM;
    shints.ai_flags    = AI_PASSIVE;

    // Get the sender's address info
    struct addrinfo *senderinfo;
    int errcode = getaddrinfo(NULL, portStr, &shints, &senderinfo);
    if (errcode != 0) {
        fprintf(stderr, "sender getaddrinfo: %s\n", gai_strerror(errcode));
        exit(EXIT_FAILURE);
    }

    // Loop through all the results of getaddrinfo and try to create a socket for sender
    int sockfd;
    struct addrinfo *sp;
    for(sp = senderinfo; sp != NULL; sp = sp->ai_next) {
        // Try to create a new socket and DON'T block
        sockfd = socket(sp->ai_family, sp->ai_socktypei | SOCK_NONBLOCK, sp->ai_protocol);
        if (sockfd == -1) {
            perror("Socket error");
            continue;
        }

        // Try to bind the socket
        if (bind(sockfd, sp->ai_addr, sp->ai_addrlen) == -1) {
            perror("Bind error");
            close(sockfd);
            continue;
        }

        break;
    }
    if (sp == NULL) perrorExit("Send socket creation failed");
    else            printf("Sender socket created.\n");

    //-------------------------------------------------------------------------
    // BEGIN NETWORK EMULATION LOOP
    struct new_packet *curPkt = NULL;
    struct addrinfo *rp;
    int delay = 0;
    unsigned long long prevMS;
    while(1){
        size_t bytesRecvd = recvfrom(sockfd, msg, sizeof(struct packet), 0,
            (struct sockaddr *)rp->ai_addr, &rp->ai_addrlen);
        
        if (bytesRecvd != -1) {
            //TODO: Consult forwarding table to see if packet is to be
            //forwarded, then enqueue it
        }
        // If packet is being delayed, and delay is not expired,
        // continue loop
        else if ((prevMS - getTimeMS()) > 0){
            // Subtract current time from 
        }
        else{

        }

    }

    
    // -----------------------------===========================================
    // REQUESTER ADDRESS INFO
    /*struct addrinfo rhints;
    bzero(&rhints, sizeof(struct addrinfo));
    rhints.ai_family   = AF_INET;
    rhints.ai_socktype = SOCK_DGRAM;
    rhints.ai_flags    = 0;

    struct addrinfo *requesterinfo;
    errcode = getaddrinfo(NULL, reqPortStr, &rhints, &requesterinfo);
    if (errcode != 0) {
        fprintf(stderr, "requester getaddrinfo: %s\n", gai_strerror(errcode));
        exit(EXIT_FAILURE);
    }

    // Loop through all the results of getaddrinfo and try to create a socket for requester
    // NOTE: this is done so that we can find which of the getaddrinfo results is the requester
    int requestsockfd;
    struct addrinfo *rp;
    for(rp = requesterinfo; rp != NULL; rp = rp->ai_next) {
        requestsockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (requestsockfd == -1) {
            perror("Socket error");
            continue;
        }

        break;
    }
    if (sp == NULL) perrorExit("Requester lookup failed to create socket");
    //else            printf("Requester socket created.\n\n");
    close(requestsockfd); // don't need this socket
*/
    // ------------------------------------------------------------------------
    puts("Sender waiting for request packet...\n");

    // Receive and discard packets until a REQUEST packet arrives
    char *filename = NULL;
    for (;;) {
        void *msg = malloc(sizeof(struct packet));
        bzero(msg, sizeof(struct packet));

        // Receive a message
        size_t bytesRecvd = recvfrom(sockfd, msg, sizeof(struct packet), 0,
            (struct sockaddr *)rp->ai_addr, &rp->ai_addrlen);
        if (bytesRecvd == -1) {
            perror("Recvfrom error");
            fprintf(stderr, "Failed/incomplete receive: ignoring\n");
            continue;
        }

        // Deserialize the message into a packet 
        struct packet *pkt = malloc(sizeof(struct packet));
        bzero(pkt, sizeof(struct packet));
        deserializePacket(msg, pkt);

        // Check for REQUEST packet
        if (pkt->type == 'R') {
            // Print some statistics for the recvd packet
            printf("<- [Received REQUEST]: ");
            printPacketInfo(pkt, (struct sockaddr_storage *)rp->ai_addr);

            // Grab a copy of the filename
            filename = strdup(pkt->payload);

            // Cleanup packets
            free(pkt);
            free(msg);
            break;
        }

        // Cleanup packets
        free(pkt);
        free(msg);
    }

    // ------------------------------------------------------------------------
    // Got REQUEST packet, start sending DATA packets
    // ------------------------------------------------------------------------

    // Open file for reading
    FILE *file = fopen(filename, "r");
    if (file == NULL) perrorExit("File open error");
    else              printf("Opened file \"%s\" for reading.\n", filename);

    unsigned long long start = getTimeMS();
    struct packet *pkt;
    for (;;) {
        // Is file part finished?
        if (feof(file) != 0) {
            // Create END packet and send it
            pkt = malloc(sizeof(struct packet));
            bzero(pkt, sizeof(struct packet));
            pkt->type = 'E';
            pkt->seq  = 0;
            pkt->len  = 0;

            sendPacketTo(sockfd, pkt, (struct sockaddr *)rp->ai_addr);

            free(pkt);
            break;
        }

        // Send rate limiter
        unsigned long long dt = getTimeMS() - start;
        if (dt < 1000 / sendRate) {
            continue; 
        } else {
            start = getTimeMS();
        }

        // Create DATA packet
        pkt = malloc(sizeof(struct packet));
        bzero(pkt, sizeof(struct packet));
        pkt->type = 'D';
        pkt->seq  = sequenceNum;
        pkt->len  = payloadLen;

        // Chunk the next batch of file data into this packet
        char buf[payloadLen];
        bzero(buf, payloadLen);
        fread(buf, 1, payloadLen, file); // TODO: check return value
        memcpy(pkt->payload, buf, sizeof(buf));

        /*
        printf("[Packet Details]\n------------------\n");
        printf("type : %c\n", pkt->type);
        printf("seq  : %lu\n", pkt->seq);
        printf("len  : %lu\n", pkt->len);
        printf("payload: %s\n\n", pkt->payload);
        */

        // Send the DATA packet to the requester 
        sendPacketTo(sockfd, pkt, (struct sockaddr *)rp->ai_addr);

        // Cleanup packets
        free(pkt);

        // Update sequence number for next packet
        sequenceNum += payloadLen;
    }

    // Cleanup the file
    if (fclose(file) != 0) fprintf(stderr, "Failed to close file \"%s\"\n", filename);
    else                   printf("File \"%s\" closed.\n", filename);
    free(filename);


    // Got what we came for, shut it down
    if (close(sockfd) == -1) perrorExit("Close error");
    else                     puts("Connection closed.\n");

    // Cleanup address info data
    freeaddrinfo(senderinfo);

    // All done!
    exit(EXIT_SUCCESS);
}

