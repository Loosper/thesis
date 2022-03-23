#ifndef _FS_TYPES_H_
#define _FS_TYPES_H_

#include <time.h>
#include <sys/types.h>

#include "btree.h"

#define FS_SECTOR_SIZE 512
// TODO: make dynamic
#define FS_BLOCK_SIZE FS_SECTOR_SIZE
#define NODESIZE FS_BLOCK_SIZE
// #define FS_BLOCK_SIZE (1 * FS_SECTOR_SIZE)
#define SUPERBLOCK_BLK		1
#define PREALLOC_AMOUNT 20

extern int backing_store;
extern struct superblock superblock;
// this is for debug, but it's nice to have access anywhere
extern int debug;

extern size_t (*blk_allocator)();
extern size_t (*blk_deallocator)();
extern gfp_t dummy_gfp;

struct fs_opts {
	char *mountpoint;
	char *backing_store;
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
	int backing_store;
};

// TODO: HFS+ has a field here saying which is the next free inode num
struct superblock {
	// the block number of their inodes
	size_t itable_blk;
	size_t flist_blk;
};

#endif
