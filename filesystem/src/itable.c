#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "flist.h"
#include "fs_helpers.h"
#include "io.h"
#include "itable.h"


// TODO: optimise this away somehow. Maybe cache it in a global?
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

static void write_num_files(size_t num)
{
	struct inode itable = get_itable_inode();

	// TODO: this is currently a workaround becuase I don't have an fseek
	// for files.  hopefully I can eventually just leave space blank
	uint8_t *ptr = malloc(FS_BLOCK_SIZE);
	memset(ptr, 0, FS_BLOCK_SIZE);
	memcpy(ptr, &num, sizeof(num));

	pwrite_ino(&itable, ptr, FS_BLOCK_SIZE, 0);
}

static size_t filenum_offset(size_t num)
{
	// don't forget to add the initial counter!!!
	return num * FS_BLOCK_SIZE + FS_BLOCK_SIZE;
}


int read_inode(size_t num, struct inode *inode)
{
	struct inode itable = get_itable_inode();

	if (!file_exists(num))
		return -ENOENT;

	pread_ino(
		&itable, inode, sizeof(*inode),
		filenum_offset(num)
	);
	return 0;
}

void write_inode(size_t num, struct inode *inode)
{
	uint8_t buf[FS_BLOCK_SIZE];
	struct inode itable = get_itable_inode();

	memcpy(buf, inode, sizeof(*inode));
	// zero the rest of the block
	memset(buf + sizeof(*inode), 0, sizeof(buf) - sizeof(*inode));

	pwrite_ino(
		&itable, buf, sizeof(buf),
		filenum_offset(num)
	);
}

// must not depend on the superblock as we are initialising it
size_t gen_itable()
{
	size_t location = blk_allocator();
	struct inode root;

	make_empty_inode(&root, S_IFREG);
	root.blk = location;

	write_data(&root, sizeof(root), location);

	return location;
}

// may now depend on the superblock
void init_itable()
{
	write_num_files(1);
	// TODO: for now I leave nothing at inode 0. I could put the itable or
	// the flist there?
	// add_file(&root);
}

bool file_exists(size_t filenum)
{
	return filenum > 0 && filenum <= get_num_files();
}

// TODO: zeroing is a MASSIVE problem. I need a routine that initializes things
// properly. Currently I zero blocks in 5 different places
size_t add_file(struct inode *inode)
{
	size_t count = get_num_files();
	struct inode itable = get_itable_inode();

	// this is the inode handle being given a number
	write_inode(count, inode);
	write_num_files(count + 1);

	// save the block num to the inode for short circuiting wherever needed
	size_t location = get_pblock_of_byte(&itable, filenum_offset(count));
	inode->blk = location;
	write_data(inode, sizeof(*inode), location);

	return count;
}
