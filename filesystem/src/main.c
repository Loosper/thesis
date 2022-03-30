#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fuse_lowlevel.h>

#include "fs_types.h"
#include "fs_init.h"
#include "fs_userapi.h"

// this is just to help debugging more dynamically
int debug = 1;

static const struct fuse_lowlevel_ops fs_ops = {
	// this is for configuring things mostly
	// .init = fs_init,
	// any cleanup on umount should be here
	// .destroy = fs_destroy,
	.lookup = fs_lookup,
	.create = fs_create,
	.flush = fs_flush,
	.release = fs_release,
	.getattr = fs_getattr,
	.setattr = fs_setattr,
	.mkdir = fs_mkdir,
	.unlink = fs_unlink,
	.rmdir = fs_rmdir,
	.opendir = fs_opendir,
	.readdir = fs_readdir,
	.open = fs_open,
	.write = fs_write,
	.read = fs_read
};

#define FS_OPT(opt, field) \
	{opt, offsetof(struct fs_opts, field), 1}
static struct fuse_opt cmdline_opts[] = {
	FS_OPT("--help",	to_help),
	FS_OPT("-help",		to_help),
	FS_OPT("%s",		mountpoint),
	// to set a var but keep the arg for anything that needs it later
	// FS_OPT("-d",		foreground),
	// FUSE_OPT_KEY("-d",	FUSE_OPT_KEY_KEEP),
	FS_OPT("--foreground",	foreground),
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

static int opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs __attribute__((unused)))
{
	struct fs_opts *fs = data;

	if (key == FUSE_OPT_KEY_NONOPT) {
		if (!fs->deva) {
			return check_path(arg, &fs->deva);
		} else if (!fs->devb) {
			return check_path(arg, &fs->devb);
		} else if (!fs->mountpoint) {
			return check_path(arg, &fs->mountpoint);
		}
	}

	return 1;
}

static FILE *file = NULL;

static void log_to_file(enum fuse_log_level level, const char *fmt, va_list ap)
{
	// TODO: segfaults if this errors :D
	if (file == NULL)
		file = fopen("fs_log.log", "w");

	if (!debug)
		return;

	if (level == FUSE_LOG_INFO)
		vfprintf(file, fmt, ap);
	else
		vfprintf(stderr, fmt, ap);

	fflush(file);
}
struct fs_metadata *fs_settings = NULL;

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	// struct fuse_cmdline_opts opts;
	struct fuse_session *sess;
	struct fs_opts opts = {0};
	int ret;

	// it will print its error, we just need to exit
	if (fuse_opt_parse(&args, &opts, cmdline_opts, opt_proc) != 0)
		return 1;

	if (opts.to_help) {
		fuse_cmdline_help();
		return 0;
	}

	if (opts.mountpoint == NULL || opts.deva == NULL || opts.devb == NULL) {
		printf("usage: %s [options] <mountpoint> <storage> <storage mirror>\n", argv[0]);
		return 1;
	}

	fuse_set_log_func(log_to_file);

	fs_settings = malloc(sizeof(*fs_settings));
	fs_settings->deva = open(opts.deva, O_RDWR);
	fs_settings->devb = open(opts.devb, O_RDWR);
	if (fs_settings->deva == -1 || fs_settings->devb == -1) {
		perror("fs: couldn't open file");
		return 1;
	}

	fs_init();

	sess = fuse_session_new(&args, &fs_ops, sizeof(fs_ops), fs_settings);
	if (sess == NULL)
		return 1;
	if (fuse_set_signal_handlers(sess) != 0)
		return 1;
	if (fuse_session_mount(sess, opts.mountpoint) != 0)
		return 1;

	// NOTE: if any operation doesn't reply, this hangs until it does
	fuse_daemonize(opts.foreground);
	ret = fuse_session_loop(sess);

	// cleanup
	fuse_session_unmount(sess);
	fuse_session_destroy(sess);
	free(opts.mountpoint);
	free(opts.deva);
	free(opts.devb);
	free(fs_settings);
	fuse_opt_free_args(&args);

	fflush(file);
	fclose(file);

	return (ret < 0) ? 1: 0;
}

