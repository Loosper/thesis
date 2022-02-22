#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fs_helpers.h"
#include "itable.h"
#include "io.h"
#include "directory.h"


size_t add_dir(struct inode *inode)
{
	size_t num = add_file(inode);
	uint8_t zeroes[FS_BLOCK_SIZE] = {0};

	assert(S_ISDIR(inode->mode));

	// directories need initialization
	pwrite_ino(inode, zeroes, FS_BLOCK_SIZE, 0);

	return num;
}

int add_direntry(struct inode *dir_ino, struct dirent *entry)
{
	size_t count;

	pread_ino(dir_ino, &count, sizeof(count), 0);
	pwrite_ino(
		dir_ino, entry, sizeof(*entry),
		(count + 1) * sizeof(*entry)
	);
	count++;
	pwrite_ino(dir_ino, &count, sizeof(count), 0);

	return 0;
}

// will read the direntry if it exists. Returns index in file.
// dirent.inode of 0 will indicate error. This is because I really want to use
// the full size_t range for the return (probably stupid)

// TODO: will not decrement the total counter. There should be a counterless
// solution (Btree?) but this one isn't meant to be efficient anyway
static size_t get_direntry_idx(struct dirent *dirent, struct inode *dir_ino, const char *filename)
{
	size_t total;

	pread_ino(dir_ino, &total, sizeof(total), 0);

	for (size_t i = 0; i < total; i++) {
		// +1 because index 0 is metadata (num files)
		pread_ino(
			dir_ino, dirent, sizeof(*dirent),
			(i + 1) * sizeof(*dirent)
		);
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
	pwrite_ino(
		dir_ino, &dirent, sizeof(dirent),
		(idx + 1) * sizeof(dirent)
	);

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
	size_t count;
	struct dirent entry;

	pread_ino(dir_ino, &count, sizeof(count), 0);
	// we're done
	if (idx >= (off_t) count) {
		entry.inode = 0;
		return entry;
	}
	pread_ino(
		dir_ino, &entry, sizeof(entry),
		(idx + 1) * sizeof(entry)
	);
	return entry;
}
