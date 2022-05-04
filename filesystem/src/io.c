/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) Boyan Karatotev */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/socket.h>

#include "flist.h"
#include "fs_helpers.h"
#include "fs_types.h"
#include "itable.h"
#include "io.h"


size_t (*blk_allocator)() = NULL;
size_t (*blk_deallocator)() = NULL;
gfp_t dummy_gfp;

// FIXME: change names of all of the below to something consistent. read_inode
// is high level and read_data isn't. This has to be communicated. You could do
// the _name things here too for static and genrally internal things

#define BLOCK_COMPUTE_CHECKSUM(block, checksum)				\
	do {								\
		int ret;						\
		ret = send(checksum_fd, block, FS_BLOCK_SIZE, 0);	\
		assert(ret == FS_BLOCK_SIZE);				\
		ret = recv(checksum_fd, checksum, CHECKSUM_LEN, 0);	\
		assert(ret == CHECKSUM_LEN);				\
	} while (0)


// no braces on purpose. This is so it expands as actual parameters
#define BLOCK_PARAMS(buf, dev)						\
	fs_settings->dev, (uint8_t *) buf + progress,			\
	FS_BLOCK_SIZE - progress, block_no * FS_SECTOR_SIZE + progress
#define CHECK_PARAMS(buf, dev)						\
	fs_settings->dev, buf,						\
	CHECKSUM_LEN, block_no * FS_SECTOR_SIZE + FS_BLOCK_SIZE

#define IO_FUNC(func, params) func(params)

// all this because write() can be interrupted by signals and partially
// complete. We assume the file has good dimensions and don't prevent infinite
// spin
#define IO_FULL(func, buf, dev)						\
	do {								\
		size_t progress = 0;					\
		ssize_t ret;						\
		while (progress < FS_BLOCK_SIZE) {			\
			ret = IO_FUNC(func, BLOCK_PARAMS(buf, dev));	\
			CHECK_ERRNO(ret);				\
			progress += ret;				\
		}							\
	} while (0)

#define CHECKSUM_IO(func, buf, dev)					\
	do {								\
		ssize_t ret;						\
		ret = IO_FUNC(func, CHECK_PARAMS(buf, dev));		\
		assert(ret == CHECKSUM_LEN);				\
	} while (0)

void write_block(void *data, size_t block_no)
{
	uint8_t checksum[CHECKSUM_LEN];

	BLOCK_COMPUTE_CHECKSUM(data, checksum);
	IO_FULL(pwrite, data, deva);
	IO_FULL(pwrite, data, devb);
	CHECKSUM_IO(pwrite, checksum, deva);
	CHECKSUM_IO(pwrite, checksum, devb);
}

void read_block(void *buf, size_t block_no)
{
	uint8_t checksum[CHECKSUM_LEN];
	uint8_t stored_checksum[CHECKSUM_LEN];

	IO_FULL(pread, buf, deva);
	CHECKSUM_IO(pread, stored_checksum, deva);
	BLOCK_COMPUTE_CHECKSUM(buf, checksum);

	if (memcmp(checksum, stored_checksum, CHECKSUM_LEN) != 0) {
		logprintf("block %ld corrupted, retrying from devb\n", block_no);

		IO_FULL(pread, buf, devb);
		CHECKSUM_IO(pread, stored_checksum, devb);
		BLOCK_COMPUTE_CHECKSUM(buf, checksum);

		assert(memcmp(checksum, stored_checksum, CHECKSUM_LEN) == 0);

		// attempt a repair
		IO_FULL(pwrite, buf, deva);
		CHECKSUM_IO(pwrite, checksum, deva);
	}
}

// any data, of any size up to a block. Will be padded and written
void write_data(void *data, size_t len, size_t block_no)
{
	uint8_t block[FS_BLOCK_SIZE] = {0};
	memcpy(block, data, len);

	write_block(block, block_no);
}

void read_data(void *data, size_t len, size_t block_no)
{
	uint8_t block[FS_BLOCK_SIZE];

	read_block(block, block_no);
	memcpy(data, block, len);
}

static long get_blk_count(struct btree_head64 *head)
{
	long last_blk = 0;
	void *ret = btree_last64(head, &last_blk);
	if (ret != NULL)
		last_blk += 1;

	return last_blk;
}

// TODO: this has to overwrite the inode. Otherwise, keep the data_tree in a
// separate block. Better yet, leave the responsibility to the parent
size_t file_add_space(struct inode *inode, size_t blk_req)
{
	size_t start_blk = 0;
	size_t blk;
	long ret;

	start_blk = get_blk_count(&inode->data_tree);

	for (size_t i = start_blk; i < start_blk + blk_req; i++) {
		blk = blk_allocator();

		ret = btree_insert64(
			&inode->data_tree, i,
			(void *)blk, dummy_gfp
		);
		assert(ret == 0);
	}

	update_file_inode(inode);
	return blk;
}


size_t get_pblock_of_byte(struct inode *inode, size_t byte)
{
	return (size_t) btree_lookup64(&inode->data_tree, byte / FS_BLOCK_SIZE);
}

// NOTE: currently only writes up to FS_BLOCK_SIZE
static ssize_t do_read_write_block(struct inode *inode, void *buf,
		size_t count, off_t offset, bool write, bool internal)
{
	uint8_t data[FS_BLOCK_SIZE];
	size_t block_n;
	size_t off_in_blk = offset % FS_BLOCK_SIZE;
	// don't try to copy beyond the end of the block
	count = MIN(off_in_blk + count, FS_BLOCK_SIZE) - off_in_blk;

	block_n = get_pblock_of_byte(inode, offset);

	// we don't use block 0, and btree uses it for error (not found in this case)
	if (block_n == 0 && write) {
		size_t avail_keys = avail_keys = get_blk_count(&inode->data_tree);
		size_t blk_needed = offset / FS_BLOCK_SIZE;

		if (blk_needed < PREALLOC_AMOUNT)
			blk_needed = PREALLOC_AMOUNT;

		file_add_space(inode, blk_needed);
		block_n = get_pblock_of_byte(inode, offset);
	} else if (block_n == 0 && !write) {
		return -EINVAL;
	}

	// TODO: add a read past EOF check
	assert(inode->blk != 0);
	assert(block_n != 0);

	read_block(data, block_n);

	if (write) {
		memcpy(data + off_in_blk, buf, count);
		write_block(data, block_n);

		// writing can update file size. Do it the naive way
		if (inode->size < offset + count) {
			inode->size = offset + count;
			update_file_inode(inode);
		}
	} else {
		memcpy(buf, data + off_in_blk, count);
	}

	return count;
}

// TODO: this is a very temporary and very ugly HACK
static ssize_t do_read_write_full(struct inode *inode, void *buf, size_t count,
		off_t offset, bool write, bool internal)
{
	size_t done = 0;
	while (done < count) {
		ssize_t ret;
		ret = do_read_write_block(
			inode, (uint8_t *) buf + done, count - done,
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
