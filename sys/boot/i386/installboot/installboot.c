/* $NetBSD: installboot.c,v 1.5 1997/11/01 06:49:50 lukem Exp $	 */

/*
 * Copyright (c) 1994 Paul Kranenburg
 * All rights reserved.
 * Copyright (c) 1996, 1997
 * 	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1996, 1997
 * 	Perry E. Metzger.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 *	This product includes software developed for the NetBSD Project
 *	by Perry E. Metzger.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/disklabel.h>
/* #include <sys/dkio.h> */
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>
#include <sys/errno.h>
#include <err.h>
#include <a.out.h>
#include <fcntl.h>
#include <nlist.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "installboot.h"

#include "bbinfo.h"

#define DEFBBLKNAME "boot"

struct fraglist *fraglist;

struct nlist nl[] = {
#define X_fraglist	0
	{{"_fraglist"}},
	{{NULL}}
};

int verbose = 0;

char *
loadprotoblocks(fname, size)
	char *fname;
	long *size;
{
	int fd;
	size_t tdsize;	/* text+data size */
	size_t bbsize;	/* boot block size (block aligned) */
	char *bp;
	struct nlist *nlp;
	struct exec eh;

	fd = -1;
	bp = NULL;

	/* Locate block number array in proto file */
	if (nlist(fname, nl) != 0) {
		warnx("nlist: %s: symbols not found", fname);
		return NULL;
	}
	/* Validate symbol types (global text!). */
	for (nlp = nl; nlp->n_un.n_name; nlp++) {
		if (nlp->n_type != (N_TEXT | N_EXT)) {
			warnx("nlist: %s: wrong type", nlp->n_un.n_name);
			return NULL;
		}
	}

	if ((fd = open(fname, O_RDONLY)) < 0) {
		warn("open: %s", fname);
		return NULL;
	}
	if (read(fd, &eh, sizeof(eh)) != sizeof(eh)) {
		warn("read: %s", fname);
		goto bad;
	}
	if (N_GETMAGIC(eh) != OMAGIC) {
		warn("bad magic: 0x%lx", eh.a_midmag);
		goto bad;
	}
	/*
	 * We need only text and data.
	 */
	tdsize = eh.a_text + eh.a_data;
	bbsize = roundup(tdsize, DEV_BSIZE);

	if ((bp = calloc(bbsize, 1)) == NULL) {
		warnx("malloc: %s: no memory", fname);
		goto bad;
	}
	/* read the rest of the file. */
	if (read(fd, bp, tdsize) != tdsize) {
		warn("read: %s", fname);
		goto bad;
	}
	*size = bbsize;		/* aligned to DEV_BSIZE */

	fraglist = (struct fraglist *) (bp + nl[X_fraglist].n_value);

	if (fraglist->magic != FRAGLISTMAGIC) {
		warnx("invalid bootblock version");
		goto bad;
	}
	if (verbose) {
		fprintf(stderr, "%s: entry point %#lx\n", fname, eh.a_entry);
		fprintf(stderr, "proto bootblock size %ld\n", *size);
		fprintf(stderr, "room for %d filesystem blocks at %#lx\n",
			fraglist->maxentries, nl[X_fraglist].n_value);
	}
	close(fd);
	return bp;

bad:
	if (bp)
		free(bp);
	if (fd >= 0)
		close(fd);
	return NULL;
}

static int
devread(fd, buf, blk, size, msg)
	int fd;
	void *buf;
	daddr_t blk;
	size_t size;
	char *msg;
{
	if (lseek(fd, dbtob(blk), SEEK_SET) != dbtob(blk)) {
		warn("%s: devread: lseek", msg);
		return (1);
	}
	if (read(fd, buf, size) != size) {
		warn("%s: devread: read", msg);
		return (1);
	}
	return (0);
}

/* add file system blocks to fraglist */
static int
add_fsblk(fs, blk, blcnt)
	struct fs *fs;
	daddr_t blk;
	int blcnt;
{
	int nblk;

	/* convert to disk blocks */
	blk = fsbtodb(fs, blk);
	nblk = fs->fs_bsize / DEV_BSIZE;
	if (nblk > blcnt)
		nblk = blcnt;

	if (verbose)
		fprintf(stderr, "dblk: %d, num: %d\n", blk, nblk);

	/* start new entry or append to previous? */
	if (!fraglist->numentries ||
	    (fraglist->entries[fraglist->numentries - 1].offset
	     + fraglist->entries[fraglist->numentries - 1].num != blk)) {

		/* need new entry */
	        if (fraglist->numentries > fraglist->maxentries - 1) {
			errx(1, "not enough fragment space in bootcode\n");
			return(-1);
		}

		fraglist->entries[fraglist->numentries].offset = blk;
		fraglist->entries[fraglist->numentries++].num = 0;
	}
	fraglist->entries[fraglist->numentries - 1].num += nblk;

	return (blcnt - nblk);
}

static char sblock[SBSIZE];

int
loadblocknums(diskdev, inode)
	char *diskdev;
	ino_t inode;
{
	int devfd = -1;
	struct fs *fs;
	char *buf = 0;
	daddr_t blk, *ap;
	struct dinode *ip;
	int i, ndb;
	int allok = 0;

	devfd = open(diskdev, O_RDONLY, 0);
	if (devfd < 0) {
		warn("open raw partition");
		return (1);
	}
	/* Read superblock */
	if (devread(devfd, sblock, SBLOCK, SBSIZE, "superblock"))
		goto out;
	fs = (struct fs *) sblock;

	if (fs->fs_magic != FS_MAGIC) {
		warnx("invalid super block");
		goto out;
	}
	/* Read inode */
	if ((buf = malloc(fs->fs_bsize)) == NULL) {
		warnx("No memory for filesystem block");
		goto out;
	}
	blk = fsbtodb(fs, ino_to_fsba(fs, inode));
	if (devread(devfd, buf, blk, fs->fs_bsize, "inode"))
		goto out;
	ip = (struct dinode *) (buf) + ino_to_fsbo(fs, inode);

	/*
	 * Have the inode.  Figure out how many blocks we need.
	 */
	ndb = ip->di_size / DEV_BSIZE;	/* size is rounded! */

	if (verbose)
		fprintf(stderr, "Will load %d blocks.\n", ndb);

	/*
	 * Get the block numbers, first direct blocks
	 */
	ap = ip->di_db;
	for (i = 0; i < NDADDR && *ap && ndb > 0; i++, ap++)
		ndb = add_fsblk(fs, *ap, ndb);

	if (ndb > 0) {
		/*
	         * Just one level of indirections; there isn't much room
	         * for more in the 1st-level bootblocks anyway.
	         */
		blk = fsbtodb(fs, ip->di_ib[0]);
		if (devread(devfd, buf, blk, fs->fs_bsize, "indirect block"))
			goto out;
		ap = (daddr_t *) buf;
		for (; i < NINDIR(fs) && *ap && ndb > 0; i++, ap++) {
			ndb = add_fsblk(fs, *ap, ndb);
		}
	}

	if (!ndb)
	    allok = 1;
	else {
	    if (ndb > 0)
		warnx("too many fs blocks");
	    /* else, ie ndb < 0, add_fsblk returned error */
	    goto out;
	}

out:
	if (buf)
		free(buf);
	if (devfd >= 0)
		close(devfd);
	return (!allok);
}

static void
usage()
{
	fprintf(stderr,
		"usage: installboot [-n] [-v] [-f] <boot> <device>\n");
	exit(1);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	char c, *bp = 0;
	long size;
	ino_t inode = (ino_t) -1;
	int devfd = -1;
	struct disklabel dl;
	int bsdoffs;
	int i, res;
	int forceifnolabel = 0;
	char *bootblkname = DEFBBLKNAME;
	int nowrite = 0;
	int allok = 0;

	while ((c = getopt(argc, argv, "vnf")) != -1) {
		switch (c) {
		case 'n':
			/* Do not actually write the bootblock to disk */
			nowrite = 1;
			break;
		case 'v':
			/* Chat */
			verbose = 1;
			break;
		case 'f':
			/* assume zero offset if no disklabel */
			forceifnolabel = 1;
			break;
		default:
			usage();
		}
	}

	if (argc - optind != 2) {
		usage();
	}

	bp = loadprotoblocks(argv[optind], &size);
	if (!bp)
		errx(1, "error reading bootblocks");

	fraglist->numentries = 0;

	/* do we need the fraglist? */
	if (size > fraglist->loadsz * DEV_BSIZE) {

		inode = createfileondev(argv[optind + 1], bootblkname,
					bp + fraglist->loadsz * DEV_BSIZE,
					size - fraglist->loadsz * DEV_BSIZE);
		if (inode == (ino_t) - 1)
			goto out;

		/* paranoia */
		sync();
		sleep(3);

		if (loadblocknums(argv[optind + 1], inode))
			goto out;

		size = fraglist->loadsz * DEV_BSIZE;
		/* size to be written to bootsect */
	}

	devfd = open(argv[optind + 1], O_RDWR, 0);
	if (devfd < 0) {
		warn("open raw partition RW");
		goto out;
	}
	if (ioctl(devfd, DIOCGDINFO, &dl) < 0) {
		if ((errno == EINVAL) || (errno == ENOTTY)) {
			if (forceifnolabel)
				bsdoffs = 0;
			else {
				warnx("no disklabel, use -f to install anyway");
				goto out;
			}
		} else {
			warn("get disklabel");
			goto out;
		}
	} else {
		char c = argv[optind + 1][strlen(argv[optind + 1]) - 1];
#define isvalidpart(c) ((c) >= 'a' && (c) <= 'z')
		if(!isvalidpart(c) || (c - 'a') >= dl.d_npartitions) {
			warnx("invalid partition");
			goto out;
		}
		bsdoffs = dl.d_partitions[c - 'a'].p_offset;
	}
	if (verbose)
		fprintf(stderr, "BSD partition starts at sector %d\n", bsdoffs);

	/*
         * add offset of BSD partition to fraglist entries
         */
	for (i = 0; i < fraglist->numentries; i++)
		fraglist->entries[i].offset += bsdoffs;

	if (!nowrite) {
		/*
	         * write first blocks (max loadsz) to start of BSD partition,
	         * skip disklabel (in second disk block)
	         */
		lseek(devfd, 0, SEEK_SET);
		res = write(devfd, bp, DEV_BSIZE);
		if (res < 0) {
			warn("final write1");
			goto out;
		}
		lseek(devfd, 2 * DEV_BSIZE, SEEK_SET);
		res = write(devfd, bp + 2 * DEV_BSIZE, size - 2 * DEV_BSIZE);
		if (res < 0) {
			warn("final write2");
			goto out;
		}
	}
	allok = 1;

out:
	if (devfd >= 0)
		close(devfd);
	if (bp)
		free(bp);
	if (inode != (ino_t) - 1) {
		cleanupfileondev(argv[optind + 1], bootblkname, !allok || nowrite);
	}
	return (!allok);
}
