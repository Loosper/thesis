#ifndef _ITABLE_H
#define _ITABLE_H
#include <stdbool.h>

size_t add_file(struct inode *inode);
bool file_exists(size_t filename);
int read_inode(size_t num, struct inode *inode);
int write_inode(size_t num, struct inode *inode);

#endif
