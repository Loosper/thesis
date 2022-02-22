#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#include "fs_helpers.h"
#include "io.h"
#include "itable.h"


static inline struct inode get_itable_inode()
{
	struct inode inode;
	read_data(&inode, sizeof(inode), superblock.itable_blk);
	return inode;
}

static size_t get_num_files()
{
	struct inode itable = get_itable_inode();
	size_t data;

	pread_ino(&itable, &data, sizeof(data), 0);

	return data;
}

bool file_exists(size_t filenum)
{
	return filenum > 0 && filenum < get_num_files();
}

// TODO: zeroing is a MASSIVE problem. I need a routine that initializes things
// properly. Currently I zero blocks in 5 different places
size_t add_file(struct inode *inode)
{
	struct inode itable = get_itable_inode();
	size_t data = get_num_files();
	inode->data_block = allocate_block();
	init_blk_zero(inode->data_block);
	struct secondary_block *blk = malloc(sizeof(*blk));

	// TODO: do this via the size of the file?
	// data isn't an index, it's max size and since index 0 is reserved, we
	// use it here
	pwrite_ino(&itable, inode, sizeof(*inode), data * FS_BLOCK_SIZE);
	data++;
	pwrite_ino(&itable, &data, sizeof(data), 0);

	free(blk);
	// -1 because the new count is the total, not the inode num
	return data - 1;
}

int read_inode(size_t num, struct inode *inode)
{
	struct inode itable = get_itable_inode();

	if (!file_exists(num))
		return -ENOENT;
	pread_ino(&itable, inode, sizeof(*inode), num * FS_BLOCK_SIZE);
	return 0;
}

int write_inode(size_t num, struct inode *inode)
{
	struct inode itable = get_itable_inode();

	pwrite_ino(&itable, inode, sizeof(*inode), num * FS_BLOCK_SIZE);
	return 0;
}
