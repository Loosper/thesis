#include <stdlib.h>
#include <string.h>

#include <linux/fs.h>
#include <sys/ioctl.h>

#include <fuse_lowlevel.h>

#include "fs_types.h"
#include "io.h"
#include "fs_helpers.h"

#define FREE_LIST_DATA_BLK (4)
#define FREE_LIST_INODE_BLK (2)


void block_set_used(struct filesystem *fs, size_t block_num)
{
	size_t seq_addr = block_num / (FS_BLOCK_SIZE * 8);
	size_t seq_off  = block_num % (FS_BLOCK_SIZE * 8);
	size_t list_blk_num = FREE_LIST_DATA_BLK;
	size_t cur_addr = 0;
	uint8_t *data = malloc(FS_BLOCK_SIZE);
	struct secondary_block *blk_ptr = malloc(FS_BLOCK_SIZE);

	for (size_t i = 0; i <= seq_addr / 7; i++) {
		read_block(fs->backing_store, blk_ptr, list_blk_num);
		list_blk_num = blk_ptr->blocks[7];
	}

	read_block(fs->backing_store, data, blk_ptr->blocks[seq_addr % 7]);
	((uint8_t *)data)[seq_off / 8] |= 1 << (seq_off % 8);
	write_block(fs->backing_store, data, blk_ptr->blocks[seq_addr % 7]);

	free(blk_ptr);
	free(data);
}

// FIXME: jfc this is bad code
static void write_empty_blocks_file(struct filesystem *fs)
{
	size_t list_size;
	// TODO: I shall assume a sector size of 512. This probably isn't true
	ioctl(fs->backing_store, BLKGETSIZE, &list_size);

	// we use 1 bit per block, a byte has 8
	list_size /= sizeof(uint8_t);
	size_t list_blk_num = list_size / FS_BLOCK_SIZE;
	size_t last_blk_used;
	fuse_log(FUSE_LOG_INFO, "free list size %ld\n", list_size);
	fuse_log(FUSE_LOG_INFO, "free list blks %ld\n", list_blk_num);

	struct inode free_list = {
		.size = list_size,
		.data_block = 3
	};

	// TODO: technically this is writing to the inode file. the indoe_blk num is the file's spot
	write_data(fs->backing_store, &free_list, sizeof(struct inode), FREE_LIST_INODE_BLK);

	size_t initial_blk = FREE_LIST_DATA_BLK;
	struct secondary_block data = {
		.used = 0
	};

	for (size_t i = 0, block_idx = 0; i < list_blk_num; block_idx = (block_idx + 1) % 8) {
		size_t block_num = initial_blk + block_idx + 1;
		last_blk_used = block_num;
		data.used++;
		data.blocks[block_idx] = block_num;

		// fuse_log(FUSE_LOG_INFO, "allocate %ld at %ld\n", data.blocks[block_idx], initial_blk);

		// don't increment allocated counter cos this one doesn't count
		if (block_idx == 7) {
			write_block(fs->backing_store, &data, initial_blk);
			initial_blk = block_num;
			data.used = 0;
		} else {
			uint8_t *blk = malloc(FS_BLOCK_SIZE);
			memset(blk, 0x00, FS_BLOCK_SIZE);
			write_block(fs->backing_store, blk, block_num);
			free(blk);
			i++;
		}
	}

	// TODO: this is a hack, i can probably hard code it
	for (size_t i = 0; i < last_blk_used; i++) {
		block_set_used(fs, i);
	}
 }

// inode file is file #1. This file has a list of all inodes. Inode 1 is the
// free blocks file, which contains a list of all free blocks. Inode 2 is
// optional. It has a list of free inodes
void write_root_inode(struct filesystem *fs)
{
	struct inode root = {
		// NOTE: only these matter for a WAFL style root inode
		.size = FS_SECTOR_SIZE,
		.data_block = 1
	};

	struct secondary_block data = {
		.used = 1,
		.blocks = {FREE_LIST_INODE_BLK}
	};

	write_data(fs->backing_store, &root, sizeof(struct inode), 0);
	write_data(fs->backing_store, &data, sizeof(struct secondary_block), 1);

	write_empty_blocks_file(fs);
}
