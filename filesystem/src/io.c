#include <fuse_lowlevel.h>


void fs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	fuse_log(FUSE_LOG_DEBUG, "name: %s, of: %ld\n", name, parent);

}

void fs_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
}
