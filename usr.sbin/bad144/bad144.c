/*
 * Copyright (c) 1993, 198019861988
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
"@(#) Copyright (c) 1993, 198019861988\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)bad144.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id: bad144.c,v 1.12.2.1 1997/09/15 06:23:56 charnier Exp $";
#endif /* not lint */

/*
 * bad144
 *
 * This program prints and/or initializes a bad block record for a pack,
 * in the format used by the DEC standard 144.
 * It can also add bad sector(s) to the record, moving the sector
 * replacements as necessary.
 *
 * It is preferable to write the bad information with a standard formatter,
 * but this program will do.
 *
 * RP06 sectors are marked as bad by inverting the format bit in the
 * header; on other drives the valid-sector bit is cleared.
 */
#include <sys/param.h>
#include <sys/dkbad.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/disklabel.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RETRIES	10		/* number of retries on reading old sectors */

int	fflag, add, copy, verbose, nflag, sflag;
int	dups;
int	badfile = -1;		/* copy of badsector table to use, -1 if any */
#define MAXSECSIZE	1024
struct	dkbad curbad, oldbad;
#define	DKBAD_MAGIC	0x4321

daddr_t	size;
struct	disklabel *dp;
char	*name;

u_short	dkcksum __P((struct disklabel *lp));

int	main __P((int argc, char *argv[]));
void	blkzero __P((int f, daddr_t sn));
int	compare __P((const void *cvb1, const void *cvb2));
daddr_t	badsn __P((struct bt_bad *bt));
daddr_t	getold __P((int f, struct dkbad *bad));
int	blkcopy __P((int f, daddr_t s1, daddr_t s2));
void	shift __P((int f, int new, int old));
int	checkold __P((struct dkbad *oldbad));
static void usage __P((void));

void
bad_scan(argc, argv, dp, f, bstart, bend)
	int *argc;
	char ***argv;
	struct disklabel *dp;
	int f;
	daddr_t bstart,bend;
{
	int curr_sec, n;
	int spc = dp->d_secpercyl;
	int ss = dp->d_secsize;
	int trk = dp->d_nsectors;
	int i;
	char **nargv,*buf;
	int nargc;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	nargv = (char **)malloc(sizeof *nargv *  DKBAD_MAXBAD);
	if (!nargv)
		errx(20,"malloc failed");
	i = 1;
	n = ioctl(f,DIOCSBADSCAN,&i);
	if (n < 0)
		warn("couldn't set disk in \"badscan\" mode");
	nargc = *argc;
	memcpy(nargv,*argv,nargc * sizeof nargv[0]);

	buf = alloca((unsigned)(trk*ss));
	if (buf == (char *)NULL)
		errx(20,"alloca failed");

	/* scan the entire disk a sector at a time.  Because of all the
	 * clustering in the kernel, we cannot scan a track at a time,
	 * If we do, we may have to read twice over the block to find
	 * exactly which one failed, and it may not fail second time.
	 */
	for (curr_sec = bstart; curr_sec < size; curr_sec++) {

		if (verbose) {
			if ((curr_sec % spc) == 0)
				printf("\r%7d of %7lu blocks (%3lu%%)",
					curr_sec,bend,(curr_sec*100/bend));
		}

		if (lseek(f, (off_t)ss * curr_sec, SEEK_SET) < 0)
			err(4, "lseek");

		if ((n = read(f, buf, ss)) != ss) {
			if (verbose)
				printf("\rBlock: %7d will be marked BAD.\n",
					curr_sec);
			else
				warnx("found bad sector: %d", curr_sec);
			sprintf(buf,"%d",curr_sec);
			nargv[nargc++] = strdup(buf);
			if (nargc >= DKBAD_MAXBAD)
				errx(1, "too many bad sectors, can only handle %d per slice",
						DKBAD_MAXBAD);
		}
	}
	fprintf(stderr, "\n");
	nargv[nargc] = 0;
	*argc = nargc;
	*argv = &nargv[0];
	i = 0;
	n = ioctl(f,DIOCSBADSCAN,&i);
	if (n < 0)
		warn("couldn't reset disk from \"badscan\" mode");
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register struct bt_bad *bt;
	daddr_t	sn, bn[DKBAD_MAXBAD];
	int i, f, nbad, new, bad, errs;
	daddr_t bstart, bend;
	char	label[BBSIZE];

	argc--, argv++;
	while (argc > 0 && **argv == '-') {
		(*argv)++;
		while (**argv) {
			switch (**argv) {
#if vax
			    case 'f':
				fflag++;
				break;
#endif
			    case 'a':
				add++;
				break;
			    case 'c':
				copy++;
				break;
			    case 'v':
				verbose++;
				break;
			    case 'n':
				nflag++;
				verbose++;
				break;
			    case 's':		/* scan partition */
				sflag++;
				add++;
				break;
			    default:
				if (**argv >= '0' && **argv <= '4') {
					badfile = **argv - '0';
					break;
				}
				usage();
			}
			(*argv)++;
		}
		argc--, argv++;
	}
	if (argc < 1)
		usage();
	if (argv[0][0] != '/')
		(void)sprintf(label, "%sr%s%c", _PATH_DEV, argv[0],
			      'a' + RAW_PART);
	else
		strcpy(label, argv[0]);
	name = strdup(label);
	f = open(name, !sflag && argc == 1? O_RDONLY : O_RDWR);
	if (f < 0)
		err(4, "%s", name);
	if (read(f, label, sizeof(label)) < 0)
		err(4, "read");
	for (dp = (struct disklabel *)(label + LABELOFFSET);
	    dp < (struct disklabel *)
		(label + sizeof(label) - sizeof(struct disklabel));
	    dp = (struct disklabel *)((char *)dp + 64))
		if (dp->d_magic == DISKMAGIC && dp->d_magic2 == DISKMAGIC)
			break;
	if (dp->d_magic != DISKMAGIC || dp->d_magic2 != DISKMAGIC)
		errx(1, "bad pack magic number (pack is unlabeled)");
	if (dp->d_secsize > MAXSECSIZE || dp->d_secsize <= 0)
		errx(7, "disk sector size too large/small (%lu)", dp->d_secsize);
	size = dp->d_secperunit;

	/*
	 * bstart is 0 since we should always be doing partition c of a slice
	 * bend is the size of the slice, less the bad block map track
	 * and the DKBAD_MAXBAD replacement blocks
	 */
	bstart = 0;
	bend = size - (dp->d_nsectors + DKBAD_MAXBAD);

	if (verbose) {
		printf("cyl: %ld, tracks: %ld, secs: %ld, "
			"sec/cyl: %ld, start: %ld, end: %ld\n",
			dp->d_ncylinders, dp->d_ntracks, dp->d_nsectors,
			dp->d_secpercyl, bstart, bend);
	}

	argc--;
	argv++;

	if (sflag)
		bad_scan(&argc,&argv,dp,f,bstart,bend);

	if (argc == 0) {
		sn = getold(f, &oldbad);
		printf("bad block information at sector %ld in %s:\n",
		    sn, name);
		printf("cartridge serial number: %ld(10)\n", oldbad.bt_csn);
		switch (oldbad.bt_flag) {

		case (u_short)-1:
			printf("alignment cartridge\n");
			break;

		case DKBAD_MAGIC:
			break;

		default:
			printf("bt_flag=%x(16)?\n", oldbad.bt_flag);
			break;
		}
		bt = oldbad.bt_bad;
		for (i = 0; i < DKBAD_MAXBAD; i++) {
			bad = (bt->bt_cyl<<16) + bt->bt_trksec;
			if (bad < 0)
				break;
			printf("sn=%ld, cn=%d, tn=%d, sn=%d\n", badsn(bt),
			    bt->bt_cyl, bt->bt_trksec>>8, bt->bt_trksec&0xff);
			bt++;
		}
		(void) checkold(&oldbad);
		exit(0);
	}
	if (add) {
		/*
		 * Read in the old badsector table.
		 * Verify that it makes sense, and the bad sectors
		 * are in order.  Copy the old table to the new one.
		 */
		(void) getold(f, &oldbad);
		i = checkold(&oldbad);
		if (verbose)
			printf("Had %d bad sectors, adding %d\n", i, argc);
		if (i + argc > DKBAD_MAXBAD) {
			printf("bad144: not enough room for %d more sectors\n",
				argc);
			printf("limited to %d by information format\n",
			       DKBAD_MAXBAD);
			exit(1);
		}
		curbad = oldbad;
	} else {
		curbad.bt_csn = atoi(*argv++);
		argc--;
		curbad.bt_mbz = 0;
		curbad.bt_flag = DKBAD_MAGIC;
		if (argc > DKBAD_MAXBAD) {
			printf("bad144: too many bad sectors specified\n");
			printf("limited to %d by information format\n",
			       DKBAD_MAXBAD);
			exit(1);
		}
		i = 0;
	}
	errs = 0;
	new = argc;
	while (argc > 0) {
		sn = atoi(*argv++);
		argc--;
		if (sn < 0 || sn >= bend) {
			printf("%ld: out of range [0,%ld) for disk %s\n",
			    sn, bend, dp->d_typename);
			errs++;
			continue;
		}
		bn[i] = sn;
		curbad.bt_bad[i].bt_cyl = sn / (dp->d_nsectors*dp->d_ntracks);
		sn %= (dp->d_nsectors*dp->d_ntracks);
		curbad.bt_bad[i].bt_trksec =
		    ((sn/dp->d_nsectors) << 8) + (sn%dp->d_nsectors);
		i++;
	}
	if (errs)
		exit(1);
	nbad = i;
	while (i < DKBAD_MAXBAD) {
		curbad.bt_bad[i].bt_trksec = DKBAD_NOTRKSEC;
		curbad.bt_bad[i].bt_cyl = DKBAD_NOCYL;
		i++;
	}
	if (add) {
		/*
		 * Sort the new bad sectors into the list.
		 * Then shuffle the replacement sectors so that
		 * the previous bad sectors get the same replacement data.
		 */
		qsort(curbad.bt_bad, nbad, sizeof (struct bt_bad), compare);
		if (dups)
			errx(3,
			"bad sectors have been duplicated; can't add existing sectors");
		shift(f, nbad, nbad-new);
	}
	if (badfile == -1)
		i = 0;
	else
		i = badfile * 2;
	for (; i < 10 && i < dp->d_nsectors; i += 2) {
		if (lseek(f, (off_t)dp->d_secsize * (size - dp->d_nsectors + i),
			  SEEK_SET) < 0)
			err(4, "lseek");
		if (verbose)
			printf("write badsect file at %lu\n",
				size - dp->d_nsectors + i);
		if (nflag == 0 && write(f, (caddr_t)&curbad, sizeof(curbad)) !=
		    sizeof(curbad)) {
			char msg[80];
			(void)sprintf(msg, "write bad sector file %d", i/2);
			warn("%s", msg);
		}
		if (badfile != -1)
			break;
	}
#ifdef vax
	if (nflag == 0 && fflag)
		for (i = nbad - new; i < nbad; i++)
			format(f, bn[i]);
#endif
	if (nflag == 0 && (dp->d_flags & D_BADSECT) == 0) {
		dp->d_flags |= D_BADSECT;
		dp->d_checksum = 0;
		dp->d_checksum = dkcksum(dp);
		if (ioctl(f, DIOCWDINFO, dp) < 0)
			err(1, "can't write disklabel to enable bad sector handling");
	}
#ifdef DIOCSBAD
	if (nflag == 0 && ioctl(f, DIOCSBAD, (caddr_t)&curbad) < 0)
		warnx("can't sync bad-sector file; reboot for changes to take effect");
#endif
	exit(0);
}

static void
usage()
{
	fprintf(stderr, "%s\n%s\n%s\n",
		"usage: bad144 [-f] disk [snum [bn ...]]",
		"       bad144 -a [-f] [-c] disk  bn ...",
		"       bad144 -s [-v] disk");
		exit(1);
}

daddr_t
getold(f, bad)
	int f;
	struct dkbad *bad;
{
	register int i;
	daddr_t sn;
	char msg[80];

	if (badfile == -1)
		i = 0;
	else
		i = badfile * 2;
	for (; i < 10 && i < dp->d_nsectors; i += 2) {
		sn = size - dp->d_nsectors + i;
		if (lseek(f, dp->d_secsize * (off_t)sn, SEEK_SET) < 0)
			err(4, "lseek");
		if (read(f, (char *) bad, dp->d_secsize) == dp->d_secsize) {
			if (i > 0)
				printf("Using bad-sector file %d\n", i/2);
			return(sn);
		}
		(void)sprintf(msg, "read bad sector file at sn %ld", sn);
		warn("%s", msg);
		if (badfile != -1)
			break;
	}
	errx(1, "%s: can't read bad block info", name);
	/*NOTREACHED*/
}

int
checkold(oldbad)
	struct dkbad *oldbad;
{
	register int i;
	register struct bt_bad *bt;
	daddr_t sn, lsn=0;
	int errors = 0, warned = 0;

	if (oldbad->bt_flag != DKBAD_MAGIC) {
		warnx("%s: bad flag in bad-sector table", name);
		errors++;
	}
	if (oldbad->bt_mbz != 0) {
		warnx("%s: bad magic number", name);
		errors++;
	}
	bt = oldbad->bt_bad;
	for (i = 0; i < DKBAD_MAXBAD; i++, bt++) {
		if (bt->bt_cyl == DKBAD_NOCYL &&
		    bt->bt_trksec == DKBAD_NOTRKSEC)
			break;
		if ((bt->bt_cyl >= dp->d_ncylinders) ||
		    ((bt->bt_trksec >> 8) >= dp->d_ntracks) ||
		    ((bt->bt_trksec & 0xff) >= dp->d_nsectors)) {
			warnx(
	"cyl/trk/sect out of range in existing entry: sn=%ld, cn=%d, tn=%d, sn=%d",
	badsn(bt), bt->bt_cyl, bt->bt_trksec>>8, bt->bt_trksec & 0xff);
			errors++;
		}
		sn = (bt->bt_cyl * dp->d_ntracks +
		    (bt->bt_trksec >> 8)) *
		    dp->d_nsectors + (bt->bt_trksec & 0xff);
		if (i > 0 && sn < lsn && !warned) {
		    warnx("bad sector file is out of order");
		    errors++;
		    warned++;
		}
		if (i > 0 && sn == lsn) {
		    warnx("bad sector file contains duplicates (sn %ld)", sn);
		    errors++;
		}
		lsn = sn;
	}
	if (errors)
		exit(1);
	return (i);
}

/*
 * Move the bad sector replacements
 * to make room for the new bad sectors.
 * new is the new number of bad sectors, old is the previous count.
 */
void
shift(f, new, old)
	int f, new, old;
{
	daddr_t repl;

	/*
	 * First replacement is last sector of second-to-last track.
	 */
	repl = size - dp->d_nsectors - 1;
	new--; old--;
	while (new >= 0 && new != old) {
		if (old < 0 ||
		    compare(&curbad.bt_bad[new], &oldbad.bt_bad[old]) > 0) {
			/*
			 * Insert new replacement here-- copy original
			 * sector if requested and possible,
			 * otherwise write a zero block.
			 */
			if (!copy ||
			    !blkcopy(f, badsn(&curbad.bt_bad[new]), repl - new))
				blkzero(f, repl - new);
		} else {
			if (blkcopy(f, repl - old, repl - new) == 0)
			    warnx("can't copy replacement sector %ld to %ld",
				repl-old, repl-new);
			old--;
		}
		new--;
	}
}

/*
 *  Copy disk sector s1 to s2.
 */
int
blkcopy(f, s1, s2)
	int f;
	daddr_t s1, s2;
{
	register tries, n;
	char *buf;

	buf = alloca((unsigned)dp->d_secsize);
	if (buf == (char *)NULL)
		errx(20, "alloca failed");

	for (tries = 0; tries < RETRIES; tries++) {
		if (lseek(f, (off_t)dp->d_secsize * s1, SEEK_SET) < 0)
			err(4, "lseek");
		if ((n = read(f, buf, dp->d_secsize)) == dp->d_secsize)
			break;
	}
	if (n != dp->d_secsize) {
		if (n < 0)
			warn("can't read sector, %ld", s1);
		else
			warnx("can't read sector, %ld", s1);
		return(0);
	}
	if (lseek(f, (off_t)dp->d_secsize * s2, SEEK_SET) < 0)
		err(4, "lseek");
	if (verbose)
		printf("copying %ld to %ld\n", s1, s2);
	if (nflag == 0 && write(f, buf, dp->d_secsize) != dp->d_secsize) {
		warn("can't write replacement sector, %ld", s2);
		return(0);
	}
	return(1);
}


void
blkzero(f, sn)
	int f;
	daddr_t sn;
{
	char *zbuf;

	zbuf = alloca((unsigned)dp->d_secsize);
	if (zbuf == (char *)NULL)
		errx(20, "alloca failed");

	memset(zbuf, 0, dp->d_secsize);

	if (lseek(f, (off_t)dp->d_secsize * sn, SEEK_SET) < 0)
		err(4, "lseek");
	if (verbose)
		printf("zeroing %ld\n", sn);
	if (nflag == 0 && write(f, zbuf, dp->d_secsize) != dp->d_secsize)
		warn("can't write replacement sector, %ld", sn);
}

int
compare(cvb1, cvb2)
	const void *cvb1, *cvb2;
{
	const struct bt_bad *b1 = cvb1, *b2 = cvb2;

	if (b1->bt_cyl > b2->bt_cyl)
		return(1);
	if (b1->bt_cyl < b2->bt_cyl)
		return(-1);
	if (b1->bt_trksec == b2->bt_trksec)
		dups++;
	return (b1->bt_trksec - b2->bt_trksec);
}

daddr_t
badsn(bt)
	register struct bt_bad *bt;
{
	return ((bt->bt_cyl*dp->d_ntracks + (bt->bt_trksec>>8)) * dp->d_nsectors
		+ (bt->bt_trksec&0xff));
}

#ifdef vax

struct rp06hdr {
	short	h_cyl;
	short	h_trksec;
	short	h_key1;
	short	h_key2;
	char	h_data[512];
#define	RP06_FMT	010000		/* 1 == 16 bit, 0 == 18 bit */
};

/*
 * Most massbus and unibus drives
 * have headers of this form
 */
struct hpuphdr {
	u_short	hpup_cyl;
	u_char	hpup_sect;
	u_char	hpup_track;
	char	hpup_data[512];
#define	HPUP_OKSECT	0xc000		/* this normally means sector is good */
#define	HPUP_16BIT	0x1000		/* 1 == 16 bit format */
};
int rp06format(), hpupformat();

struct	formats {
	char	*f_name;		/* disk name */
	int	f_bufsize;		/* size of sector + header */
	int	f_bic;			/* value to bic in hpup_cyl */
	int	(*f_routine)();		/* routine for special handling */
} formats[] = {
	{ "rp06",	sizeof (struct rp06hdr), RP06_FMT,	rp06format },
	{ "eagle",	sizeof (struct hpuphdr), HPUP_OKSECT,	hpupformat },
	{ "capricorn",	sizeof (struct hpuphdr), HPUP_OKSECT,	hpupformat },
	{ "rm03",	sizeof (struct hpuphdr), HPUP_OKSECT,	hpupformat },
	{ "rm05",	sizeof (struct hpuphdr), HPUP_OKSECT,	hpupformat },
	{ "9300",	sizeof (struct hpuphdr), HPUP_OKSECT,	hpupformat },
	{ "9766",	sizeof (struct hpuphdr), HPUP_OKSECT,	hpupformat },
	{ 0, 0, 0, 0 }
};

/*ARGSUSED*/
hpupformat(fp, dp, blk, buf, count)
	struct formats *fp;
	struct disklabel *dp;
	daddr_t blk;
	char *buf;
	int count;
{
	struct hpuphdr *hdr = (struct hpuphdr *)buf;
	int sect;

	if (count < sizeof(struct hpuphdr)) {
		hdr->hpup_cyl = (HPUP_OKSECT | HPUP_16BIT) |
			(blk / (dp->d_nsectors * dp->d_ntracks));
		sect = blk % (dp->d_nsectors * dp->d_ntracks);
		hdr->hpup_track = (u_char)(sect / dp->d_nsectors);
		hdr->hpup_sect = (u_char)(sect % dp->d_nsectors);
	}
	return (0);
}

/*ARGSUSED*/
rp06format(fp, dp, blk, buf, count)
	struct formats *fp;
	struct disklabel *dp;
	daddr_t blk;
	char *buf;
	int count;
{

	if (count < sizeof(struct rp06hdr)) {
		warnx("can't read header on blk %ld, can't reformat", blk);
		return (-1);
	}
	return (0);
}

format(fd, blk)
	int fd;
	daddr_t blk;
{
	register struct formats *fp;
	static char *buf;
	static char bufsize;
	struct format_op fop;
	int n;

	for (fp = formats; fp->f_name; fp++)
		if (strcmp(dp->d_typename, fp->f_name) == 0)
			break;
	if (fp->f_name == 0)
		errx(2, "don't know how to format %s disks", dp->d_typename);
	if (buf && bufsize < fp->f_bufsize) {
		free(buf);
		buf = NULL;
	}
	if (buf == NULL)
		buf = malloc((unsigned)fp->f_bufsize);
	if (buf == NULL)
		errx(3, "can't allocate sector buffer");
	bufsize = fp->f_bufsize;
	/*
	 * Here we do the actual formatting.  All we really
	 * do is rewrite the sector header and flag the bad sector
	 * according to the format table description.  If a special
	 * purpose format routine is specified, we allow it to
	 * process the sector as well.
	 */
	if (verbose)
		printf("format blk %ld\n", blk);
	bzero((char *)&fop, sizeof(fop));
	fop.df_buf = buf;
	fop.df_count = fp->f_bufsize;
	fop.df_startblk = blk;
	bzero(buf, fp->f_bufsize);
	if (ioctl(fd, DIOCRFORMAT, &fop) < 0)
		warn("read format");
	if (fp->f_routine &&
	    (*fp->f_routine)(fp, dp, blk, buf, fop.df_count) != 0)
		return;
	if (fp->f_bic) {
		struct hpuphdr *xp = (struct hpuphdr *)buf;

		xp->hpup_cyl &= ~fp->f_bic;
	}
	if (nflag)
		return;
	bzero((char *)&fop, sizeof(fop));
	fop.df_buf = buf;
	fop.df_count = fp->f_bufsize;
	fop.df_startblk = blk;
	if (ioctl(fd, DIOCWFORMAT, &fop) < 0)
		err(4, "write format");
	if (fop.df_count != fp->f_bufsize) {
		char msg[80];
		(void)sprintf(msg, "write format %ld", blk);
		warn("%s", msg);
	}
}
#endif
