#ifndef _FS_TYPES_H_
#define _FS_TYPES_H_

#include <time.h>
#include <sys/types.h>


#define FS_SECTOR_SIZE 512
// TODO: make dynamic
#define FS_BLOCK_SIZE FS_SECTOR_SIZE
// #define FS_BLOCK_SIZE (1 * FS_SECTOR_SIZE)


struct fs_opts {
	char *mountpoint;
	char *backing_store;
	int debug;
	int to_help;
	int foreground;
};

enum inode_type {INODE_FILE, INODE_DIR};

// simplest way to use this: first 7 point to direct data. 8th points to
// another one of these. Ad infinitum.
// FIXME: ^ is FAT style (i.e EXTREMELY slow)
struct secondary_block {
	int used;
	// TODO: ???
	size_t blocks[FS_SECTOR_SIZE / sizeof(size_t)];
};

// TODO: types are bad
struct inode {
	// char name[128];
	size_t size;
	size_t refs;
	size_t data_block;
	uid_t uid;
	gid_t gid;
	int type;
	mode_t mode;
	time_t atime;
	time_t mtime;
	time_t ctime;
};

struct filesystem {
	int backing_store;
};


#endif
