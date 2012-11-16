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
#include <sys/utsname.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "utilities.h"
#include "newpacket.h"
#include "table.h"


struct packet_node {
	struct new_packet *pkt;
	struct packet_node *next;
};

struct forward_entry *head;

struct packet_node *queue1h, *queue2h, *queue3h,
		   *queue1t, *queue2t, *queue3t;


unsigned int q1num = 0, q2num = 0, q3num = 0;
unsigned int MAX_QUEUE = 0;

void enqueuePkt(struct new_packet *pkt);
struct new_packet *dequeuePkt(struct packet_node *q);


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

	queue1h = malloc(sizeof(struct packet_node));
	queue2h = malloc(sizeof(struct packet_node));
	queue3h = malloc(sizeof(struct packet_node));

	queue1t = queue1h;
	queue2t = queue2h;
	queue3t = queue3h;


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

	// Setup emu sending socket
	struct sockaddr_in emuaddr;

	int sockfd;

	if( (sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0)) == -1){
		perrorExit("Socket error");
	}

	emuaddr.sin_family = AF_INET;
	emuaddr.sin_port = htons(port);
	emuaddr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(emuaddr.sin_zero), 8);

	
	if (bind(sockfd, (struct sockaddr*)&emuaddr, sizeof(emuaddr)) == -1) {
		close(sockfd);
		perrorExit("Bind error");
	}

	printf("Emulator socket created on port:%d\n", port );
	
	//-------------------------------------------------------------------------
	// BEGIN NETWORK EMULATION LOOP
	puts("Emulator waiting for packets...\n");

	struct new_packet *delayedPkt = NULL;
	struct sockaddr_in recvAddr;
	struct addrinfo *np;

	socklen_t recvLen = sizeof(recvAddr);
	//socklen_t sendLen = sizeof(sendAddr);
	// HACK: Don't like hard coding this, but don't know any other way
	size_t MAX_HOST_LEN = 256;
	char name[MAX_HOST_LEN];
	gethostname(name, MAX_HOST_LEN);
	//Need to just get lowest level dns name i.e. mumble-30
	char *pch;
	pch = strtok(name, ".");
	head = parseTable(filename, pch, port);

	unsigned long long prevMS = getTimeMS();

	while (1) {
		void *msg = malloc(sizeof(struct new_packet));
		bzero(msg, sizeof(struct new_packet));
		size_t bytesRecvd;
		bytesRecvd = recvfrom(sockfd, msg, sizeof(struct new_packet), 0,
				    (struct sockaddr *)&recvAddr, &recvLen);
		struct forward_entry *curEntry;
		if (bytesRecvd != -1) {
			printf("Received %d bytes\n", (int)bytesRecvd);
			// Deserialize the message into a packet 
			struct new_packet *pkt = malloc(sizeof(struct new_packet));
			bzero(pkt, sizeof(struct new_packet));
			deserializePacket(msg, pkt);


			//TODO: Consult forwarding table to see if packet is to be
			//forwarded, then enqueue it
			//
			// for now, just forward it directly to sender...
			//sendPacketTo(sockfd, pkt, (struct sockaddr *)sp->ai_addr);


			curEntry = head;
			unsigned long curEntryIP = 0;

			struct addrinfo entryHints;
			bzero(&entryHints, sizeof(struct addrinfo));
			entryHints.ai_family   = AF_INET;
			entryHints.ai_socktype = SOCK_DGRAM;
			entryHints.ai_flags    = 0;

			struct addrinfo *entryInfo;
			int dstsockfd = 0;
			int errcode = getaddrinfo(curEntry->dst_hostname, NULL, &entryHints, &entryInfo);
			
			if( errcode != 0 ){
				//fprintf(stderr, "cannot resolve hostname:%s", curEntry->dst_hostname);
				//fprintf(logFile, "PACKET DROPPED\nDST_HOSTNAME:%s", curEntry->dst_hostname);
                logOut("Packet dropped: cannot resolve hostname", getTimeMS(), pkt);
				free(pkt);
				continue;
			}
			struct addrinfo *ep;
			for(ep = entryInfo; ep != NULL; ep = ep->ai_next){
				dstsockfd = socket(ep->ai_family, ep->ai_socktype, ep->ai_protocol);
				if ( dstsockfd == -1){
					perror("Socket error");
					continue;
				}

				if ( bind(dstsockfd, ep->ai_addr, ep->ai_addrlen) == -1){
					perror("Bind error");
					close(dstsockfd);
					continue;
				}
				break;
			}
			if (ep == NULL){ 
				printf("Error: cannot resolve hostname:%s", curEntry->dst_hostname);
				continue;
			}
			else{
				close(dstsockfd);
			}

			curEntryIP = ((struct sockaddr_in*)ep)->sin_addr.s_addr;
			
			while ( curEntry != NULL && (curEntryIP != pkt->dst_ip || curEntry->dst_port != pkt->dst_port)){
				curEntry = curEntry->next;
			}
			
			if(curEntry == NULL){
				//printf("Error: no forwarding info for destination:%lu on port:%hu", pkt->dst_ip, pkt->dst_port);
				//fprintf(logFile, "PACKET DROPPED\nDST_IP:%lu", pkt->dst_ip);
                logOut("Packet dropped: no forwarding info for dest", getTimeMS(), pkt);
				free(pkt);
				continue;
			}
			
			struct addrinfo nextHints;
			bzero(&nextHints, sizeof(struct addrinfo));
			nextHints.ai_family   = AF_INET;
			nextHints.ai_socktype = SOCK_DGRAM;
			nextHints.ai_flags    = 0;
			
			struct addrinfo *nextInfo;
			
			int nextsockfd = 0;
			char str[6];
			sprintf(str, "%d", curEntry->next_port);
			errcode = getaddrinfo(curEntry->next_hostname, str, &nextHints, &nextInfo);
			
			if( errcode != 0 ){
				//fprintf(stderr, "cannot resolve hostname:%s", curEntry->next_hostname);
				//fprintf(logFile, "PACKET DROPPED\nDST_HOSTNAME:%s", curEntry->next_hostname);
                logOut("Packet dropped: cannot resolve hostname", getTimeMS(), pkt);
				free(pkt);
				continue;
			}
			for(np = nextInfo; np != NULL; np = np->ai_next){
				nextsockfd = socket(np->ai_family, np->ai_socktype, np->ai_protocol);
				if ( nextsockfd == -1){
					perror("Socket error");
					continue;
				}

				break;
			}

			if (np == NULL){ 
				printf("Error: cannot resolve hostname:%s", curEntry->next_hostname);
				continue;
			}
			else{
				close(nextsockfd);
			}

			// By this point, pkt must be forwarded
			enqueuePkt(pkt);
		}
		else if(delayedPkt != NULL){
			// If pkt delay is greater than elapsed time
			if( getTimeMS() - prevMS < curEntry->delay){
				continue;
			}
			else{
				// Determine whether or not to drop pkt
				int r = ( 100.0 * rand() / ( RAND_MAX + 1.0));
                // Don't drop END or REQ pkts
				if(r <= curEntry->loss
                 && delayedPkt->pkt.type != 'E'
                 && delayedPkt->pkt.type != 'R') {
					//fprintf(logFile, "PACKET DROPPED\nDST_HOSTNAME:%s", curEntry->dst_hostname);
                    logOut("Packet dropped", getTimeMS(), delayedPkt);
				}
				else{
					sendPacketTo(sockfd, delayedPkt, (struct sockaddr *)np->ai_addr);
				}
				delayedPkt = NULL;
			}	
		}
		else{
			if ((delayedPkt = dequeuePkt(queue1h)) != NULL){
				queue1h = queue1h->next;
				--q1num;
				prevMS = getTimeMS();
			}
			else if((delayedPkt = dequeuePkt(queue2h)) != NULL){
				queue2h = queue2h->next;
				--q2num;
				prevMS = getTimeMS();
			}
			else if((delayedPkt = dequeuePkt(queue3h)) != NULL){
				queue3h = queue3h->next;
				--q3num;
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
	struct packet_node *q = NULL, *queue = NULL;
	switch (1) { // TODO: implement packet priority: pkt->priority) {
		case 1: q = queue1t; 
			q->next = malloc(sizeof(struct packet_node));
			queue1t = q->next;
			break;
		case 2: q = queue2t; 
			q->next = malloc(sizeof(struct packet_node));
			queue2t = q->next;
			break;
		case 3: q = queue3t; 
			q->next = malloc(sizeof(struct packet_node));
			queue3t = q->next;
			break;
		default:
			logOut("Packet has invalid priority value", getTimeMS(), pkt);
			return;
	};

	// Hack so that we can increment the right queue number after adding a pkt
	queue = q;

	// Check if the queue is already full
	if (q == queue1t && q1num >= MAX_QUEUE - 1) {
		logOut("Priority queue 1 was full", getTimeMS(), pkt);
		return;
	} else if (q == queue2t && q2num >= MAX_QUEUE - 1) {
		logOut("Priority queue 2 was full", getTimeMS(), pkt);
		return;
	} else if (q == queue3t && q3num >= MAX_QUEUE - 1) {
		logOut("Priority queue 3 was full", getTimeMS(), pkt);
		return;
	}

	// Add the packet to the end of the queue
	q->next->pkt  = pkt;
	q->next->next = NULL;
	q = q->next;

	// Update the number of enqueued packets
	if      (queue == queue1t) ++q1num;
	else if (queue == queue2t) ++q2num;
	else if (queue == queue3t) ++q3num;

	// DEBUG
	printf("Enqueued pkt: seq = %lu\n", pkt->pkt.seq);
}

struct new_packet *dequeuePkt(struct packet_node *q) {

	// Update the number of enqueued packets
	if (q->next != NULL){
		if (q->next->pkt != NULL){
			printf("Dequeued pkt: seq = %lu\n", q->next->pkt->pkt.seq);
		}
		return q->next->pkt;
	}
	return NULL;
}

void logOut(const char *msg, unsigned long long timestamp, struct new_packet *pkt) {
	fprintf(logFile, "%s : ", msg);
	if (pkt != NULL) {
		fprintf(logFile,
                "source: %s:%d, dest: %s:%d, time: %llu, priority: %d, payld len: %lu, type: %c\n",
				inet_ntoa(*(struct in_addr *)&pkt->src_ip), // TODO: not working correctly, always 2.0.0.0
                pkt->src_port,
				inet_ntoa(*(struct in_addr *)&pkt->dst_ip), // TODO: not working correctly, always 2.0.0.0
                pkt->dst_port,
				timestamp,
				(int)pkt->priority, // TODO: always reported as 0.... 
				pkt->pkt.len,
                pkt->pkt.type); // TODO for debug.. remove eventually
	} else {
		fprintf(logFile, "[]\n");
	}

    fflush(logFile);
}

