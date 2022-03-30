#include <stdlib.h>
#include <string.h>

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

// minimal bootstrap: superblock is on block 1. After that everything is laid
// out "dynamically" on a rolling basis. Currently that's first the inode of
// the flist and then the flist itself, after that the proper allocator is to
// be used. It is expected that the first block after that will contain root
// inode and so on
// TODO: if I can short circuit the allocator to not read the free list, I can
// skip this
static size_t dummy_allocate()
{
	static size_t blk_cur_num = SUPERBLOCK_BLK;

	blk_cur_num++;
	init_blk_zero(blk_cur_num);
	return blk_cur_num;
}

// this is first as it substantially reduces bootrstrap efforts
// after that we can mostly use all the normal routines
size_t gen_flist()
{
	size_t list_size;
	// TODO: check devies are identical
	int ret = ioctl(fs_settings->deva, BLKGETSIZE, &list_size);
	// we use 1 bit per block, a byte has 8
	// TODO: I shall assume a sector size of 512. This probably isn't true
	list_size /= sizeof(uint8_t) * (FS_SECTOR_SIZE / 512);

	// put the inode here (no allocator yet)
	size_t location = dummy_allocate();
	struct inode free_list;
	size_t blk_req = list_size / FS_BLOCK_SIZE;
	size_t blk_cur_num;

	fuse_log(FUSE_LOG_INFO, "free list size %ld\n", list_size);
	fuse_log(FUSE_LOG_INFO, "free list blks %ld\n", blk_req);

	blk_allocator = dummy_allocate;
	make_empty_inode(&free_list, S_IFREG);
	free_list.blk = location;
	write_data(&free_list, sizeof(free_list), free_list.blk);

	blk_cur_num = file_add_space(&free_list, blk_req);
	blk_allocator = allocate_block;

	// ++ becuase above returns last one in use. We now want a count now
	blk_cur_num++;

	// this will allocate only the blocks the free list itself uses. We
	// expect everyone else to use the allocation utilities
	// round up bits to nearest mutliple of 8 and convert to bytes
	size_t bits_len = (((blk_cur_num / 8 + 1) * 8) & ~0x7) / 8;
	uint8_t *bits = malloc(bits_len);
	uint8_t last_byte = 0;

	// make a bit mask for the last few bits
	for (size_t i = 0; i < blk_cur_num % 8; i++) {
		last_byte |= 1 << i;
	}
	memset(bits, 0xff, bits_len - 1);
	bits[bits_len - 1] = last_byte;

	pwrite_ino(&free_list, bits, bits_len, 0);
	free(bits);

	return location;
}

// TODO: this is dumb as fuck, but it works for now!
// the byte offset is backwards too (as in it start from LSB and not MSB)
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
