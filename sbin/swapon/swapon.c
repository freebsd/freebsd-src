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
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)swapon.c	8.1 (Berkeley) 6/5/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/stat.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static void usage(void);
static int swap_on_off(char *name, int ignoreebusy);
static void swaplist(int, int, int);

enum { SWAPON, SWAPOFF, SWAPCTL } orig_prog, which_prog = SWAPCTL;

int
main(int argc, char **argv)
{
	struct fstab *fsp;
	char *ptr;
	int stat;
	int ch, doall;
	int sflag = 0, lflag = 0, hflag = 0;

	if ((ptr = strrchr(argv[0], '/')) == NULL)
		ptr = argv[0];
	if (strstr(ptr, "swapon"))
		which_prog = SWAPON;
	else if (strstr(ptr, "swapoff"))
		which_prog = SWAPOFF;
	orig_prog = which_prog;
	
	doall = 0;
	while ((ch = getopt(argc, argv, "AadlhksU")) != -1) {
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
		case 's':
			sflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'h':
			hflag = 'M';
			break;
		case 'k':
			hflag = 'K';
			break;
		case 'U':
			if (which_prog == SWAPCTL) {
				doall = 1;
				which_prog = SWAPOFF;
			} else {
				usage();
			}
			break;
		case '?':
		default:
			usage();
		}
	}
	argv += optind;

	stat = 0;
	if (which_prog == SWAPON || which_prog == SWAPOFF) {
		if (doall) {
			while ((fsp = getfsent()) != NULL) {
				if (strcmp(fsp->fs_type, FSTAB_SW))
					continue;
				if (strstr(fsp->fs_mntops, "noauto"))
					continue;
				if (swap_on_off(fsp->fs_spec, 0)) {
					stat = 1;
				} else {
					printf("%s: %sing %s as swap device\n",
					    getprogname(), which_prog == SWAPOFF ? "remov" : "add",
					    fsp->fs_spec);
				}
			}
		}
		else if (!*argv)
			usage();
		for (; *argv; ++argv) {
			if (swap_on_off(*argv, 0)) {
				stat = 1;
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
	exit(stat);
}

static int
swap_on_off(char *name, int ignoreebusy)
{
	if ((which_prog == SWAPOFF ? swapoff(name) : swapon(name)) == -1) {
		switch (errno) {
		case EBUSY:
			if (!ignoreebusy)
				warnx("%s: device already in use", name);
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
	case SWAPOFF:
	    fprintf(stderr, "[-a] [special_file ...]\n");
	    break;
	case SWAPON:
	    fprintf(stderr, "[-a] [special_file ...]\n");
	    break;
	case SWAPCTL:
	    fprintf(stderr, "[-lshAU] [-a/-d special_file ...]\n");
	    break;
	}
	exit(1);
}

static void
swaplist(int lflag, int sflag, int hflag)
{
	size_t mibsize, size;
	struct xswdev xsw;
	int hlen, mib[16], n, pagesize;
	size_t hsize;
	long blocksize;
	long long total = 0;
	long long used = 0;
	long long tmp_total;
	long long tmp_used;
	
	pagesize = getpagesize();
	switch(hflag) {
	case 'K':
	    blocksize = 1024;
	    hlen = 10;
	    break;
	case 'M':
	    blocksize = 1024 * 1024;
	    hlen = 10;
	    break;
	default:
	    getbsize(&hsize, &blocksize);
	    hlen = hsize;
	    break;
	}
	
	mibsize = sizeof mib / sizeof mib[0];
	if (sysctlnametomib("vm.swap_info", mib, &mibsize) == -1)
		err(1, "sysctlnametomib()");
	
	if (lflag) {
		char buf[32];
		snprintf(buf, sizeof(buf), "%ld-blocks", blocksize);
		printf("%-13s %*s %*s\n",
		    "Device:", 
		    hlen, buf,
		    hlen, "Used:");
	}
	
	for (n = 0; ; ++n) {
		mib[mibsize] = n;
		size = sizeof xsw;
		if (sysctl(mib, mibsize + 1, &xsw, &size, NULL, NULL) == -1)
			break;
		if (xsw.xsw_version != XSWDEV_VERSION)
			errx(1, "xswdev version mismatch");
		
		tmp_total = (long long)xsw.xsw_nblks * pagesize / blocksize;
		tmp_used  = (long long)xsw.xsw_used * pagesize / blocksize;
		total += tmp_total;
		used  += tmp_used;
		if (lflag) {
			printf("/dev/%-8s %*lld %*lld\n", 
			    devname(xsw.xsw_dev, S_IFCHR),
			    hlen, tmp_total,
			    hlen, tmp_used);
		}
	}
	if (errno != ENOENT)
		err(1, "sysctl()");
	
	if (sflag) {
		printf("Total:        %*lld %*lld\n",
		       hlen, total,
		       hlen, used);
	}
}

