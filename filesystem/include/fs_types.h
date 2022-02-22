#ifndef _FS_TYPES_H_
#define _FS_TYPES_H_

#include <time.h>
#include <sys/types.h>


#define FS_SECTOR_SIZE 512
// TODO: make dynamic
#define FS_BLOCK_SIZE FS_SECTOR_SIZE
// #define FS_BLOCK_SIZE (1 * FS_SECTOR_SIZE)

// minimal bootstrap:
// ROOT      inode is block #0
// FREE LIST inode is block #1
// #2 and #3 are their data ptrs
// #4 to #n is the free list data
// after that filesystem is ready for use
// the root inode file contains a block for bookkeeping info (number of indoes)
// followed by a list of all data inodes (1 block = 1 inode for now)
#define BLK_ROOT_INO		0
#define BLK_FREE_LIST_INO	1
#define BLK_ROOT_SCND		2
#define BLK_FREE_LIST_SCND	3
#define BLK_FREE_LIST_DATA	4

#define SUPERBLOCK_BLK		1

#define NUM_INT_ROOT 0
#define NUM_INT_FREE 1


extern int backing_store;
extern struct superblock superblock;


struct fs_opts {
	char *mountpoint;
	char *backing_store;
	int debug;
	int to_help;
	int foreground;
};

// simplest way to use this: first 7 point to direct data. 8th points to
// another one of these. Ad infinitum.
// FIXME: ^ is FAT style (i.e EXTREMELY slow)
#define SCND_CAPACITY 8
struct secondary_block {
	int used;
	// TODO: ???
	size_t blocks[FS_SECTOR_SIZE / sizeof(size_t)];
};

// TODO: types are bad
// TODO: checksum this separately of the block?
struct inode {
	size_t size;
	size_t refs;
	size_t data_block;
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

// TODO: put free list/inode list pointers here. This way the macros don't have
// to exit this file and can be changed more easily

// TODO: HFS+ has a field here saying which is the next free inode num

// NOTE: not a superblock
struct fs_metadata {
	int backing_store;
};

struct superblock {
	// the block number of their inodes
	size_t itable_blk;
	size_t flist_blk;
};

#endif
