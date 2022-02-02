#ifndef _FS_HELPERS_H_
#define _FS_HELPERS_H_

#include <sys/stat.h>

#include <fuse_lowlevel.h>

#define logprintf(...) fuse_log(FUSE_LOG_INFO, ##__VA_ARGS__)


struct stat stat_from_inode(struct inode *inode, size_t num);
void print_inode(struct inode *ino, size_t num);
void print_block(struct filesystem *fs, size_t block_num);
void check_errno(ssize_t ret);
int req_fd(fuse_req_t req);

#endif
