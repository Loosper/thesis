#include "io.h"
#include "fs_types.h"

#include <fuse_lowlevel.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <errno.h>

#define SECTOR_SIZE 512
#define BLOCK_SIZE (1 * SECTOR_SIZE)


static inline struct filesystem *req_userdata(fuse_req_t req)
{
	return fuse_req_userdata(req);
}

static inline int req_fd(fuse_req_t req)
{
	return req_userdata(req)->backing_store;
}

static inline struct stat stat_from_inode(struct inode *inode, size_t num)
{
	struct stat stat = {
		// .st_dev = 1,
		.st_ino = num,
		.st_uid = inode->uid,
		.st_gid = inode->gid,
		.st_mode = inode->mode,
		.st_size = inode->size,
		.st_blocks = inode->blocks,
		.st_blksize = BLOCK_SIZE,
		.st_atime = inode->atime,
		.st_mtime = inode->mtime,
		.st_ctime = inode->ctime
	};
	return stat;
}

static void print_inode(struct inode *ino, size_t num)
{
	fuse_log(FUSE_LOG_INFO,
		"inode (%d) %s:\n"
		"\tsize: %ld\n"
		"\trefs: %ld\n"
		"\tblocks: %ld\n"
		"\tuid/gid: %d/%d\n"
		"\tmode: %x\n"
		"\tctime: %ld\n",
		num, ino->name,
		ino->size, ino->refs, ino->blocks, ino->uid, ino->gid,
		ino->mode, ino->ctime

	);
}

// the file returning error is unrecoverable for now
static inline void check_errno(ssize_t ret)
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

// all this because write() can be interrupted by signals and partially
// complete. We assume the file has good dimensions and don't prevent infinite
// spin
ssize_t write_block(int file, void *data, size_t block_no)
{
	size_t written = 0;
	ssize_t ret;

	while (written < BLOCK_SIZE) {
		ret = pwrite(
			file, data + written, BLOCK_SIZE - written,
			block_no * BLOCK_SIZE + written
		);
		check_errno(ret);
		written += ret;
	}

	fuse_log(FUSE_LOG_INFO, "written %ld\n", written);
	return written;
}

// see write_block() comment
ssize_t read_block(int file, void *buf, size_t block_no)
{
	size_t rread = 0;
	ssize_t ret;

	while (rread < BLOCK_SIZE) {
		ret = pread(
			file, buf + rread, BLOCK_SIZE - rread,
			block_no * BLOCK_SIZE + rread
		);
		check_errno(ret);
		rread += ret;
	}

	return rread;
}

ssize_t write_data(int file, void *data, size_t len, size_t block_no)
{
	uint8_t block[BLOCK_SIZE] = {0};
	memcpy(block, data, len);

	return write_block(file, block, block_no);
}

// TODO: terribly wasteful but simple to implement
struct inode *read_inode(int file, fuse_ino_t ino)
{
	struct inode *inode = malloc(BLOCK_SIZE);
	read_block(file, inode, ino);

	return inode;
}

int cur_inode = 1;

void fs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	if (cur_inode < 2) {
		fuse_reply_err(req, ENOENT);
		return;
	}

	struct inode *ino = read_inode(req_fd(req), cur_inode);
	struct fuse_entry_param entry = {
		.ino = cur_inode,
		.attr_timeout = 1,
		.entry_timeout = 1,
	};
	entry.attr = stat_from_inode(ino, cur_inode);

	fuse_log(FUSE_LOG_INFO, "lookup: '%s', of: %ld\n", name, parent);
	print_inode(ino, cur_inode);
	free(ino);
	fuse_reply_entry(req, &entry);
}

// TODO: this increments lookup count. What does that mean?
void fs_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi)
{
	++cur_inode;
	fuse_log(FUSE_LOG_INFO, "create: '%s' (%ld), at: %ld\n", name, cur_inode, parent);

	struct inode ino = {
		.size = 0, .refs = 1, .blocks = 1,
		.uid = 0, .gid = 0, // TODO: I don't actually know who called?
		.mode = mode,
		// .mode = S_IFREG | 0755,
		.atime = time(NULL), .mtime = time(NULL), .ctime = time(NULL),
	};
	strcpy(ino.name, name); // TODO: Do properly
	write_block(req_fd(req), &ino, cur_inode);

	struct fuse_entry_param entry = {
		.ino = cur_inode,
		.attr_timeout = 1,
		.entry_timeout = 1,
	};
	entry.attr = stat_from_inode(&ino, cur_inode);

	print_inode(&ino, cur_inode);

	fuse_reply_create(req, &entry, fi);
}

void fs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	struct inode *inode = read_inode(req_fd(req), ino);
	struct stat reply = stat_from_inode(inode, ino);

	print_inode(inode, ino);
	fuse_log(FUSE_LOG_INFO, "getattr: %ld\n", ino);
	free(inode);

	fuse_reply_attr(req, &reply, 1);
}

void fs_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi)
{
	struct inode *inode = read_inode(req_fd(req), cur_inode);
	inode->mode = attr->st_mode;
	inode->size = attr->st_size;
	inode->mtime = attr->st_mtime;
	inode->ctime = attr->st_ctime;
	write_block(req_fd(req), inode, cur_inode);

	fuse_log(FUSE_LOG_INFO, "to_set %08x\n", to_set);
	struct stat reply = stat_from_inode(inode, ino);
	free(inode);

	fuse_reply_attr(req, &reply, 10);
}

void fs_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	fuse_reply_err(req, 0);
}

void fs_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	fuse_reply_err(req, 0);
}

void fs_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
	fuse_reply_err(req, ENOENT);
}

// TODO: I do not implement a releasedir; will probably need it when/if opening
// does something
void fs_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	fuse_reply_open(req, fi);
}

void fs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	fuse_reply_buf(req, NULL, 0);
}
