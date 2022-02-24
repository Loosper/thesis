#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "flist.h"
#include "fs_helpers.h"
#include "itable.h"
#include "io.h"
#include "directory.h"


static inline size_t dir_get_num_files(struct inode *dir)
{
	size_t count;

	pread_ino(dir, &count, sizeof(count), 0);

	return count;
}

static inline void dir_write_num_files(struct inode *dir, size_t num)
{
	pwrite_ino(dir, &num, sizeof(num), 0);
}

static inline void dir_read_entry(struct inode *dir, struct dirent *entry, size_t num)
{
	pread_ino(
		dir, entry, sizeof(*entry),
		num * sizeof(*entry) + sizeof(num)
	);
}

static inline void dir_write_entry(struct inode *dir, struct dirent *entry, size_t num)
{
	pwrite_ino(
		dir, entry, sizeof(*entry),
		num * sizeof(*entry) + sizeof(num)
	);
}

size_t add_dir(struct inode *inode)
{
	size_t num = add_file(inode);

	assert(S_ISDIR(inode->mode));
	// directories need initialization
	dir_write_num_files(inode, 0);

	return num;
}

int add_direntry(struct inode *dir_ino, struct dirent *entry)
{
	size_t count = dir_get_num_files(dir_ino);

	dir_write_entry(dir_ino, entry, count);
	dir_write_num_files(dir_ino, count + 1);

	return 0;
}

// will read the direntry if it exists. Returns index in file.
// dirent.inode of 0 will indicate error. This is because I really want to use
// the full size_t range for the return (probably stupid)

// TODO: will not decrement the total counter. There should be a counterless
// solution (Btree?) but this one isn't meant to be efficient anyway
static size_t get_direntry_idx(struct dirent *dirent, struct inode *dir_ino, const char *filename)
{
	size_t total = dir_get_num_files(dir_ino);

	for (size_t i = 0; i < total; i++) {
		dir_read_entry(dir_ino, dirent, i);
		if (strncmp(filename, dirent->name, MAX_NAME_LEN) == 0) {
			return i;
		}
	}

	dirent->name[0] = '\0';
	dirent->inode = 0;
	// it's a stupidly large value so it raises alarm bells when I see it
	return SIZE_MAX;
}

// TODO:
static int unlink_inode(size_t num)
{
	assert(num != 0);
	return 0;
}

int rm_direntry(struct inode *dir_ino, const char *filename)
{
	struct dirent dirent;
	size_t idx = get_direntry_idx(&dirent, dir_ino, filename);
	// conscious choice to not check it. I assume the kernel will only
	// feed me valid stuff for now
	assert(dirent.inode != 0);

	unlink_inode(dirent.inode);

	dirent.name[0] = '\0';
	dirent.inode = 0;
	dir_write_entry(dir_ino, &dirent, idx);

	return 0;
}

size_t get_direntry(struct inode *dir_ino, const char *filename)
{
	struct dirent dirent;

	get_direntry_idx(&dirent, dir_ino, filename);

	// will be 0 on error
	return dirent.inode;
}

struct dirent list_dir(struct inode *dir_ino, off_t idx)
{
	size_t count = dir_get_num_files(dir_ino);
	struct dirent entry;

	// we're done
	if (idx >= (off_t) count) {
		entry.inode = 0;
		return entry;
	}
	dir_read_entry(dir_ino, &entry, idx);
	return entry;
}
