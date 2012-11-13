#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>

#include "utilities.h"

unsigned long long getTimeMS() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (unsigned long long)((t.tv_sec + (t.tv_usec / 1000000.0)) * 1000.0);
}

// ----------------------------------------------------------------------------
// Print hostname and port to stdout
// ----------------------------------------------------------------------------
void printNameInfo(struct addrinfo *addr) {
    if (addr == NULL) {
        fprintf(stderr, "Cannot print name info from null sockaddr\n");
        return;
    }

    char host[NI_MAXHOST], service[NI_MAXSERV];
    int s = getnameinfo((struct sockaddr *)addr->ai_addr,
                        addr->ai_addrlen, host, NI_MAXHOST,
                        service, NI_MAXSERV, NI_NUMERICSERV);
    if (s != 0)
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));
    else {
        printf("Connected to %s:%s\n", host, service);
    }
}

// ----------------------------------------------------------------------------
// Print error from errno and exit with a failure indication
// ----------------------------------------------------------------------------
void perrorExit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// ----------------------------------------------------------------------------
// Print error to stderr and exit with a failure indication
// ----------------------------------------------------------------------------
void ferrorExit(const char *msg) {
    fputs(msg, stderr);
    exit(EXIT_FAILURE);
}

