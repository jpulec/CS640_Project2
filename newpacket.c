#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <arpa/inet.h>

#include "new_packet.h"
#include "utilities.h"


void *serializePacket(struct new_packet *pkt) {
    if (pkt == NULL) {
        fprintf(stderr, "Serialize: invalid new_packet\n");
        return NULL;
    }

    struct new_packet *spkt = malloc(sizeof(struct new_packet));
    bzero(spkt, sizeof(struct new_packet));

    spkt->pkt->type = pkt->pkt->type;
    spkt->pkt->seq  = htonl(pkt->pkt->seq);
    spkt->pkt->len  = htonl(pkt->pkt->len);
    memcpy(spkt->pkt->payload, pkt->pkt->payload, MAX_PAYLOAD);

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
    pkt->type = p->type;
    pkt->seq  = ntohl(p->seq);
    pkt->len  = ntohl(p->len);
    memcpy(pkt->payload, p->payload, MAX_PAYLOAD);
}

void sendPacketTo(int sockfd, struct new_packet *pkt, struct sockaddr *addr) {
    struct new_packet *spkt = serializePacket(pkt);
    size_t bytesSent = sendto(sockfd, spkt, NEW_PACKET_SIZE,
                              0, addr, sizeof(struct sockaddr));

    if (bytesSent == -1) {
        perror("Sendto error");
        fprintf(stderr, "Error sending new_packet\n");
    } else {
        const char *typeStr;
        if      (pkt->type == 'R') typeStr = "**REQUEST**";
        else if (pkt->type == 'D') typeStr = "DATA";
        else if (pkt->type == 'E') typeStr = "**END***";
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

    char pl_bytes[5];
    pl_bytes[0] = pkt->payload[0]; 
    pl_bytes[1] = pkt->payload[1]; 
    pl_bytes[2] = pkt->payload[2]; 
    pl_bytes[3] = pkt->payload[3]; 
    pl_bytes[4] = '\0';

    printf("@%llu ms : ip %s:%u : seq %lu : len %lu : pld \"%s\"\n",
        getTimeMS(), ipstr, ipport, pkt->seq, pkt->len, pl_bytes);
    /*
    printf("  new_packet from %s:%u (%lu payload bytes):\n",ipstr,ipport,pkt->len);
    printf("    type = %c\n", pkt->type);
    printf("    seq  = %lu\n", pkt->seq);
    printf("    len  = %lu\n", pkt->len);
    printf("    data = %s\n", pkt->payload);
    puts("");
    */
}

