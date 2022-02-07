#include <stdlib.h>
#include <string.h>

#include <linux/fs.h>
#include <sys/ioctl.h>

#include "fs_helpers.h"
#include "fs_types.h"
#include "io.h"

int backing_store = 0;


static void write_empty_blocks_file()
{
	size_t list_size;
	// TODO: I shall assume a sector size of 512. This probably isn't true
	ioctl(backing_store, BLKGETSIZE, &list_size);
	// we use 1 bit per block, a byte has 8
	list_size /= sizeof(uint8_t);

	struct inode free_list = {
		.size = list_size,
		.data_block = BLK_FREE_LIST_SCND
	};
	size_t blk_req = list_size / FS_BLOCK_SIZE;
	size_t blk_written = 0;
	size_t blk_cur_num = BLK_FREE_LIST_SCND;
	size_t blk_ptr_loc;

	fuse_log(FUSE_LOG_INFO, "free list size %ld\n", list_size);
	fuse_log(FUSE_LOG_INFO, "free list blks %ld\n", blk_req);
	write_data(backing_store, &free_list, sizeof(struct inode), BLK_FREE_LIST_INO);

	while (blk_written < blk_req) {
		struct secondary_block data_ptr = {.used = 0};
		// place the intermediary pointer on the first available block
		blk_ptr_loc = blk_cur_num++;

		// fill in the ptr struct ("allocate" blocks in a sense)
		for (int i = 0; i <= 7; i++) {
			data_ptr.used++;
			data_ptr.blocks[i] = blk_cur_num;
			// logprintf("used %ld\n", blk_cur_num);
			init_blk_zero(blk_cur_num);
			// last block is bookkeeping, so it doesn't count
			if (i != 7)
				blk_written++;
			blk_cur_num++;
			if (blk_written == blk_req)
				break;
		}

		// logprintf("written %ld\n", blk_ptr_loc);
		// write the intermediary block as filled in
		write_data(backing_store, &data_ptr, sizeof(struct secondary_block), blk_ptr_loc);
	}

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
	struct secondary_block *data = malloc(FS_BLOCK_SIZE);
	data->used = 0;
	size_t data_blk_num = BLK_ROOT_SCND;

	// TODO: allocate 10 static blocks for simplicity
	for (int allocated = 0; allocated < 10; allocated++) {
		data->blocks[data->used] = allocate_block();
		// logprintf("root needs %ld\n", data->blocks[data->used]);
		init_blk_zero(data->blocks[data->used]);
		data->used++;

		if (data->used >= 8) {
			write_block(backing_store, data, data_blk_num);
			// get the number of the next one, reset and start over
			data_blk_num = data->blocks[7];
			data->used = 0;
		}
	}
	// flush out any incomplete ptr blocks
	write_block(backing_store, data, data_blk_num);

	free(data);
}

static void write_root_dir()
{
	struct inode root_dir;
	// TODO: it's double on purpose. Simplest way to get rid of inode 0.
	// Do I need to store something there?
	make_empty_inode(&root_dir, S_IFDIR | 0777);
	make_empty_inode(&root_dir, S_IFDIR | 0777);
}

static void write_root_inode()
{
	struct inode root = {
		// NOTE: only these matter for a WAFL style root inode
		.size = 0,
		.data_block = BLK_ROOT_SCND
	};

	struct secondary_block data = {
		.used = 0,
	};

	write_data(backing_store, &root, sizeof(struct inode), BLK_ROOT_INO);
	write_data(backing_store, &data, sizeof(struct secondary_block), BLK_ROOT_SCND);
}

void fs_init(struct filesystem *fs)
{
	backing_store = fs->backing_store;
	write_root_inode();
	write_empty_blocks_file();
	allocate_root_file();
	write_root_dir();
}
