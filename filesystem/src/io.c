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

// FIXME: change names of all of the below to something consistent. read_inode
// is high level and read_data isn't. This has to be communicated. You could do
// the _name things here too for static and genrally internal things

// NOTE: Let vblock be virtual address of block, pblock be the actual address
// of a block

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

ssize_t read_data(int file, void *data, size_t len, size_t block_no)
{
	uint8_t block[FS_BLOCK_SIZE];
	ssize_t ret;

	ret = read_block(file, block, block_no);
	memcpy(data, block, len);

	return ret;
}

// assumes secondary_blk is valid;
// returns last accessible block
size_t file_add_space(size_t secondary_blk, size_t blk_req, size_t (*allocator)())
{
	struct secondary_block data_ptr;
	size_t blk_written = 0;
	size_t blk_ptr_loc = secondary_blk;
	bool first = true;
	size_t new_block;

	read_data(backing_store, &data_ptr, sizeof(data_ptr), secondary_blk);

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
			data_ptr.blocks[data_ptr.used++] = new_block;
			init_blk_zero(new_block);

			// last block is bookkeeping, so it doesn't count
			if (data_ptr.used != SCND_CAPACITY - 1)
				blk_written++;
			if (blk_written == blk_req)
				break;
		}

		// write the intermediary block as filled in
		write_data(backing_store, &data_ptr, sizeof(data_ptr), blk_ptr_loc);
	}

	// logprintf("out %ld\n", new_block);
	return new_block;
}

// returns the block number where this byte will be located in. Checks if
// reading after end of file. If writing, will extend the file
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

		read_block(backing_store, blk_ptr, pblock);
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

			pblock = file_add_space(pblock, vblocks_needed - vblocks_found, &allocate_block);
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
static ssize_t do_read_write(size_t ino, void *buf, size_t count, off_t offset, bool write, bool internal)
{
	uint8_t data[FS_BLOCK_SIZE];
	struct inode inode;
	size_t block_n;
	// offset is %-ed because we only want offset within the block
	size_t blk_offset = offset % FS_BLOCK_SIZE;
	// don't try to copy beyond the end of the block TODO: for now
	count = MIN(blk_offset + count, FS_BLOCK_SIZE) - blk_offset;


	// get the inode of the file (read AND write)
	if (internal) {
		size_t inode_blk;
		assert(ino == NUM_INT_ROOT || ino == NUM_INT_FREE);
		if (ino == NUM_INT_ROOT)
			inode_blk = BLK_ROOT_INO;
		else if (ino == NUM_INT_FREE)
			inode_blk = BLK_FREE_LIST_INO;
		read_data(backing_store, &inode, sizeof(inode), inode_blk);
	} else {
		// TODO: this has to be connected with read_inode somehow. code
		// is duplicate
		do_read_write(NUM_INT_ROOT, &inode, sizeof(inode), ino * FS_BLOCK_SIZE, false, true);
	}

	// TODO: ideally, this won't extend the space itself, but rather will
	// let us know.  When it does, we'd set the file size
	block_n = get_pblock_of_byte(&inode, offset, write);

	if (block_n == 0)
		return -EINVAL;

	read_block(backing_store, data, block_n);

	if (write) {
		memcpy(data + blk_offset, buf, count);
		write_block(backing_store, data, block_n);

		if (inode.size < offset + count) {
			inode.size = offset + count;
			if (internal) {
				// TODO: lazy; I don't expect the free list to change so
				// I let myself not handle it
				assert(ino != NUM_INT_FREE);
				write_data(backing_store, &inode, sizeof(inode), BLK_ROOT_INO);
			} else {
				do_read_write(
					NUM_INT_ROOT, &inode, sizeof(inode),
					ino * FS_BLOCK_SIZE, true, true
				);
			}
		}
	} else {
		memcpy(buf, data + blk_offset, count);
	}

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

size_t add_dir(struct inode *inode)
{
	size_t num = add_file(inode);
	uint8_t zeroes[FS_BLOCK_SIZE] = {0};

	assert(S_ISDIR(inode->mode));

	// directories need initialization
	fs_pwrite(num, zeroes, FS_BLOCK_SIZE, 0);

	return num;
}

size_t add_file(struct inode *inode)
{
	size_t data = get_num_files();
	inode->data_block = allocate_block();
	struct secondary_block *blk = malloc(sizeof(*blk));

	// TODO: do this via the size of the file?
	// data isn't an index, it's max size and since index 0 is reserved, we
	// use it here
	fs_int_pwrite(NUM_INT_ROOT, inode, sizeof(*inode), data * FS_BLOCK_SIZE);
	data++;
	fs_int_pwrite(NUM_INT_ROOT, &data, sizeof(data), 0);

	free(blk);
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
