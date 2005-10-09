/*
 * This program, created 2002-10-03 by Garrett A. Wollman
 * <wollman@FreeBSD.org>, is in the public domain.  Use at your own risk.
 *
 * $FreeBSD$
 */

#include <sys/param.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static union {
	char buf[SBLOCKSIZE];
	struct fs sblock;
} u;

int
main(int argc, char **argv)
{
	off_t end, last;
	size_t len;
	ssize_t justread;
	int fd;

	if (argv[1] == NULL)
		errx(1, "usage");

	fd = open(argv[1], O_RDONLY, 0);
	if (fd < 0)
		err(1, "%s", argv[1]);

	end = len = 0;
	last = -1;
	while (1) {
		justread = read(fd, &u.buf[len], DEV_BSIZE);
		if (justread != DEV_BSIZE) {
			if (justread == 0) {
				printf("reached end-of-file at %jd\n",
				       (intmax_t)end);
				exit (0);
			}
			if (justread < 0)
				err(1, "read");
			errx(1, "short read %jd (wanted %d) at %jd",
			     (intmax_t)justread, DEV_BSIZE, (intmax_t)end);
		}
		len += DEV_BSIZE;
		end += DEV_BSIZE;
		if (len >= sizeof(struct fs)) {
			intmax_t offset = end - len;

			if (u.sblock.fs_magic == FS_UFS1_MAGIC) {
				intmax_t fsbegin = offset - SBLOCK_UFS1;
				printf("Found UFS1 superblock at offset %jd, "
				       "block %jd\n", offset,
				       offset / DEV_BSIZE);
				printf("Filesystem might begin at offset %jd, "
				       "block %jd\n", fsbegin,
				       fsbegin / DEV_BSIZE);
				if (last >= 0) {
					printf("%jd blocks from last guess\n",
					       fsbegin / DEV_BSIZE - last);
				}
				last = fsbegin / DEV_BSIZE;
			} else if (u.sblock.fs_magic == FS_UFS2_MAGIC) {
				intmax_t fsbegin = offset - SBLOCK_UFS1;
				printf("Found UFS2 superblock at offset %jd, "
				       "block %jd\n", offset,
				       offset / DEV_BSIZE);
				printf("Filesystem might begin at offset %jd, "
				       "block %jd\n", fsbegin,
				       fsbegin / DEV_BSIZE);
				if (last >= 0) {
					printf("%jd blocks from last guess\n",
					       fsbegin / DEV_BSIZE - last);
				}
				last = fsbegin / DEV_BSIZE;
			}
		}
		if (len >= SBLOCKSIZE) {
			memmove(u.buf, &u.buf[DEV_BSIZE], 
				SBLOCKSIZE - DEV_BSIZE);
			len -= DEV_BSIZE;
		}
	}
}
