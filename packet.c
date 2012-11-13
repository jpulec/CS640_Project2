#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <arpa/inet.h>

#include "packet.h"
#include "utilities.h"


void *serializePacket(struct packet *pkt) {
    if (pkt == NULL) {
        fprintf(stderr, "Serialize: invalid packet\n");
        return NULL;
    }

    struct packet *spkt = malloc(sizeof(struct packet));
    bzero(spkt, sizeof(struct packet));

    spkt->type = pkt->type;
    spkt->seq  = htonl(pkt->seq);
    spkt->len  = htonl(pkt->len);
    memcpy(spkt->payload, pkt->payload, MAX_PAYLOAD);

    return spkt;
}

void deserializePacket(void *msg, struct packet *pkt) {
    if (msg == NULL) {
        fprintf(stderr, "Deserialize: invalid message\n");
        return;
    }
    if (pkt == NULL) {
        fprintf(stderr, "Deserialize: invalid packet\n");
        return;
    }

    struct packet *p = (struct packet *)msg;
    pkt->type = p->type;
    pkt->seq  = ntohl(p->seq);
    pkt->len  = ntohl(p->len);
    memcpy(pkt->payload, p->payload, MAX_PAYLOAD);
}

void sendPacketTo(int sockfd, struct packet *pkt, struct sockaddr *addr) {
    struct packet *spkt = serializePacket(pkt);
    size_t bytesSent = sendto(sockfd, spkt, PACKET_SIZE,
                              0, addr, sizeof(struct sockaddr));

    if (bytesSent == -1) {
        perror("Sendto error");
        fprintf(stderr, "Error sending packet\n");
    } else {
        const char *typeStr;
        if      (pkt->type == 'R') typeStr = "**REQUEST**";
        else if (pkt->type == 'D') typeStr = "DATA";
        else if (pkt->type == 'E') typeStr = "**END***";
        else                       typeStr = "UNDEFINED";

        printf("-> [Sent %s packet] ", typeStr);
        printPacketInfo(pkt, (struct sockaddr_storage *)addr);
    }
}

void printPacketInfo(struct packet *pkt, struct sockaddr_storage *saddr) {
    if (pkt == NULL) {
        fprintf(stderr, "Unable to print info for null packet\n");
        return;
    }

    char *ipstr = ""; 
    unsigned short ipport = 0;
    if (saddr == NULL) {
        fprintf(stderr, "Unable to print packet source from null sockaddr\n");
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
    printf("  Packet from %s:%u (%lu payload bytes):\n",ipstr,ipport,pkt->len);
    printf("    type = %c\n", pkt->type);
    printf("    seq  = %lu\n", pkt->seq);
    printf("    len  = %lu\n", pkt->len);
    printf("    data = %s\n", pkt->payload);
    puts("");
    */
}

