#include <string.h>

#include "fs_types.h"
#include "io.h"


void write_root_inode(struct filesystem *fs)
{
	struct inode root = {
		.size = 512,
		.refs = 2, // TODO: why?
		.uid = 0,
		.gid = 0,
		.type = INODE_DIR,
		.mode = S_IFDIR | 0777,
		.atime = time(NULL),
		.mtime = time(NULL),
		.ctime = time(NULL),
	};
	// fuse_log(FUSE_LOG_INFO, "hi? %x\n", S_IFDIR | 0755);
	strcpy(root.name, "root");

	write_data(fs->backing_store, &root, sizeof(struct inode), 1);
}
