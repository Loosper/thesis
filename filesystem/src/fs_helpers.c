/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) Boyan Karatotev */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include <fuse_lowlevel.h>

#include "fs_types.h"
#include "io.h"
#include "fs_helpers.h"


void make_empty_inode(struct inode *inode, mode_t mode)
{
	inode->size = 0;
	inode->refs = 1;
	// TODO: I don't actually know who called?
	inode->uid = 0;
	inode->gid = 0;
	inode->blk = 0;
	inode->mode = mode;
	inode->atime = time(NULL);
	inode->mtime = time(NULL);
	inode->ctime = time(NULL);

	btree_init64(&inode->data_tree);
}

struct stat stat_from_inode(struct inode *inode, size_t num)
{
	struct stat stat = {
		// .st_dev = 1,
		// .st_nlink = 1,
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

void init_blk_zero(size_t blk_num)
{
	uint8_t *blk = malloc(FS_BLOCK_SIZE);
	memset(blk, 0x00, FS_BLOCK_SIZE);
	write_block(blk, blk_num);
	free(blk);
}

void print_inode(struct inode *ino, size_t num)
{
	fuse_log(FUSE_LOG_INFO,
		"inode (%d):\n"
		"\tsize: %ld\n"
		"\trefs: %ld\n"
		"\tnum: %ld\n"
		"\tuid/gid: %d/%d\n"
		"\tmode: %x\n"
		"\tatime: %ld\n"
		"\tmtime: %ld\n"
		"\tctime: %ld\n",
		num,
		// ino->name,
		ino->size, ino->refs, ino->blk, ino->uid, ino->gid,
		ino->mode, ino->atime, ino->mtime, ino->ctime
	);
}

void print_block(size_t block_num)
{
	uint8_t *data = malloc(FS_BLOCK_SIZE);
	void *to_free = data;
	read_block(data, block_num);

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
