/*
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and Network Associates Laboratories, the Security
 * Research Division of Network Associates, Inc. under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS
 * research program
 *
 * Copyright (c) 1982, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * Copyright (c) 1983, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1983, 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)dumpfs.c	8.5 (Berkeley) 4/29/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/disklabel.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <libufs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define	afs	disk.d_fs

union {
	struct cg cg;
	char pad[MAXBSIZE];
} cgun;
#define	acg	cgun.cg

struct uufsd disk;

int	dumpfs(const char *);
int	dumpcg(int);
void	pbits(void *, int);
void	usage(void) __dead2;

int
main(int argc, char *argv[])
{
	struct fstab *fs;
	int ch, eval;

	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	for (eval = 0; *argv; ++argv)
		if ((fs = getfsfile(*argv)) == NULL)
			eval |= dumpfs(*argv);
		else
			eval |= dumpfs(fs->fs_spec);
	exit(eval);
}

int
dumpfs(const char *name)
{
	time_t time;
	int64_t fssize;
	int i;

	if (ufs_disk_fillout(&disk, name) == -1)
			goto err;

	switch (disk.d_ufs) {
	case 2:
		fssize = afs.fs_size;
		time = afs.fs_time;
		printf("magic\t%x (UFS2)\ttime\t%s",
		    afs.fs_magic, ctime(&time));
		printf("offset\t%qd\tid\t[ %x %x ]\n",
		    afs.fs_sblockloc, afs.fs_id[0], afs.fs_id[1]);
		printf("ncg\t%d\tsize\t%qd\tblocks\t%d\n",
		    afs.fs_ncg, fssize, afs.fs_dsize);
		break;
	case 1:
		fssize = afs.fs_old_size;
		printf("magic\t%x (UFS1)\ttime\t%s",
		    afs.fs_magic, ctime(&afs.fs_old_time));
		printf("id\t[ %x %x ]\n", afs.fs_id[0], afs.fs_id[1]);
		printf("ncg\t%d\tsize\t%qd\tblocks\t%d\n",
		    afs.fs_ncg, fssize, afs.fs_dsize);
		break;
	default:
		break;
	}
	printf("bsize\t%d\tshift\t%d\tmask\t0x%08x\n",
	    afs.fs_bsize, afs.fs_bshift, afs.fs_bmask);
	printf("fsize\t%d\tshift\t%d\tmask\t0x%08x\n",
	    afs.fs_fsize, afs.fs_fshift, afs.fs_fmask);
	printf("frag\t%d\tshift\t%d\tfsbtodb\t%d\n",
	    afs.fs_frag, afs.fs_fragshift, afs.fs_fsbtodb);
	printf("minfree\t%d%%\toptim\t%s\tsymlinklen %d\n",
	    afs.fs_minfree, afs.fs_optim == FS_OPTSPACE ? "space" : "time",
	    afs.fs_maxsymlinklen);
	switch (disk.d_ufs) {
	case 2:
		printf("%s %d\tmaxbpg\t%d\tmaxcontig %d\tcontigsumsize %d\n",
		    "maxbsize", afs.fs_maxbsize, afs.fs_maxbpg,
		    afs.fs_maxcontig, afs.fs_contigsumsize);
		printf("nbfree\t%qd\tndir\t%qd\tnifree\t%qd\tnffree\t%qd\n",
		    afs.fs_cstotal.cs_nbfree, afs.fs_cstotal.cs_ndir,
		    afs.fs_cstotal.cs_nifree, afs.fs_cstotal.cs_nffree);
		printf("bpg\t%d\tfpg\t%d\tipg\t%d\n",
		    afs.fs_fpg / afs.fs_frag, afs.fs_fpg, afs.fs_ipg);
		printf("nindir\t%d\tinopb\t%d\tmaxfilesize\t%qu\n",
		    afs.fs_nindir, afs.fs_inopb, afs.fs_maxfilesize);
		printf("sbsize\t%d\tcgsize\t%d\tcsaddr\t%d\tcssize\t%d\n",
		    afs.fs_sbsize, afs.fs_cgsize, afs.fs_csaddr, afs.fs_cssize);
		break;
	case 1:
		printf("maxbpg\t%d\tmaxcontig %d\tcontigsumsize %d\n",
		    afs.fs_maxbpg, afs.fs_maxcontig, afs.fs_contigsumsize);
		printf("nbfree\t%d\tndir\t%d\tnifree\t%d\tnffree\t%d\n",
		    afs.fs_old_cstotal.cs_nbfree, afs.fs_old_cstotal.cs_ndir,
		    afs.fs_old_cstotal.cs_nifree, afs.fs_old_cstotal.cs_nffree);
		printf("cpg\t%d\tbpg\t%d\tfpg\t%d\tipg\t%d\n",
		    afs.fs_old_cpg, afs.fs_fpg / afs.fs_frag, afs.fs_fpg,
		    afs.fs_ipg);
		printf("nindir\t%d\tinopb\t%d\tnspf\t%d\tmaxfilesize\t%qu\n",
		    afs.fs_nindir, afs.fs_inopb, afs.fs_old_nspf,
		    afs.fs_maxfilesize);
		printf("sbsize\t%d\tcgsize\t%d\tcgoffset %d\tcgmask\t0x%08x\n",
		    afs.fs_sbsize, afs.fs_cgsize, afs.fs_old_cgoffset,
		    afs.fs_old_cgmask);
		printf("csaddr\t%d\tcssize\t%d\n",
		    afs.fs_old_csaddr, afs.fs_cssize);
		printf("rotdelay %dms\trps\t%d\ttrackskew %d\tinterleave %d\n",
		    afs.fs_old_rotdelay, afs.fs_old_rps, afs.fs_old_trackskew,
		    afs.fs_old_interleave);
		printf("nsect\t%d\tnpsect\t%d\tspc\t%d\n",
		    afs.fs_old_nsect, afs.fs_old_npsect, afs.fs_old_spc);
		break;
	default:
		break;
	}
	printf("sblkno\t%d\tcblkno\t%d\tiblkno\t%d\tdblkno\t%d\n",
	    afs.fs_sblkno, afs.fs_cblkno, afs.fs_iblkno, afs.fs_dblkno);
	printf("cgrotor\t%d\tfmod\t%d\tronly\t%d\tclean\t%d\n",
	    afs.fs_cgrotor, afs.fs_fmod, afs.fs_ronly, afs.fs_clean);
	printf("flags\t");
	if (afs.fs_flags == 0)
		printf("none");
	if (afs.fs_flags & FS_UNCLEAN)
			printf("unclean ");
	if (afs.fs_flags & FS_DOSOFTDEP)
			printf("soft-updates ");
	if (afs.fs_flags & FS_NEEDSFSCK)
			printf("needs fsck run ");
	if (afs.fs_flags & FS_INDEXDIRS)
			printf("indexed directories ");
	if ((afs.fs_flags &
	    ~(FS_UNCLEAN | FS_DOSOFTDEP | FS_NEEDSFSCK | FS_INDEXDIRS)) != 0)
			printf("unknown flags (%#x)", afs.fs_flags &
			    ~(FS_UNCLEAN | FS_DOSOFTDEP |
			      FS_NEEDSFSCK | FS_INDEXDIRS));
	putchar('\n');
	printf("\ncs[].cs_(nbfree,ndir,nifree,nffree):\n\t");
	afs.fs_csp = calloc(1, afs.fs_cssize);
	if (bread(&disk, fsbtodb(&afs, afs.fs_csaddr), afs.fs_csp, afs.fs_cssize) == -1)
		goto err;
	for (i = 0; i < afs.fs_ncg; i++) {
		struct csum *cs = &afs.fs_cs(&afs, i);
		if (i && i % 4 == 0)
			printf("\n\t");
		printf("(%d,%d,%d,%d) ",
		    cs->cs_nbfree, cs->cs_ndir, cs->cs_nifree, cs->cs_nffree);
	}
	printf("\n");
	if (fssize % afs.fs_fpg) {
		if (disk.d_ufs == 1)
			printf("cylinders in last group %d\n",
			    howmany(afs.fs_old_size % afs.fs_fpg,
			    afs.fs_old_spc / afs.fs_old_nspf));
		printf("blocks in last group %d\n\n",
		    (fssize % afs.fs_fpg) / afs.fs_frag);
	}
	for (i = 0; i < afs.fs_ncg; i++)
		if (dumpcg(i))
			goto err;
	ufs_disk_close(&disk);
	return (0);

err:	if (errno || disk.d_error != NULL) {
		if (disk.d_error != NULL)
			warnx("%s: %s", name, disk.d_error);
		else
			warn("%s", name);
	}
	ufs_disk_close(&disk);
	return (1);
}

int
dumpcg(int c)
{
	time_t time;
	off_t cur;
	int i, j;

	printf("\ncg %d:\n", c);
	cur = fsbtodb(&afs, cgtod(&afs, c)) * disk.d_bsize;
	if (bread(&disk, fsbtodb(&afs, cgtod(&afs, c)), &acg, afs.fs_bsize) == -1)
		return (1);
	switch (disk.d_ufs) {
	case 2:
		time = acg.cg_time;
		printf("magic\t%x\ttell\t%qx\ttime\t%s",
		    acg.cg_magic, cur, ctime(&time));
		printf("cgx\t%d\tndblk\t%d\tniblk\t%d\tinitiblk %d\n",
		    acg.cg_cgx, acg.cg_ndblk, acg.cg_niblk, acg.cg_initediblk);
		break;
	case 1:
		printf("magic\t%x\ttell\t%qx\ttime\t%s",
		    acg.cg_magic, cur, ctime(&acg.cg_old_time));
		printf("cgx\t%d\tncyl\t%d\tniblk\t%d\tndblk\t%d\n",
		    acg.cg_cgx, acg.cg_old_ncyl, acg.cg_old_niblk,
		    acg.cg_ndblk);
		break;
	default:
		break;
	}
	printf("nbfree\t%d\tndir\t%d\tnifree\t%d\tnffree\t%d\n",
	    acg.cg_cs.cs_nbfree, acg.cg_cs.cs_ndir,
	    acg.cg_cs.cs_nifree, acg.cg_cs.cs_nffree);
	printf("rotor\t%d\tirotor\t%d\tfrotor\t%d\nfrsum",
	    acg.cg_rotor, acg.cg_irotor, acg.cg_frotor);
	for (i = 1, j = 0; i < afs.fs_frag; i++) {
		printf("\t%d", acg.cg_frsum[i]);
		j += i * acg.cg_frsum[i];
	}
	printf("\nsum of frsum: %d", j);
	if (afs.fs_contigsumsize > 0) {
		for (i = 1; i < afs.fs_contigsumsize; i++) {
			if ((i - 1) % 8 == 0)
				printf("\nclusters %d-%d:", i,
				    afs.fs_contigsumsize - 1 < i + 7 ?
				    afs.fs_contigsumsize - 1 : i + 7);
			printf("\t%d", cg_clustersum(&acg)[i]);
		}
		printf("\nclusters size %d and over: %d\n",
		    afs.fs_contigsumsize,
		    cg_clustersum(&acg)[afs.fs_contigsumsize]);
		printf("clusters free:\t");
		pbits(cg_clustersfree(&acg), acg.cg_nclusterblks);
	} else
		printf("\n");
	printf("inodes used:\t");
	pbits(cg_inosused(&acg), afs.fs_ipg);
	printf("blks free:\t");
	pbits(cg_blksfree(&acg), afs.fs_fpg);
	return (0);
}

void
pbits(void *vp, int max)
{
	int i;
	char *p;
	int count, j;

	for (count = i = 0, p = vp; i < max; i++)
		if (isset(p, i)) {
			if (count)
				printf(",%s", count % 6 ? " " : "\n\t");
			count++;
			printf("%d", i);
			j = i;
			while ((i+1)<max && isset(p, i+1))
				i++;
			if (i != j)
				printf("-%d", i);
		}
	printf("\n");
}

void
usage(void)
{
	(void)fprintf(stderr, "usage: dumpfs filesys | device\n");
	exit(1);
}
