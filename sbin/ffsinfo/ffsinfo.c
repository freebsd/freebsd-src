/*
 * Copyright (c) 2000 Christoph Herrmann, Thomas-Henning von Kamptz
 * Copyright (c) 1980, 1989, 1993 The Regents of the University of California.
 * All rights reserved.
 * 
 * This code is derived from software contributed to Berkeley by
 * Christoph Herrmann and Thomas-Henning von Kamptz, Munich and Frankfurt.
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
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors, as well as Christoph
 *      Herrmann and Thomas-Henning von Kamptz.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $TSHeader: src/sbin/ffsinfo/ffsinfo.c,v 1.4 2000/12/12 19:30:55 tomsoft Exp $
 * $FreeBSD$
 *
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 2000 Christoph Herrmann, Thomas-Henning von Kamptz\n\
Copyright (c) 1980, 1989, 1993 The Regents of the University of California.\n\
All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/* ********************************************************** INCLUDES ***** */
#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/stat.h>

#include <stdio.h>
#include <paths.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"

/* *********************************************************** GLOBALS ***** */
#ifdef FS_DEBUG
int	_dbg_lvl_ = (DL_INFO); /* DL_TRC */
#endif /* FS_DEBUG */

static union {
	struct fs	fs;
	char	pad[SBSIZE];
} fsun1, fsun2;
#define	sblock	fsun1.fs
#define	osblock	fsun2.fs

static union {
	struct cg	cg;
	char	pad[MAXBSIZE];
} cgun1;
#define	acg	cgun1.cg

static char	ablk[MAXBSIZE];
static char	i1blk[MAXBSIZE];
static char	i2blk[MAXBSIZE];
static char	i3blk[MAXBSIZE];

static struct csum	*fscs;

/* ******************************************************** PROTOTYPES ***** */
static void	rdfs(daddr_t, size_t, void *, int);
static void	usage(void);
static struct disklabel	*get_disklabel(int);
static struct dinode	*ginode(ino_t, int);
static void	dump_whole_inode(ino_t, int, int);

/* ************************************************************** rdfs ***** */
/*
 * Here we read some block(s) from disk.
 */
void
rdfs(daddr_t bno, size_t size, void *bf, int fsi)
{
	DBG_FUNC("rdfs")
	ssize_t	n;

	DBG_ENTER;

	if (lseek(fsi, (off_t)bno * DEV_BSIZE, 0) < 0) {
		err(33, "rdfs: seek error: %ld", (long)bno);
	}
	n = read(fsi, bf, size);
	if (n != (ssize_t)size) {
		err(34, "rdfs: read error: %ld", (long)bno);
	}

	DBG_LEAVE;
	return;
}

/* ************************************************************** main ***** */
/*
 * ffsinfo(8) is a tool to dump all metadata of a filesystem. It helps to find
 * errors is the filesystem much easier. You can run ffsinfo before and  after
 * an  fsck(8),  and compare the two ascii dumps easy with diff, and  you  see
 * directly where the problem is. You can control how much detail you want  to
 * see  with some command line arguments. You can also easy check  the  status
 * of  a filesystem, like is there is enough space for growing  a  filesystem,
 * or  how  many active snapshots do we have. It provides much  more  detailed
 * information  then dumpfs. Snapshots, as they are very new, are  not  really
 * supported.  They  are just mentioned currently, but it is  planned  to  run
 * also over active snapshots, to even get that output.
 */
int
main(int argc, char **argv)
{
	DBG_FUNC("main")
	char	*device, *special, *cp;
	char	ch;
	size_t	len;
	struct stat	st;
	struct disklabel	*lp;
	struct partition	*pp;
	int	fsi;
	struct csum	*dbg_csp;
	int	dbg_csc;
	char	dbg_line[80];
	int	cylno,i;
	int	cfg_cg, cfg_in, cfg_lv;
	int	cg_start, cg_stop;
	ino_t	in;
	char	*out_file;
	int	Lflag=0;

	DBG_ENTER;

	cfg_lv=0xff;
	cfg_in=-2;
	cfg_cg=-2;
	out_file=strdup("/var/tmp/ffsinfo");
	if(out_file == NULL) {
		errx(1, "strdup failed");
	}

	while ((ch=getopt(argc, argv, "Lg:i:l:o:")) != -1) {
		switch(ch) {
		case 'L':
			Lflag=1;
			break;
		case 'g':
			cfg_cg=atol(optarg);
			if(cfg_cg < -1) {
				usage();
			}
			break;
		case 'i':
			cfg_in=atol(optarg);
			if(cfg_in < 0) {
				usage();
			}
			break; 
		case 'l':
			cfg_lv=atol(optarg);
			if(cfg_lv < 0x1||cfg_lv > 0x3ff) {
				usage();
			}
			break;
		case 'o':
			free(out_file);
			out_file=strdup(optarg);
			if(out_file == NULL) {
				errx(1, "strdup failed");
			}
			break;
		case '?':
			/* FALLTHROUGH */
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if(argc != 1) {
		usage();
	}
	device=*argv;
	
	/*
	 * Now we try to guess the (raw)device name.
	 */
	if (0 == strrchr(device, '/') && (stat(device, &st) == -1)) {
		/*
		 * No path prefix was given, so try in that order:
		 *     /dev/r%s
		 *     /dev/%s
		 *     /dev/vinum/r%s
		 *     /dev/vinum/%s.
		 * 
		 * FreeBSD now doesn't distinguish between raw and  block
		 * devices any longer, but it should still work this way.
		 */
		len=strlen(device)+strlen(_PATH_DEV)+2+strlen("vinum/");
		special=(char *)malloc(len);
		if(special == NULL) {
			errx(1, "malloc failed");
		}
		snprintf(special, len, "%sr%s", _PATH_DEV, device);
		if (stat(special, &st) == -1) {
			snprintf(special, len, "%s%s", _PATH_DEV, device);
			if (stat(special, &st) == -1) {
				snprintf(special, len, "%svinum/r%s",
				    _PATH_DEV, device);
				if (stat(special, &st) == -1) {
					/*
					 * For now this is the 'last resort'.
					 */
					snprintf(special, len, "%svinum/%s",
					    _PATH_DEV, device);
				}
			}
		}
		device = special;
	}

	/*
	 * Open our device for reading.
	 */
	fsi = open(device, O_RDONLY);
	if (fsi < 0) {
		err(1, "%s", device);
	}

	stat(device, &st);
	
	if(S_ISREG(st.st_mode)) { /* label check not supported for files */
		Lflag=1;
	}

	if(!Lflag) {
		/*
		 * Try  to read a label and gess the slice if not  specified.
		 * This code should guess the right thing and avaid to bother
		 * the user user with the task of specifying the option -v on
		 * vinum volumes.
		 */
		cp=device+strlen(device)-1;
		lp = get_disklabel(fsi);
		if(lp->d_type == DTYPE_VINUM) {
			pp = &lp->d_partitions[0];
		} else if (isdigit(*cp)) {
			pp = &lp->d_partitions[2];
		} else if (*cp>='a' && *cp<='h') {
			pp = &lp->d_partitions[*cp - 'a'];
		} else {
			errx(1, "unknown device");
		}
	
		/*
		 * Check if that partition looks suited for dumping.
		 */
		if (pp->p_size < 1) {
			errx(1, "partition is unavailable");
		}
		if (pp->p_fstype != FS_BSDFFS) {
			errx(1, "partition not 4.2BSD");
		}
	}

	/*
	 * Read the current superblock.
	 */
	rdfs((daddr_t)(SBOFF/DEV_BSIZE), (size_t)SBSIZE, (void *)&sblock, fsi);
	if (sblock.fs_magic != FS_MAGIC) {
		errx(1, "superblock not recognized");
	}

	DBG_OPEN(out_file); /* already here we need a superblock */

	if(cfg_lv & 0x001) {
		DBG_DUMP_FS(&sblock,
		    "primary sblock");
	}

	/*
	 * Determine here what cylinder groups to dump.
	 */
	if(cfg_cg==-2) {
		cg_start=0;
		cg_stop=sblock.fs_ncg;
	} else if (cfg_cg==-1) {
		cg_start=sblock.fs_ncg-1;
		cg_stop=sblock.fs_ncg;
	} else if (cfg_cg<sblock.fs_ncg) {
		cg_start=cfg_cg;
		cg_stop=cfg_cg+1;
	} else {
		cg_start=sblock.fs_ncg;
		cg_stop=sblock.fs_ncg;
	}

	if (cfg_lv & 0x004) {
		fscs = (struct csum *)calloc((size_t)1,
		    (size_t)sblock.fs_cssize);
		if(fscs == NULL) {
			errx(1, "calloc failed");
		}

		/*
		 * Get the cylinder summary into the memory ...
		 */
		for (i = 0; i < sblock.fs_cssize; i += sblock.fs_bsize) {
			rdfs(fsbtodb(&sblock, sblock.fs_csaddr +
			    numfrags(&sblock, i)), (size_t)(sblock.fs_cssize-i<
			    sblock.fs_bsize ? sblock.fs_cssize - i :
			    sblock.fs_bsize), (void *)(((char *)fscs)+i), fsi);
		}

		dbg_csp=fscs;
		/*
		 * ... and dump it.
		 */
		for(dbg_csc=0; dbg_csc<sblock.fs_ncg; dbg_csc++) {
			snprintf(dbg_line, sizeof(dbg_line),
			    "%d. csum in fscs", dbg_csc);
			DBG_DUMP_CSUM(&sblock,
			    dbg_line,
			    dbg_csp++);
		}
	}

	/*
	 * For each requested cylinder group ...
	 */
	for(cylno=cg_start; cylno<cg_stop; cylno++) {
		snprintf(dbg_line, sizeof(dbg_line), "cgr %d", cylno);
		if(cfg_lv & 0x002) {
			/*
			 * ... dump the superblock copies ...
			 */
			rdfs(fsbtodb(&sblock, cgsblock(&sblock, cylno)),
			    (size_t)SBSIZE, (void *)&osblock, fsi);
			DBG_DUMP_FS(&osblock,
			    dbg_line);
		}
		/*
		 * ... read the cylinder group and dump whatever was requested.
		 */
		rdfs(fsbtodb(&sblock, cgtod(&sblock, cylno)),
		    (size_t)sblock.fs_cgsize, (void *)&acg, fsi);
		if(cfg_lv & 0x008) {
			DBG_DUMP_CG(&sblock,
			    dbg_line,
			    &acg);
		}
		if(cfg_lv & 0x010) {
			DBG_DUMP_INMAP(&sblock,
			    dbg_line,
			    &acg);
		}
		if(cfg_lv & 0x020) {
			DBG_DUMP_FRMAP(&sblock,
			    dbg_line,
			    &acg);
		}
		if(cfg_lv & 0x040) {
			DBG_DUMP_CLMAP(&sblock,
			    dbg_line,
			    &acg);
			DBG_DUMP_CLSUM(&sblock,
			    dbg_line,
			    &acg);
		}
		if(cfg_lv & 0x080) {
			DBG_DUMP_SPTBL(&sblock,
			    dbg_line,
			    &acg);
		}
	}
	/*
	 * Dump the requested inode(s).
	 */
	if(cfg_in != -2) {
		dump_whole_inode((ino_t)cfg_in, fsi, cfg_lv);
	} else {
		for(in=cg_start*sblock.fs_ipg; in<(ino_t)cg_stop*sblock.fs_ipg;
		    in++) {
			dump_whole_inode(in, fsi, cfg_lv);
		}
	}

	DBG_CLOSE;

	close(fsi);

	DBG_LEAVE;
	return 0;
}

/* ************************************************** dump_whole_inode ***** */
/*
 * Here we dump a list of all blocks allocated by this inode. We follow
 * all indirect blocks.
 */
void
dump_whole_inode(ino_t inode, int fsi, int level)
{
	DBG_FUNC("dump_whole_inode")
	struct dinode	*ino;
	int	rb;
	unsigned int	ind2ctr, ind3ctr;
	ufs_daddr_t	*ind2ptr, *ind3ptr;
	char	comment[80];
	
	DBG_ENTER;

	/*
	 * Read the inode from disk/cache.
	 */
	ino=ginode(inode, fsi);

	if(ino->di_nlink==0) {
		DBG_LEAVE;
		return;	/* inode not in use */
	}

	/*
	 * Dump the main inode structure.
	 */
	snprintf(comment, sizeof(comment), "Inode 0x%08x", inode);
	if (level & 0x100) {
		DBG_DUMP_INO(&sblock,
		    comment,
		    ino);
	}

	if (!(level & 0x200)) {
		DBG_LEAVE;
		return;
	}

	/*
	 * Ok, now prepare for dumping all direct and indirect pointers.
	 */
	rb=howmany(ino->di_size, sblock.fs_bsize)-NDADDR;
	if(rb>0) {
		/*
		 * Dump single indirect block.
		 */
		rdfs(fsbtodb(&sblock, ino->di_ib[0]), (size_t)sblock.fs_bsize,
		    (void *)&i1blk, fsi);
		snprintf(comment, sizeof(comment), "Inode 0x%08x: indirect 0",
		    inode);
		DBG_DUMP_IBLK(&sblock,
		    comment,
		    i1blk,
		    (size_t)rb);
		rb-=howmany(sblock.fs_bsize, sizeof(ufs_daddr_t));
	}
	if(rb>0) {
		/*
		 * Dump double indirect blocks.
		 */
		rdfs(fsbtodb(&sblock, ino->di_ib[1]), (size_t)sblock.fs_bsize,
		    (void *)&i2blk, fsi);
		snprintf(comment, sizeof(comment), "Inode 0x%08x: indirect 1",
		    inode);
		DBG_DUMP_IBLK(&sblock,
		    comment,
		    i2blk,
		    howmany(rb, howmany(sblock.fs_bsize, sizeof(ufs_daddr_t))));
		for(ind2ctr=0; ((ind2ctr < howmany(sblock.fs_bsize,
		    sizeof(ufs_daddr_t)))&&(rb>0)); ind2ctr++) {
			ind2ptr=&((ufs_daddr_t *)(void *)&i2blk)[ind2ctr];

			rdfs(fsbtodb(&sblock, *ind2ptr),
			    (size_t)sblock.fs_bsize, (void *)&i1blk, fsi);
			snprintf(comment, sizeof(comment),
			    "Inode 0x%08x: indirect 1->%d", inode, ind2ctr);
			DBG_DUMP_IBLK(&sblock,
			    comment,
			    i1blk,
			    (size_t)rb);
			rb-=howmany(sblock.fs_bsize, sizeof(ufs_daddr_t));
		}
	}
	if(rb>0) {
		/*
		 * Dump triple indirect blocks.
		 */
		rdfs(fsbtodb(&sblock, ino->di_ib[2]), (size_t)sblock.fs_bsize,
		    (void *)&i3blk, fsi);
		snprintf(comment, sizeof(comment), "Inode 0x%08x: indirect 2",
		    inode);
#define SQUARE(a) ((a)*(a))
		DBG_DUMP_IBLK(&sblock,
		    comment,
		    i3blk,
		    howmany(rb,
		      SQUARE(howmany(sblock.fs_bsize, sizeof(ufs_daddr_t)))));
#undef SQUARE
		for(ind3ctr=0; ((ind3ctr < howmany(sblock.fs_bsize,
		    sizeof(ufs_daddr_t)))&&(rb>0)); ind3ctr ++) {
			ind3ptr=&((ufs_daddr_t *)(void *)&i3blk)[ind3ctr];

			rdfs(fsbtodb(&sblock, *ind3ptr),
			    (size_t)sblock.fs_bsize, (void *)&i2blk, fsi);
			snprintf(comment, sizeof(comment),
			    "Inode 0x%08x: indirect 2->%d", inode, ind3ctr);
			DBG_DUMP_IBLK(&sblock,
			    comment,
			    i2blk,
			    howmany(rb,
			      howmany(sblock.fs_bsize, sizeof(ufs_daddr_t))));
			for(ind2ctr=0; ((ind2ctr < howmany(sblock.fs_bsize,
			    sizeof(ufs_daddr_t)))&&(rb>0)); ind2ctr ++) {
				ind2ptr=&((ufs_daddr_t *)(void *)&i2blk)
				    [ind2ctr];
				rdfs(fsbtodb(&sblock, *ind2ptr),
				    (size_t)sblock.fs_bsize, (void *)&i1blk,
				    fsi);
				snprintf(comment, sizeof(comment),
				    "Inode 0x%08x: indirect 2->%d->%d", inode,
				    ind3ctr, ind3ctr);
				DBG_DUMP_IBLK(&sblock,
				    comment,
				    i1blk,
				    (size_t)rb);
				rb-=howmany(sblock.fs_bsize,
				    sizeof(ufs_daddr_t));
			}
		}
	}

	DBG_LEAVE;
	return;
}

/* ***************************************************** get_disklabel ***** */
/*
 * Read the disklabel from disk.
 */
struct disklabel *
get_disklabel(int fd)
{
	DBG_FUNC("get_disklabel")
	static struct disklabel	*lab;

	DBG_ENTER;

	lab=(struct disklabel *)malloc(sizeof(struct disklabel));
	if (!lab) {
		errx(1, "malloc failed");
	}
	if (ioctl(fd, DIOCGDINFO, (char *)lab) < 0) {
		errx(1, "DIOCGDINFO failed");
		exit(-1);
	}

	DBG_LEAVE;
	return (lab);
}


/* ************************************************************* usage ***** */
/*
 * Dump a line of usage.
 */
void
usage(void)
{
	DBG_FUNC("usage")	

	DBG_ENTER;

	fprintf(stderr,
	    "usage: ffsinfo [-L] [-g cylgrp] [-i inode] [-l level] "
	    "[-o outfile]\n"
	    "               special | file\n");

	DBG_LEAVE;
	exit(1);
}

/* ************************************************************ ginode ***** */
/*
 * This function provides access to an individual inode. We find out in which
 * block  the  requested inode is located, read it from disk if  needed,  and
 * return  the pointer into that block. We maintain a cache of one  block  to
 * not  read the same block again and again if we iterate linearly  over  all
 * inodes.
 */
struct dinode *
ginode(ino_t inumber, int fsi)
{
	DBG_FUNC("ginode")
	ufs_daddr_t	iblk;
	static ino_t	startinum=0;	/* first inode in cached block */
	struct dinode	*pi;

	DBG_ENTER;

	pi=(struct dinode *)(void *)ablk;
	if (startinum == 0 || inumber < startinum ||
	    inumber >= startinum + INOPB(&sblock)) {
		/*
		 * The block needed is not cached, so we have to read it from
		 * disk now.
		 */
		iblk = ino_to_fsba(&sblock, inumber);
		rdfs(fsbtodb(&sblock, iblk), (size_t)sblock.fs_bsize,
		    (void *)&ablk, fsi);
		startinum = (inumber / INOPB(&sblock)) * INOPB(&sblock);
	}

	DBG_LEAVE;
	return (&(pi[inumber % INOPB(&sblock)]));
}

