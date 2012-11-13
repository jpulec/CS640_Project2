#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <err.h>


int main(int argc, char **argv){
    if (argc != 9) {
        printf("Usage: emulator -p <port> -q <queue_size> -f <filename> -l <log>\n");
        return 1;
    }
    int port, queue_size, opt;
    char *filename = NULL;
    char *log = NULL;
    //To parse and see if any unknown characters are encountered
    //after a number
    char *endptr = NULL;

    while( (opt = getopt(argc, argv, "p:q:f:l:")) != -1) {
        switch(opt) {
          case 'p':
              port = strtol(optarg, &endptr, 10);
              if (*endptr != 0 || port < 1024 || port > 65536) {
                  printf("Port number %s is invalid. Port must be between 1024 and 65536.\n", optarg);
                  return 1;
              }
              break;
          case 'q': 
              queue_size = strtol(optarg, &endptr, 10);
              if (*endptr != 0 || queue_size < 0) {
                  printf("Queue size %s is invalid. Must be greater than 0.\n", optarg);
                  return 1;
              }
              break;
          case 'f':
              filename = optarg;
              break;
          case 'l': 
              log = optarg;
              break;
          default:
              printf("Usage: emulator -p <port> -q <queue_size> -f <filename> -l <log>\n");
              return 1;
        }

    }
    printf("Port           : %d\n", port);
    printf("Queue Size     : %d\n", queue_size);
    printf("Filename       : %s\n", filename);
    printf("Log File       : %s\n", log);
    return 0;
}

