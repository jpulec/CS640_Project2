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
#include "newpacket.h"
#include "table.h"


struct packet_queue {
	struct new_packet *pkt;
	unsigned short retransmissions;
	struct packet_queue *next;
	struct packet_queue *prev;
};

struct forward_entry *head;

struct packet_queue queue1, queue2, queue3;
unsigned int q1num = 0, q2num = 0, q3num = 0;
unsigned int MAX_QUEUE = 0;

void enqueuePkt(struct new_packet *pkt);
struct new_packet *dequeuePkt(struct packet_queue *q);


FILE *logFile = NULL;
void logOut(const char *reason, unsigned long long timestamp, struct new_packet *pkt);


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


	// TODO: this is temporary, need to get sender address info from fwd table
	// Setup sender address info 
	struct addrinfo ehints;
	bzero(&ehints, sizeof(struct addrinfo));
	ehints.ai_family   = AF_INET;
	ehints.ai_socktype = SOCK_DGRAM;
	ehints.ai_flags    = 0;

	// Get the sender's address info
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
		// Try to create a new socket
		sockfd = socket(ep->ai_family, ep->ai_socktype, ep->ai_protocol);
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
	else            close(sockfd);

	//-------------------------------------------------------------------------
	// BEGIN NETWORK EMULATION LOOP
	puts("Emulator waiting for packets...\n");

	struct new_packet *delayedPkt = NULL;
	struct sockaddr_in emuAddr, nextAddr;
	socklen_t emuLen = sizeof(emuAddr);
	//socklen_t sendLen = sizeof(sendAddr);

	head = parseTable(filename, getenv("HOSTNAME"), port);

	unsigned long long prevMS = getTimeMS();

	while (1) {
		void *msg = malloc(sizeof(struct new_packet));
		bzero(msg, sizeof(struct new_packet));

		size_t bytesRecvd = recvfrom(sockfd, msg, sizeof(struct new_packet), 0,
				(struct sockaddr *)&emuAddr, &emuLen);

		struct forward_entry *curEntry;
		if (bytesRecvd != -1) {
			printf("Received %d bytes\n", (int)bytesRecvd);

			// Deserialize the message into a packet 
			struct new_packet *pkt = malloc(sizeof(struct new_packet));
			bzero(pkt, sizeof(struct new_packet));
			deserializeNewPacket(msg, pkt);


			//TODO: Consult forwarding table to see if packet is to be
			//forwarded, then enqueue it
			//
			// for now, just forward it directly to sender...
			//sendPacketTo(sockfd, pkt, (struct sockaddr *)sp->ai_addr);


			curEntry = head;
			int curEntryIP = 0;

			struct addrinfo entryHints;
			bzero(&entryHints, sizeof(struct addrinfo));
			entryHints.ai_family   = AF_INET;
			entryHints.ai_socktype = SOCK_DGRAM;
			entryHints.ai_flags    = 0;

			struct addrinfo *entryInfo;
			errcode = getaddrinfo(curEntry->dst_hostname, NULL, &entryHints, &entryInfo);
			if( errcode != 0 ){
				fprintf(stderr, "cannot resolve hostname:%s", curEntry->dst_hostname);
				fprintf(logFile, "PACKET DROPPED\nDST_HOSTNAME:%s", curEntry->dst_hostname);
				free(pkt);
				continue;
			}
			struct addrinfo *ep;
			while(ep == NULL){
				ep = ep->ai_next;
			}
			if( ep == NULL){
				fprintf(stderr, "cannot resolve hostname:%s", curEntry->dst_hostname);
				fprintf(logFile, "PACKET DROPPED\nDST_HOSTNAME:%s", curEntry->dst_hostname);
			}
			else 		{ printf("Emulator socket: "); printNameInfo(ep); }

			while ( curEntryIP != pkt->dst_ip || curEntry->dst_port != pkt->dst_port){
				curEntry = curEntry->next;
			}
			if(curEntry == NULL){
				printf("Error: no forwarding info for destination:%lu on port:%hu", pkt->dst_ip, pkt->dst_port);
				fprintf(logFile, "PACKET DROPPED\nDST_HOSTNAME:%s", curEntry->dst_hostname);
				free(pkt);
				continue;
			}

			// By this point, pkt must be forwarded
			free(pkt);
			enqueuePkt(pkt);
		}
		else if(delayedPkt != NULL){
			// If pkt delay is greater than elapsed time
			if( prevMS - getTimeMS() > curEntry->delay){
				continue;
			}
			else{
				// Determine whether or not to drop pkt
				int r = ( 100 * rand() / ( RAND_MAX + 1.0));
				if(r <= curEntry->loss){
					fprintf(logFile, "PACKET DROPPED\nDST_HOSTNAME:%s", curEntry->dst_hostname);
				}
				else{
					sendNewPacketTo(sockfd, delayedPkt, (struct sockaddr*)&nextAddr);
				}
			}	
		}
		else{
			if ((delayedPkt = dequeuePkt(&queue1)) != NULL){
				prevMS = getTimeMS();
			}
			else if((delayedPkt = dequeuePkt(&queue2)) != NULL){
				prevMS = getTimeMS();
			}
			else if((delayedPkt = dequeuePkt(&queue3)) != NULL){
				prevMS = getTimeMS();
			}
		}

		free(msg);
	/*
		// If packet is being delayed, and delay is not expired,
		// continue loop
		else if ((prevMS - getTimeMS()) > 0){
		// Subtract current time from 
		} else {

		}
		*/
	}	


	/*// Close the logger
	  fclose(logFile);

	// Got what we came for, shut it down
	if (close(sockfd) == -1) perrorExit("Close error");
	else                     puts("Connection closed.\n");

	// Cleanup address info data
	freeaddrinfo(emuinfo);

	// All done!
	exit(EXIT_SUCCESS);
	*/

}
void enqueuePkt(struct new_packet *pkt) {
	// Validate the packet
	if (pkt == NULL) {
		logOut("Unable to enqueue NULL pkt", getTimeMS(), NULL);
		return;
	}

	// Pick the appropriate queue
	struct packet_queue *q = NULL, *queue = NULL;
	switch (pkt->priority) { // TODO: implement packet priority: pkt->priority) {
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
	printf("Enqueued pkt: seq = %lu\n", pkt->pkt.seq);
}

struct new_packet *dequeuePkt(struct packet_queue *q) {
	// TODO
	return NULL;
}

void logOut(const char *msg, unsigned long long timestamp, struct new_packet *pkt) {
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

