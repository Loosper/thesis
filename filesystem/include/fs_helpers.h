#ifndef _FS_HELPERS_H_
#define _FS_HELPERS_H_

#include <sys/stat.h>

#include <fuse_lowlevel.h>

#include "fs_types.h"

#define logprintf(...) fuse_log(FUSE_LOG_INFO, ##__VA_ARGS__)


int req_fd(fuse_req_t req);
struct stat stat_from_inode(struct inode *inode, size_t num);
void init_blk_zero(size_t blk_num);
void print_inode(struct inode *ino, size_t num);
void print_block(size_t block_num);
void check_errno(ssize_t ret);

#endif
