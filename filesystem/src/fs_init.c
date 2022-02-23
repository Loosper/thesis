#include <stdlib.h>
#include <string.h>

#include "directory.h"
#include "flist.h"
#include "fs_helpers.h"
#include "fs_types.h"
#include "itable.h"
#include "io.h"

int backing_store = 0;
struct superblock superblock;



// static void allocate_root_file()
// {
// 	struct secondary_block data;
// 	struct inode inode;
// 	size_t count = 1;

// 	read_data(&inode, sizeof(struct inode), BLK_ROOT_INO);

// 	// the internal data (number of files)
// 	inode.size = FS_BLOCK_SIZE;
// 	data.blocks[0] = allocate_block();
// 	data.used = 1;
// 	// init with a single file to skip inode 0
// 	init_blk_zero(data.blocks[0]);
// 	write_data(&count, sizeof(count), data.blocks[0]);

// 	write_data(&inode, sizeof(struct inode), BLK_ROOT_INO);
// 	write_data(&data, sizeof(struct secondary_block), BLK_ROOT_SCND);
// }

static void write_root_dir()
{
	struct inode root_dir;
	// TODO: it's double on purpose. Simplest way to get rid of inode 0.
	// Do I need to store something there?
	make_empty_inode(&root_dir, S_IFDIR | 0777, allocate_block());

	add_dir(&root_dir);
}

void fs_init(struct fs_metadata *fs)
{
	backing_store = fs->backing_store;
	superblock.flist_blk = gen_flist();
	superblock.itable_blk = gen_itable();
	write_root_dir();

	// write the superblock
	write_data(&superblock, sizeof(superblock), 0);
}
