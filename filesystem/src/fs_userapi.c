#include <assert.h>
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

#define ASSERT_GOOD(reply) assert(reply == 0)

void fs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct inode inode;
	size_t ino_num;
	int ret;

	ino_num = get_direntry(parent, name);
	ret = read_inode(ino_num, &inode);
	if (ret < 0) {
		logprintf("lookup refused: '%s', of: %ld\n", name, parent);
		ASSERT_GOOD(fuse_reply_err(req, -ret));
		return;
	}

	logprintf("lookup: '%s' (%ld), of: %ld\n", name, ino_num, parent);
	struct fuse_entry_param entry = {
		.ino = ino_num,
		.attr_timeout = 1,
		.entry_timeout = 1,
	};
	entry.attr = stat_from_inode(&inode, ino_num);

	ASSERT_GOOD(fuse_reply_entry(req, &entry));
}

// TODO: this increments lookup count. What does that mean?
void fs_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi)
{
	struct inode inode = {
		.size = 0, .refs = 1,
		.uid = 0, .gid = 0, // TODO: I don't actually know who called?
		.mode = mode,
		// .mode = S_IFREG | 0755,
		.atime = time(NULL), .mtime = time(NULL), .ctime = time(NULL),
	};

	struct dirent entry;
	entry.inode = add_file(&inode);
	strncpy(entry.name, name, MAX_NAME_LEN);

	int ret = add_direntry(parent, &entry);
	if (ret < 0) {
		ASSERT_GOOD(fuse_reply_err(req, -ret));
		return;
	}
	fuse_log(FUSE_LOG_INFO, "create: '%s' (%ld), at: %ld\n", name, entry.inode, parent);

	struct fuse_entry_param reply = {
		.ino = entry.inode,
		.attr_timeout = 1,
		.entry_timeout = 1,
	};
	reply.attr = stat_from_inode(&inode, entry.inode);

	ASSERT_GOOD(fuse_reply_create(req, &reply, fi));
}

void fs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	struct inode inode;
	if (read_inode(ino, &inode) == -1) {
		logprintf("getattr refused %ld\n", ino);
		ASSERT_GOOD(fuse_reply_err(req, ENOENT));
		return;
	}
	struct stat reply = stat_from_inode(&inode, ino);

	fuse_log(FUSE_LOG_INFO, "getattr: %ld\n", ino);

	ASSERT_GOOD(fuse_reply_attr(req, &reply, 1));
}

void fs_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi)
{
	struct inode inode;
	int ret = read_inode(ino, &inode);
	if (ret < 0) {
		ASSERT_GOOD(fuse_reply_err(req, -ret));
		return;
	}
	logprintf("setattr of %ld,\n", ino);

	if (
		to_set & FUSE_SET_ATTR_MODE ||
		to_set & FUSE_SET_ATTR_UID ||
		to_set & FUSE_SET_ATTR_GID ||
		to_set & FUSE_SET_ATTR_SIZE
	)
		logprintf("bad to_set for now\n");

	if (to_set & FUSE_SET_ATTR_MTIME) {
		inode.mtime = attr->st_mtime;
		if (to_set & FUSE_SET_ATTR_MTIME_NOW)
			inode.mtime = time(NULL);
	}
	if (to_set & FUSE_SET_ATTR_ATIME) {
		inode.atime = attr->st_atime;
		if (to_set & FUSE_SET_ATTR_ATIME_NOW)
			inode.atime = time(NULL);
	}
	if (to_set & FUSE_SET_ATTR_CTIME)
		inode.ctime = attr->st_ctime;

	write_inode(ino, &inode);

	struct stat reply = stat_from_inode(&inode, ino);
	ASSERT_GOOD(fuse_reply_attr(req, &reply, 1));
}

void fs_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	ASSERT_GOOD(fuse_reply_err(req, 0));
}

void fs_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	ASSERT_GOOD(fuse_reply_err(req, 0));
}

void fs_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
	ASSERT_GOOD(fuse_reply_err(req, ENOENT));
}

// TODO: I do not implement a releasedir; will probably need it when/if opening
// does something
void fs_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	ASSERT_GOOD(fuse_reply_open(req, fi));
}

void fs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	struct inode inode;
	struct dirent entry;
	struct stat statbuf;

	logprintf("list %ld at %ld\n", off, ino);

	entry = list_dir(ino, off);
	// we're done
	if (entry.inode == 0) {
		ASSERT_GOOD(fuse_reply_buf(req, NULL, 0));
		return;
	}

	read_inode(entry.inode, &inode);
	statbuf = stat_from_inode(&inode, entry.inode);

	size_t req_size = fuse_add_direntry(req, NULL, 0, entry.name, &statbuf, off);

	ASSERT_GOOD(fuse_reply_buf(req, NULL, 0));
}
