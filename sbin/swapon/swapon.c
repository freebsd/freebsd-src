/*
 * Copyright (c) 1980, 1993
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

#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)swapon.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/stat.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <vm/vm_param.h>

#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <libutil.h>

static void usage(void);
static int swap_on_off(char *name, int ignoreebusy);
static void swaplist(int, int, int);

enum { SWAPON, SWAPOFF, SWAPCTL } orig_prog, which_prog = SWAPCTL;

int
main(int argc, char **argv)
{
	struct fstab *fsp;
	char *ptr;
	int ret;
	int ch, doall;
	int sflag = 0, lflag = 0, hflag = 0, qflag = 0;
	const char *etc_fstab;

	if ((ptr = strrchr(argv[0], '/')) == NULL)
		ptr = argv[0];
	if (strstr(ptr, "swapon"))
		which_prog = SWAPON;
	else if (strstr(ptr, "swapoff"))
		which_prog = SWAPOFF;
	orig_prog = which_prog;
	
	doall = 0;
	etc_fstab = NULL;
	while ((ch = getopt(argc, argv, "AadghklmqsUF:")) != -1) {
		switch(ch) {
		case 'A':
			if (which_prog == SWAPCTL) {
				doall = 1;
				which_prog = SWAPON;
			} else {
				usage();
			}
			break;
		case 'a':
			if (which_prog == SWAPON || which_prog == SWAPOFF)
				doall = 1;
			else
				which_prog = SWAPON;
			break;
		case 'd':
			if (which_prog == SWAPCTL)
				which_prog = SWAPOFF;
			else
				usage();
			break;
		case 'g':
			hflag = 'G';
			break;
		case 'h':
			hflag = 'H';
			break;
		case 'k':
			hflag = 'K';
			break;
		case 'l':
			lflag = 1;
			break;
		case 'm':
			hflag = 'M';
			break;
		case 'q':
			if (which_prog == SWAPON || which_prog == SWAPOFF)
				qflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 'U':
			if (which_prog == SWAPCTL) {
				doall = 1;
				which_prog = SWAPOFF;
			} else {
				usage();
			}
			break;
		case 'F':
			etc_fstab = optarg;
			break;
		case '?':
		default:
			usage();
		}
	}
	argv += optind;

	ret = 0;
	if (etc_fstab != NULL)
		setfstab(etc_fstab);
	if (which_prog == SWAPON || which_prog == SWAPOFF) {
		if (doall) {
			while ((fsp = getfsent()) != NULL) {
				if (strcmp(fsp->fs_type, FSTAB_SW))
					continue;
				if (strstr(fsp->fs_mntops, "noauto"))
					continue;
				if (swap_on_off(fsp->fs_spec, 1)) {
					ret = 1;
				} else {
					if (!qflag) {
						printf("%s: %sing %s as swap device\n",
						    getprogname(),
						    which_prog == SWAPOFF ? "remov" : "add",
						    fsp->fs_spec);
					}
				}
			}
		}
		else if (!*argv)
			usage();
		for (; *argv; ++argv) {
			if (swap_on_off(*argv, 0)) {
				ret = 1;
			} else if (orig_prog == SWAPCTL) {
				printf("%s: %sing %s as swap device\n",
				    getprogname(), which_prog == SWAPOFF ? "remov" : "add",
				    *argv);
			}
		}
	} else {
		if (lflag || sflag)
			swaplist(lflag, sflag, hflag);
		else 
			usage();
	}
	exit(ret);
}

static int
swap_on_off(char *name, int doingall)
{
	if ((which_prog == SWAPOFF ? swapoff(name) : swapon(name)) == -1) {
		switch (errno) {
		case EBUSY:
			if (!doingall)
				warnx("%s: device already in use", name);
			break;
		case EINVAL:
			if (which_prog == SWAPON)
				warnx("%s: NSWAPDEV limit reached", name);
			else if (!doingall)
				warn("%s", name);
			break;
		default:
			warn("%s", name);
			break;
		}
		return(1);
	}
	return(0);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s ", getprogname());
	switch(orig_prog) {
	case SWAPON:
	case SWAPOFF:
	    fprintf(stderr, "[-F fstab] -aq | file ...\n");
	    break;
	case SWAPCTL:
	    fprintf(stderr, "[-AghklmsU] [-a file ... | -d file ...]\n");
	    break;
	}
	exit(1);
}

static void
sizetobuf(char *buf, size_t bufsize, int hflag, long long val, int hlen,
    long blocksize)
{

	if (hflag == 'H') {
		char tmp[16];

		humanize_number(tmp, 5, (int64_t)val, "", HN_AUTOSCALE,
		    HN_B | HN_NOSPACE | HN_DECIMAL);
		snprintf(buf, bufsize, "%*s", hlen, tmp);
	} else {
		snprintf(buf, bufsize, "%*lld", hlen, val / blocksize);
	}
}

static void
swaplist(int lflag, int sflag, int hflag)
{
	size_t mibsize, size;
	struct xswdev xsw;
	int hlen, mib[16], n, pagesize;
	long blocksize;
	long long total = 0;
	long long used = 0;
	long long tmp_total;
	long long tmp_used;
	char buf[32];
	
	pagesize = getpagesize();
	switch(hflag) {
	case 'G':
	    blocksize = 1024 * 1024 * 1024;
	    strlcpy(buf, "1GB-blocks", sizeof(buf));
	    hlen = 10;
	    break;
	case 'H':
	    blocksize = -1;
	    strlcpy(buf, "Bytes", sizeof(buf));
	    hlen = 10;
	    break;
	case 'K':
	    blocksize = 1024;
	    strlcpy(buf, "1kB-blocks", sizeof(buf));
	    hlen = 10;
	    break;
	case 'M':
	    blocksize = 1024 * 1024;
	    strlcpy(buf, "1MB-blocks", sizeof(buf));
	    hlen = 10;
	    break;
	default:
	    getbsize(&hlen, &blocksize);
	    snprintf(buf, sizeof(buf), "%ld-blocks", blocksize);
	    break;
	}
	
	mibsize = sizeof mib / sizeof mib[0];
	if (sysctlnametomib("vm.swap_info", mib, &mibsize) == -1)
		err(1, "sysctlnametomib()");
	
	if (lflag) {
		printf("%-13s %*s %*s\n",
		    "Device:", 
		    hlen, buf,
		    hlen, "Used:");
	}
	
	for (n = 0; ; ++n) {
		mib[mibsize] = n;
		size = sizeof xsw;
		if (sysctl(mib, mibsize + 1, &xsw, &size, NULL, 0) == -1)
			break;
		if (xsw.xsw_version != XSWDEV_VERSION)
			errx(1, "xswdev version mismatch");
		
		tmp_total = (long long)xsw.xsw_nblks * pagesize;
		tmp_used  = (long long)xsw.xsw_used * pagesize;
		total += tmp_total;
		used  += tmp_used;
		if (lflag) {
			sizetobuf(buf, sizeof(buf), hflag, tmp_total, hlen,
			    blocksize);
			printf("/dev/%-8s %s ", devname(xsw.xsw_dev, S_IFCHR),
			    buf);
			sizetobuf(buf, sizeof(buf), hflag, tmp_used, hlen,
			    blocksize);
			printf("%s\n", buf);
		}
	}
	if (errno != ENOENT)
		err(1, "sysctl()");
	
	if (sflag) {
		sizetobuf(buf, sizeof(buf), hflag, total, hlen, blocksize);
		printf("Total:        %s ", buf);
		sizetobuf(buf, sizeof(buf), hflag, used, hlen, blocksize);
		printf("%s\n", buf);
	}
}

