#ifndef _TRACKER_H_
#define _TRACKER_H_

struct file_info {
    char *filename;
    struct file_part *parts;
};

struct file_part {
    int id;
    int sender_port;
    char *sender_hostname;
    struct file_part *next_part;
};


// ----------------------------------------------------------------------------
// Parse the tracker file.
//   Builds a file_info struct for the specified file consisting of 
//   a linked list of file_part structures that contain the location 
//   and sequence information from the tracker for the specified file.
// ----------------------------------------------------------------------------
struct file_info *parseTracker(const char *filename);

// ----------------------------------------------------------------------------
void printFileInfo(struct file_info *info);

// ----------------------------------------------------------------------------
void printFilePartInfo(struct file_part *part);

// ----------------------------------------------------------------------------
void freeFileInfo(struct file_info *info);

#endif

