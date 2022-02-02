#ifndef _IO_H_
#define _IO_H_

#include <sys/types.h>

#include <fuse_lowlevel.h>


ssize_t write_block(int file, void *data, size_t block_no);
ssize_t read_block(int file, void *buf, size_t block_no);
ssize_t write_data(int file, void *data, size_t len, size_t block_no);
struct inode *read_inode(int file, fuse_ino_t ino);

ssize_t fs_int_pread(size_t ino, void *buf, size_t count, off_t offset);
ssize_t fs_int_pwrite(size_t ino, void *buf, size_t count, off_t offset);
ssize_t fs_pread(size_t ino, void *buf, size_t count, off_t offset);
ssize_t fs_pwrite(size_t ino, void *buf, size_t count, off_t offset);

size_t add_file(struct inode *inode);

#endif
