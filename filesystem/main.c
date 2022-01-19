#define FUSE_USE_VERSION 35

#include <fuse_lowlevel.h>
#include <limits.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const struct fuse_lowlevel_ops fs_ops = {
	// this is for configuring things mostly
	// .init = fs_init,
	// any cleanup on umount should be here
	// .destroy = fs_destroy,

};

struct filesystem {
	char *mountpoint;
	char *backing_store;
	int debug;
	int to_help;
};

#define FS_OPT(opt, field) \
	{opt, offsetof(struct filesystem, field), 1}

static struct fuse_opt cmdline_opts[] = {
	FS_OPT("--help",	to_help),
	FS_OPT("-help",		to_help),
	FS_OPT("%s",		mountpoint),
	// to set a var but keep the arg for anything that needs it later
	// FS_OPT("-d",		foreground),
	// FUSE_OPT_KEY("-d",	FUSE_OPT_KEY_KEEP),
	FUSE_OPT_END
};

static int check_path(const char *path, char **dst)
{
	if ((*dst = realpath(path, NULL)) == NULL) {
		fuse_log(
			FUSE_LOG_ERR,
			"fs: bad mount point: %s, %s\n",
			path, strerror(errno)
		);
		return -1;
	}
	return 0;
}

int opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs __attribute__((unused)))
{
	struct filesystem *fs = data;

	if (key == FUSE_OPT_KEY_NONOPT) {
		if (!fs->backing_store) {
			return check_path(arg, &fs->backing_store);
		} else if (!fs->mountpoint) {
			return check_path(arg, &fs->mountpoint);
		}
	}

	return 1;
}

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	// struct fuse_cmdline_opts opts;
	struct fuse_session *sess;
	struct filesystem fs = {0};
	int ret;

	// it will print its error, we just need to exit
	if (fuse_opt_parse(&args, &fs, cmdline_opts, opt_proc) != 0)
		return 1;

	if (fs.to_help) {
		fuse_cmdline_help();
		return 0;
	}

	if (fs.mountpoint == NULL || fs.backing_store == NULL) {
		printf("usage: %s [options] <mountpoint> <storage>\n", argv[0]);
		return 1;
	}

	sess = fuse_session_new(&args, &fs_ops, sizeof(fs_ops), NULL);
	if (sess == NULL)
		return 1;
	if (fuse_set_signal_handlers(sess) != 0)
		return 1;
	if (fuse_session_mount(sess, fs.mountpoint) != 0)
		return 1;

	// TODO: an arg for this
	fuse_daemonize(1);
	ret = fuse_session_loop(sess);

	// // cleanup
	fuse_session_unmount(sess);
	fuse_session_destroy(sess);
	free(fs.mountpoint);
	free(fs.backing_store);
	fuse_opt_free_args(&args);

	return (ret < 0) ? 1: 0;
}

