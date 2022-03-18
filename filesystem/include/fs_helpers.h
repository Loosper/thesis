#ifndef _FS_HELPERS_H_
#define _FS_HELPERS_H_

#include <stdlib.h>

#include <sys/stat.h>

#include <fuse_lowlevel.h>

#include "fs_types.h"

#define member_size(type, member) sizeof(((type *)0)->member)
#define logprintf(...) fuse_log(FUSE_LOG_INFO, ##__VA_ARGS__)

// sole purpose is to make the macro below debugable
static inline void dummy_exit(int code)
{
	exit(code);
}

// TODO: fuse_session_exit?
#define CHECK_ERRNO(ret) do { \
	if (ret < 0) { \
		logprintf( \
			"pread/pwrite failed at %s:%d: %s\n", \
			__FILE__, __LINE__, strerror(errno) \
		); \
		dummy_exit(1); \
	} \
} while (0)

int req_fd(fuse_req_t req);
void make_empty_inode(struct inode *inode, mode_t mode);
struct stat stat_from_inode(struct inode *inode, size_t num);
void init_blk_zero(size_t blk_num);
void print_inode(struct inode *ino, size_t num);
void print_block(size_t block_num);

#endif
