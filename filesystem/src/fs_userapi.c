#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <fuse_lowlevel.h>

#include "fs_userapi.h"
#include "fs_types.h"
#include "fs_helpers.h"
#include "io.h"


int cur_inode = 1;
void fs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	if (cur_inode < 2) {
		fuse_reply_err(req, ENOENT);
		return;
	}

	struct inode *ino = read_inode(req_fd(req), cur_inode);
	fuse_log(FUSE_LOG_INFO, "lookup: '%s', of: %ld\n", name, parent);
	struct fuse_entry_param entry = {
		.ino = cur_inode,
		// TODO: not critical
		// .generation = ++generation,
		.attr_timeout = 1,
		.entry_timeout = 1,
	};
	entry.attr = stat_from_inode(ino, cur_inode);

	free(ino);
	fuse_reply_entry(req, &entry);
}

// TODO: this increments lookup count. What does that mean?
void fs_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi)
{
	++cur_inode;
	fuse_log(FUSE_LOG_INFO, "create: '%s' (%ld), at: %ld\n", name, cur_inode, parent);

	struct inode ino = {
		.size = 0, .refs = 1,
		.uid = 0, .gid = 0, // TODO: I don't actually know who called?
		.mode = mode,
		// .mode = S_IFREG | 0755,
		.atime = time(NULL), .mtime = time(NULL), .ctime = time(NULL),
	};
	// strcpy(ino.name, name); // TODO: Do properly
	// write_block(req_fd(req), &ino, cur_inode);

	struct fuse_entry_param entry = {
		.ino = cur_inode,
		.attr_timeout = 1,
		.entry_timeout = 1,
	};
	entry.attr = stat_from_inode(&ino, cur_inode);

	fuse_reply_create(req, &entry, fi);
}

void fs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	// ino = REAL_INODE(ino);
	struct inode *inode = read_inode(req_fd(req), ino);
	struct stat reply = stat_from_inode(inode, ino);

	fuse_log(FUSE_LOG_INFO, "getattr: %ld\n", ino);
	free(inode);

	fuse_reply_attr(req, &reply, 1);
}

void fs_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi)
{
	struct inode *inode = read_inode(req_fd(req), cur_inode);
	// inode->mode = attr->st_mode;
	// inode->size = attr->st_size;
	inode->atime = attr->st_atime;
	inode->mtime = attr->st_mtime;
	// write_block(req_fd(req), inode, cur_inode);

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
