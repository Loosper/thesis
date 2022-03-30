#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>

#include <linux/if_alg.h>

#include "directory.h"
#include "flist.h"
#include "fs_helpers.h"
#include "fs_types.h"
#include "itable.h"
#include "io.h"

int backing_store = -1;
struct superblock superblock;
int checksum_fd;


static void write_root_dir()
{
	struct inode root_dir;

	make_empty_inode(&root_dir, S_IFDIR | 0777);
	add_dir(&root_dir);
}

void fs_init()
{
	int tfm_soc;
	struct sockaddr_alg sa = {
		.salg_family = AF_ALG,
		.salg_type   = "hash",
		.salg_name   = "crc32c"
	};

	tfm_soc = socket(AF_ALG, SOCK_SEQPACKET, 0);
	bind(tfm_soc, (struct sockaddr *)&sa, sizeof(sa));
	checksum_fd = accept(tfm_soc, NULL, 0);

	for (int i = 0; i <= SUPERBLOCK_BLK; i++) {
		init_blk_zero(i);
	}

	superblock.flist_blk = gen_flist();
	superblock.itable_blk = gen_itable();
	init_itable();
	write_root_dir();

	// write the superblock
	write_data(&superblock, sizeof(superblock), SUPERBLOCK_BLK);
}
