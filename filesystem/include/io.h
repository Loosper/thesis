#ifndef _IO_H_
#define _IO_H_

#include <sys/types.h>

#include <fuse_lowlevel.h>

#include "fs_types.h"


ssize_t write_block(void *data, size_t block_no);
ssize_t read_block(void *buf, size_t block_no);
ssize_t write_data(void *data, size_t len, size_t block_no);
ssize_t read_data(void *data, size_t len, size_t block_no);

size_t file_add_space(struct inode *inode, size_t blk_req, size_t (*allocator)());
ssize_t pread_ino(struct inode* ino, void *buf, size_t count, off_t offset);
ssize_t pwrite_ino(struct inode* ino, const void *buf, size_t count, off_t offset);

#endif
