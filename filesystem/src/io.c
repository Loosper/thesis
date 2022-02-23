#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/param.h>

#include "flist.h"
#include "fs_helpers.h"
#include "fs_types.h"
#include "itable.h"
#include "io.h"

// all this because write() can be interrupted by signals and partially
// complete. We assume the file has good dimensions and don't prevent infinite
// spin

// FIXME: change names of all of the below to something consistent. read_inode
// is high level and read_data isn't. This has to be communicated. You could do
// the _name things here too for static and genrally internal things

// TODO: now that files extend themselves, we don't need to write whole blocks
// to them, can be just like normal files. I.E. the bookkeeping data for dirs
// and such could be small

ssize_t write_block(void *data, size_t block_no)
{
	size_t written = 0;
	ssize_t ret;

	while (written < FS_BLOCK_SIZE) {
		ret = pwrite(
			backing_store, data + written, FS_BLOCK_SIZE - written,
			block_no * FS_BLOCK_SIZE + written
		);
		CHECK_ERRNO(ret);
		written += ret;
	}

	return written;
}

// see write_block() comment
ssize_t read_block(void *buf, size_t block_no)
{
	size_t rread = 0;
	ssize_t ret;

	while (rread < FS_BLOCK_SIZE) {
		ret = pread(
			backing_store, buf + rread, FS_BLOCK_SIZE - rread,
			block_no * FS_BLOCK_SIZE + rread
		);
		CHECK_ERRNO(ret);
		rread += ret;
	}

	return rread;
}

// any data, of any size up to a block. Will be padded and written
ssize_t write_data(void *data, size_t len, size_t block_no)
{
	uint8_t block[FS_BLOCK_SIZE] = {0};
	memcpy(block, data, len);

	return write_block(block, block_no);
}

ssize_t read_data(void *data, size_t len, size_t block_no)
{
	uint8_t block[FS_BLOCK_SIZE];
	ssize_t ret;

	ret = read_block(block, block_no);
	memcpy(data, block, len);

	return ret;
}

// assumes secondary_blk is valid;
// returns last accessible block
size_t file_add_space(struct inode *inode, size_t blk_req, size_t (*allocator)())
{
	struct secondary_block data_ptr;
	size_t blk_written = 0;
	size_t blk_ptr_loc = inode->data_block;
	bool first = true;
	size_t new_block;

	read_data(&data_ptr, sizeof(data_ptr), inode->data_block);

	while (blk_written < blk_req) {
		if (!first) {
			data_ptr.used = 0;
			blk_ptr_loc = allocator();
		}
		first = false;

		// TODO: this can be shrank by replacing i with block.used
		// fill in the ptr struct ("allocate" blocks in a sense)
		while (data_ptr.used < SCND_CAPACITY) {
			new_block = allocator();
			// TODO: see add_file comment. This is bad.
			// Keep it for sanity for now
			init_blk_zero(new_block);
			data_ptr.blocks[data_ptr.used++] = new_block;
			init_blk_zero(new_block);

			// last block is bookkeeping, so it doesn't count
			if (data_ptr.used != SCND_CAPACITY - 1)
				blk_written++;
			if (blk_written == blk_req)
				break;
		}

		// write the intermediary block as filled in
		write_data(&data_ptr, sizeof(data_ptr), blk_ptr_loc);
	}

	return new_block;
}

// THE READ/WRITE ROUTINE. EVERYTHING AFTER DEPENDS ON THIS
// above this point it is not usable. After, it's free for all and assumed "set up"

// returns the block number where this byte will be located in. Checks if
// reading after end of file. If writing, will extend the file
// NOTE: Let vblock be virtual address of block, pblock be the actual address
// of a block
static size_t get_pblock_of_byte(struct inode *inode, size_t byte_n, bool write)
{
	// virtual/physical
	// +1 to make it a count instead of an index
	// if (byte_n > 1000)
	size_t vblocks_needed = byte_n / FS_BLOCK_SIZE + 1;
	size_t pblock;
	struct secondary_block *blk_ptr = malloc(FS_BLOCK_SIZE);
	size_t vblocks_found = 0;

	pblock = inode->data_block;
	// TODO: can probably be MUCH prettier, but works for now and i'm deep fried
	while (1) {
		size_t new_found;

		read_block(blk_ptr, pblock);
		// last one is internal so drop it if it's there
		new_found = MIN(blk_ptr->used, SCND_CAPACITY - 1);

		// FOUND!
		if (vblocks_found + new_found >= vblocks_needed) {
			// we know it is there and we're on the right one set so just get it
			pblock = blk_ptr->blocks[(vblocks_needed - 1) % SCND_CAPACITY];
			vblocks_found = ((vblocks_needed - 1) % SCND_CAPACITY) + 1;
			break;
		}

		vblocks_found += new_found;

		// means we reached the last one
		if (blk_ptr->used != SCND_CAPACITY) {
			// error
			if (!write) {
				pblock = 0;
				goto cleanup;
			}

			pblock = file_add_space(inode, vblocks_needed - vblocks_found, &allocate_block);
			goto cleanup;
		}
		pblock = blk_ptr->blocks[SCND_CAPACITY - 1];
	}

	assert(vblocks_found == vblocks_needed);

cleanup:
	free(blk_ptr);
	return pblock;
}

// TODO: idea: have the 0th inode in the inode file be the inode for the file
// itself. It can be at a fixed location and will drop special handling and
// kinda short circuit bootstrap

// NOTE: currently only writes up to FS_BLOCK_SIZE
static ssize_t do_read_write_block(struct inode *inode, void *buf, size_t count, off_t offset, bool write, bool internal)
{
	uint8_t data[FS_BLOCK_SIZE];
	size_t block_n;
	// offset is %-ed because we only want offset within the block
	size_t blk_offset = offset % FS_BLOCK_SIZE;
	// don't try to copy beyond the end of the block TODO: for now
	count = MIN(blk_offset + count, FS_BLOCK_SIZE) - blk_offset;

	// TODO: ideally, this won't extend the space itself, but rather will
	// let us know.  When it does, we'd set the file size
	block_n = get_pblock_of_byte(inode, offset, write);

	if (block_n == 0)
		return -EINVAL;

	read_block(data, block_n);

	if (write) {
		memcpy(data + blk_offset, buf, count);
		write_block(data, block_n);

		if (inode->size < offset + count) {
			// TODO: how to write it back with updated value
			inode->size = offset + count;
		}
	} else {
		memcpy(buf, data + blk_offset, count);
	}

	return count;
}

// TODO: this is a very temporary and very ugly HACK
static ssize_t do_read_write_full(struct inode *inode, void *buf, size_t count, off_t offset, bool write, bool internal)
{
	size_t done = 0;
	while (done < count) {
		ssize_t ret;
		ret = do_read_write_block(
			inode, buf + done, count - done,
			offset + done, write, internal
		);
		if (ret < 0)
			return ret;
		done += ret;
	}

	assert(done == count);

	return done;
}

// shorthands for the above. Please use these and not that monstrocity
ssize_t pread_ino(struct inode* ino, void *buf, size_t count, off_t offset)
{
	return do_read_write_full(ino, buf, count, offset, false, false);
}

ssize_t pwrite_ino(struct inode* ino, const void *buf, size_t count, off_t offset)
{
	return do_read_write_full(ino, (void *) buf, count, offset, true, false);
}
