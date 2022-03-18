#include <stdlib.h>
#include <string.h>

#include "directory.h"
#include "flist.h"
#include "fs_helpers.h"
#include "fs_types.h"
#include "itable.h"
#include "io.h"

int backing_store = -1;
struct superblock superblock;


static void write_root_dir()
{
	struct inode root_dir;

	make_empty_inode(&root_dir, S_IFDIR | 0777);
	add_dir(&root_dir);
}

void fs_init(struct fs_metadata *fs)
{
	backing_store = fs->backing_store;
	for (int i = 0; i <= SUPERBLOCK_BLK; i++) {
		init_blk_zero(i);
	}

	superblock.flist_blk = gen_flist();
	superblock.itable_blk = gen_itable();
	init_itable();
	write_root_dir();

	// write the superblock
	write_data(&superblock, sizeof(superblock), SUPERBLOCK_BLK);
}
