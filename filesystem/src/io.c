#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/param.h>

#include "fs_helpers.h"
#include "fs_types.h"
#include "io.h"

extern int backing_store;


// all this because write() can be interrupted by signals and partially
// complete. We assume the file has good dimensions and don't prevent infinite
// spin
ssize_t write_block(int file, void *data, size_t block_no)
{
	size_t written = 0;
	ssize_t ret;

	while (written < FS_BLOCK_SIZE) {
		ret = pwrite(
			file, data + written, FS_BLOCK_SIZE - written,
			block_no * FS_BLOCK_SIZE + written
		);
		check_errno(ret);
		written += ret;
	}

	return written;
}

// see write_block() comment
ssize_t read_block(int file, void *buf, size_t block_no)
{
	size_t rread = 0;
	ssize_t ret;

	while (rread < FS_BLOCK_SIZE) {
		ret = pread(
			file, buf + rread, FS_BLOCK_SIZE - rread,
			block_no * FS_BLOCK_SIZE + rread
		);
		check_errno(ret);
		rread += ret;
	}

	return rread;
}

// any data, of any size up to a block. Will be padded and written
ssize_t write_data(int file, void *data, size_t len, size_t block_no)
{
	uint8_t block[FS_BLOCK_SIZE] = {0};
	memcpy(block, data, len);

	return write_block(file, block, block_no);
}

// TODO: terribly wasteful but simple to implement
struct inode *read_inode(int file, fuse_ino_t ino)
{
	struct inode *inode = malloc(FS_BLOCK_SIZE);
	// read_block(file, inode, ino);

	return inode;
}

static size_t get_byte_to_block(size_t ino, size_t byte_n, bool internal)
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
		// TODO: check the inode exists. Porbably extract the finding
		// of the number in a separate helper fuction
		fs_int_pread(NUM_INT_ROOT, file, FS_BLOCK_SIZE, ino + 1);
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

size_t add_file(struct inode *inode)
{
	size_t data;

	fs_int_pread(NUM_INT_ROOT, &data, sizeof(size_t), 0);
	fs_int_pwrite(NUM_INT_ROOT, inode, sizeof(struct inode), data * FS_BLOCK_SIZE);
	data++;
	fs_int_pwrite(NUM_INT_ROOT, &data, sizeof(size_t), 0);

	return data;
}

