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
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsserver/nfs.h>
#include <nfs/nfssvc.h>

#include <fs/nfs/nfsport.h>

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
	{ .n_name = "nfsstats" },
#define	N_NFSRVSTAT	1
	{ .n_name = "nfsrvstats" },
	{ .n_name = NULL },
};
kvm_t *kd;

static int deadkernel = 0;
static int widemode = 0;
static int zflag = 0;
static int run_v4 = 1;
static int printtitle = 1;
static struct ext_nfsstats ext_nfsstats;
static int extra_output = 0;

void intpr(int, int);
void printhdr(int, int);
void sidewaysintpr(u_int, int, int);
void usage(void);
char *sperc1(int, int);
char *sperc2(int, int);
void exp_intpr(int, int);
void exp_sidewaysintpr(u_int, int, int);

#define DELTA(field)	(nfsstats.field - lastst.field)

int
main(int argc, char **argv)
{
	u_int interval;
	int clientOnly = -1;
	int serverOnly = -1;
	int ch;
	char *memf, *nlistf;
	char errbuf[_POSIX2_LINE_MAX];
	int mntlen, i;
	char buf[1024];
	struct statfs *mntbuf;
	struct nfscl_dumpmntopts dumpmntopts;

	interval = 0;
	memf = nlistf = NULL;
	while ((ch = getopt(argc, argv, "cesWM:mN:ow:z")) != -1)
		switch(ch) {
		case 'M':
			memf = optarg;
			break;
		case 'm':
			/* Display mount options for NFS mount points. */
			mntlen = getmntinfo(&mntbuf, MNT_NOWAIT);
			for (i = 0; i < mntlen; i++) {
				if (strcmp(mntbuf->f_fstypename, "nfs") == 0) {
					dumpmntopts.ndmnt_fname =
					    mntbuf->f_mntonname;
					dumpmntopts.ndmnt_buf = buf;
					dumpmntopts.ndmnt_blen = sizeof(buf);
					if (nfssvc(NFSSVC_DUMPMNTOPTS,
					    &dumpmntopts) >= 0)
						printf("%s on %s\n%s\n",
						    mntbuf->f_mntfromname,
						    mntbuf->f_mntonname, buf);
					else if (errno == EPERM)
						errx(1, "Only priviledged users"
						    " can use the -m option");
				}
				mntbuf++;
			}
			exit(0);
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
		case 'z':
			zflag = 1;
			break;
		case 'o':
			if (extra_output != 0)
				err(1, "-o incompatible with -e");
			run_v4 = 0;
			break;
		case 'e':
			if (run_v4 == 0)
				err(1, "-e incompatible with -o");
			extra_output = 1;
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
	if (run_v4 != 0 && modfind("nfscommon") < 0)
		errx(1, "new client/server not loaded");

	if (run_v4 == 0 && (nlistf != NULL || memf != NULL)) {
		deadkernel = 1;

		if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY,
					errbuf)) == 0) {
			errx(1, "kvm_openfiles: %s", errbuf);
		}
		if (kvm_nlist(kd, nl) != 0) {
			errx(1, "kvm_nlist: can't get names");
		}
	}

	if (interval) {
		if (run_v4 > 0)
			exp_sidewaysintpr(interval, clientOnly, serverOnly);
		else
			sidewaysintpr(interval, clientOnly, serverOnly);
	} else {
		if (extra_output != 0)
			exp_intpr(clientOnly, serverOnly);
		else
			intpr(clientOnly, serverOnly);
	}
	exit(0);
}

/*
 * Read the nfs stats using sysctl(3) for live kernels, or kvm_read
 * for dead ones.
 */
static void
readstats(struct nfsstats **stp, struct nfsrvstats **srvstp, int zero)
{
	union {
		struct nfsstats client;
		struct nfsrvstats server;
	} zerostat;
	size_t buflen;

	if (deadkernel) {
		if (*stp != NULL && kvm_read(kd, (u_long)nl[N_NFSSTAT].n_value,
		    *stp, sizeof(struct nfsstats)) < 0) {
			*stp = NULL;
		}
		if (*srvstp != NULL && kvm_read(kd,
		    (u_long)nl[N_NFSRVSTAT].n_value, *srvstp,
		    sizeof(struct nfsrvstats)) < 0) {
			*srvstp = NULL;
		}
	} else {
		if (zero)
			bzero(&zerostat, sizeof(zerostat));
		buflen = sizeof(struct nfsstats);
		if (*stp != NULL && sysctlbyname("vfs.oldnfs.nfsstats", *stp,
		    &buflen, zero ? &zerostat : NULL, zero ? buflen : 0) < 0) {
			if (errno != ENOENT)
				err(1, "sysctl: vfs.oldnfs.nfsstats");
			*stp = NULL;
		}
		buflen = sizeof(struct nfsrvstats);
		if (*srvstp != NULL && sysctlbyname("vfs.nfsrv.nfsrvstats",
		    *srvstp, &buflen, zero ? &zerostat : NULL,
		    zero ? buflen : 0) < 0) {
			if (errno != ENOENT)
				err(1, "sysctl: vfs.nfsrv.nfsrvstats");
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
	int nfssvc_flag;

	if (run_v4 == 0) {
		/*
		 * Only read the stats we are going to display to avoid zeroing
		 * stats the user didn't request.
		 */
		if (clientOnly)
			nfsstatsp = &nfsstats;
		else
			nfsstatsp = NULL;
		if (serverOnly)
			nfsrvstatsp = &nfsrvstats;
		else
			nfsrvstatsp = NULL;
	
		readstats(&nfsstatsp, &nfsrvstatsp, zflag);
	
		if (clientOnly && !nfsstatsp) {
			printf("Client not present!\n");
			clientOnly = 0;
		}
	} else {
		nfssvc_flag = NFSSVC_GETSTATS;
		if (zflag != 0) {
			if (clientOnly != 0)
				nfssvc_flag |= NFSSVC_ZEROCLTSTATS;
			if (serverOnly != 0)
				nfssvc_flag |= NFSSVC_ZEROSRVSTATS;
		}
		if (nfssvc(nfssvc_flag, &ext_nfsstats) < 0)
			err(1, "Can't get stats");
	}
	if (clientOnly) {
		printf("Client Info:\n");
		printf("Rpc Counts:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Getattr", "Setattr", "Lookup", "Readlink", "Read",
			"Write", "Create", "Remove");
		if (run_v4 == 0)
			printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
				nfsstats.rpccnt[NFSPROC_GETATTR],
				nfsstats.rpccnt[NFSPROC_SETATTR],
				nfsstats.rpccnt[NFSPROC_LOOKUP],
				nfsstats.rpccnt[NFSPROC_READLINK],
				nfsstats.rpccnt[NFSPROC_READ],
				nfsstats.rpccnt[NFSPROC_WRITE],
				nfsstats.rpccnt[NFSPROC_CREATE],
				nfsstats.rpccnt[NFSPROC_REMOVE]);
		else
			printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
				ext_nfsstats.rpccnt[NFSPROC_GETATTR],
				ext_nfsstats.rpccnt[NFSPROC_SETATTR],
				ext_nfsstats.rpccnt[NFSPROC_LOOKUP],
				ext_nfsstats.rpccnt[NFSPROC_READLINK],
				ext_nfsstats.rpccnt[NFSPROC_READ],
				ext_nfsstats.rpccnt[NFSPROC_WRITE],
				ext_nfsstats.rpccnt[NFSPROC_CREATE],
				ext_nfsstats.rpccnt[NFSPROC_REMOVE]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Rename", "Link", "Symlink", "Mkdir", "Rmdir",
			"Readdir", "RdirPlus", "Access");
		if (run_v4 == 0)
			printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
				nfsstats.rpccnt[NFSPROC_RENAME],
				nfsstats.rpccnt[NFSPROC_LINK],
				nfsstats.rpccnt[NFSPROC_SYMLINK],
				nfsstats.rpccnt[NFSPROC_MKDIR],
				nfsstats.rpccnt[NFSPROC_RMDIR],
				nfsstats.rpccnt[NFSPROC_READDIR],
				nfsstats.rpccnt[NFSPROC_READDIRPLUS],
				nfsstats.rpccnt[NFSPROC_ACCESS]);
		else
			printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
				ext_nfsstats.rpccnt[NFSPROC_RENAME],
				ext_nfsstats.rpccnt[NFSPROC_LINK],
				ext_nfsstats.rpccnt[NFSPROC_SYMLINK],
				ext_nfsstats.rpccnt[NFSPROC_MKDIR],
				ext_nfsstats.rpccnt[NFSPROC_RMDIR],
				ext_nfsstats.rpccnt[NFSPROC_READDIR],
				ext_nfsstats.rpccnt[NFSPROC_READDIRPLUS],
				ext_nfsstats.rpccnt[NFSPROC_ACCESS]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Mknod", "Fsstat", "Fsinfo", "PathConf", "Commit");
		if (run_v4 == 0)
			printf("%9d %9d %9d %9d %9d\n",
				nfsstats.rpccnt[NFSPROC_MKNOD],
				nfsstats.rpccnt[NFSPROC_FSSTAT],
				nfsstats.rpccnt[NFSPROC_FSINFO],
				nfsstats.rpccnt[NFSPROC_PATHCONF],
				nfsstats.rpccnt[NFSPROC_COMMIT]);
		else
			printf("%9d %9d %9d %9d %9d\n",
				ext_nfsstats.rpccnt[NFSPROC_MKNOD],
				ext_nfsstats.rpccnt[NFSPROC_FSSTAT],
				ext_nfsstats.rpccnt[NFSPROC_FSINFO],
				ext_nfsstats.rpccnt[NFSPROC_PATHCONF],
				ext_nfsstats.rpccnt[NFSPROC_COMMIT]);
		printf("Rpc Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"TimedOut", "Invalid", "X Replies", "Retries", 
			"Requests");
		if (run_v4 == 0)
			printf("%9d %9d %9d %9d %9d\n",
				nfsstats.rpctimeouts,
				nfsstats.rpcinvalid,
				nfsstats.rpcunexpected,
				nfsstats.rpcretries,
				nfsstats.rpcrequests);
		else
			printf("%9d %9d %9d %9d %9d\n",
				ext_nfsstats.rpctimeouts,
				ext_nfsstats.rpcinvalid,
				ext_nfsstats.rpcunexpected,
				ext_nfsstats.rpcretries,
				ext_nfsstats.rpcrequests);
		printf("Cache Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s",
			"Attr Hits", "Misses", "Lkup Hits", "Misses");
		printf(" %9.9s %9.9s %9.9s %9.9s\n",
			"BioR Hits", "Misses", "BioW Hits", "Misses");
		if (run_v4 == 0) {
			printf("%9d %9d %9d %9d",
				nfsstats.attrcache_hits,
				nfsstats.attrcache_misses,
				nfsstats.lookupcache_hits,
				nfsstats.lookupcache_misses);
			printf(" %9d %9d %9d %9d\n",
				nfsstats.biocache_reads-nfsstats.read_bios,
				nfsstats.read_bios,
				nfsstats.biocache_writes-nfsstats.write_bios,
				nfsstats.write_bios);
		} else {
			printf("%9d %9d %9d %9d",
				ext_nfsstats.attrcache_hits,
				ext_nfsstats.attrcache_misses,
				ext_nfsstats.lookupcache_hits,
				ext_nfsstats.lookupcache_misses);
			printf(" %9d %9d %9d %9d\n",
				ext_nfsstats.biocache_reads -
				ext_nfsstats.read_bios,
				ext_nfsstats.read_bios,
				ext_nfsstats.biocache_writes -
				ext_nfsstats.write_bios,
				ext_nfsstats.write_bios);
		}
		printf("%9.9s %9.9s %9.9s %9.9s",
			"BioRLHits", "Misses", "BioD Hits", "Misses");
		printf(" %9.9s %9.9s %9.9s %9.9s\n", "DirE Hits", "Misses", "Accs Hits", "Misses");
		if (run_v4 == 0) {
			printf("%9d %9d %9d %9d",
				nfsstats.biocache_readlinks -
				nfsstats.readlink_bios,
				nfsstats.readlink_bios,
				nfsstats.biocache_readdirs -
				nfsstats.readdir_bios,
				nfsstats.readdir_bios);
			printf(" %9d %9d %9d %9d\n",
				nfsstats.direofcache_hits,
				nfsstats.direofcache_misses,
				nfsstats.accesscache_hits,
				nfsstats.accesscache_misses);
		} else {
			printf("%9d %9d %9d %9d",
				ext_nfsstats.biocache_readlinks -
				ext_nfsstats.readlink_bios,
				ext_nfsstats.readlink_bios,
				ext_nfsstats.biocache_readdirs -
				ext_nfsstats.readdir_bios,
				ext_nfsstats.readdir_bios);
			printf(" %9d %9d %9d %9d\n",
				ext_nfsstats.direofcache_hits,
				ext_nfsstats.direofcache_misses,
				ext_nfsstats.accesscache_hits,
				ext_nfsstats.accesscache_misses);
		}
	}
	if (run_v4 == 0 && serverOnly && !nfsrvstatsp) {
		printf("Server not present!\n");
		serverOnly = 0;
	}
	if (serverOnly) {
		printf("\nServer Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Getattr", "Setattr", "Lookup", "Readlink", "Read",
			"Write", "Create", "Remove");
		if (run_v4 == 0)
			printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
				nfsrvstats.srvrpccnt[NFSPROC_GETATTR],
				nfsrvstats.srvrpccnt[NFSPROC_SETATTR],
				nfsrvstats.srvrpccnt[NFSPROC_LOOKUP],
				nfsrvstats.srvrpccnt[NFSPROC_READLINK],
				nfsrvstats.srvrpccnt[NFSPROC_READ],
				nfsrvstats.srvrpccnt[NFSPROC_WRITE],
				nfsrvstats.srvrpccnt[NFSPROC_CREATE],
				nfsrvstats.srvrpccnt[NFSPROC_REMOVE]);
		else
			printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
				ext_nfsstats.srvrpccnt[NFSV4OP_GETATTR],
				ext_nfsstats.srvrpccnt[NFSV4OP_SETATTR],
				ext_nfsstats.srvrpccnt[NFSV4OP_LOOKUP],
				ext_nfsstats.srvrpccnt[NFSV4OP_READLINK],
				ext_nfsstats.srvrpccnt[NFSV4OP_READ],
				ext_nfsstats.srvrpccnt[NFSV4OP_WRITE],
				ext_nfsstats.srvrpccnt[NFSV4OP_CREATE],
				ext_nfsstats.srvrpccnt[NFSV4OP_REMOVE]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Rename", "Link", "Symlink", "Mkdir", "Rmdir",
			"Readdir", "RdirPlus", "Access");
		if (run_v4 == 0)
			printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
				nfsrvstats.srvrpccnt[NFSPROC_RENAME],
				nfsrvstats.srvrpccnt[NFSPROC_LINK],
				nfsrvstats.srvrpccnt[NFSPROC_SYMLINK],
				nfsrvstats.srvrpccnt[NFSPROC_MKDIR],
				nfsrvstats.srvrpccnt[NFSPROC_RMDIR],
				nfsrvstats.srvrpccnt[NFSPROC_READDIR],
				nfsrvstats.srvrpccnt[NFSPROC_READDIRPLUS],
				nfsrvstats.srvrpccnt[NFSPROC_ACCESS]);
		else
			printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
				ext_nfsstats.srvrpccnt[NFSV4OP_RENAME],
				ext_nfsstats.srvrpccnt[NFSV4OP_LINK],
				ext_nfsstats.srvrpccnt[NFSV4OP_SYMLINK],
				ext_nfsstats.srvrpccnt[NFSV4OP_MKDIR],
				ext_nfsstats.srvrpccnt[NFSV4OP_RMDIR],
				ext_nfsstats.srvrpccnt[NFSV4OP_READDIR],
				ext_nfsstats.srvrpccnt[NFSV4OP_READDIRPLUS],
				ext_nfsstats.srvrpccnt[NFSV4OP_ACCESS]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Mknod", "Fsstat", "Fsinfo", "PathConf", "Commit");
		if (run_v4 == 0)
			printf("%9d %9d %9d %9d %9d\n",
				nfsrvstats.srvrpccnt[NFSPROC_MKNOD],
				nfsrvstats.srvrpccnt[NFSPROC_FSSTAT],
				nfsrvstats.srvrpccnt[NFSPROC_FSINFO],
				nfsrvstats.srvrpccnt[NFSPROC_PATHCONF],
				nfsrvstats.srvrpccnt[NFSPROC_COMMIT]);
		else
			printf("%9d %9d %9d %9d %9d\n",
				ext_nfsstats.srvrpccnt[NFSV4OP_MKNOD],
				ext_nfsstats.srvrpccnt[NFSV4OP_FSSTAT],
				ext_nfsstats.srvrpccnt[NFSV4OP_FSINFO],
				ext_nfsstats.srvrpccnt[NFSV4OP_PATHCONF],
				ext_nfsstats.srvrpccnt[NFSV4OP_COMMIT]);
		printf("Server Ret-Failed\n");
		if (run_v4 == 0)
			printf("%17d\n", nfsrvstats.srvrpc_errs);
		else
			printf("%17d\n", ext_nfsstats.srvrpc_errs);
		printf("Server Faults\n");
		if (run_v4 == 0)
			printf("%13d\n", nfsrvstats.srv_errs);
		else
			printf("%13d\n", ext_nfsstats.srv_errs);
		printf("Server Cache Stats:\n");
		printf("%9.9s %9.9s %9.9s %9.9s\n",
			"Inprog", "Idem", "Non-idem", "Misses");
		if (run_v4 == 0)
			printf("%9d %9d %9d %9d\n",
				nfsrvstats.srvcache_inproghits,
				nfsrvstats.srvcache_idemdonehits,
				nfsrvstats.srvcache_nonidemdonehits,
				nfsrvstats.srvcache_misses);
		else
			printf("%9d %9d %9d %9d\n",
				ext_nfsstats.srvcache_inproghits,
				ext_nfsstats.srvcache_idemdonehits,
				ext_nfsstats.srvcache_nonidemdonehits,
				ext_nfsstats.srvcache_misses);
		printf("Server Write Gathering:\n");
		printf("%9.9s %9.9s %9.9s\n",
			"WriteOps", "WriteRPC", "Opsaved");
		if (run_v4 == 0)
			printf("%9d %9d %9d\n",
				nfsrvstats.srvvop_writes,
				nfsrvstats.srvrpccnt[NFSPROC_WRITE],
				nfsrvstats.srvrpccnt[NFSPROC_WRITE] - 
				    nfsrvstats.srvvop_writes);
		else
			/*
			 * The new client doesn't do write gathering. It was
			 * only useful for NFSv2.
			 */
			printf("%9d %9d %9d\n",
				ext_nfsstats.srvrpccnt[NFSV4OP_WRITE],
				ext_nfsstats.srvrpccnt[NFSV4OP_WRITE], 0);
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
	readstats(&nfsstatsp, &nfsrvstatsp, 0);
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
		readstats(&nfsstatsp, &nfsrvstatsp, 0);

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
usage(void)
{
	(void)fprintf(stderr,
	    "usage: nfsstat [-cemoszW] [-M core] [-N system] [-w wait]\n");
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

/*
 * Print a description of the nfs stats for the experimental client/server.
 */
void
exp_intpr(int clientOnly, int serverOnly)
{
	int nfssvc_flag;

	nfssvc_flag = NFSSVC_GETSTATS;
	if (zflag != 0) {
		if (clientOnly != 0)
			nfssvc_flag |= NFSSVC_ZEROCLTSTATS;
		if (serverOnly != 0)
			nfssvc_flag |= NFSSVC_ZEROSRVSTATS;
	}
	if (nfssvc(nfssvc_flag, &ext_nfsstats) < 0)
		err(1, "Can't get stats");
	if (clientOnly != 0) {
		if (printtitle) {
			printf("Client Info:\n");
			printf("Rpc Counts:\n");
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Getattr", "Setattr", "Lookup", "Readlink",
			    "Read", "Write", "Create", "Remove");
		}
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.rpccnt[NFSPROC_GETATTR],
		    ext_nfsstats.rpccnt[NFSPROC_SETATTR],
		    ext_nfsstats.rpccnt[NFSPROC_LOOKUP],
		    ext_nfsstats.rpccnt[NFSPROC_READLINK],
		    ext_nfsstats.rpccnt[NFSPROC_READ],
		    ext_nfsstats.rpccnt[NFSPROC_WRITE],
		    ext_nfsstats.rpccnt[NFSPROC_CREATE],
		    ext_nfsstats.rpccnt[NFSPROC_REMOVE]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Rename", "Link", "Symlink", "Mkdir", "Rmdir",
			    "Readdir", "RdirPlus", "Access");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.rpccnt[NFSPROC_RENAME],
		    ext_nfsstats.rpccnt[NFSPROC_LINK],
		    ext_nfsstats.rpccnt[NFSPROC_SYMLINK],
		    ext_nfsstats.rpccnt[NFSPROC_MKDIR],
		    ext_nfsstats.rpccnt[NFSPROC_RMDIR],
		    ext_nfsstats.rpccnt[NFSPROC_READDIR],
		    ext_nfsstats.rpccnt[NFSPROC_READDIRPLUS],
		    ext_nfsstats.rpccnt[NFSPROC_ACCESS]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Mknod", "Fsstat", "Fsinfo", "PathConf",
			    "Commit", "SetClId", "SetClIdCf", "Lock");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.rpccnt[NFSPROC_MKNOD],
		    ext_nfsstats.rpccnt[NFSPROC_FSSTAT],
		    ext_nfsstats.rpccnt[NFSPROC_FSINFO],
		    ext_nfsstats.rpccnt[NFSPROC_PATHCONF],
		    ext_nfsstats.rpccnt[NFSPROC_COMMIT],
		    ext_nfsstats.rpccnt[NFSPROC_SETCLIENTID],
		    ext_nfsstats.rpccnt[NFSPROC_SETCLIENTIDCFRM],
		    ext_nfsstats.rpccnt[NFSPROC_LOCK]);
		if (printtitle)
			printf("%9.9s %9.9s %9.9s %9.9s\n",
			    "LockT", "LockU", "Open", "OpenCfr");
		printf("%9d %9d %9d %9d\n",
		    ext_nfsstats.rpccnt[NFSPROC_LOCKT],
		    ext_nfsstats.rpccnt[NFSPROC_LOCKU],
		    ext_nfsstats.rpccnt[NFSPROC_OPEN],
		    ext_nfsstats.rpccnt[NFSPROC_OPENCONFIRM]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "OpenOwner", "Opens", "LockOwner",
			    "Locks", "Delegs", "LocalOwn",
			    "LocalOpen", "LocalLOwn");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.clopenowners,
		    ext_nfsstats.clopens,
		    ext_nfsstats.cllockowners,
		    ext_nfsstats.cllocks,
		    ext_nfsstats.cldelegates,
		    ext_nfsstats.cllocalopenowners,
		    ext_nfsstats.cllocalopens,
		    ext_nfsstats.cllocallockowners);
		if (printtitle)
			printf("%9.9s\n", "LocalLock");
		printf("%9d\n", ext_nfsstats.cllocallocks);
		if (printtitle) {
			printf("Rpc Info:\n");
			printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			    "TimedOut", "Invalid", "X Replies", "Retries",
			    "Requests");
		}
		printf("%9d %9d %9d %9d %9d\n",
		    ext_nfsstats.rpctimeouts,
		    ext_nfsstats.rpcinvalid,
		    ext_nfsstats.rpcunexpected,
		    ext_nfsstats.rpcretries,
		    ext_nfsstats.rpcrequests);
		if (printtitle) {
			printf("Cache Info:\n");
			printf("%9.9s %9.9s %9.9s %9.9s",
			    "Attr Hits", "Misses", "Lkup Hits", "Misses");
			printf(" %9.9s %9.9s %9.9s %9.9s\n",
			    "BioR Hits", "Misses", "BioW Hits", "Misses");
		}
		printf("%9d %9d %9d %9d",
		    ext_nfsstats.attrcache_hits,
		    ext_nfsstats.attrcache_misses,
		    ext_nfsstats.lookupcache_hits,
		    ext_nfsstats.lookupcache_misses);
		printf(" %9d %9d %9d %9d\n",
		    ext_nfsstats.biocache_reads - ext_nfsstats.read_bios,
		    ext_nfsstats.read_bios,
		    ext_nfsstats.biocache_writes - ext_nfsstats.write_bios,
		    ext_nfsstats.write_bios);
		if (printtitle) {
			printf("%9.9s %9.9s %9.9s %9.9s",
			    "BioRLHits", "Misses", "BioD Hits", "Misses");
			printf(" %9.9s %9.9s\n", "DirE Hits", "Misses");
		}
		printf("%9d %9d %9d %9d",
		    ext_nfsstats.biocache_readlinks -
		    ext_nfsstats.readlink_bios,
		    ext_nfsstats.readlink_bios,
		    ext_nfsstats.biocache_readdirs -
		    ext_nfsstats.readdir_bios,
		    ext_nfsstats.readdir_bios);
		printf(" %9d %9d\n",
		    ext_nfsstats.direofcache_hits,
		    ext_nfsstats.direofcache_misses);
	}
	if (serverOnly != 0) {
		if (printtitle) {
			printf("\nServer Info:\n");
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Getattr", "Setattr", "Lookup", "Readlink",
			    "Read", "Write", "Create", "Remove");
		}
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.srvrpccnt[NFSV4OP_GETATTR],
		    ext_nfsstats.srvrpccnt[NFSV4OP_SETATTR],
		    ext_nfsstats.srvrpccnt[NFSV4OP_LOOKUP],
		    ext_nfsstats.srvrpccnt[NFSV4OP_READLINK],
		    ext_nfsstats.srvrpccnt[NFSV4OP_READ],
		    ext_nfsstats.srvrpccnt[NFSV4OP_WRITE],
		    ext_nfsstats.srvrpccnt[NFSV4OP_V3CREATE],
		    ext_nfsstats.srvrpccnt[NFSV4OP_REMOVE]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Rename", "Link", "Symlink", "Mkdir", "Rmdir",
			    "Readdir", "RdirPlus", "Access");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.srvrpccnt[NFSV4OP_RENAME],
		    ext_nfsstats.srvrpccnt[NFSV4OP_LINK],
		    ext_nfsstats.srvrpccnt[NFSV4OP_SYMLINK],
		    ext_nfsstats.srvrpccnt[NFSV4OP_MKDIR],
		    ext_nfsstats.srvrpccnt[NFSV4OP_RMDIR],
		    ext_nfsstats.srvrpccnt[NFSV4OP_READDIR],
		    ext_nfsstats.srvrpccnt[NFSV4OP_READDIRPLUS],
		    ext_nfsstats.srvrpccnt[NFSV4OP_ACCESS]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Mknod", "Fsstat", "Fsinfo", "PathConf",
			    "Commit", "LookupP", "SetClId", "SetClIdCf");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.srvrpccnt[NFSV4OP_MKNOD],
		    ext_nfsstats.srvrpccnt[NFSV4OP_FSSTAT],
		    ext_nfsstats.srvrpccnt[NFSV4OP_FSINFO],
		    ext_nfsstats.srvrpccnt[NFSV4OP_PATHCONF],
		    ext_nfsstats.srvrpccnt[NFSV4OP_COMMIT],
		    ext_nfsstats.srvrpccnt[NFSV4OP_LOOKUPP],
		    ext_nfsstats.srvrpccnt[NFSV4OP_SETCLIENTID],
		    ext_nfsstats.srvrpccnt[NFSV4OP_SETCLIENTIDCFRM]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Open", "OpenAttr", "OpenDwnGr", "OpenCfrm",
			    "DelePurge", "DeleRet", "GetFH", "Lock");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.srvrpccnt[NFSV4OP_OPEN],
		    ext_nfsstats.srvrpccnt[NFSV4OP_OPENATTR],
		    ext_nfsstats.srvrpccnt[NFSV4OP_OPENDOWNGRADE],
		    ext_nfsstats.srvrpccnt[NFSV4OP_OPENCONFIRM],
		    ext_nfsstats.srvrpccnt[NFSV4OP_DELEGPURGE],
		    ext_nfsstats.srvrpccnt[NFSV4OP_DELEGRETURN],
		    ext_nfsstats.srvrpccnt[NFSV4OP_GETFH],
		    ext_nfsstats.srvrpccnt[NFSV4OP_LOCK]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "LockT", "LockU", "Close", "Verify", "NVerify",
			    "PutFH", "PutPubFH", "PutRootFH");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.srvrpccnt[NFSV4OP_LOCKT],
		    ext_nfsstats.srvrpccnt[NFSV4OP_LOCKU],
		    ext_nfsstats.srvrpccnt[NFSV4OP_CLOSE],
		    ext_nfsstats.srvrpccnt[NFSV4OP_VERIFY],
		    ext_nfsstats.srvrpccnt[NFSV4OP_NVERIFY],
		    ext_nfsstats.srvrpccnt[NFSV4OP_PUTFH],
		    ext_nfsstats.srvrpccnt[NFSV4OP_PUTPUBFH],
		    ext_nfsstats.srvrpccnt[NFSV4OP_PUTROOTFH]);
		if (printtitle)
			printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			    "Renew", "RestoreFH", "SaveFH", "Secinfo",
			    "RelLckOwn", "V4Create");
		printf("%9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.srvrpccnt[NFSV4OP_RENEW],
		    ext_nfsstats.srvrpccnt[NFSV4OP_RESTOREFH],
		    ext_nfsstats.srvrpccnt[NFSV4OP_SAVEFH],
		    ext_nfsstats.srvrpccnt[NFSV4OP_SECINFO],
		    ext_nfsstats.srvrpccnt[NFSV4OP_RELEASELCKOWN],
		    ext_nfsstats.srvrpccnt[NFSV4OP_CREATE]);
		if (printtitle) {
			printf("Server:\n");
			printf("%9.9s %9.9s %9.9s\n",
			    "Retfailed", "Faults", "Clients");
		}
		printf("%9d %9d %9d\n",
		    ext_nfsstats.srv_errs, ext_nfsstats.srvrpc_errs,
		    ext_nfsstats.srvclients);
		if (printtitle)
			printf("%9.9s %9.9s %9.9s %9.9s %9.9s \n",
			    "OpenOwner", "Opens", "LockOwner",
			    "Locks", "Delegs");
		printf("%9d %9d %9d %9d %9d \n",
		    ext_nfsstats.srvopenowners,
		    ext_nfsstats.srvopens,
		    ext_nfsstats.srvlockowners,
		    ext_nfsstats.srvlocks,
		    ext_nfsstats.srvdelegates);
		if (printtitle) {
			printf("Server Cache Stats:\n");
			printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			    "Inprog", "Idem", "Non-idem", "Misses", 
			    "CacheSize", "TCPPeak");
		}
		printf("%9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.srvcache_inproghits,
		    ext_nfsstats.srvcache_idemdonehits,
		    ext_nfsstats.srvcache_nonidemdonehits,
		    ext_nfsstats.srvcache_misses,
		    ext_nfsstats.srvcache_size,
		    ext_nfsstats.srvcache_tcppeak);
	}
}

/*
 * Print a running summary of nfs statistics for the experimental client and/or
 * server.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
void
exp_sidewaysintpr(u_int interval, int clientOnly, int serverOnly)
{
	struct ext_nfsstats nfsstats, lastst, *ext_nfsstatsp;
	int hdrcnt = 1;

	ext_nfsstatsp = &lastst;
	if (nfssvc(NFSSVC_GETSTATS, ext_nfsstatsp) < 0)
		err(1, "Can't get stats");
	sleep(interval);

	for (;;) {
		ext_nfsstatsp = &nfsstats;
		if (nfssvc(NFSSVC_GETSTATS, ext_nfsstatsp) < 0)
			err(1, "Can't get stats");

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
			nfsstats.rpccnt[NFSPROC_RENAME] -
			lastst.rpccnt[NFSPROC_RENAME],
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
		}
		if (serverOnly) {
		    printf("%s %6d %6d %6d %6d %6d %6d %6d %6d",
			((clientOnly && serverOnly) ? "Server:" : ""),
			nfsstats.srvrpccnt[NFSV4OP_GETATTR] -
			lastst.srvrpccnt[NFSV4OP_GETATTR],
			nfsstats.srvrpccnt[NFSV4OP_LOOKUP] -
			lastst.srvrpccnt[NFSV4OP_LOOKUP],
			nfsstats.srvrpccnt[NFSV4OP_READLINK] -
			lastst.srvrpccnt[NFSV4OP_READLINK],
			nfsstats.srvrpccnt[NFSV4OP_READ] -
			lastst.srvrpccnt[NFSV4OP_READ],
			nfsstats.srvrpccnt[NFSV4OP_WRITE] -
			lastst.srvrpccnt[NFSV4OP_WRITE],
			nfsstats.srvrpccnt[NFSV4OP_RENAME] -
			lastst.srvrpccnt[NFSV4OP_RENAME],
			nfsstats.srvrpccnt[NFSV4OP_ACCESS] -
			lastst.srvrpccnt[NFSV4OP_ACCESS],
			(nfsstats.srvrpccnt[NFSV4OP_READDIR] -
			 lastst.srvrpccnt[NFSV4OP_READDIR]) +
			(nfsstats.srvrpccnt[NFSV4OP_READDIRPLUS] -
			 lastst.srvrpccnt[NFSV4OP_READDIRPLUS]));
		    printf("\n");
		}
		lastst = nfsstats;
		fflush(stdout);
		sleep(interval);
	}
	/*NOTREACHED*/
}

