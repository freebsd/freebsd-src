/*-
 * Copyright (c) 1980, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2002 Networks Associates Technologies, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed for the FreeBSD Project by
 * ThinkSec AS and NAI Labs, the Security Research Division of Network
 * Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
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
#include <sys/file.h>
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

enum {
	NL_CONSTTY,
	NL_MAXFILES,
	NL_NFILES,
	NL_TTY_LIST
};

static struct nlist nl[] = {
	{ "_constty", 0 },
	{ "_maxfiles", 0 },
	{ "_nfiles", 0 },
	{ "_tty_list", 0 },
	{ "" }
};

static int	usenumflag;
static int	totalflag;
static int	swapflag;
static char	*nlistf;
static char	*memf;
static kvm_t	*kd;

static char	*usagestr;

static void	filemode(void);
static int	getfiles(char **, size_t *);
static void	swapmode(void);
static void	ttymode(void);
static void	ttyprt(struct xtty *);
static void	usage(void);

int
main(int argc, char *argv[])
{
	int ch, i, quit, ret;
	int fileflag, ttyflag;
	char buf[_POSIX2_LINE_MAX],*opts;

	fileflag = swapflag = ttyflag = 0;

	/* We will behave like good old swapinfo if thus invoked */
	opts = strrchr(argv[0], '/');
	if (opts)
		opts++;
	else
		opts = argv[0];
	if (!strcmp(opts, "swapinfo")) {
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
			for (i = 0; nl[i].n_name[0] != '\0'; ++i)
				if (nl[i].n_value == 0) {
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

static void
ttymode_kvm(void)
{
	SLIST_HEAD(, tty) tl;
	struct tty *tp, tty;
	struct xtty xt;

	(void)printf("%s", hdr);
	bzero(&xt, sizeof xt);
	xt.xt_size = sizeof xt;
	if (kvm_read(kd, nl[NL_TTY_LIST].n_value, &tl, sizeof tl) != sizeof tl)
		errx(1, "kvm_read(): %s", kvm_geterr(kd));
	tp = SLIST_FIRST(&tl);
	while (tp != NULL) {
		if (kvm_read(kd, (u_long)tp, &tty, sizeof tty) != sizeof tty)
			errx(1, "kvm_read(): %s", kvm_geterr(kd));
		xt.xt_rawcc = tty.t_rawq.c_cc;
		xt.xt_cancc = tty.t_canq.c_cc;
		xt.xt_outcc = tty.t_outq.c_cc;
#define XT_COPY(field) xt.xt_##field = tty.t_##field
		XT_COPY(line);
		XT_COPY(state);
		XT_COPY(column);
		XT_COPY(ihiwat);
		XT_COPY(ilowat);
		XT_COPY(ohiwat);
		XT_COPY(olowat);
#undef XT_COPY
		ttyprt(&xt);
		tp = tty.t_list.sle_next;
	}
}

static void
ttymode_sysctl(void)
{
	struct xtty *xt, *end;
	void *xttys;
	size_t len;

	(void)printf("%s", hdr);
	if ((xttys = malloc(len = sizeof *xt)) == NULL)
		err(1, "malloc()");
	while (sysctlbyname("kern.ttys", xttys, &len, 0, 0) == -1) {
		if (errno != ENOMEM)
			err(1, "sysctlbyname()");
		len *= 2;
		if ((xttys = realloc(xttys, len)) == NULL)
			err(1, "realloc()");
	}
	if (len > 0) {
		end = (struct xtty *)((char *)xttys + len);
		for (xt = xttys; xt < end; xt++)
			ttyprt(xt);
	}
}

static void
ttymode(void)
{

	if (kd != NULL)
		ttymode_kvm();
	else
		ttymode_sysctl();
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
ttyprt(struct xtty *xt)
{
	int i, j;
	pid_t pgid;
	char *name, state[20];

	if (xt->xt_size != sizeof *xt)
		errx(1, "struct xtty size mismatch");
	if (usenumflag || xt->xt_dev == 0 ||
	   (name = devname(xt->xt_dev, S_IFCHR)) == NULL)
		(void)printf("   %2d,%-2d", major(xt->xt_dev), minor(xt->xt_dev));
	else
		(void)printf("%7s ", name);
	(void)printf("%2ld %3ld ", xt->xt_rawcc, xt->xt_cancc);
	(void)printf("%3ld %5d %5d %4d %3d %7d ", xt->xt_outcc,
		xt->xt_ihiwat, xt->xt_ilowat, xt->xt_ohiwat, xt->xt_olowat,
		xt->xt_column);
	for (i = j = 0; ttystates[i].flag; i++)
		if (xt->xt_state & ttystates[i].flag)
			state[j++] = ttystates[i].val;
	if (j == 0)
		state[j++] = '-';
	state[j] = '\0';
	(void)printf("%-6s %8d", state, xt->xt_sid);
	pgid = 0;
	(void)printf("%6d ", xt->xt_pgid);
	switch (xt->xt_line) {
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
		(void)printf("%d\n", xt->xt_line);
		break;
	}
}

static void
filemode(void)
{
	struct xfile *fp;
	char *buf, flagbuf[16], *fbp;
	int maxf, openf;
	size_t len;
	static char *dtypes[] = { "???", "inode", "socket" };
	int i;

	if (kd != NULL) {
		if (kvm_read(kd, nl[NL_MAXFILES].n_value,
			&maxf, sizeof maxf) != sizeof maxf ||
		    kvm_read(kd, nl[NL_NFILES].n_value,
			&openf, sizeof openf) != sizeof openf)
			errx(1, "kvm_read(): %s", kvm_geterr(kd));
	} else {
		len = sizeof(int);
		if (sysctlbyname("kern.maxfiles", &maxf, &len, 0, 0) == -1 ||
		    sysctlbyname("kern.openfiles", &openf, &len, 0, 0) == -1)
			err(1, "sysctlbyname()");
	}

	if (totalflag) {
		(void)printf("%3d/%3d files\n", openf, maxf);
		return;
	}
	if (getfiles(&buf, &len) == -1)
		return;
	openf = len / sizeof *fp;
	(void)printf("%d/%d open files\n", openf, maxf);
	(void)printf("   LOC   TYPE    FLG     CNT  MSG    DATA    OFFSET\n");
	for (fp = (struct xfile *)buf, i = 0; i < openf; ++fp, ++i) {
		if ((unsigned)fp->xf_type > DTYPE_SOCKET)
			continue;
		(void)printf("%8jx ", (uintmax_t)(uintptr_t)fp->xf_file);
		(void)printf("%-8.8s", dtypes[fp->xf_type]);
		fbp = flagbuf;
		if (fp->xf_flag & FREAD)
			*fbp++ = 'R';
		if (fp->xf_flag & FWRITE)
			*fbp++ = 'W';
		if (fp->xf_flag & FAPPEND)
			*fbp++ = 'A';
		if (fp->xf_flag & FASYNC)
			*fbp++ = 'I';
		*fbp = '\0';
		(void)printf("%6s  %3d", flagbuf, fp->xf_count);
		(void)printf("  %3d", fp->xf_msgcount);
		(void)printf("  %8lx", (u_long)(void *)fp->xf_data);
		(void)printf("  %jx\n", (uintmax_t)fp->xf_offset);
	}
	free(buf);
}

static int
getfiles(char **abuf, size_t *alen)
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
	size_t hlen;
	long blocksize;
	const char *header;

	header = getbsize(&hlen, &blocksize);
	if (totalflag == 0)
		(void)printf("%-15s %*s %8s %8s %8s  %s\n",
		    "Device", (int)hlen, header,
		    "Used", "Avail", "Capacity", "Type");
}

static void
print_swap(struct kvm_swap *ksw)
{
	size_t hlen;
	int pagesize;
	long blocksize;

	pagesize = getpagesize();
	getbsize(&hlen, &blocksize);
	swtot.ksw_total += ksw->ksw_total;
	swtot.ksw_used += ksw->ksw_used;
	++nswdev;
	if (totalflag == 0) {
		(void)printf("%-15s %*d ",
		    ksw->ksw_devname, (int)hlen,
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
	size_t hlen;
	int pagesize;
	long blocksize;

	pagesize = getpagesize();
	getbsize(&hlen, &blocksize);
	if (totalflag) {
		blocksize = 1024 * 1024;
		(void)printf("%dM/%dM swap space\n",
		    CONVERT(swtot.ksw_used), CONVERT(swtot.ksw_total));
	} else if (nswdev > 1) {
		(void)printf("%-15s %*d %8d %8d %5.0f%%\n",
		    "Total", (int)hlen, CONVERT(swtot.ksw_total),
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
