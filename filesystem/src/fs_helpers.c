#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include <fuse_lowlevel.h>

#include "fs_types.h"
#include "fs_types.h"
#include "io.h"


struct filesystem *req_userdata(fuse_req_t req)
{
	return fuse_req_userdata(req);
}

int req_fd(fuse_req_t req)
{
	return req_userdata(req)->backing_store;
}

struct stat stat_from_inode(struct inode *inode, size_t num)
{
	struct stat stat = {
		// .st_dev = 1,
		.st_ino = num,
		.st_uid = inode->uid,
		.st_gid = inode->gid,
		.st_mode = inode->mode,
		.st_size = inode->size,
		.st_blksize = FS_BLOCK_SIZE,
		.st_atime = inode->atime,
		.st_mtime = inode->mtime,
		.st_ctime = inode->ctime
	};
	return stat;
}

void print_inode(struct inode *ino, size_t num)
{
	fuse_log(FUSE_LOG_INFO,
		"inode (%d) %s:\n"
		"\tsize: %ld\n"
		"\trefs: %ld\n"
		"\tuid/gid: %d/%d\n"
		"\tmode: %x\n"
		"\tctime: %ld\n",
		num, ino->name,
		ino->size, ino->refs, ino->uid, ino->gid,
		ino->mode, ino->ctime
	);
}

void print_block(struct filesystem *fs, size_t block_num)
{
	uint8_t *data = malloc(FS_BLOCK_SIZE);
	void *to_free = data;
	read_block(fs->backing_store, data, block_num);

	fuse_log(FUSE_LOG_INFO, "block %ld:\n", block_num);
	for (int i = 0; i < FS_BLOCK_SIZE / 32; i++) {
		for (int j = 0; j < 4; j++) {
			fuse_log(FUSE_LOG_INFO,
				"%02x%02x%02x%02x%02x%02x%02x%02x",
				data[0], data[1], data[2], data[3],
				data[4], data[5], data[6], data[7]
			);
			data += 8;
		}
		fuse_log(FUSE_LOG_INFO, "\n");
	}
	free(to_free);
}

// the file returning error is unrecoverable for now
void check_errno(ssize_t ret)
{
	if (ret < 0) {
		fuse_log(
			FUSE_LOG_ERR,
			"pread/pwrite failed: %s\n", strerror(errno)
		);
		// TODO: fuse_session_exit?
		exit(1);
	}
}
