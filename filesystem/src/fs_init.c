#include <stdlib.h>
#include <string.h>

#include <linux/fs.h>
#include <sys/ioctl.h>

#include "fs_helpers.h"
#include "fs_types.h"
#include "io.h"

int backing_store = 0;

// TODO: if I can short circuit the allocator to not read the free list, I can
// skip this
static size_t dummy_allocate()
{
	static size_t blk_cur_num = BLK_FREE_LIST_SCND;

	blk_cur_num++;
	return blk_cur_num;
}

static void write_empty_blocks_file()
{
	size_t list_size;
	// TODO: I shall assume a sector size of 512. This probably isn't true
	ioctl(backing_store, BLKGETSIZE, &list_size);
	// we use 1 bit per block, a byte has 8
	list_size /= sizeof(uint8_t);

	struct secondary_block scnd = {0};
	struct inode free_list = {
		.size = list_size,
		.data_block = BLK_FREE_LIST_SCND
	};
	size_t blk_req = list_size / FS_BLOCK_SIZE;
	size_t blk_cur_num;

	fuse_log(FUSE_LOG_INFO, "free list size %ld\n", list_size);
	fuse_log(FUSE_LOG_INFO, "free list blks %ld\n", blk_req);
	write_data(backing_store, &free_list, sizeof(free_list), BLK_FREE_LIST_INO);
	write_data(backing_store, &scnd, sizeof(scnd), BLK_FREE_LIST_SCND);

	blk_cur_num = file_add_space(BLK_FREE_LIST_SCND, blk_req, &dummy_allocate);


	// TODO: this is a hack. Since we are making the free list, we can't
	// call allocate yet.  We can do it postfactum though. Can probably be
	// hardcoded tbh
	for (size_t i = 0; i < blk_cur_num; i++) {
		allocate_block();
	}

	// do this for the root inode too
	for (size_t i = 0; i < BLK_FREE_LIST_DATA; i++) {
		allocate_block();
	}
}

static void allocate_root_file()
{
	struct secondary_block data;
	struct inode inode;
	size_t count = 1;

	read_data(backing_store, &inode, sizeof(struct inode), BLK_ROOT_INO);

	// the internal data (number of files)
	inode.size = FS_BLOCK_SIZE;
	data.blocks[0] = allocate_block();
	data.used = 1;
	// init with a single file to skip inode 0
	init_blk_zero(data.blocks[0]);
	write_data(backing_store, &count, sizeof(count), data.blocks[0]);

	write_data(backing_store, &inode, sizeof(struct inode), BLK_ROOT_INO);
	write_data(backing_store, &data, sizeof(struct secondary_block), BLK_ROOT_SCND);
}

static void write_root_dir()
{
	struct inode root_dir;
	// TODO: it's double on purpose. Simplest way to get rid of inode 0.
	// Do I need to store something there?
	make_empty_inode(&root_dir, S_IFDIR | 0777);
}

static void write_root_inode()
{
	struct inode root = {
		// NOTE: only these matter for a WAFL style root inode
		.size = 0,
		.data_block = BLK_ROOT_SCND
	};

	struct secondary_block data = {0};

	write_data(backing_store, &root, sizeof(root), BLK_ROOT_INO);
	write_data(backing_store, &data, sizeof(data), BLK_ROOT_SCND);
}

void fs_init(struct filesystem *fs)
{
	backing_store = fs->backing_store;
	write_root_inode();
	write_empty_blocks_file();
	allocate_root_file();
	write_root_dir();
}
