/*
 * Copyright (c) 1983, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
char copyright[] =
"@(#) Copyright (c) 1983, 1989 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)nfsstat.c	5.9 (Berkeley) 7/1/91";
#endif /* not lint */

#include <sys/param.h>
#if BSD >= 199103
#define NEWVM
#endif
#ifndef NEWVM
#include <sys/vmmac.h>
#include <machine/pte.h>
#endif
#include <sys/mount.h>
#include <nfs/nfsv2.h>
#include <nfs/nfs.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <nlist.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>

struct nlist nl[] = {
#define	N_NFSSTAT	0
	{ "_nfsstats" },
#ifndef NEWVM
#define	N_SYSMAP	1
	{ "_Sysmap" },
#define	N_SYSSIZE	2
	{ "_Syssize" },
#endif
	"",
};

#ifndef NEWVM
struct pte *Sysmap;
#endif

int kflag, kmem;
char *kernel = _PATH_UNIX;
char *kmemf = _PATH_KMEM;

off_t klseek();
void intpr(), printhdr(), sidewaysintpr(), usage();

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	extern char *optarg;
	u_int interval;
	int ch;

	interval = 0;
	while ((ch = getopt(argc, argv, "M:N:w:")) != EOF)
		switch(ch) {
		case 'M':
			kmemf = optarg;
			kflag = 1;
			break;
		case 'N':
			kernel = optarg;
			break;
		case 'w':
			interval = atoi(optarg);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		kernel = *++argv;
		if (*++argv) {
			kmemf = *argv;
			kflag = 1;
		}
	}
#endif
	if (nlist(kernel, nl) < 0 || nl[0].n_type == 0) {
		(void)fprintf(stderr, "nfsstate: %s: no namelist\n", kernel);
		exit(1);
	}
	kmem = open(kmemf, O_RDONLY);
	if (kmem < 0) {
		(void)fprintf(stderr,
		    "nfsstat: %s: %s\n", kmemf, strerror(errno));
		exit(1);
	}
	if (kflag) {
#ifdef NEWVM
		(void)fprintf(stderr, "nfsstat: can't do core files yet\n");
		exit(1);
#else
		off_t off;

		Sysmap = (struct pte *)
		   malloc((u_int)(nl[N_SYSSIZE].n_value * sizeof(struct pte)));
		if (!Sysmap) {
			(void)fprintf(stderr, "nfsstat: %s\n", strerror(errno));
			exit(1);
		}
		off = nl[N_SYSMAP].n_value & ~KERNBASE;
		(void)lseek(kmem, off, L_SET);
		(void)read(kmem, (char *)Sysmap,
		    (int)(nl[N_SYSSIZE].n_value * sizeof(struct pte)));
#endif
	}

	if (!nl[N_NFSSTAT].n_value) {
		(void)fprintf(stderr, "nfsstat: nfsstats symbol not defined\n");
		exit(1);
	}
	if (interval)
		sidewaysintpr(interval, nl[N_NFSSTAT].n_value);
	else
		intpr(nl[N_NFSSTAT].n_value);
	exit(0);
}

/*
 * Print a description of the network interfaces.
 */
void
intpr(nfsstataddr)
	off_t nfsstataddr;
{
	struct nfsstats nfsstats;

	klseek(kmem, nfsstataddr, 0L);
	read(kmem, (char *)&nfsstats, sizeof(struct nfsstats));
	printf("Client Info:\n");
	printf("Rpc Counts:\n");
	printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
		"Getattr", "Setattr", "Lookup", "Readlink", "Read",
		"Write", "Create", "Remove");
	printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		nfsstats.rpccnt[1],
		nfsstats.rpccnt[2],
		nfsstats.rpccnt[4],
		nfsstats.rpccnt[5],
		nfsstats.rpccnt[6],
		nfsstats.rpccnt[8],
		nfsstats.rpccnt[9],
		nfsstats.rpccnt[10]);
	printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
		"Rename", "Link", "Symlink", "Mkdir", "Rmdir",
		"Readdir", "Statfs");
	printf("%9d %9d %9d %9d %9d %9d %9d\n",
		nfsstats.rpccnt[11],
		nfsstats.rpccnt[12],
		nfsstats.rpccnt[13],
		nfsstats.rpccnt[14],
		nfsstats.rpccnt[15],
		nfsstats.rpccnt[16],
		nfsstats.rpccnt[17]);
	printf("Rpc Info:\n");
	printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
		"TimedOut", "Invalid", "X Replies", "Retries", "Requests");
	printf("%9d %9d %9d %9d %9d\n",
		nfsstats.rpctimeouts,
		nfsstats.rpcinvalid,
		nfsstats.rpcunexpected,
		nfsstats.rpcretries,
		nfsstats.rpcrequests);
	printf("Cache Info:\n");
	printf("%9.9s %9.9s %9.9s %9.9s",
		"Attr Hits", "Misses", "Lkup Hits", "Misses");
	printf(" %9.9s %9.9s %9.9s %9.9s\n",
		"BioR Hits", "Misses", "BioW Hits", "Misses");
	printf("%9d %9d %9d %9d",
		nfsstats.attrcache_hits, nfsstats.attrcache_misses,
		nfsstats.lookupcache_hits, nfsstats.lookupcache_misses);
	printf(" %9d %9d %9d %9d\n",
		nfsstats.biocache_reads-nfsstats.read_bios,
		nfsstats.read_bios,
		nfsstats.biocache_writes-nfsstats.write_bios,
		nfsstats.write_bios);
	printf("%9.9s %9.9s %9.9s %9.9s",
		"BioRLHits", "Misses", "BioD Hits", "Misses");
	printf(" %9.9s %9.9s\n", "DirE Hits", "Misses");
	printf("%9d %9d %9d %9d",
		nfsstats.biocache_readlinks-nfsstats.readlink_bios,
		nfsstats.readlink_bios,
		nfsstats.biocache_readdirs-nfsstats.readdir_bios,
		nfsstats.readdir_bios);
	printf(" %9d %9d\n",
		nfsstats.direofcache_hits, nfsstats.direofcache_misses);
	printf("\nServer Info:\n");
	printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
		"Getattr", "Setattr", "Lookup", "Readlink", "Read",
		"Write", "Create", "Remove");
	printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		nfsstats.srvrpccnt[1],
		nfsstats.srvrpccnt[2],
		nfsstats.srvrpccnt[4],
		nfsstats.srvrpccnt[5],
		nfsstats.srvrpccnt[6],
		nfsstats.srvrpccnt[8],
		nfsstats.srvrpccnt[9],
		nfsstats.srvrpccnt[10]);
	printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
		"Rename", "Link", "Symlink", "Mkdir", "Rmdir",
		"Readdir", "Statfs");
	printf("%9d %9d %9d %9d %9d %9d %9d\n",
		nfsstats.srvrpccnt[11],
		nfsstats.srvrpccnt[12],
		nfsstats.srvrpccnt[13],
		nfsstats.srvrpccnt[14],
		nfsstats.srvrpccnt[15],
		nfsstats.srvrpccnt[16],
		nfsstats.srvrpccnt[17]);
	printf("Server Ret-Failed\n");
	printf("%17d\n", nfsstats.srvrpc_errs);
	printf("Server Faults\n");
	printf("%13d\n", nfsstats.srv_errs);
	printf("Server Cache Stats:\n");
	printf("%9.9s %9.9s %9.9s %9.9s\n",
		"Inprog", "Idem", "Non-idem", "Misses");
	printf("%9d %9d %9d %9d\n",
		nfsstats.srvcache_inproghits,
		nfsstats.srvcache_idemdonehits,
		nfsstats.srvcache_nonidemdonehits,
		nfsstats.srvcache_misses);
}

u_char	signalled;			/* set if alarm goes off "early" */

/*
 * Print a running summary of nfs statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
void
sidewaysintpr(interval, off)
	u_int interval;
	off_t off;
{
	struct nfsstats nfsstats, lastst;
	int hdrcnt, oldmask;
	void catchalarm();

	klseek(kmem, off, 0L);

	(void)signal(SIGALRM, catchalarm);
	signalled = 0;
	(void)alarm(interval);
	bzero((caddr_t)&lastst, sizeof(lastst));

	for (hdrcnt = 1;;) {
		if (!--hdrcnt) {
			printhdr();
			hdrcnt = 20;
		}
		klseek(kmem, off, 0L);
		read(kmem, (char *)&nfsstats, sizeof nfsstats);
		printf("Client: %8d %8d %8d %8d %8d %8d %8d %8d\n",
		    nfsstats.rpccnt[1]-lastst.rpccnt[1],
		    nfsstats.rpccnt[4]-lastst.rpccnt[4],
		    nfsstats.rpccnt[5]-lastst.rpccnt[5],
		    nfsstats.rpccnt[6]-lastst.rpccnt[6],
		    nfsstats.rpccnt[8]-lastst.rpccnt[8],
		    nfsstats.rpccnt[11]-lastst.rpccnt[11],
		    nfsstats.rpccnt[12]-lastst.rpccnt[12],
		    nfsstats.rpccnt[16]-lastst.rpccnt[16]);
		printf("Server: %8d %8d %8d %8d %8d %8d %8d %8d\n",
		    nfsstats.srvrpccnt[1]-lastst.srvrpccnt[1],
		    nfsstats.srvrpccnt[4]-lastst.srvrpccnt[4],
		    nfsstats.srvrpccnt[5]-lastst.srvrpccnt[5],
		    nfsstats.srvrpccnt[6]-lastst.srvrpccnt[6],
		    nfsstats.srvrpccnt[8]-lastst.srvrpccnt[8],
		    nfsstats.srvrpccnt[11]-lastst.srvrpccnt[11],
		    nfsstats.srvrpccnt[12]-lastst.srvrpccnt[12],
		    nfsstats.srvrpccnt[16]-lastst.srvrpccnt[16]);
		lastst = nfsstats;
		fflush(stdout);
		oldmask = sigblock(sigmask(SIGALRM));
		if (!signalled)
			sigpause(0);
		sigsetmask(oldmask);
		signalled = 0;
		(void)alarm(interval);
	}
	/*NOTREACHED*/
}

void
printhdr()
{
	printf("        %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s\n",
	    "Getattr", "Lookup", "Readlink", "Read", "Write", "Rename",
	    "Link", "Readdir");
	fflush(stdout);
}

/*
 * Called if an interval expires before sidewaysintpr has completed a loop.
 * Sets a flag to not wait for the alarm.
 */
void
catchalarm()
{
	signalled = 1;
}

/*
 * Seek into the kernel for a value.
 */
off_t
klseek(fd, base, off)
	int fd, off;
	off_t base;
{
#ifndef NEWVM
	if (kflag) {
		/* get kernel pte */
		base &= ~KERNBASE;
		base = ctob(Sysmap[btop(base)].pg_pfnum) + (base & PGOFSET);
	}
#endif
	return (lseek(fd, base, off));
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: nfsstat [-M core] [-N system] [-w interval]\n");
	exit(1);
}
