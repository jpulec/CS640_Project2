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


struct packet_queue {
    struct packet *pkt;
    unsigned short retransmissions;
    struct packet_queue *next;
    struct packet_queue *prev;
};
struct packet_queue queue1, queue2, queue3;
unsigned int q1num = 0, q2num = 0, q3num = 0;
unsigned int MAX_QUEUE = 0;

void enqueuePkt(struct packet *pkt);
struct packet *dequeuePkt(struct packet_queue *q);


FILE *logFile = NULL;
void logOut(const char *reason, unsigned long long timestamp, struct packet *pkt);


int main(int argc, char **argv) {
    // ------------------------------------------------------------------------
    // Handle commandline arguments
    if (argc < 9) {
        printf("usage: emulator -p <port> -q <queue_size> ");
        printf("-f <filename> -l <log>\n");
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
    if (queueSize < 1)
        ferrorExit("Invalid queue size");
    puts("");

    MAX_QUEUE = queueSize;

    // ------------------------------------------------------------------------
    // Initialize logger
    logFile = fopen(log, "wt");
    if (logFile == NULL) perrorExit("File open error");
    else                 printf("Opened file \"%s\" for logging.\n", log);
    logOut("[LOG INITIALIZED]", getTimeMS(), NULL);

    // ------------------------------------------------------------------------
    // Setup emulator address info 
    struct addrinfo ehints;
    bzero(&ehints, sizeof(struct addrinfo));
    ehints.ai_family   = AF_INET;
    ehints.ai_socktype = SOCK_DGRAM;
    ehints.ai_flags    = AI_PASSIVE;

    // Get the emulator's address info
    struct addrinfo *emuinfo;
    int errcode = getaddrinfo(NULL, portStr, &ehints, &emuinfo);
    if (errcode != 0) {
        fprintf(stderr, "emulator getaddrinfo: %s\n", gai_strerror(errcode));
        exit(EXIT_FAILURE);
    }

    // Loop through all the results of getaddrinfo and try to create a socket for sender
    int sockfd;
    struct addrinfo *ep;
    for(ep = emuinfo; ep != NULL; ep = ep->ai_next) {
        // Try to create a new socket and DON'T block
        sockfd = socket(ep->ai_family, ep->ai_socktype /*| SOCK_NONBLOCK*/, ep->ai_protocol);
        if (sockfd == -1) {
            perror("Socket error");
            continue;
        }

        // Try to bind the socket
        if (bind(sockfd, ep->ai_addr, ep->ai_addrlen) == -1) {
            perror("Bind error");
            close(sockfd);
            continue;
        }

        break;
    }
    if (ep == NULL) perrorExit("Emulator socket creation failed");
    else            printf("Emulator socket created.\n");


    // TODO: this is temporary, need to get sender address info from fwd table
    // Setup sender address info 
    struct addrinfo shints;
    bzero(&shints, sizeof(struct addrinfo));
    shints.ai_family   = AF_INET;
    shints.ai_socktype = SOCK_DGRAM;
    shints.ai_flags    = 0;

    // Get the sender's address info
    struct addrinfo *senderinfo;
    char *senderPort = "2000\0";
    errcode = getaddrinfo("mumble-30", senderPort, &shints, &senderinfo);
    if (errcode != 0) {
        fprintf(stderr, "sender getaddrinfo: %s\n", gai_strerror(errcode));
        exit(EXIT_FAILURE);
    }

    // Loop through all the results of getaddrinfo and try to create a socket for sender
    int sendsockfd;
    struct addrinfo *sp;
    for(sp = senderinfo; sp != NULL; sp = sp->ai_next) {
        // Try to create a new socket
        sendsockfd = socket(sp->ai_family, sp->ai_socktype, sp->ai_protocol);
        if (sendsockfd == -1) {
            perror("Socket error");
            continue;
        }

        // Try to bind the socket
        if (bind(sendsockfd, sp->ai_addr, sp->ai_addrlen) == -1) {
            perror("Bind error");
            close(sendsockfd);
            continue;
        }

        break;
    }
    if (sp == NULL) perrorExit("Sender socket creation failed");
    else            close(sendsockfd);

    //-------------------------------------------------------------------------
    // BEGIN NETWORK EMULATION LOOP
    puts("Emulator waiting for request packet...\n");

    struct new_packet *curPkt = NULL;
    struct sockaddr_in reqAddr, sendAddr;
    socklen_t reqLen = sizeof(reqAddr);
    socklen_t sendLen = sizeof(sendAddr);

    int delay = 0;
    int hasRequestPacket = 0;

    unsigned long long prevMS = getTimeMS();
    unsigned long long sendRate = 1;

    while (!hasRequestPacket) {
        void *msg = malloc(sizeof(struct packet));
        bzero(msg, sizeof(struct packet));

        size_t bytesRecvd = recvfrom(sockfd, msg, sizeof(struct packet), 0,
            (struct sockaddr *)&reqAddr, &reqLen);
        if (bytesRecvd != -1) {
            printf("Received %d bytes\n", (int)bytesRecvd);
            
            // Deserialize the message into a packet 
            struct packet *pkt = malloc(sizeof(struct packet));
            bzero(pkt, sizeof(struct packet));
            deserializePacket(msg, pkt);

            if (pkt->type == 'R') {
                hasRequestPacket = 1;

                // Print some statistics for the recvd packet
                printf("<- [Received REQUEST]: ");
                printPacketInfo(pkt, (struct sockaddr_storage *)&reqAddr);
            }

            // Send ACK packet back
            struct packet *ack = malloc(sizeof(struct packet));
            bzero(ack, sizeof(struct packet));
            ack->type = 'A';
            ack->seq  = 0;
            ack->len  = pkt->len;
            //strcpy(ack->payload, fileOption);
        
            sendPacketTo(sockfd, ack, (struct sockaddr *)&reqAddr);
        
            // Cleanup packets
            free(ack);

            //TODO: Consult forwarding table to see if packet is to be
            //forwarded, then enqueue it
            //
            // for now, just forward it directly to sender...
            sendPacketTo(sockfd, pkt, (struct sockaddr *)sp->ai_addr);

            free (msg);

            int recvdEndPacket = 0;
            while (!recvdEndPacket) {
                msg = malloc(sizeof(struct packet));
                bzero(msg, sizeof(struct packet));

                // Wait for a response 
                size_t recvd = recvfrom(sockfd, msg, sizeof(struct packet), 0,
                    (struct sockaddr *)sp->ai_addr, &sp->ai_addrlen);
                if (recvd != -1) {
                    //printf("Received %d bytes\n", (int)recvd);
                    
                    // Deserialize the message into a packet 
                    free (pkt);
                    pkt = malloc(sizeof(struct packet));
                    bzero(pkt, sizeof(struct packet));
                    deserializePacket(msg, pkt);

                    if (pkt->type == 'D') {
                        printf("<- [Received DATA]: ");
                        printPacketInfo(pkt, (struct sockaddr_storage *)&sendAddr);

                        // TODO: put pkt in queue and then forward according to routing table
                        enqueuePkt(pkt);

                        printf("---> [Forwarding] :\n  ");
                        sendPacketTo(sockfd, pkt, (struct sockaddr *)&reqAddr);
                    } else if (pkt->type == 'E') {
                        printf("<- [Received END]: ");
                        printPacketInfo(pkt, (struct sockaddr_storage *)&sendAddr);

                        // TODO: put pkt in queue and then forward according to routing table
                        enqueuePkt(pkt);

                        printf("---> [Forwarding] : ");
                        sendPacketTo(sockfd, pkt, (struct sockaddr *)&reqAddr);

                        recvdEndPacket = 1;
                    }
                } else {
                    printf("** Failed to receive response from sender **\n");
                }
            }

            free(pkt);
        }
        /*
        // If packet is being delayed, and delay is not expired,
        // continue loop
        else if ((prevMS - getTimeMS()) > 0){
            // Subtract current time from 
        } else {

        }
        */

        free(msg);
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
    /*
    puts("Sender waiting for request packet...\n");

    // Receive and discard packets until a REQUEST packet arrives
    //char *filename = NULL;
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
        * /
        free(msg);
    }
    */

    // ------------------------------------------------------------------------
    // Got REQUEST packet, start sending DATA packets
    // ------------------------------------------------------------------------
    /*
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

            //sendPacketTo(sockfd, pkt, (struct sockaddr *)rp->ai_addr);

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
        * /

        // TODO 
        unsigned long sequenceNum = 1;
        unsigned long payloadLen  = 32;

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

        printf("[Packet Details]\n------------------\n");
        printf("type : %c\n", pkt->type);
        printf("seq  : %lu\n", pkt->seq);
        printf("len  : %lu\n", pkt->len);
        printf("payload: %s\n\n", pkt->payload);
        * /

        // Send the DATA packet to the requester 
        //sendPacketTo(sockfd, pkt, (struct sockaddr *)rp->ai_addr);

        // Cleanup packets
        free(pkt);

        // Update sequence number for next packet
        sequenceNum += payloadLen;
    }
    */

    // Cleanup the file
    /*
    if (fclose(file) != 0) fprintf(stderr, "Failed to close file \"%s\"\n", filename);
    else                   printf("File \"%s\" closed.\n", filename);
    free(filename);
    */

    // Close the logger
    fclose(logFile);

    // Got what we came for, shut it down
    if (close(sockfd) == -1) perrorExit("Close error");
    else                     puts("Connection closed.\n");

    // Cleanup address info data
    freeaddrinfo(emuinfo);

    // All done!
    exit(EXIT_SUCCESS);
}


void enqueuePkt(struct packet *pkt) {
    // Validate the packet
    if (pkt == NULL) {
        logOut("Unable to enqueue NULL pkt", getTimeMS(), NULL);
        return;
    }

    // Pick the appropriate queue
    struct packet_queue *q = NULL, *queue = NULL;
    switch (1) { // TODO: implement packet priority: pkt->priority) {
        case 1: q = &queue1; break;
        case 2: q = &queue2; break;
        case 3: q = &queue3; break;
        default:
            logOut("Packet has invalid priority value", getTimeMS(), pkt);
            return;
    };

    // Hack so that we can increment the right queue number after adding a pkt
    queue = q;

    // Check if the queue is already full
    if (q == &queue1 && q1num >= MAX_QUEUE - 1) {
        logOut("Priority queue 1 was full", getTimeMS(), pkt);
        return;
    } else if (q == &queue2 && q2num >= MAX_QUEUE - 1) {
        logOut("Priority queue 2 was full", getTimeMS(), pkt);
        return;
    } else if (q == &queue3 && q3num >= MAX_QUEUE - 1) {
        logOut("Priority queue 3 was full", getTimeMS(), pkt);
        return;
    }

    // Move to end of queue
    while (q->next != NULL) {
        q = q->next;
    }

    // Add the packet to the end of the queue
    q->next = malloc(sizeof(struct packet_queue));
    q->next->pkt  = pkt;
    q->next->retransmissions = 0;
    q->next->next = NULL;
    q->next->prev = q;

    // Update the number of enqueued packets
    if      (queue == &queue1) ++q1num;
    else if (queue == &queue2) ++q2num;
    else if (queue == &queue3) ++q3num;

    // DEBUG
    printf("Enqueued pkt: seq = %lu\n", pkt->seq);
}

struct packet *dequeuePkt(struct packet_queue *q) {
    // TODO
    return NULL;
}

void logOut(const char *msg, unsigned long long timestamp, struct packet *pkt) {
    fprintf(logFile, "%s : ", msg);
    if (pkt != NULL) {
        fprintf(logFile, "source: %s:%s, dest: %s:%s, time: %llu, priority: %d, payld len: %lu\n",
                "SRC_HOST", "SRC_PORT", // TODO: get from pkt
                "DST_HOST", "DST_PORT", // TODO: get from pkt
                timestamp,
                1,         // TODO: pkt->priority,
                pkt->len); // TODO: pkt payload length
    } else {
        fprintf(logFile, "[]\n");
    }
}

