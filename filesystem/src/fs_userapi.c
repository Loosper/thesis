#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <fuse_lowlevel.h>

#include "flist.h"
#include "directory.h"
#include "fs_userapi.h"
#include "fs_types.h"
#include "fs_helpers.h"
#include "io.h"
#include "itable.h"

#define ASSERT_GOOD(reply) assert(reply == 0)
#define READ_INODE(ino, inode)					\
	do {							\
		int ret = read_inode(ino, inode);		\
		if (ret < 0) {					\
			ASSERT_GOOD(fuse_reply_err(req, -ret));	\
			return;					\
		}						\
	} while(0)


// TODO: malloc()ed buffers need to be freed by ME after the reply. Function
// doesn't end on reply (if you rememeber)
static inline struct fuse_entry_param make_reply_entry(size_t ino_num, struct inode *inode)
{
	struct fuse_entry_param entry = {
		.ino = ino_num,
		.attr_timeout = 1,
		.entry_timeout = 1,
	};
	entry.attr = stat_from_inode(inode, ino_num);

	return entry;
}

void fs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fuse_entry_param reply;
	struct inode inode;
	size_t ino_num;
	struct inode parent_ino;
	int ret;

	READ_INODE(parent, &parent_ino);

	ino_num = get_direntry(&parent_ino, name);
	ret = read_inode(ino_num, &inode);
	if (ret < 0) {
		logprintf("lookup refused: '%s', of: %ld\n", name, parent);
		ASSERT_GOOD(fuse_reply_err(req, -ret));
		return;
	}

	logprintf("lookup: '%s' (%ld), of: %ld\n", name, ino_num, parent);

	reply = make_reply_entry(ino_num, &inode);
	ASSERT_GOOD(fuse_reply_entry(req, &reply));
}

static int create(struct fuse_entry_param *reply, fuse_ino_t parent, const char *name, mode_t mode)
{
	struct dirent entry;
	struct inode inode;
	struct inode parent_ino;
	int ret;

	make_empty_inode(&inode, mode);
	entry.inode = add_file(&inode);

	strncpy(entry.name, name, MAX_NAME_LEN);

	ret = read_inode(parent, &parent_ino);
	assert(ret == 0);

	ret = add_direntry(&parent_ino, &entry);

	if (ret == 0) {
		*reply = make_reply_entry(entry.inode, &inode);

		logprintf("create: '%s' (%ld), at: %ld\n", name, entry.inode, parent);
	}

	return ret;
}

// TODO: this increments lookup count. What does that mean?
void fs_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi)
{
	struct fuse_entry_param reply;
	int ret = create(&reply, parent, name, mode);

	if (ret < 0) {
		ASSERT_GOOD(fuse_reply_err(req, -ret));
		return;
	}

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
		to_set & FUSE_SET_ATTR_GID
	)
		logprintf("bad to_set for now\n");

	if (to_set & FUSE_SET_ATTR_SIZE) {
		truncate_file(&inode, attr->st_size);
	}

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

void fs_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	if (fi->flags | O_CREAT) {
		logprintf("IGNORING: O_CREAT\n");
	}

	if (fi->flags | O_TRUNC) {
		struct inode inode;
		READ_INODE(ino, &inode);
		truncate_file(&inode, 0);
	}

	ASSERT_GOOD(fuse_reply_open(req, fi));
}


// TODO: these are extremely buggy. They only work on single blocks of data
// (and should work on any length) and will probably break massively on EOF
void fs_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
	size_t size, off_t off, struct fuse_file_info *fi)
{
	struct inode inode;
	READ_INODE(ino, &inode);

	logprintf("write %ld at %ld for %ld bytes\n", ino, off, size);
	ssize_t ret = pwrite_ino(&inode, buf, size, off);
	ASSERT_GOOD(fuse_reply_write(req, ret));
}

void fs_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	struct inode inode;
	char *buf = malloc(size);
	READ_INODE(ino, &inode);

	logprintf("read %ld at %ld for %ld bytes\n", ino, off, size);
	ssize_t ret = pread_ino(&inode, buf, size, off);

	ASSERT_GOOD(fuse_reply_buf(req, buf, ret));
	free(buf);
}

void fs_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct inode inode;
	READ_INODE(parent, &inode);

	int ret = rm_direntry(&inode, name);

	fuse_reply_err(req, ret);
}

void fs_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	// TODO: even though this will defo work, is it correct?
	fs_unlink(req, parent, name);
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

void fs_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode)
{
	struct fuse_entry_param reply;
	struct dirent entry;
	struct inode new_inode;
	struct inode dir_inode;
	int ret;

	make_empty_inode(&new_inode, mode | S_IFDIR);
	entry.inode = add_dir(&new_inode);
	strncpy(entry.name, name, MAX_NAME_LEN);

	READ_INODE(parent, &dir_inode);

	ret = add_direntry(&dir_inode, &entry);
	if (ret < 0) {
		ASSERT_GOOD(fuse_reply_err(req, -ret));
		return;
	}

	logprintf("mkdir: '%s' (%ld), at: %ld\n", name, entry.inode, parent);

	reply = make_reply_entry(entry.inode, &new_inode);
	ASSERT_GOOD(fuse_reply_entry(req, &reply));
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
	size_t reply_size;
	void *reply;

	logprintf("list %ld, step %ld\n", ino, off);

	READ_INODE(ino, &inode);

	entry = list_dir(&inode, off);
	// we're done
	if (entry.inode == 0) {
		ASSERT_GOOD(fuse_reply_buf(req, NULL, 0));
		return;
	}

	READ_INODE(entry.inode, &inode);
	statbuf = stat_from_inode(&inode, entry.inode);

	// get the size we need for the reply
	reply_size = fuse_add_direntry(req, NULL, 0, entry.name, &statbuf, off + 1);
	reply = malloc(reply_size);
	// actually make the reply
	fuse_add_direntry(req, reply, reply_size, entry.name, &statbuf, off + 1);

	ASSERT_GOOD(fuse_reply_buf(req, reply, reply_size));
}
