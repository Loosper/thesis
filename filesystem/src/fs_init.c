#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#include <fuse_lowlevel.h>

#include "fs_types.h"
#include "io.h"
#include "fs_helpers.h"


// minimal bootstrap:
// ROOT      inode is block #0
// FREE LIST inode is block #1
// #2 and #3 are their data ptrs
// #4 to #n is the free list data
// after that filesystem is ready for use
// the root inode file contains a list of all data inodes (1 block = 1 inode
// for now)
#define BLK_ROOT_INO		0
#define BLK_FREE_LIST_INO	1
#define BLK_ROOT_SCND		2
#define BLK_FREE_LIST_SCND	3
#define BLK_FREE_LIST_DATA	4

#define NUM_INT_ROOT 0
#define NUM_INT_FREE 1

int backing_store = 0;


ssize_t fs_int_pread(size_t ino, void *buf, size_t count, off_t offset);
size_t get_byte_to_block(size_t ino, size_t byte_n, bool internal)
{
	size_t blk_offset_in_file = byte_n / FS_BLOCK_SIZE;
	size_t data_ptr_blk_num;
	size_t ret;
	struct secondary_block *blk_ptr = malloc(FS_BLOCK_SIZE);

	if (internal) {
		assert(ino == NUM_INT_ROOT || ino == NUM_INT_FREE);
		data_ptr_blk_num = (ino) ? BLK_FREE_LIST_SCND : BLK_ROOT_SCND;
	} else {
		struct inode *file = (struct inode *) blk_ptr; // use the same memory
		fs_int_pread(NUM_INT_ROOT, file, FS_BLOCK_SIZE, ino - 1);
		data_ptr_blk_num = file->data_block;
	}

	for (size_t i = 0; i <= blk_offset_in_file / 7; i++) {
		read_block(backing_store, blk_ptr, data_ptr_blk_num);
		data_ptr_blk_num = blk_ptr->blocks[7];
	}

	ret = blk_ptr->blocks[blk_offset_in_file % 7];
	free(blk_ptr);

	return ret;
}

// NOTE: currently only writes up to FS_BLOCK_SIZE
static ssize_t do_read_write(size_t ino, void *buf, size_t count, off_t offset, bool write, bool internal)
{
	uint8_t *data = malloc(FS_BLOCK_SIZE);
	size_t block_n = get_byte_to_block(ino, offset, internal);
	logprintf("block %ld, int %d, ino %ld\n", block_n, internal, ino);

	read_block(backing_store, data, block_n);

	// offset is %-ed because we only want offset within the block
	// and don't try to copy beyond the end of the block TODO: for now
	offset %= FS_BLOCK_SIZE;
	count = MIN(offset + count, FS_BLOCK_SIZE) - offset;

	if (write) {
		memcpy(data + offset, buf, count);
		write_block(backing_store, data, block_n);
	} else {
		memcpy(buf, data + offset, count);
	}

	free(data);
	return count;
}

ssize_t fs_int_pread(size_t ino, void *buf, size_t count, off_t offset)
{
	return do_read_write(ino, buf, count, offset, false, true);
}

ssize_t fs_int_pwrite(size_t ino, void *buf, size_t count, off_t offset)
{
	return do_read_write(ino, buf, count, offset, true, true);
}

ssize_t fs_pread(size_t ino, void *buf, size_t count, off_t offset)
{
	return do_read_write(ino, buf, count, offset, false, false);
}

ssize_t fs_pwrite(size_t ino, void *buf, size_t count, off_t offset)
{
	return do_read_write(ino, buf, count, offset, true, false);
}

static void block_set_used(size_t block_num)
{
	size_t seq_off = block_num % (FS_BLOCK_SIZE * 8);
	uint8_t byte;

	fs_int_pread(1, &byte, 1, seq_off / 8);
	byte |= 1 << (seq_off % 8);
	fs_int_pwrite(1, &byte, 1, seq_off / 8);
}

static void init_blk_zero(size_t blk_num)
{
	uint8_t *blk = malloc(FS_BLOCK_SIZE);
	memset(blk, 0x00, FS_BLOCK_SIZE);
	write_block(backing_store, blk, blk_num);
	free(blk);
}

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
			init_blk_zero(blk_cur_num);
			// last block is bookkeeping, so it doesn't count
			if (i != 7)
				blk_written++;
			blk_cur_num++;
			if (blk_written == blk_req)
				break;
		}

		// write the intermediary block as filled in
		write_data(backing_store, &data_ptr, sizeof(struct secondary_block), blk_ptr_loc);
	}

	// TODO: this is a hack, i can probably hard code it
	// blk_cur_num points to first "unallocated" block
	for (size_t i = 0; i < blk_cur_num; i++) {
		block_set_used(i);
	}
}


// static void write_root_dir()
// {
// 	struct inode root_dir = {
// 		.size = 0, .refs = 1,
// 		.uid = 0, .gid = 0,
// 		.mode = S_IFREG | 0755,
// 		.atime = time(NULL), .mtime = time(NULL), .ctime = time(NULL),
// 	};

// 	fs_pwrite(fs, 2, &root_dir, sizeof(struct inode), FS_BLOCK_SIZE);
// }

void write_root_inode()
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
	// write_root_dir();
}
