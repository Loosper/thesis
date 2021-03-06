/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) Boyan Karatotev */
#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_

#include "fs_types.h"

size_t add_dir(struct inode *inode);
int add_direntry(struct inode *dir_ino, struct dirent *entry);
size_t get_direntry(struct inode *dir_ino, const char *filename);
int rm_direntry(struct inode *dir_ino, const char *filename);
struct dirent list_dir(struct inode *dir_ino, off_t idx);

#endif
