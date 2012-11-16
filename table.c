#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "table.h"
#include "utilities.h"

#define TOK_PER_LINE 8

enum token { EMU_HOSTNAME, EMU_PORT, DST_HOSTNAME, DST_PORT, NEXT_HOSTNAME, NEXT_PORT, DELAY, LOSS };


// ----------------------------------------------------------------------------
// Parse the tracker file.
//   Builds a file_info struct for the specified file consisting of 
//   a linked list of file_part structures that contain the location 
//   and sequence information from the tracker for the specified file.
// ----------------------------------------------------------------------------
struct forward_entry *parseTable(const char *filename, const char *emu_hostname, int emu_port) {
    if (filename == NULL) ferrorExit("ParseTable: invalid filename");

    // Setup dummy head entry
    struct forward_entry *head = malloc(sizeof(struct forward_entry));
    struct forward_entry *tail = malloc(sizeof(struct forward_entry));
    head = tail;

    // Open the tracker file
    FILE *file = fopen(filename, "r");
    if (file == NULL) perrorExit("Table open error");
    else              puts("\nTable file opened.");

    // Read in a line at a time from the tracker file
    char *line = NULL;
    size_t lineLen = 0;
    size_t bytesRead = getline(&line, &lineLen, file);
    if (bytesRead == -1) perrorExit("Getline error");
    while (bytesRead != -1) { 
        // Tokenize line
        int n = 0;

        struct forward_entry *curEntry = malloc(sizeof(struct forward_entry));
        char *tokens[TOK_PER_LINE];
        char *tok = strtok(line, " ");
        while (tok != NULL) {
            tokens[n++] = tok;
            tok  = strtok(NULL, " ");
        }
	if( strcmp(tokens[EMU_HOSTNAME], emu_hostname) == 0 && 
	    atoi(tokens[EMU_PORT]) == emu_port){
	    curEntry->emu_hostname  = strdup(tokens[EMU_HOSTNAME]);
	    curEntry->emu_port      = atoi(tokens[EMU_PORT]);
	    curEntry->dst_hostname  = strdup(tokens[DST_HOSTNAME]);
	    curEntry->dst_port      = atoi(tokens[DST_PORT]);
	    curEntry->next_hostname = strdup(tokens[NEXT_HOSTNAME]);
	    curEntry->next_port	    = atoi(tokens[NEXT_PORT]);
	    curEntry->delay	    = atoi(tokens[DELAY]);
	    curEntry->loss	    = atof(tokens[LOSS]);
	    tail->next = curEntry;
	    tail = curEntry;
	}

        // Get the next tracker line
        free(line);
        line = NULL;
        bytesRead = getline(&line, &lineLen, file);
    }
    free(line);
    printf("Table file:%s parsed\n", filename);

    // Close the tracker file
    if (fclose(file) != 0) perrorExit("Table close error");
    else                   puts("Table file closed.\n");

    return head->next; // success
}

