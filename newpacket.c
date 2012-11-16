#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <arpa/inet.h>

#include "newpacket.h"
#include "utilities.h"


void *serializePacket(struct new_packet *pkt) {
    if (pkt == NULL) {
        fprintf(stderr, "Serialize: invalid new_packet\n");
        return NULL;
    }

    struct new_packet *spkt = malloc(sizeof(struct new_packet));
    bzero(spkt, sizeof(struct new_packet));

    spkt->priority = pkt->priority;
    spkt->src_ip   = htonl(pkt->src_ip);
    spkt->src_port = htons(pkt->src_port);
    spkt->dst_ip   = htonl(pkt->dst_ip);
    spkt->dst_port = htons(pkt->dst_port);
    spkt->len      = htonl(pkt->len);
    spkt->pkt.type = pkt->pkt.type;
    spkt->pkt.seq  = htonl(pkt->pkt.seq);
    spkt->pkt.len  = htonl(pkt->pkt.len);
    memcpy(spkt->pkt.payload, pkt->pkt.payload, MAX_PAYLOAD);

    return spkt;
}

void deserializePacket(void *msg, struct new_packet *pkt) {
    if (msg == NULL) {
        fprintf(stderr, "Deserialize: invalid message\n");
        return;
    }
    if (pkt == NULL) {
        fprintf(stderr, "Deserialize: invalid new_packet\n");
        return;
    }

    struct new_packet *p = (struct new_packet *)msg;
    pkt->priority = pkt->priority;
    pkt->src_ip   = ntohl(p->src_ip);
    pkt->src_port = ntohs(p->src_port);
    pkt->dst_ip   = ntohl(p->dst_ip);
    pkt->dst_port = ntohs(p->dst_port);
    pkt->len      = ntohl(p->len);
    pkt->pkt.type = p->pkt.type;
    pkt->pkt.seq  = ntohl(p->pkt.seq);
    pkt->pkt.len  = ntohl(p->pkt.len);
    memcpy(pkt->pkt.payload, p->pkt.payload, MAX_PAYLOAD);
}

void sendPacketTo(int sockfd, struct new_packet *pkt, struct sockaddr *addr) {
    struct new_packet *spkt = serializePacket(pkt);
    printPacketInfo(pkt, (struct sockaddr_storage *)addr);
    size_t bytesSent = sendto(sockfd, spkt, NEW_PACKET_SIZE,
                              0, addr, sizeof(struct sockaddr));

    if (bytesSent == -1) {
        perror("Sendto error");
        fprintf(stderr, "Error sending new_packet\n");
    } else {
        const char *typeStr;
        if      (pkt->pkt.type == 'R') typeStr = "**REQUEST**";
        else if (pkt->pkt.type == 'D') typeStr = "DATA";
        else if (pkt->pkt.type == 'E') typeStr = "**END***";
        else if (pkt->pkt.type == 'A') typeStr = "ACK";
        else                       typeStr = "UNDEFINED";

        printf("-> [Sent %s new_packet] ", typeStr);
        printPacketInfo(pkt, (struct sockaddr_storage *)addr);
    }
}

void printPacketInfo(struct new_packet *pkt, struct sockaddr_storage *saddr) {
    if (pkt == NULL) {
        fprintf(stderr, "Unable to print info for null new_packet\n");
        return;
    }

    char *ipstr = ""; 
    unsigned short ipport = 0;
    if (saddr == NULL) {
        fprintf(stderr, "Unable to print new_packet source from null sockaddr\n");
    } else {
        struct sockaddr_in *sin = (struct sockaddr_in *)saddr;
        ipstr  = inet_ntoa(sin->sin_addr);
        ipport = ntohs(sin->sin_port);
    }

    // Get 'preview' bytes (replacing unprintables with '_')
    char pl_bytes[5];
    pl_bytes[0] = (pkt->pkt.payload[0] >= 0 && pkt->pkt.payload[0] <= 31) ? '_' : pkt->pkt.payload[0];
    pl_bytes[1] = (pkt->pkt.payload[1] >= 0 && pkt->pkt.payload[1] <= 31) ? '_' : pkt->pkt.payload[1];
    pl_bytes[2] = (pkt->pkt.payload[2] >= 0 && pkt->pkt.payload[2] <= 31) ? '_' : pkt->pkt.payload[2];
    pl_bytes[3] = (pkt->pkt.payload[3] >= 0 && pkt->pkt.payload[3] <= 31) ? '_' : pkt->pkt.payload[3];
    pl_bytes[4] = '\0';

    printf("@%llu ms : ip %s:%u : seq %lu : len %lu : pld \"%s\"\n",
        getTimeMS(), ipstr, ipport, pkt->pkt.seq, pkt->pkt.len, pl_bytes);
    /*
    printf("  new_packet from %s:%u (%lu payload bytes):\n",ipstr,ipport,pkt->pkt.len);
    printf("    type = %c\n",  pkt->pkt.type);
    printf("    seq  = %lu\n", pkt->pkt.seq);
    printf("    len  = %lu\n", pkt->pkt.len);
    printf("    data = %s\n",  pkt->pkt.payload);
    puts("");
    */
}

