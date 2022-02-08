#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/param.h>

#include "fs_helpers.h"
#include "fs_types.h"
#include "io.h"

extern int backing_store;


// TODO: this is dumb as fuck, but it works for now!
size_t allocate_block()
{
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
			fs_int_pread(
				BLK_FREE_LIST_INO, data, FS_BLOCK_SIZE,
				file_offset
			);

		// it's allocated
		if (data[index_in_block] & bitmask)
			continue;

		// allocate and quit
		data[index_in_block] |= bitmask;
		fs_int_pwrite(
			BLK_FREE_LIST_INO, data, FS_BLOCK_SIZE,
			file_offset
		);
		break;
	}

	free(data);
	// logprintf("allocated %ld\n", bit_num);
	return bit_num;
}

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
		read_inode(ino, file);
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

ssize_t fs_int_pwrite(size_t ino, const void *buf, size_t count, off_t offset)
{
	return do_read_write(ino, (void *) buf, count, offset, true, true);
}

ssize_t fs_pread(size_t ino, void *buf, size_t count, off_t offset)
{
	return do_read_write(ino, buf, count, offset, false, false);
}

ssize_t fs_pwrite(size_t ino, const void *buf, size_t count, off_t offset)
{
	return do_read_write(ino, (void *) buf, count, offset, true, false);
}

static size_t get_num_files()
{
	size_t data;
	fs_int_pread(NUM_INT_ROOT, &data, sizeof(data), 0);
	return data;
}

// TODO: eventually do a proper check
static bool file_exists(size_t filenum)
{
	return filenum > 0 && filenum < get_num_files();
}

size_t add_file(struct inode *inode)
{
	size_t data = get_num_files();
	inode->data_block = allocate_block();
	struct secondary_block *blk = malloc(sizeof(*blk));

	// TODO: dynamic size
	blk->used = 7;
	for (int i = 0; i < blk->used; i++) {
		blk->blocks[i] = allocate_block();
		init_blk_zero(blk->blocks[i]);
	}
	write_block(backing_store, blk, inode->data_block);

	// TODO: do this via the size of the file?
	// data isn't an index, it's max size and since index 0 is reserved, we
	// use it here
	fs_int_pwrite(NUM_INT_ROOT, inode, sizeof(*inode), data * FS_BLOCK_SIZE);
	data++;
	fs_int_pwrite(NUM_INT_ROOT, &data, sizeof(data), 0);

	// -1 because the new count is the total, not the inode num
	return data - 1;
}

int read_inode(size_t num, struct inode *inode)
{
	if (!file_exists(num))
		return -ENOENT;
	fs_int_pread(NUM_INT_ROOT, inode, sizeof(*inode), num * FS_BLOCK_SIZE);
	return 0;
}

int write_inode(size_t num, struct inode *inode)
{
	fs_int_pwrite(NUM_INT_ROOT, inode, sizeof(*inode), num * FS_BLOCK_SIZE);
	return 0;
}

int add_direntry(size_t dir_ino, struct dirent *entry)
{
	size_t count;

	// TODO: not sure if i need to check the directory (kernel might do it
	// for me)
	if (!file_exists(dir_ino) || !file_exists(entry->inode))
		return -ENOENT;

	fs_pread(dir_ino, &count, sizeof(count), 0);
	fs_pwrite(
		dir_ino, entry, sizeof(*entry),
		(count + 1) * sizeof(*entry)
	);
	count++;
	fs_pwrite(dir_ino, &count, sizeof(count), 0);

	return 0;
}

size_t get_direntry(size_t dir_ino, const char *filename)
{
	assert(file_exists(dir_ino));

	size_t total;

	fs_pread(dir_ino, &total, sizeof(total), 0);
	for (size_t i = 0; i < total; i++) {
		struct dirent dirent;

		// mildly inefficient, as it will read inode num too,
		// but insignificant
		// +1 because index 0 is metadata (num files)
		fs_pread(
			dir_ino, &dirent, sizeof(dirent),
			(i + 1) * sizeof(dirent)
		);
		if (strncmp(filename, dirent.name, MAX_NAME_LEN) == 0) {
			return dirent.inode;
		}
	}

	return 0;
}

struct dirent list_dir(size_t dir_ino, off_t idx)
{
	size_t count;
	struct dirent entry;
	assert(file_exists(dir_ino));

	fs_pread(dir_ino, &count, sizeof(count), 0);
	// we're done
	if (idx >= (off_t) count) {
		entry.inode = 0;
		return entry;
	}
	fs_pread(
		dir_ino, &entry, sizeof(entry),
		(idx + 1) * sizeof(entry)
	);
	return entry;
}
