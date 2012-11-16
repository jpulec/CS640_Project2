#ifndef _TABLE_H_
#define _TABLE_H_

struct forward_entry {
    char *emu_hostname;
    int emu_port;
    char *dst_hostname;
    int dst_port;
    char *next_hostname;
    int next_port;
    int delay;
    float loss;
    struct forward_entry *next;
};


// ----------------------------------------------------------------------------
// Parse the forwarding table.
// ----------------------------------------------------------------------------
struct forward_entry *parseTable(const char *filename, const char *emu_hostname, int emu_port);

#endif

