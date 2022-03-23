#ifndef _USERAPI_H_
#define _USERAPI_H_

#include <fuse_lowlevel.h>

void fs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
void fs_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi);
void fs_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void fs_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void fs_access(fuse_req_t req, fuse_ino_t ino, int mask);
void fs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void fs_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi);
void fs_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void fs_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
	size_t size, off_t off, struct fuse_file_info *fi);
void fs_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
void fs_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode);
void fs_unlink(fuse_req_t req, fuse_ino_t parent, const char *name);
void fs_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name);
void fs_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void fs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);

#endif
