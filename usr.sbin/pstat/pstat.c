/*-
 * Copyright (c) 1980, 1991, 1993, 1994
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
"@(#) Copyright (c) 1980, 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)pstat.c	8.16 (Berkeley) 5/9/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#define _KERNEL
#include <sys/file.h>
#include <sys/uio.h>
#undef _KERNEL
#include <sys/stat.h>
#include <sys/stdint.h>
#include <sys/ioctl.h>
#include <sys/ioctl_compat.h>	/* XXX NTTYDISC is too well hidden */
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/blist.h>

#include <sys/user.h>
#include <sys/sysctl.h>

#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct nlist nl[] = {
#define NLMANDATORYBEG	0
#define	V_MOUNTLIST	0
	{ "_mountlist" },	/* address of head of mount list. */
#define V_NUMV		1
	{ "_numvnodes" },
#define	FNL_NFILES	2
	{"_nfiles"},
#define FNL_MAXFILES	3
	{"_maxfiles"},
#define NLMANDATORYEND FNL_MAXFILES	/* names up to here are mandatory */
#define	SCONS		NLMANDATORYEND + 1
	{ "_constty" },
#define	SPTY		NLMANDATORYEND + 2
	{ "_pt_tty" },
#define	SNPTY		NLMANDATORYEND + 3
	{ "_npty" },



#ifdef __FreeBSD__
#define SCCONS	(SNPTY+1)
	{ "_sccons" },
#define NSCCONS	(SNPTY+2)
	{ "_nsccons" },
#define SIO  (SNPTY+3)
	{ "_sio_tty" },
#define NSIO (SNPTY+4)
	{ "_nsio_tty" },
#define RC  (SNPTY+5)
	{ "_rc_tty" },
#define NRC (SNPTY+6)
	{ "_nrc_tty" },
#define CY  (SNPTY+7)
	{ "_cy_tty" },
#define NCY (SNPTY+8)
	{ "_ncy_tty" },
#define SI  (SNPTY+9)
	{ "_si_tty" },
#define NSI (SNPTY+10)
	{ "_si_Nports" },
#endif
	{ "" }
};

static int	usenumflag;
static int	totalflag;
static int	swapflag;
static char	*nlistf;
static char	*memf;
static kvm_t	*kd;

static char	*usagestr;

#define	SVAR(var) __STRING(var)	/* to force expansion */
#define	KGET(idx, var)							\
	KGET1(idx, &var, sizeof(var), SVAR(var))
#define	KGET1(idx, p, s, msg)						\
	KGET2(nl[idx].n_value, p, s, msg)
#define	KGET2(addr, p, s, msg)						\
	if (kvm_read(kd, (u_long)(addr), p, s) != s)			\
		warnx("cannot read %s: %s", msg, kvm_geterr(kd))
#define	KGETN(idx, var)							\
	KGET1N(idx, &var, sizeof(var), SVAR(var))
#define	KGET1N(idx, p, s, msg)						\
	KGET2N(nl[idx].n_value, p, s, msg)
#define	KGET2N(addr, p, s, msg)						\
	((kvm_read(kd, (u_long)(addr), p, s) == s) ? 1 : 0)
#define	KGETRET(addr, p, s, msg)					\
	if (kvm_read(kd, (u_long)(addr), p, s) != s) {			\
		warnx("cannot read %s: %s", msg, kvm_geterr(kd));	\
		return (0);						\
	}

static void	filemode(void);
static int	getfiles(char **, int *);
static void	swapmode(void);
static void	ttymode(void);
static void	ttyprt(struct tty *, int);
static void	ttytype(struct tty *, char *, int, int, int);
static void	usage(void);

int
main(int argc, char *argv[])
{
	int ch, i, quit, ret;
	int fileflag, ttyflag;
	char buf[_POSIX2_LINE_MAX],*opts;

	fileflag = swapflag = ttyflag = 0;

	/* We will behave like good old swapinfo if thus invoked */
	opts = strrchr(argv[0],'/');
	if (opts)
		opts++;
	else
		opts = argv[0];
	if (!strcmp(opts,"swapinfo")) {
		swapflag = 1;
		opts = "kM:N:";
		usagestr = "swapinfo [-k] [-M core] [-N system]";
	} else {
		opts = "TM:N:fknst";
		usagestr = "pstat [-Tfknst] [-M core] [-N system]";
	}

	while ((ch = getopt(argc, argv, opts)) != -1)
		switch (ch) {
		case 'f':
			fileflag = 1;
			break;
		case 'k':
			putenv("BLOCKSIZE=1K");
			break;
		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'n':
			usenumflag = 1;
			break;
		case 's':
			++swapflag;
			break;
		case 'T':
			totalflag = 1;
			break;
		case 't':
			ttyflag = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (memf != NULL) {
		kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, buf);
		if (kd == NULL)
			errx(1, "kvm_openfiles: %s", buf);
		if ((ret = kvm_nlist(kd, nl)) != 0) {
			if (ret == -1)
				errx(1, "kvm_nlist: %s", kvm_geterr(kd));
			quit = 0;
			for (i = NLMANDATORYBEG; i <= NLMANDATORYEND; i++)
				if (!nl[i].n_value) {
					quit = 1;
					warnx("undefined symbol: %s",
					    nl[i].n_name);
				}
			if (quit)
				exit(1);
		}
	}
	if (!(fileflag | ttyflag | swapflag | totalflag))
		usage();
	if (fileflag || totalflag)
		filemode();
	if (ttyflag)
		ttymode();
	if (swapflag || totalflag)
		swapmode();
	exit (0);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s\n", usagestr);
	exit (1);
}

static const char hdr[] =
"  LINE RAW CAN OUT IHIWT ILOWT OHWT LWT     COL STATE  SESS      PGID DISC\n";
static int ttyspace = 128;

static void
ttymode(void)
{
	struct tty *tty;
	struct tty ttyb[1000];
	int error;
	size_t len, i;

	(void)printf("%s", hdr);
	len = sizeof(ttyb);
	error = sysctlbyname("kern.ttys", &ttyb, &len, 0, 0);
	if (!error) {
		len /= sizeof(ttyb[0]);
		for (i = 0; i < len; i++) {
			ttyprt(&ttyb[i], 0);
		}
	}
	/* XXX */
	if (kd == NULL)
		return;
	if ((tty = malloc(ttyspace * sizeof(*tty))) == NULL)
		errx(1, "malloc");
	if (nl[SCONS].n_type != 0) {
		(void)printf("1 console\n");
		KGET(SCONS, *tty);
		ttyprt(&tty[0], 0);
	}
#ifdef __FreeBSD__
	if (nl[NSCCONS].n_type != 0)
		ttytype(tty, "vty", SCCONS, NSCCONS, 0);
	if (nl[NSIO].n_type != 0)
		ttytype(tty, "sio", SIO, NSIO, 0);
	if (nl[NRC].n_type != 0)
		ttytype(tty, "rc", RC, NRC, 0);
	if (nl[NCY].n_type != 0)
		ttytype(tty, "cy", CY, NCY, 0);
	if (nl[NSI].n_type != 0)
		ttytype(tty, "si", SI, NSI, 1);
#endif
	if (nl[SNPTY].n_type != 0)
		ttytype(tty, "pty", SPTY, SNPTY, 0);
}

static void
ttytype(struct tty *tty, char *name, int type, int number, int indir)
{
	struct tty *tp;
	int ntty;
	struct tty **ttyaddr;

	if (tty == NULL)
		return;
	KGET(number, ntty);
	(void)printf("%d %s %s\n", ntty, name, (ntty == 1) ? "line" : "lines");
	if (ntty > ttyspace) {
		ttyspace = ntty;
		if ((tty = realloc(tty, ttyspace * sizeof(*tty))) == 0)
			errx(1, "realloc");
	}
	if (indir) {
		KGET(type, ttyaddr);
		KGET2(ttyaddr, tty, ntty * sizeof(struct tty), "tty structs");
	} else {
		KGET1(type, tty, ntty * sizeof(struct tty), "tty structs");
	}
	(void)printf("%s", hdr);
	for (tp = tty; tp < &tty[ntty]; tp++)
		ttyprt(tp, tp - tty);
}

static struct {
	int flag;
	char val;
} ttystates[] = {
#ifdef TS_WOPEN
	{ TS_WOPEN,	'W'},
#endif
	{ TS_ISOPEN,	'O'},
	{ TS_CARR_ON,	'C'},
#ifdef TS_CONNECTED
	{ TS_CONNECTED,	'c'},
#endif
	{ TS_TIMEOUT,	'T'},
	{ TS_FLUSH,	'F'},
	{ TS_BUSY,	'B'},
#ifdef TS_ASLEEP
	{ TS_ASLEEP,	'A'},
#endif
#ifdef TS_SO_OLOWAT
	{ TS_SO_OLOWAT,	'A'},
#endif
#ifdef TS_SO_OCOMPLETE
	{ TS_SO_OCOMPLETE, 'a'},
#endif
	{ TS_XCLUDE,	'X'},
	{ TS_TTSTOP,	'S'},
#ifdef TS_CAR_OFLOW
	{ TS_CAR_OFLOW,	'm'},
#endif
#ifdef TS_CTS_OFLOW
	{ TS_CTS_OFLOW,	'o'},
#endif
#ifdef TS_DSR_OFLOW
	{ TS_DSR_OFLOW,	'd'},
#endif
	{ TS_TBLOCK,	'K'},
	{ TS_ASYNC,	'Y'},
	{ TS_BKSL,	'D'},
	{ TS_ERASE,	'E'},
	{ TS_LNCH,	'L'},
	{ TS_TYPEN,	'P'},
	{ TS_CNTTB,	'N'},
#ifdef TS_CAN_BYPASS_L_RINT
	{ TS_CAN_BYPASS_L_RINT, 'l'},
#endif
#ifdef TS_SNOOP
	{ TS_SNOOP,     's'},
#endif
#ifdef TS_ZOMBIE
	{ TS_ZOMBIE,	'Z'},
#endif
	{ 0,	       '\0'},
};

static void
ttyprt(struct tty *tp, int line)
{
	int i, j;
	pid_t pgid;
	char *name, state[20];

	if (usenumflag || tp->t_dev == 0 ||
	   (name = devname(tp->t_dev, S_IFCHR)) == NULL)
		(void)printf("   %2d,%-2d", major(tp->t_dev), minor(tp->t_dev));
	else
		(void)printf("%7s ", name);
	(void)printf("%2d %3d ", tp->t_rawq.c_cc, tp->t_canq.c_cc);
	(void)printf("%3d %5d %5d %4d %3d %7d ", tp->t_outq.c_cc,
		tp->t_ihiwat, tp->t_ilowat, tp->t_ohiwat, tp->t_olowat,
		tp->t_column);
	for (i = j = 0; ttystates[i].flag; i++)
		if (tp->t_state & ttystates[i].flag)
			state[j++] = ttystates[i].val;
	if (j == 0)
		state[j++] = '-';
	state[j] = '\0';
	(void)printf("%-6s %8lx", state, (u_long)(void *)tp->t_session);
	pgid = 0;
	if (kd != NULL /* XXX */ && tp->t_pgrp != NULL)
		KGET2(&tp->t_pgrp->pg_id, &pgid, sizeof(pid_t), "pgid");
	(void)printf("%6d ", pgid);
	switch (tp->t_line) {
	case TTYDISC:
		(void)printf("term\n");
		break;
	case NTTYDISC:
		(void)printf("ntty\n");
		break;
	case SLIPDISC:
		(void)printf("slip\n");
		break;
	case PPPDISC:
		(void)printf("ppp\n");
		break;
	default:
		(void)printf("%d\n", tp->t_line);
		break;
	}
}

static void
filemode(void)
{
	struct file *fp;
	struct file *addr;
	char *buf, flagbuf[16], *fbp;
	int maxfile, nfiles;
	size_t len;
	static char *dtypes[] = { "???", "inode", "socket" };

	if (kd != NULL) {
		KGET(FNL_MAXFILES, maxfile);
		KGET(FNL_NFILES, nfiles);
	} else {
		len = sizeof maxfile;
		if (sysctlbyname("kern.maxfiles", &maxfile, &len, 0, 0) == -1)
			err(1, "sysctlbyname()");
		len = sizeof nfiles;
		if (sysctlbyname("kern.openfiles", &nfiles, &len, 0, 0) == -1)
			err(1, "sysctlbyname()");
	}

	if (totalflag) {
		(void)printf("%3d/%3d files\n", nfiles, maxfile);
		return;
	}
	if (getfiles(&buf, &len) == -1)
		return;
	/*
	 * Getfiles returns in malloc'd memory a pointer to the first file
	 * structure, and then an array of file structs (whose addresses are
	 * derivable from the previous entry).
	 */
	addr = LIST_FIRST((struct filelist *)buf);
	fp = (struct file *)(buf + sizeof(struct filelist));
	nfiles = (len - sizeof(struct filelist)) / sizeof(struct file);

	(void)printf("%d/%d open files\n", nfiles, maxfile);
	(void)printf("   LOC   TYPE    FLG     CNT  MSG    DATA    OFFSET\n");
	for (; (char *)fp < buf + len; addr = LIST_NEXT(fp, f_list), fp++) {
		if ((unsigned)fp->f_type > DTYPE_SOCKET)
			continue;
		(void)printf("%8lx ", (u_long)(void *)addr);
		(void)printf("%-8.8s", dtypes[fp->f_type]);
		fbp = flagbuf;
		if (fp->f_flag & FREAD)
			*fbp++ = 'R';
		if (fp->f_flag & FWRITE)
			*fbp++ = 'W';
		if (fp->f_flag & FAPPEND)
			*fbp++ = 'A';
		if (fp->f_flag & FASYNC)
			*fbp++ = 'I';
		*fbp = '\0';
		(void)printf("%6s  %3d", flagbuf, fp->f_count);
		(void)printf("  %3d", fp->f_msgcount);
		(void)printf("  %8lx", (u_long)(void *)fp->f_data);
		if (fp->f_offset < 0)
			(void)printf("  %qx\n", fp->f_offset);
		else
			(void)printf("  %qd\n", fp->f_offset);
	}
	free(buf);
}

static int
getfiles(char **abuf, int *alen)
{
	size_t len;
	int mib[2];
	char *buf;

	/*
	 * XXX
	 * Add emulation of KINFO_FILE here.
	 */
	if (kd != NULL)
		errx(1, "files on dead kernel, not implemented");

	mib[0] = CTL_KERN;
	mib[1] = KERN_FILE;
	if (sysctl(mib, 2, NULL, &len, NULL, 0) == -1) {
		warn("sysctl: KERN_FILE");
		return (-1);
	}
	if ((buf = malloc(len)) == NULL)
		errx(1, "malloc");
	if (sysctl(mib, 2, buf, &len, NULL, 0) == -1) {
		warn("sysctl: KERN_FILE");
		return (-1);
	}
	*abuf = buf;
	*alen = len;
	return (0);
}

/*
 * swapmode is based on a program called swapinfo written
 * by Kevin Lahey <kml@rokkaku.atl.ga.us>.
 */

#define CONVERT(v)	((int)((intmax_t)(v) * pagesize / blocksize))
static struct kvm_swap swtot;
static int nswdev;

static void
print_swap_header(void)
{
	int hlen;
	long blocksize;
	const char *header;

	header = getbsize(&hlen, &blocksize);
	if (totalflag == 0)
		(void)printf("%-15s %*s %8s %8s %8s  %s\n",
		    "Device", hlen, header,
		    "Used", "Avail", "Capacity", "Type");
}

static void
print_swap(struct kvm_swap *ksw)
{
	int hlen, pagesize;
	long blocksize;

	pagesize = getpagesize();
	getbsize(&hlen, &blocksize);
	swtot.ksw_total += ksw->ksw_total;
	swtot.ksw_used += ksw->ksw_used;
	++nswdev;
	if (totalflag == 0) {
		(void)printf("/dev/%-10s %*d ",
		    ksw->ksw_devname, hlen,
		    CONVERT(ksw->ksw_total));
		(void)printf("%8d %8d %5.0f%%    %s\n",
		    CONVERT(ksw->ksw_used),
		    CONVERT(ksw->ksw_total - ksw->ksw_used),
		    (ksw->ksw_used * 100.0) / ksw->ksw_total,
		    (ksw->ksw_flags & SW_SEQUENTIAL) ?
		    "Sequential" : "Interleaved");
	}
}

static void
print_swap_total(void)
{
	int hlen, pagesize;
	long blocksize;

	pagesize = getpagesize();
	getbsize(&hlen, &blocksize);
	if (totalflag) {
		blocksize = 1024 * 1024;
		(void)printf("%dM/%dM swap space\n",
		    CONVERT(swtot.ksw_used), CONVERT(swtot.ksw_total));
	} else if (nswdev > 1) {
		(void)printf("%-15s %*d %8d %8d %5.0f%%\n",
		    "Total", hlen, CONVERT(swtot.ksw_total),
		    CONVERT(swtot.ksw_used),
		    CONVERT(swtot.ksw_total - swtot.ksw_used),
		    (swtot.ksw_used * 100.0) / swtot.ksw_total);
	}
}

static void
swapmode_kvm(void)
{
	struct kvm_swap kswap[16];
	int i, n;

	n = kvm_getswapinfo(kd, kswap, sizeof kswap / sizeof kswap[0],
	    ((swapflag > 1) ? SWIF_DUMP_TREE : 0) | SWIF_DEV_PREFIX);

	print_swap_header();
	for (i = 0; i < n; ++i)
		print_swap(&kswap[i]);
	print_swap_total();
}

static void
swapmode_sysctl(void)
{
	struct kvm_swap ksw;
	struct xswdev xsw;
	size_t mibsize, size;
	int mib[16], n;

	print_swap_header();
	mibsize = sizeof mib / sizeof mib[0];
	if (sysctlnametomib("vm.swap_info", mib, &mibsize) == -1)
		err(1, "sysctlnametomib()");
	for (n = 0; ; ++n) {
		mib[mibsize] = n;
		size = sizeof xsw;
		if (sysctl(mib, mibsize + 1, &xsw, &size, NULL, NULL) == -1)
			break;
		if (xsw.xsw_version != XSWDEV_VERSION)
			errx(1, "xswdev version mismatch");
		snprintf(ksw.ksw_devname, sizeof ksw.ksw_devname,
		    "/dev/%s", devname(xsw.xsw_dev, S_IFCHR));
		ksw.ksw_used = xsw.xsw_used;
		ksw.ksw_total = xsw.xsw_nblks;
		ksw.ksw_flags = xsw.xsw_flags;
		print_swap(&ksw);
	}
	if (errno != ENOENT)
		err(1, "sysctl()");
	print_swap_total();
}

static void
swapmode(void)
{
	if (kd != NULL)
		swapmode_kvm();
	else
		swapmode_sysctl();
}
