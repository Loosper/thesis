#ifndef _FS_TYPES_H_
#define _FS_TYPES_H_

#include <time.h>
#include <sys/types.h>


struct fs_opts {
	char *mountpoint;
	char *backing_store;
	int debug;
	int to_help;
	int foreground;
};

enum inode_type {INODE_FILE, INODE_DIR};

// TODO: types are bad
struct inode {
	char name[128];
	size_t size;
	size_t refs;
	size_t blocks;
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
