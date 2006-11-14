/*
 * Copyright (c) 1983, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char copyright[] =
"@(#) Copyright (c) 1983, 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)nfsstat.c	8.2 (Berkeley) 3/31/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsserver/nfs.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <err.h>

struct nlist nl[] = {
#define	N_NFSSTAT	0
	{ "nfsstats" },
#define	N_NFSRVSTAT	1
	{ "nfsrvstats" },
	"",
};
kvm_t *kd;

static int deadkernel = 0;
static int widemode = 0;

void intpr(int, int);
void printhdr(int, int);
void sidewaysintpr(u_int, int, int);
void usage(void);
char *sperc1(int, int);
char *sperc2(int, int);

#define DELTA(field)	(nfsstats.field - lastst.field)

int
main(argc, argv)
	int argc;
	char **argv;
{
	u_int interval;
	int clientOnly = -1;
	int serverOnly = -1;
	int ch;
	char *memf, *nlistf;
	char errbuf[_POSIX2_LINE_MAX];

	interval = 0;
	memf = nlistf = NULL;
	while ((ch = getopt(argc, argv, "csWM:N:w:")) != -1)
		switch(ch) {
		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'W':
			widemode = 1;
			break;
		case 'w':
			interval = atoi(optarg);
			break;
		case 'c':
			clientOnly = 1;
			if (serverOnly < 0)
				serverOnly = 0;
			break;
		case 's':
			serverOnly = 1;
			if (clientOnly < 0)
				clientOnly = 0;
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
		interval = atoi(*argv);
		if (*++argv) {
			nlistf = *argv;
			if (*++argv)
				memf = *argv;
		}
	}
#endif
	if (nlistf != NULL || memf != NULL) {
		deadkernel = 1;

		if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY,
					errbuf)) == 0) {
			errx(1, "kvm_openfiles: %s", errbuf);
		}
		if (kvm_nlist(kd, nl) != 0) {
			errx(1, "kvm_nlist: can't get names");
		}
	}

	if (interval)
		sidewaysintpr(interval, clientOnly, serverOnly);
	else
		intpr(clientOnly, serverOnly);
	exit(0);
}

/*
 * Read the nfs stats using sysctl(3) for live kernels, or kvm_read
 * for dead ones.
 */
void
readstats(stp, srvstp)
	struct nfsstats **stp;
	struct nfsrvstats **srvstp;
{
	size_t buflen;

	if (deadkernel) {
		if (kvm_read(kd, (u_long)nl[N_NFSSTAT].n_value, *stp,
			    sizeof(struct nfsstats)) < 0) {
			*stp = NULL;
		}
		if (kvm_read(kd, (u_long)nl[N_NFSRVSTAT].n_value, *srvstp,
			    sizeof(struct nfsrvstats)) < 0) {
			*srvstp = NULL;
		}
	} else {
		buflen = sizeof(struct nfsstats);
		if (sysctlbyname("vfs.nfs.nfsstats", *stp, &buflen,
		    (void *)0, (size_t)0) < 0) {
			*stp = NULL;
		}
		buflen = sizeof(struct nfsrvstats);
		if (sysctlbyname("vfs.nfsrv.nfsrvstats", *srvstp, &buflen,
		    (void *)0, (size_t)0) < 0) {
			*srvstp = NULL;
		}
	}
}

/*
 * Print a description of the nfs stats.
 */
void
intpr(int clientOnly, int serverOnly)
{
	struct nfsstats nfsstats, *nfsstatsp;
	struct nfsrvstats nfsrvstats, *nfsrvstatsp;

	nfsstatsp = &nfsstats;
	nfsrvstatsp = &nfsrvstats;

	readstats(&nfsstatsp, &nfsrvstatsp);

	if (clientOnly && !nfsstatsp) {
		printf("Client not present!\n");
		clientOnly = 0;
	}
	if (clientOnly) {
		printf("Client Info:\n");
		printf("Rpc Counts:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Getattr", "Setattr", "Lookup", "Readlink", "Read",
			"Write", "Create", "Remove");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
			nfsstats.rpccnt[NFSPROC_GETATTR],
			nfsstats.rpccnt[NFSPROC_SETATTR],
			nfsstats.rpccnt[NFSPROC_LOOKUP],
			nfsstats.rpccnt[NFSPROC_READLINK],
			nfsstats.rpccnt[NFSPROC_READ],
			nfsstats.rpccnt[NFSPROC_WRITE],
			nfsstats.rpccnt[NFSPROC_CREATE],
			nfsstats.rpccnt[NFSPROC_REMOVE]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Rename", "Link", "Symlink", "Mkdir", "Rmdir",
			"Readdir", "RdirPlus", "Access");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
			nfsstats.rpccnt[NFSPROC_RENAME],
			nfsstats.rpccnt[NFSPROC_LINK],
			nfsstats.rpccnt[NFSPROC_SYMLINK],
			nfsstats.rpccnt[NFSPROC_MKDIR],
			nfsstats.rpccnt[NFSPROC_RMDIR],
			nfsstats.rpccnt[NFSPROC_READDIR],
			nfsstats.rpccnt[NFSPROC_READDIRPLUS],
			nfsstats.rpccnt[NFSPROC_ACCESS]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Mknod", "Fsstat", "Fsinfo", "PathConf", "Commit");
		printf("%9d %9d %9d %9d %9d\n",
			nfsstats.rpccnt[NFSPROC_MKNOD],
			nfsstats.rpccnt[NFSPROC_FSSTAT],
			nfsstats.rpccnt[NFSPROC_FSINFO],
			nfsstats.rpccnt[NFSPROC_PATHCONF],
			nfsstats.rpccnt[NFSPROC_COMMIT]);
		printf("Rpc Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"TimedOut", "Invalid", "X Replies", "Retries", 
			"Requests");
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
	}
	if (serverOnly && !nfsrvstatsp) {
		printf("Server not present!\n");
		serverOnly = 0;
	}
	if (serverOnly) {
		printf("\nServer Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Getattr", "Setattr", "Lookup", "Readlink", "Read",
			"Write", "Create", "Remove");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
			nfsrvstats.srvrpccnt[NFSPROC_GETATTR],
			nfsrvstats.srvrpccnt[NFSPROC_SETATTR],
			nfsrvstats.srvrpccnt[NFSPROC_LOOKUP],
			nfsrvstats.srvrpccnt[NFSPROC_READLINK],
			nfsrvstats.srvrpccnt[NFSPROC_READ],
			nfsrvstats.srvrpccnt[NFSPROC_WRITE],
			nfsrvstats.srvrpccnt[NFSPROC_CREATE],
			nfsrvstats.srvrpccnt[NFSPROC_REMOVE]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Rename", "Link", "Symlink", "Mkdir", "Rmdir",
			"Readdir", "RdirPlus", "Access");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
			nfsrvstats.srvrpccnt[NFSPROC_RENAME],
			nfsrvstats.srvrpccnt[NFSPROC_LINK],
			nfsrvstats.srvrpccnt[NFSPROC_SYMLINK],
			nfsrvstats.srvrpccnt[NFSPROC_MKDIR],
			nfsrvstats.srvrpccnt[NFSPROC_RMDIR],
			nfsrvstats.srvrpccnt[NFSPROC_READDIR],
			nfsrvstats.srvrpccnt[NFSPROC_READDIRPLUS],
			nfsrvstats.srvrpccnt[NFSPROC_ACCESS]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Mknod", "Fsstat", "Fsinfo", "PathConf", "Commit");
		printf("%9d %9d %9d %9d %9d\n",
			nfsrvstats.srvrpccnt[NFSPROC_MKNOD],
			nfsrvstats.srvrpccnt[NFSPROC_FSSTAT],
			nfsrvstats.srvrpccnt[NFSPROC_FSINFO],
			nfsrvstats.srvrpccnt[NFSPROC_PATHCONF],
			nfsrvstats.srvrpccnt[NFSPROC_COMMIT]);
		printf("Server Ret-Failed\n");
		printf("%17d\n", nfsrvstats.srvrpc_errs);
		printf("Server Faults\n");
		printf("%13d\n", nfsrvstats.srv_errs);
		printf("Server Cache Stats:\n");
		printf("%9.9s %9.9s %9.9s %9.9s\n",
			"Inprog", "Idem", "Non-idem", "Misses");
		printf("%9d %9d %9d %9d\n",
			nfsrvstats.srvcache_inproghits,
			nfsrvstats.srvcache_idemdonehits,
			nfsrvstats.srvcache_nonidemdonehits,
			nfsrvstats.srvcache_misses);
		printf("Server Write Gathering:\n");
		printf("%9.9s %9.9s %9.9s\n",
			"WriteOps", "WriteRPC", "Opsaved");
		printf("%9d %9d %9d\n",
			nfsrvstats.srvvop_writes,
			nfsrvstats.srvrpccnt[NFSPROC_WRITE],
			nfsrvstats.srvrpccnt[NFSPROC_WRITE] - 
			    nfsrvstats.srvvop_writes);
	}
}

u_char	signalled;			/* set if alarm goes off "early" */

/*
 * Print a running summary of nfs statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
void
sidewaysintpr(u_int interval, int clientOnly, int serverOnly)
{
	struct nfsstats nfsstats, lastst, *nfsstatsp;
	struct nfsrvstats nfsrvstats, lastsrvst, *nfsrvstatsp;
	int hdrcnt = 1;

	nfsstatsp = &lastst;
	nfsrvstatsp = &lastsrvst;
	readstats(&nfsstatsp, &nfsrvstatsp);
	if (clientOnly && !nfsstatsp) {
		printf("Client not present!\n");
		clientOnly = 0;
	}
	if (serverOnly && !nfsrvstatsp) {
		printf("Server not present!\n");
		serverOnly = 0;
	}
	sleep(interval);

	for (;;) {
		nfsstatsp = &nfsstats;
		nfsrvstatsp = &nfsrvstats;
		readstats(&nfsstatsp, &nfsrvstatsp);

		if (--hdrcnt == 0) {
			printhdr(clientOnly, serverOnly);
			if (clientOnly && serverOnly)
				hdrcnt = 10;
			else
				hdrcnt = 20;
		}
		if (clientOnly) {
		    printf("%s %6d %6d %6d %6d %6d %6d %6d %6d",
			((clientOnly && serverOnly) ? "Client:" : ""),
			DELTA(attrcache_hits) + DELTA(attrcache_misses),
			DELTA(lookupcache_hits) + DELTA(lookupcache_misses),
			DELTA(biocache_readlinks),
			DELTA(biocache_reads),
			DELTA(biocache_writes),
			nfsstats.rpccnt[NFSPROC_RENAME]-lastst.rpccnt[NFSPROC_RENAME],
			DELTA(accesscache_hits) + DELTA(accesscache_misses),
			DELTA(biocache_readdirs)
		    );
		    if (widemode) {
			    printf(" %s %s %s %s %s %s",
				sperc1(DELTA(attrcache_hits),
				    DELTA(attrcache_misses)),
				sperc1(DELTA(lookupcache_hits), 
				    DELTA(lookupcache_misses)),
				sperc2(DELTA(biocache_reads),
				    DELTA(read_bios)),
				sperc2(DELTA(biocache_writes),
				    DELTA(write_bios)),
				sperc1(DELTA(accesscache_hits),
				    DELTA(accesscache_misses)),
				sperc2(DELTA(biocache_readdirs),
				    DELTA(readdir_bios))
			    );
		    }
		    printf("\n");
		    lastst = nfsstats;
		}
		if (serverOnly) {
		    printf("%s %6d %6d %6d %6d %6d %6d %6d %6d",
			((clientOnly && serverOnly) ? "Server:" : ""),
			nfsrvstats.srvrpccnt[NFSPROC_GETATTR]-lastsrvst.srvrpccnt[NFSPROC_GETATTR],
			nfsrvstats.srvrpccnt[NFSPROC_LOOKUP]-lastsrvst.srvrpccnt[NFSPROC_LOOKUP],
			nfsrvstats.srvrpccnt[NFSPROC_READLINK]-lastsrvst.srvrpccnt[NFSPROC_READLINK],
			nfsrvstats.srvrpccnt[NFSPROC_READ]-lastsrvst.srvrpccnt[NFSPROC_READ],
			nfsrvstats.srvrpccnt[NFSPROC_WRITE]-lastsrvst.srvrpccnt[NFSPROC_WRITE],
			nfsrvstats.srvrpccnt[NFSPROC_RENAME]-lastsrvst.srvrpccnt[NFSPROC_RENAME],
			nfsrvstats.srvrpccnt[NFSPROC_ACCESS]-lastsrvst.srvrpccnt[NFSPROC_ACCESS],
			(nfsrvstats.srvrpccnt[NFSPROC_READDIR]-lastsrvst.srvrpccnt[NFSPROC_READDIR])
			+(nfsrvstats.srvrpccnt[NFSPROC_READDIRPLUS]-lastsrvst.srvrpccnt[NFSPROC_READDIRPLUS]));
		    printf("\n");
		    lastsrvst = nfsrvstats;
		}
		fflush(stdout);
		sleep(interval);
	}
	/*NOTREACHED*/
}

void
printhdr(int clientOnly, int serverOnly)
{
	printf("%s%6.6s %6.6s %6.6s %6.6s %6.6s %6.6s %6.6s %6.6s",
	    ((serverOnly && clientOnly) ? "        " : " "),
	    "GtAttr", "Lookup", "Rdlink", "Read", "Write", "Rename",
	    "Access", "Rddir");
	if (widemode && clientOnly) {
		printf(" Attr Lkup BioR BioW Accs BioD");
	}
	printf("\n");
	fflush(stdout);
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: nfsstat [-csW] [-M core] [-N system] [-w interval]\n");
	exit(1);
}

static char SPBuf[64][8];
static int SPIndex;

char * 
sperc1(int hits, int misses)
{
	char *p = SPBuf[SPIndex];

	if (hits + misses) {
		sprintf(p, "%3d%%", 
		    (int)(char)((quad_t)hits * 100 / (hits + misses)));
	} else {
		sprintf(p, "   -");
	}
	SPIndex = (SPIndex + 1) & 63;
	return(p);
}

char * 
sperc2(int ttl, int misses)
{
	char *p = SPBuf[SPIndex];

	if (ttl) {
		sprintf(p, "%3d%%",
		    (int)(char)((quad_t)(ttl - misses) * 100 / ttl));
	} else {
		sprintf(p, "   -");
	}
	SPIndex = (SPIndex + 1) & 63;
	return(p);
}

