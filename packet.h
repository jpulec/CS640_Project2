#ifndef _PACKET_H_
#define _PACKET_H_

#include <netinet/in.h>

#define MAX_PAYLOAD 5120


struct packet {
    char type;
    unsigned long seq;
    unsigned long len;
    char payload[MAX_PAYLOAD];
} __attribute__((packed));

#define PACKET_SIZE sizeof(struct packet)

/*
void *serializePacket(struct packet *pkt);
void deserializePacket(void *msg, struct packet *pkt);

void sendPacketTo(int sockfd, struct packet *pkt, struct sockaddr *addr);
void recvPacket(int sockfd, struct packet *pkt);

void printPacketInfo(struct packet *pkt, struct sockaddr_storage *saddr);
*/

#endif

