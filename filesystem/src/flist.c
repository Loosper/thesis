#include <stdlib.h>

#include <linux/fs.h>
#include <sys/ioctl.h>

#include "io.h"
#include "fs_helpers.h"
#include "fs_types.h"
#include "flist.h"


static inline struct inode get_flist_inode()
{
	struct inode inode;
	read_data(&inode, sizeof(inode), superblock.flist_blk);
	return inode;
}

// TODO: if I can short circuit the allocator to not read the free list, I can
// skip this
static size_t dummy_allocate()
{
	static size_t blk_cur_num = BLK_FREE_LIST_SCND;

	blk_cur_num++;
	return blk_cur_num;
}

// this is first as it substantially reduces bootrstrap efforts
// after that we can mostly use all the normal routines
size_t gen_flist()
{
	size_t list_size;
	// TODO: I shall assume a sector size of 512. This probably isn't true
	ioctl(backing_store, BLKGETSIZE, &list_size);
	// we use 1 bit per block, a byte has 8
	list_size /= sizeof(uint8_t);

	struct secondary_block scnd = {0};
	// put the inode here (no allocator yet)
	size_t location = SUPERBLOCK_BLK + 1;
	struct inode free_list = {
		.size = list_size,
		.data_block = SUPERBLOCK_BLK + 2 // just after inode
	};
	size_t blk_req = list_size / FS_BLOCK_SIZE;
	size_t blk_cur_num;

	fuse_log(FUSE_LOG_INFO, "free list size %ld\n", list_size);
	fuse_log(FUSE_LOG_INFO, "free list blks %ld\n", blk_req);
	write_data(&free_list, sizeof(free_list), location);
	write_data(&scnd, sizeof(scnd), SUPERBLOCK_BLK + 2);

	blk_cur_num = file_add_space(&free_list, blk_req, &dummy_allocate);

	// TODO: this is a hack. Since we are making the free list, we can't
	// call allocate yet. We can do it postfactum though. Can probably be
	// hardcoded tbh. Should be linear (all blocks from 0 to however many)
	for (size_t i = 0; i < blk_cur_num; i++) {
		allocate_block();
	}

	return location;
}

// TODO: this is dumb as fuck, but it works for now!
// DOES NOT initialize blocks!!! It's caller responsibility
size_t allocate_block()
{
	struct inode flist = get_flist_inode();
	uint8_t *data = malloc(FS_BLOCK_SIZE);
	size_t bit_num;

	for (bit_num = 0; ; bit_num++) {
		size_t cur_blk = bit_num / (FS_BLOCK_SIZE * 8);
		size_t file_offset = cur_blk * FS_BLOCK_SIZE;

		size_t bit_index_in_blk = bit_num % FS_BLOCK_SIZE;
		size_t index_in_block = bit_index_in_blk / 8;
		uint8_t bitmask = 1 << (bit_index_in_blk % 8);

		// refresh block when we reach its end
		if (bit_num == cur_blk * (FS_BLOCK_SIZE * 8))
			pread_ino(&flist, data, FS_BLOCK_SIZE, file_offset);

		// it's allocated
		if (data[index_in_block] & bitmask)
			continue;

		// allocate and quit
		data[index_in_block] |= bitmask;
		pwrite_ino(&flist, data, FS_BLOCK_SIZE, file_offset);
		break;
	}

	free(data);
	// logprintf("allocated %ld\n", bit_num);

	// TODO: not sure if inefficient, but will reduce A LOT of errorS
	init_blk_zero(bit_num);
	return bit_num;
}