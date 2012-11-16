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
#include "newpacket.h"


int main(int argc, char **argv) {
    // ------------------------------------------------------------------------
    // Handle commandline arguments
    if (argc != 11) {
        printf("usage: requester -p <port> -f <f_hostname> -h <f_port> ");
        printf("-o <file option> -w <window>\n");
        exit(1);
    }

    char *portStr    = NULL;
    char *fileOption = NULL;
    char *emuHostStr = NULL;
    char *emuPortStr = NULL;
    char *windowStr  = NULL;

    int cmd;
    while ((cmd = getopt(argc, argv, "p:o:f:h:w:")) != -1) {
        switch(cmd) {
            case 'p': portStr    = optarg; break;
            case 'o': fileOption = optarg; break;
            case 'f': emuHostStr = optarg; break;
            case 'h': emuPortStr = optarg; break;
            case 'w': windowStr  = optarg; break;
            case '?':
                if (optopt == 'p' || optopt == 'o' || optopt == 'f'
                 || optopt == 'h' || optopt == 'w')
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
    printf("Emu Name: %s\n", emuHostStr);
    printf("Emu Port: %s\n", emuPortStr);
    printf("Window: %s\n", windowStr);

    // Convert program args to values
    int requesterPort = atoi(portStr);
    // TODO: uncomment these once they are used
    //int emuPort       = atoi(emuPortStr);
    //int window        = atoi(windowStr);

    // Validate the argument values
    if (requesterPort <= 1024 || requesterPort >= 65536)
        ferrorExit("Invalid requester port");
    puts("");

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
    // Emulator hints
    struct addrinfo ehints;
    bzero(&ehints, sizeof(struct addrinfo));
    ehints.ai_family   = AF_INET;
    ehints.ai_socktype = SOCK_DGRAM;
    ehints.ai_flags    = 0;
    
    FILE *file = fopen("recvd.txt", "at");
    if (file == NULL) perrorExit("File open error");
    
    struct file_part *part = fileParts->parts;
    while (part != NULL) {
        // Convert the sender's port # to a string
        /*
        char senderPortStr[6] = "\0\0\0\0\0\0";
        sprintf(senderPortStr, "%d", part->sender_port);
        */

        // Setup emulator address info
        struct addrinfo *emuinfo;
        errcode = getaddrinfo(emuHostStr, emuPortStr, &ehints, &emuinfo);
        if (errcode != 0) {
            fprintf(stderr, "emulator getaddrinfo: %s\n", gai_strerror(errcode));
            exit(EXIT_FAILURE);
        }
    
        // Loop through all the results of getaddrinfo and try to create a socket for emulator
        // NOTE: this is done so that we can find which of the getaddrinfo results is the emulator 
        int emusockfd;
        struct addrinfo *ep;
        for(ep = emuinfo; ep != NULL; ep = ep->ai_next) {
            emusockfd = socket(ep->ai_family, ep->ai_socktype, ep->ai_protocol);
            if (emusockfd == -1) {
                perror("Socket error");
                continue;
            }
    
            break;
        }
        if (ep == NULL) perrorExit("Emulator socket creation failed");
        //else            printf("Emulator socket created.\n\n");
        close(emusockfd); // don't need this socket, just created for getaddrinfo
    
        // ------------------------------------------------------------------------
    
        // Setup variables for statistics
        unsigned long numPacketsRecvd = 0;
        unsigned long numBytesRecvd = 0;
        time_t startTime = time(NULL);
    
        // ------------------------------------------------------------------------
        // Construct a REQUEST packet and send it to the emulator
        struct new_packet *pkt = NULL;
        pkt = malloc(sizeof(struct new_packet));
        bzero(pkt, sizeof(struct new_packet));
        pkt->priority = 1;
        pkt->src_ip   = 0; // TODO
        pkt->src_port = 0; // TODO
        pkt->dst_ip   = 0; // TODO
        pkt->dst_port = 0; // TODO
        pkt->len      = sizeof(struct packet) - MAX_PAYLOAD + strlen(fileOption) + 1;
        // encapsulated packet
        pkt->pkt.type = 'R';
        pkt->pkt.seq  = 0;
        pkt->pkt.len  = strlen(fileOption) + 1;
        strcpy(pkt->pkt.payload, fileOption);
    
        sendPacketTo(sockfd, pkt, (struct sockaddr *)ep->ai_addr);
    
        free(pkt);
    
        // ------------------------------------------------------------------------
        // Wait for an ACK on the request packet
        // TODO: this is temporary
        struct sockaddr_in emuAddr;
        socklen_t emuLen = sizeof(emuAddr);
        int bytesRecvd = recvfrom(sockfd, pkt, sizeof(struct new_packet), 0,
            (struct sockaddr *)&emuAddr, &emuLen);
        if (bytesRecvd != -1) {
            printf("<- [Received ACK]: ");
            printPacketInfo(pkt, (struct sockaddr_storage *)&emuAddr);
        } else {
            perrorExit("Recv failed.\n");
        }

        // Create the file to write data to
        /*
        if (access(fileOption, F_OK) != -1) // if it already exists
            remove(fileOption);             // delete it
        */

        // ------------------------------------------------------------------------
        // Connect to emulator to receive all parts of requested file

        // Start a recv loop here to get all packets for the given part
        for (;;) {
            // Receive a message 
            struct new_packet msg;
            bzero(&msg, sizeof(struct new_packet));
            size_t bytesRecvd = recvfrom(sockfd, &msg, sizeof(struct new_packet), 0,
                (struct sockaddr *)ep->ai_addr, &ep->ai_addrlen);
            if (bytesRecvd == -1) perrorExit("Receive error");
    
            // Deserialize the message into a packet
            struct new_packet p;
            bzero(&p, sizeof(struct new_packet));
            deserializePacket(&msg, &p);
    
            // Handle DATA packet
            if (p.pkt.type == 'D') {
                // Update statistics
                ++numPacketsRecvd;
                numBytesRecvd += p.len;

                /* FOR DEBUG
                printf("[Packet Details]\n------------------\n");
                printf("type : %c\n", p.type);
                printf("seq  : %lu\n", p.seq);
                printf("len  : %lu\n", p.len);
                printf("payload: %s\n\n", p.payload);
                */
    
                // Print details about the received packet
                printf("<- [Received DATA packet] ");
                printPacketInfo(&p, (struct sockaddr_storage *)ep->ai_addr);
    
                // Save the data so the file can be reassembled later
                size_t bytesWritten = fprintf(file, "%s", p.pkt.payload);
                fflush(file);
                if (bytesWritten == -1) {
                    fprintf(stderr,
                        "Incomplete file write: %d bytes written, %lu p len",
                        (int)bytesWritten, p.pkt.len);
                }
            }
    
            // Handle END packet
            if (p.pkt.type == 'E') {
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

        freeaddrinfo(emuinfo);
    }

    // TODO: this crashes the program... figure out why
    //fclose(file);

    // Got what we came for, shut it down
    if (close(sockfd) == -1) perrorExit("Close error");
    else                     puts("Connection closed.\n");

    // Cleanup address and file info data 
    freeaddrinfo(requesterinfo);
    freeFileInfo(fileParts);

    // All done!
    exit(EXIT_SUCCESS);
}

