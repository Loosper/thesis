#ifndef _IO_H_
#define _IO_H_

#include <fuse_lowlevel.h>

void fs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
void fs_access(fuse_req_t req, fuse_ino_t ino, int mask);

#endif
