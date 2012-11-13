#ifndef _UTILITIES_H_
#define _UTILITIES_H_

#include <netdb.h>

#define LOOPBACK "127.0.0.1"

// ----------------------------------------------------------------------------
// Returns the current system time with millisecond granularity
// ----------------------------------------------------------------------------
unsigned long long getTimeMS();

// ----------------------------------------------------------------------------
// Print hostname and port to stdout
// ----------------------------------------------------------------------------
void printNameInfo(struct addrinfo *addr);

// ----------------------------------------------------------------------------
// Print error from errno and exit with a failure indication
// ----------------------------------------------------------------------------
void perrorExit(const char *msg);

// ----------------------------------------------------------------------------
// Print error to stderr and exit with a failure indication
// ----------------------------------------------------------------------------
void ferrorExit(const char *msg);

#endif

