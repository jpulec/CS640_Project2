#ifndef _TRACKER_H_
#define _TRACKER_H_

struct file_info {
    char *filename;
    struct file_part *parts;
};

struct file_part {
    char *emu_hostname;
    int emu_port;
    char *dst_hostname;
    int dst_port;
    char *next_hostname;
    int next_port;
    unsigned long long delay;
    float loss;
    struct file_part *next_part;
};


// ----------------------------------------------------------------------------
// Parse the forwarding table file.
//   Builds a file_info struct for the specified file consisting of 
//   a linked list of file_part structures that contain the location 
//   and sequence information from the tracker for the specified file.
// ----------------------------------------------------------------------------
struct file_info *parseTable(const char *filename);

// ----------------------------------------------------------------------------
void printFileInfo(struct file_info *info);

// ----------------------------------------------------------------------------
void printFilePartInfo(struct file_part *part);

// ----------------------------------------------------------------------------
void freeFileInfo(struct file_info *info);

#endif

