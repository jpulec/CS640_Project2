#ifndef _NEW_PACKET_H_
#define _NEW_PACKET_H_

#include <netinet/in.h>
#include "packet.h"

struct new_packet {
    char priority;
    unsigned long  src_ip;
    unsigned short src_port;
    unsigned long  dst_ip;
    unsigned short dst_port;
    unsigned long  len; 
    struct packet  pkt;
} __attribute__((packed));

#define NEW_PACKET_SIZE sizeof(struct new_packet)

void *serializePacket(struct new_packet *pkt);
void deserializePacket(void *msg, struct new_packet *pkt);

void sendPacketTo(int sockfd, struct new_packet *pkt, struct sockaddr *addr);
void recvPacket(int sockfd, struct new_packet *pkt);

void printPacketInfo(struct new_packet *pkt, struct sockaddr_storage *saddr);

#endif

