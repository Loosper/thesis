#ifndef _IO_H_
#define _IO_H_

#include <fuse_lowlevel.h>
#include <sys/stat.h>

ssize_t write_data(int file, void *data, size_t len, size_t block_no);
ssize_t write_block(int file, void *data, size_t block_no);
ssize_t read_block(int file, void *buf, size_t block_no);

void fs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
void fs_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi);
void fs_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void fs_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void fs_access(fuse_req_t req, fuse_ino_t ino, int mask);
void fs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void fs_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi);
void fs_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void fs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);


#endif
