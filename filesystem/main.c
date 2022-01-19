#define FUSE_USE_VERSION 35

#include <fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>


static const struct fuse_lowlevel_ops fs_ops = {

};


int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_cmdline_opts opts;
	struct fuse_session *sess;
	int ret;

	// I suppose this rarely blows up so no print
	if (fuse_parse_cmdline(&args, &opts) != 0)
		return 1;

	if (opts.show_help) {
		fuse_cmdline_help();
		fuse_lowlevel_help();
		return 0;
	}

	if (opts.mountpoint == NULL) {
		printf("usage: %s [options] <mountpoint>\n", argv[0]);
		// printf("       %s --help\n", argv[0]);
		return 1;
	}

	sess = fuse_session_new(&args, &fs_ops, sizeof(fs_ops), NULL);
	if (sess == NULL)
		return 1;
	if (fuse_set_signal_handlers(sess) != 0)
		return 1;
	if (fuse_session_mount(sess, opts.mountpoint) != 0)
		return 1;

	fuse_daemonize(opts.foreground);
	ret = fuse_session_loop(sess);

	// cleanup
	fuse_session_unmount(sess);
	fuse_session_destroy(sess);
	free(opts.mountpoint);
	fuse_opt_free_args(&args);

	return (ret < 0) ? 1: 0;
}

