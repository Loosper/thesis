/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) Boyan Karatotev */
#ifndef _FS_TYPES_H_
#define _FS_TYPES_H_

#include <time.h>
#include <sys/types.h>

#include "btree.h"

#define FS_SECTOR_SIZE		4096
#define CHECKSUM_LEN		4
// TODO: make dynamic
// this is the operational unit
#define FS_BLOCK_SIZE		(FS_SECTOR_SIZE - CHECKSUM_LEN)

#define NODESIZE		FS_BLOCK_SIZE
#define SUPERBLOCK_BLK		1
#define PREALLOC_AMOUNT		20

extern int checksum_fd;
extern struct superblock superblock;
// this is for debug, but it's nice to have access anywhere
extern int debug;

extern size_t (*blk_allocator)();
extern size_t (*blk_deallocator)();
extern gfp_t dummy_gfp;

struct fs_opts {
	char *mountpoint;
	char *deva;
	char *devb;
	int debug;
	int to_help;
	int foreground;
};


// TODO: types are bad
// TODO: checksum this separately of the block?
struct inode {
	size_t size;
	size_t refs;
	struct btree_head64 data_tree;
	// we reserve blk 0 to mean "uninitialised"
	size_t blk; // blk where this is stored (short circuit the itable)
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t atime;
	time_t mtime;
	time_t ctime;
};

#define MAX_NAME_LEN 128
struct dirent {
	size_t inode;
	char name[MAX_NAME_LEN];
};

// NOTE: not a superblock
struct fs_metadata {
	int deva;
	int devb;
};
extern struct fs_metadata *fs_settings;

// TODO: HFS+ has a field here saying which is the next free inode num
struct superblock {
	// the block number of their inodes
	size_t itable_blk;
	size_t flist_blk;
};

#endif
