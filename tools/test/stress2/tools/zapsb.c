/*
 * Zero out a superblock check hash.
 * By Kirk McKusick <mckusick@mckusick.com>
 */

#include <sys/param.h>
#include <sys/disklabel.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/*
 * Possible superblock locations ordered from most to least likely.
 */
static int sblock_try[] = SBLOCKSEARCH;

static void
usage(void)
{
	(void)fprintf(stderr, "usage: zapsb special_device\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *fs, sblock[SBLOCKSIZE];
	struct fs *sbp;
	int i, fd;

	if (argc != 2)
		usage();

	fs = *++argv;

	/* get the superblock. */
	if ((fd = open(fs, O_RDWR, 0)) < 0)
		err(1, "%s", fs);
	for (i = 0; sblock_try[i] != -1; i++) {
		if (lseek(fd, (off_t)(sblock_try[i]), SEEK_SET) < 0)
			err(1, "%s", fs);
		if (read(fd, sblock, sizeof(sblock)) != sizeof(sblock))
			errx(1, "%s: can't read superblock", fs);
		sbp = (struct fs *)sblock;
		if ((sbp->fs_magic == FS_UFS1_MAGIC ||
		     (sbp->fs_magic == FS_UFS2_MAGIC &&
		      sbp->fs_sblockloc == sblock_try[i])) &&
		    sbp->fs_bsize <= MAXBSIZE &&
		    sbp->fs_bsize >= (int)sizeof(struct fs))
			break;
	}
	if (sblock_try[i] == -1) {
		fprintf(stderr, "Cannot find file system superblock\n");
		exit(2);
	}
	if ((sbp->fs_metackhash & CK_SUPERBLOCK) == 0) {
		fprintf(stderr, "file system superblock has no check hash\n");
		exit(3);
	}
	printf("zeroing superblock checksum at location %d\n", sblock_try[i]);
	sbp->fs_ckhash = 0;
	if (lseek(fd, (off_t)(sblock_try[i]), SEEK_SET) < 0)
		err(1, "%s", fs);
	if (write(fd, sblock, sizeof(sblock)) != sizeof(sblock))
		errx(1, "%s: can't write superblock", fs);
	(void)close(fd);
	exit(0);
}
