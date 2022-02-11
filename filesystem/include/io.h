#ifndef _IO_H_
#define _IO_H_

#include <sys/types.h>

#include <fuse_lowlevel.h>


size_t allocate_block();
ssize_t write_block(int file, void *data, size_t block_no);
ssize_t read_block(int file, void *buf, size_t block_no);
ssize_t write_data(int file, void *data, size_t len, size_t block_no);
ssize_t read_data(int file, void *data, size_t len, size_t block_no);
size_t file_add_space(size_t secondary_blk, size_t blk_req, size_t (*allocator)());

ssize_t fs_int_pread(size_t ino, void *buf, size_t count, off_t offset);
ssize_t fs_int_pwrite(size_t ino, const void *buf, size_t count, off_t offset);
ssize_t fs_pread(size_t ino, void *buf, size_t count, off_t offset);
ssize_t fs_pwrite(size_t ino, const void *buf, size_t count, off_t offset);

size_t add_file(struct inode *inode);
size_t add_dir(struct inode *inode);
int read_inode(size_t num, struct inode *inode);
int write_inode(size_t num, struct inode *inode);
int add_direntry(size_t dir_ino, struct dirent *entry);
size_t get_direntry(size_t dir_ino, const char *filename);
int rm_direntry(size_t dir_ino, const char *filename);
struct dirent list_dir(size_t dir_ino, off_t idx);

#endif
