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
#include "newpacket.h"


int main(int argc, char **argv) {
    // ------------------------------------------------------------------------
    // Handle commandline arguments
    if (argc < 19) {
        printf("usage: sender -p <port> -g <requester port> ");
        printf("-r <rate> -q <seq_no> -l <length> -f <f_hostname> ");
        printf("-h <f_port> -i <priority> -t <timeout>\n");
        exit(1);
    }

    char *portStr    = NULL;
    char *reqPortStr = NULL;
    char *rateStr    = NULL;
    char *seqNumStr  = NULL;
    char *lenStr     = NULL;
    char *emu        = NULL;
    char *emuPortStr = NULL;
    char *priorityStr= NULL;
    char *timeoutStr = NULL;

    int cmd;
    while ((cmd = getopt(argc, argv, "p:g:r:q:l:f:h:i:t:")) != -1) {
        switch(cmd) {
            case 'p': portStr    = optarg; break;
            case 'g': reqPortStr = optarg; break;
            case 'r': rateStr    = optarg; break;
            case 'q': seqNumStr  = optarg; break;
            case 'l': lenStr     = optarg; break;
            case 'f': emu        = optarg; break;
            case 'h': emuPortStr = optarg; break;
            case 'i': priorityStr= optarg; break;
            case 't': timeoutStr = optarg; break;
            case '?':
                if (optopt == 'p' || optopt == 'g' || optopt == 'r'
                 || optopt == 'q' || optopt == 'l' || optopt == 'f'
                 || optopt == 'h' || optopt == 'i' || optopt == 't')
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
    printf("Requester Port : %s\n", reqPortStr);
    printf("Rate           : %s\n", rateStr);
    printf("Sequence #     : %s\n", seqNumStr);
    printf("Length         : %s\n", lenStr);
    printf("Emu Name       : %s\n", emu);
    printf("Emu Port       : %s\n", emuPortStr);
    printf("Priority       : %s\n", priorityStr);
    printf("Timeout        : %s\n", timeoutStr);

    // Convert program args to values
    int senderPort    = atoi(portStr);
    int requesterPort = atoi(reqPortStr);
    int sequenceNum   = 1; //atoi(seqNumStr);  // Note: Force initial seq=1 
    int payloadLen    = atoi(lenStr);
    unsigned sendRate = (unsigned) atoi(rateStr);
    int priority      = atoi(priorityStr);
    int windowSize    = 0; // Set from REQ pkt->pkt.len
    // TODO: uncomment these once they are used
    //int emuPort       = atoi(emuPortStr);
    //int timeout       = atoi(timeoutStr);

    // Validate the argument values
    if (senderPort <= 1024 || senderPort >= 65536)
        ferrorExit("Invalid sender port");
    if (requesterPort <= 1024 || requesterPort >= 65536)
        ferrorExit("Invalid requester port");
    if (sendRate > 1000 || sendRate < 1) 
        ferrorExit("Invalid sendrate");
    // TODO: validate emuPort
    // TODO: validate timeout
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
        // Try to create a new socket
        sockfd = socket(sp->ai_family, sp->ai_socktype, sp->ai_protocol);
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

    // -----------------------------===========================================
    // REQUESTER ADDRESS INFO
    struct addrinfo rhints;
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

    // ------------------------------------------------------------------------
    puts("Sender waiting for request packet...\n");

    // Receive and discard packets until a REQUEST packet arrives
    char *filename = NULL;
    for (;;) {
        void *msg = malloc(sizeof(struct new_packet));
        bzero(msg, sizeof(struct new_packet));

        // Receive a message
        size_t bytesRecvd = recvfrom(sockfd, msg, sizeof(struct new_packet), 0,
            (struct sockaddr *)rp->ai_addr, &rp->ai_addrlen);
        if (bytesRecvd == -1) {
            perror("Recvfrom error");
            fprintf(stderr, "Failed/incomplete receive: ignoring\n");
            continue;
        }

        // Deserialize the message into a packet 
        struct new_packet *pkt = malloc(sizeof(struct new_packet));
        bzero(pkt, sizeof(struct new_packet));
        deserializePacket(msg, pkt);

        // Check for REQUEST packet
        if (pkt->pkt.type == 'R') {
            // Print some statistics for the recvd packet
            printf("<- [Received REQUEST]: ");
            printPacketInfo(pkt, (struct sockaddr_storage *)rp->ai_addr);

            // Set the window size
            windowSize = pkt->pkt.len;

            // Grab a copy of the filename
            filename = strdup(pkt->pkt.payload);

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
    unsigned int packetsSent = 0;
    struct new_packet *pkt;
    for (;;) {
        // Is file part finished or a full window worth of pkts been sent?
        // TODO: handle window properly by waiting for ACKs and retransmitting
        if (feof(file) != 0 || packetsSent >= windowSize) {
            // Create END packet and send it
            pkt = malloc(sizeof(struct new_packet));
            bzero(pkt, sizeof(struct new_packet));
            pkt->priority = priority;
            pkt->src_ip   = 0; // TODO
            pkt->src_port = 0; // TODO
            pkt->dst_ip   = 0; // TODO
            pkt->dst_port = 0; // TODO
            pkt->len      = 0; // TODO
            pkt->pkt.type = 'E';
            pkt->pkt.seq  = 0;
            pkt->pkt.len  = 0;

            sendPacketTo(sockfd, pkt, (struct sockaddr *)rp->ai_addr);

            printf("** [ Sent full window of packets ] **\n");

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
        pkt = malloc(sizeof(struct new_packet));
        bzero(pkt, sizeof(struct new_packet));
        pkt->priority = priority;
        pkt->src_ip   = 0; // TODO
        pkt->src_port = 0; // TODO
        pkt->dst_ip   = 0; // TODO
        pkt->dst_port = 0; // TODO
        pkt->len      = 0; // TODO
        pkt->pkt.type = 'D';
        pkt->pkt.seq  = sequenceNum++;
        pkt->pkt.len  = payloadLen;

        // Chunk the next batch of file data into this packet
        char buf[payloadLen];
        bzero(buf, payloadLen);
        fread(buf, 1, payloadLen, file); // TODO: check return value
        memcpy(pkt->pkt.payload, buf, sizeof(buf));

        // Send the DATA packet to the requester 
        sendPacketTo(sockfd, pkt, (struct sockaddr *)rp->ai_addr);

        // Update packets sent counter for window size
        ++packetsSent;

        // Cleanup packets
        free(pkt);
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

