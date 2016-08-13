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
/*-
 * Copyright (c) 2004, 2008, 2009 Silicon Graphics International Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
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
#include <limits.h>
#include <nlist.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <devstat.h>
#include <err.h>

static int widemode = 0;
static int zflag = 0;
static int printtitle = 1;
static struct nfsstatsv1 ext_nfsstats;
static int extra_output = 0;

static void intpr(int, int);
static void printhdr(int, int, int);
static void usage(void);
static char *sperc1(int, int);
static char *sperc2(int, int);
static void exp_intpr(int, int);
static void exp_sidewaysintpr(u_int, int, int, int);
static void compute_new_stats(struct nfsstatsv1 *cur_stats,
    struct nfsstatsv1 *prev_stats, int curop, long double etime,
    long double *mbsec, long double *kb_per_transfer,
    long double *transfers_per_second, long double *ms_per_transfer,
    uint64_t *queue_len, long double *busy_pct);

#define DELTA(field)	(nfsstats.field - lastst.field)

#define	STAT_TYPE_READ		0
#define	STAT_TYPE_WRITE		1
#define	STAT_TYPE_COMMIT	2
#define	NUM_STAT_TYPES		3

struct stattypes {
	int stat_type;
	int nfs_type;
} static statstruct[] = {
	{STAT_TYPE_READ, NFSV4OP_READ},
	{STAT_TYPE_WRITE, NFSV4OP_WRITE},
	{STAT_TYPE_COMMIT, NFSV4OP_COMMIT}
};

#define	STAT_TYPE_TO_NFS(stat_type)	statstruct[stat_type].nfs_type

int
main(int argc, char **argv)
{
	u_int interval;
	int clientOnly = -1;
	int serverOnly = -1;
	int newStats = 0;
	int ch;
	char *memf, *nlistf;
	int mntlen, i;
	char buf[1024];
	struct statfs *mntbuf;
	struct nfscl_dumpmntopts dumpmntopts;

	interval = 0;
	memf = nlistf = NULL;
	while ((ch = getopt(argc, argv, "cdesWM:mN:w:z")) != -1)
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
		case 'd':
			newStats = 1;
			if (interval == 0)
				interval = 1;
			break;
		case 's':
			serverOnly = 1;
			if (clientOnly < 0)
				clientOnly = 0;
			break;
		case 'z':
			zflag = 1;
			break;
		case 'e':
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
	if (modfind("nfscommon") < 0)
		errx(1, "NFS client/server not loaded");

	if (interval) {
		exp_sidewaysintpr(interval, clientOnly, serverOnly,
		    newStats);
	} else {
		if (extra_output != 0)
			exp_intpr(clientOnly, serverOnly);
		else
			intpr(clientOnly, serverOnly);
	}
	exit(0);
}

/*
 * Print a description of the nfs stats.
 */
static void
intpr(int clientOnly, int serverOnly)
{
	int nfssvc_flag;

	nfssvc_flag = NFSSVC_GETSTATS | NFSSVC_NEWSTRUCT;
	if (zflag != 0) {
		if (clientOnly != 0)
			nfssvc_flag |= NFSSVC_ZEROCLTSTATS;
		if (serverOnly != 0)
			nfssvc_flag |= NFSSVC_ZEROSRVSTATS;
	}
	ext_nfsstats.vers = NFSSTATS_V1;
	if (nfssvc(nfssvc_flag, &ext_nfsstats) < 0)
		err(1, "Can't get stats");
	if (clientOnly) {
		printf("Client Info:\n");
		printf("Rpc Counts:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Getattr", "Setattr", "Lookup", "Readlink", "Read",
			"Write", "Create", "Remove");
		printf("%9ju %9ju %9ju %9ju %9ju %9ju %9ju %9ju\n",
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_GETATTR],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_SETATTR],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_LOOKUP],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_READLINK],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_READ],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_WRITE],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_CREATE],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_REMOVE]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Rename", "Link", "Symlink", "Mkdir", "Rmdir",
			"Readdir", "RdirPlus", "Access");
		printf("%9ju %9ju %9ju %9ju %9ju %9ju %9ju %9ju\n",
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_RENAME],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_LINK],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_SYMLINK],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_MKDIR],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_RMDIR],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_READDIR],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_READDIRPLUS],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_ACCESS]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Mknod", "Fsstat", "Fsinfo", "PathConf", "Commit");
		printf("%9ju %9ju %9ju %9ju %9ju\n",
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_MKNOD],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_FSSTAT],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_FSINFO],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_PATHCONF],
			(uintmax_t)ext_nfsstats.rpccnt[NFSPROC_COMMIT]);
		printf("Rpc Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"TimedOut", "Invalid", "X Replies", "Retries", 
			"Requests");
		printf("%9ju %9ju %9ju %9ju %9ju\n",
			(uintmax_t)ext_nfsstats.rpctimeouts,
			(uintmax_t)ext_nfsstats.rpcinvalid,
			(uintmax_t)ext_nfsstats.rpcunexpected,
			(uintmax_t)ext_nfsstats.rpcretries,
			(uintmax_t)ext_nfsstats.rpcrequests);
		printf("Cache Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s",
			"Attr Hits", "Misses", "Lkup Hits", "Misses");
		printf(" %9.9s %9.9s %9.9s %9.9s\n",
			"BioR Hits", "Misses", "BioW Hits", "Misses");
		printf("%9ju %9ju %9ju %9ju",
			(uintmax_t)ext_nfsstats.attrcache_hits,
			(uintmax_t)ext_nfsstats.attrcache_misses,
			(uintmax_t)ext_nfsstats.lookupcache_hits,
			(uintmax_t)ext_nfsstats.lookupcache_misses);
		printf(" %9ju %9ju %9ju %9ju\n",
			(uintmax_t)(ext_nfsstats.biocache_reads -
			ext_nfsstats.read_bios),
			(uintmax_t)ext_nfsstats.read_bios,
			(uintmax_t)(ext_nfsstats.biocache_writes -
			ext_nfsstats.write_bios),
			(uintmax_t)ext_nfsstats.write_bios);
		printf("%9.9s %9.9s %9.9s %9.9s",
			"BioRLHits", "Misses", "BioD Hits", "Misses");
		printf(" %9.9s %9.9s %9.9s %9.9s\n", "DirE Hits", "Misses", "Accs Hits", "Misses");
		printf("%9ju %9ju %9ju %9ju",
			(uintmax_t)(ext_nfsstats.biocache_readlinks -
			ext_nfsstats.readlink_bios),
			(uintmax_t)ext_nfsstats.readlink_bios,
			(uintmax_t)(ext_nfsstats.biocache_readdirs -
			ext_nfsstats.readdir_bios),
			(uintmax_t)ext_nfsstats.readdir_bios);
		printf(" %9ju %9ju %9ju %9ju\n",
			(uintmax_t)ext_nfsstats.direofcache_hits,
			(uintmax_t)ext_nfsstats.direofcache_misses,
			(uintmax_t)ext_nfsstats.accesscache_hits,
			(uintmax_t)ext_nfsstats.accesscache_misses);
	}
	if (serverOnly) {
		printf("\nServer Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Getattr", "Setattr", "Lookup", "Readlink", "Read",
			"Write", "Create", "Remove");
		printf("%9ju %9ju %9ju %9ju %9ju %9ju %9ju %9ju\n",
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_GETATTR],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_SETATTR],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_LOOKUP],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_READLINK],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_READ],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_WRITE],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_CREATE],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_REMOVE]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Rename", "Link", "Symlink", "Mkdir", "Rmdir",
			"Readdir", "RdirPlus", "Access");
		printf("%9ju %9ju %9ju %9ju %9ju %9ju %9ju %9ju\n",
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_RENAME],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_LINK],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_SYMLINK],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_MKDIR],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_RMDIR],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_READDIR],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_READDIRPLUS],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_ACCESS]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Mknod", "Fsstat", "Fsinfo", "PathConf", "Commit");
		printf("%9ju %9ju %9ju %9ju %9ju\n",
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_MKNOD],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_FSSTAT],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_FSINFO],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_PATHCONF],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_COMMIT]);
		printf("Server Ret-Failed\n");
		printf("%17ju\n", (uintmax_t)ext_nfsstats.srvrpc_errs);
		printf("Server Faults\n");
		printf("%13ju\n", (uintmax_t)ext_nfsstats.srv_errs);
		printf("Server Cache Stats:\n");
		printf("%9.9s %9.9s %9.9s %9.9s\n",
			"Inprog", "Idem", "Non-idem", "Misses");
		printf("%9ju %9ju %9ju %9ju\n",
			(uintmax_t)ext_nfsstats.srvcache_inproghits,
			(uintmax_t)ext_nfsstats.srvcache_idemdonehits,
			(uintmax_t)ext_nfsstats.srvcache_nonidemdonehits,
			(uintmax_t)ext_nfsstats.srvcache_misses);
		printf("Server Write Gathering:\n");
		printf("%9.9s %9.9s %9.9s\n",
			"WriteOps", "WriteRPC", "Opsaved");
		/*
		 * The new client doesn't do write gathering. It was
		 * only useful for NFSv2.
		 */
		printf("%9ju %9ju %9d\n",
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_WRITE],
			(uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_WRITE], 0);
	}
}

static void
printhdr(int clientOnly, int serverOnly, int newStats)
{

	if (newStats) {
		printf(" [%s Read %s]  [%s Write %s]  "
		    "%s[=========== Total ============]\n"
		    " KB/t   tps    MB/s%s  KB/t   tps    MB/s%s  "
		    "%sKB/t   tps    MB/s    ms  ql  %%b",
		    widemode ? "========" : "=====",
		    widemode ? "========" : "=====",
		    widemode ? "========" : "=====",
		    widemode ? "======="  : "====",
		    widemode ? "[Commit ]  " : "",
		    widemode ? "    ms" : "",
		    widemode ? "    ms" : "",
		    widemode ? "tps    ms  " : "");
	} else {
		printf("%s%6.6s %6.6s %6.6s %6.6s %6.6s %6.6s %6.6s %6.6s",
		    ((serverOnly && clientOnly) ? "        " : " "),
		    "GtAttr", "Lookup", "Rdlink", "Read", "Write", "Rename",
		    "Access", "Rddir");
		if (widemode && clientOnly) {
			printf(" Attr Lkup BioR BioW Accs BioD");
		}
	}
	printf("\n");
	fflush(stdout);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: nfsstat [-cdemszW] [-M core] [-N system] [-w wait]\n");
	exit(1);
}

static char SPBuf[64][8];
static int SPIndex;

static char * 
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

static char * 
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

#define DELTA_T(field)					\
	devstat_compute_etime(&cur_stats->field,	\
	(prev_stats ? &prev_stats->field : NULL))

/*
 * XXX KDM mostly copied from ctlstat.  We should commonize the code (and
 * the devstat code) somehow.
 */
static void
compute_new_stats(struct nfsstatsv1 *cur_stats,
		  struct nfsstatsv1 *prev_stats, int curop,
		  long double etime, long double *mbsec,
		  long double *kb_per_transfer,
		  long double *transfers_per_second,
		  long double *ms_per_transfer, uint64_t *queue_len,
		  long double *busy_pct)
{
	uint64_t total_bytes = 0, total_operations = 0;
	struct bintime total_time_bt;
	struct timespec total_time_ts;

	bzero(&total_time_bt, sizeof(total_time_bt));
	bzero(&total_time_ts, sizeof(total_time_ts));

	total_bytes = cur_stats->srvbytes[curop];
	total_operations = cur_stats->srvops[curop];
	if (prev_stats != NULL) {
		total_bytes -= prev_stats->srvbytes[curop];
		total_operations -= prev_stats->srvops[curop];
	}

	*mbsec = total_bytes;
	*mbsec /= 1024 * 1024;
	if (etime > 0.0) {
		*busy_pct = DELTA_T(busytime);
		if (*busy_pct < 0)
			*busy_pct = 0;
		*busy_pct /= etime;
		*busy_pct *= 100;
		if (*busy_pct < 0)
			*busy_pct = 0;
		*mbsec /= etime;
	} else {
		*busy_pct = 0;
		*mbsec = 0;
	}
	*kb_per_transfer = total_bytes;
	*kb_per_transfer /= 1024;
	if (total_operations > 0)
		*kb_per_transfer /= total_operations;
	else
		*kb_per_transfer = 0;
	if (etime > 0.0) {
		*transfers_per_second = total_operations;
		*transfers_per_second /= etime;
	} else {
		*transfers_per_second = 0.0;
	}
                        
	if (total_operations > 0) {
		*ms_per_transfer = DELTA_T(srvduration[curop]);
		*ms_per_transfer /= total_operations;
		*ms_per_transfer *= 1000;
	} else
		*ms_per_transfer = 0.0;

	*queue_len = cur_stats->srvstartcnt - cur_stats->srvdonecnt;
}

/*
 * Print a description of the nfs stats for the experimental client/server.
 */
static void
exp_intpr(int clientOnly, int serverOnly)
{
	int nfssvc_flag;

	nfssvc_flag = NFSSVC_GETSTATS | NFSSVC_NEWSTRUCT;
	if (zflag != 0) {
		if (clientOnly != 0)
			nfssvc_flag |= NFSSVC_ZEROCLTSTATS;
		if (serverOnly != 0)
			nfssvc_flag |= NFSSVC_ZEROSRVSTATS;
	}
	ext_nfsstats.vers = NFSSTATS_V1;
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
		printf("%9ju %9ju %9ju %9ju %9ju %9ju %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_GETATTR],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_SETATTR],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_LOOKUP],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_READLINK],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_READ],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_WRITE],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_CREATE],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_REMOVE]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Rename", "Link", "Symlink", "Mkdir", "Rmdir",
			    "Readdir", "RdirPlus", "Access");
		printf("%9ju %9ju %9ju %9ju %9ju %9ju %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_RENAME],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_LINK],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_SYMLINK],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_MKDIR],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_RMDIR],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_READDIR],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_READDIRPLUS],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_ACCESS]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Mknod", "Fsstat", "Fsinfo", "PathConf",
			    "Commit", "SetClId", "SetClIdCf", "Lock");
		printf("%9ju %9ju %9ju %9ju %9ju %9ju %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_MKNOD],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_FSSTAT],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_FSINFO],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_PATHCONF],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_COMMIT],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_SETCLIENTID],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_SETCLIENTIDCFRM],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_LOCK]);
		if (printtitle)
			printf("%9.9s %9.9s %9.9s %9.9s\n",
			    "LockT", "LockU", "Open", "OpenCfr");
		printf("%9ju %9ju %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_LOCKT],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_LOCKU],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_OPEN],
		    (uintmax_t)ext_nfsstats.rpccnt[NFSPROC_OPENCONFIRM]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "OpenOwner", "Opens", "LockOwner",
			    "Locks", "Delegs", "LocalOwn",
			    "LocalOpen", "LocalLOwn");
		printf("%9ju %9ju %9ju %9ju %9ju %9ju %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.clopenowners,
		    (uintmax_t)ext_nfsstats.clopens,
		    (uintmax_t)ext_nfsstats.cllockowners,
		    (uintmax_t)ext_nfsstats.cllocks,
		    (uintmax_t)ext_nfsstats.cldelegates,
		    (uintmax_t)ext_nfsstats.cllocalopenowners,
		    (uintmax_t)ext_nfsstats.cllocalopens,
		    (uintmax_t)ext_nfsstats.cllocallockowners);
		if (printtitle)
			printf("%9.9s\n", "LocalLock");
		printf("%9ju\n", (uintmax_t)ext_nfsstats.cllocallocks);
		if (printtitle) {
			printf("Rpc Info:\n");
			printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			    "TimedOut", "Invalid", "X Replies", "Retries",
			    "Requests");
		}
		printf("%9ju %9ju %9ju %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.rpctimeouts,
		    (uintmax_t)ext_nfsstats.rpcinvalid,
		    (uintmax_t)ext_nfsstats.rpcunexpected,
		    (uintmax_t)ext_nfsstats.rpcretries,
		    (uintmax_t)ext_nfsstats.rpcrequests);
		if (printtitle) {
			printf("Cache Info:\n");
			printf("%9.9s %9.9s %9.9s %9.9s",
			    "Attr Hits", "Misses", "Lkup Hits", "Misses");
			printf(" %9.9s %9.9s %9.9s %9.9s\n",
			    "BioR Hits", "Misses", "BioW Hits", "Misses");
		}
		printf("%9ju %9ju %9ju %9ju",
		    (uintmax_t)ext_nfsstats.attrcache_hits,
		    (uintmax_t)ext_nfsstats.attrcache_misses,
		    (uintmax_t)ext_nfsstats.lookupcache_hits,
		    (uintmax_t)ext_nfsstats.lookupcache_misses);
		printf(" %9ju %9ju %9ju %9ju\n",
		    (uintmax_t)(ext_nfsstats.biocache_reads -
		    ext_nfsstats.read_bios),
		    (uintmax_t)ext_nfsstats.read_bios,
		    (uintmax_t)(ext_nfsstats.biocache_writes -
		    ext_nfsstats.write_bios),
		    (uintmax_t)ext_nfsstats.write_bios);
		if (printtitle) {
			printf("%9.9s %9.9s %9.9s %9.9s",
			    "BioRLHits", "Misses", "BioD Hits", "Misses");
			printf(" %9.9s %9.9s\n", "DirE Hits", "Misses");
		}
		printf("%9ju %9ju %9ju %9ju",
		    (uintmax_t)(ext_nfsstats.biocache_readlinks -
		    ext_nfsstats.readlink_bios),
		    (uintmax_t)ext_nfsstats.readlink_bios,
		    (uintmax_t)(ext_nfsstats.biocache_readdirs -
		    ext_nfsstats.readdir_bios),
		    (uintmax_t)ext_nfsstats.readdir_bios);
		printf(" %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.direofcache_hits,
		    (uintmax_t)ext_nfsstats.direofcache_misses);
	}
	if (serverOnly != 0) {
		if (printtitle) {
			printf("\nServer Info:\n");
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Getattr", "Setattr", "Lookup", "Readlink",
			    "Read", "Write", "Create", "Remove");
		}
		printf("%9ju %9ju %9ju %9ju %9ju %9ju %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_GETATTR],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_SETATTR],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_LOOKUP],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_READLINK],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_READ],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_WRITE],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_V3CREATE],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_REMOVE]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Rename", "Link", "Symlink", "Mkdir", "Rmdir",
			    "Readdir", "RdirPlus", "Access");
		printf("%9ju %9ju %9ju %9ju %9ju %9ju %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_RENAME],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_LINK],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_SYMLINK],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_MKDIR],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_RMDIR],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_READDIR],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_READDIRPLUS],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_ACCESS]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Mknod", "Fsstat", "Fsinfo", "PathConf",
			    "Commit", "LookupP", "SetClId", "SetClIdCf");
		printf("%9ju %9ju %9ju %9ju %9ju %9ju %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_MKNOD],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_FSSTAT],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_FSINFO],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_PATHCONF],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_COMMIT],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_LOOKUPP],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_SETCLIENTID],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_SETCLIENTIDCFRM]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Open", "OpenAttr", "OpenDwnGr", "OpenCfrm",
			    "DelePurge", "DeleRet", "GetFH", "Lock");
		printf("%9ju %9ju %9ju %9ju %9ju %9ju %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_OPEN],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_OPENATTR],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_OPENDOWNGRADE],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_OPENCONFIRM],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_DELEGPURGE],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_DELEGRETURN],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_GETFH],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_LOCK]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "LockT", "LockU", "Close", "Verify", "NVerify",
			    "PutFH", "PutPubFH", "PutRootFH");
		printf("%9ju %9ju %9ju %9ju %9ju %9ju %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_LOCKT],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_LOCKU],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_CLOSE],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_VERIFY],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_NVERIFY],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_PUTFH],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_PUTPUBFH],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_PUTROOTFH]);
		if (printtitle)
			printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			    "Renew", "RestoreFH", "SaveFH", "Secinfo",
			    "RelLckOwn", "V4Create");
		printf("%9ju %9ju %9ju %9ju %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_RENEW],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_RESTOREFH],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_SAVEFH],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_SECINFO],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_RELEASELCKOWN],
		    (uintmax_t)ext_nfsstats.srvrpccnt[NFSV4OP_CREATE]);
		if (printtitle) {
			printf("Server:\n");
			printf("%9.9s %9.9s %9.9s\n",
			    "Retfailed", "Faults", "Clients");
		}
		printf("%9ju %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.srv_errs,
		    (uintmax_t)ext_nfsstats.srvrpc_errs,
		    (uintmax_t)ext_nfsstats.srvclients);
		if (printtitle)
			printf("%9.9s %9.9s %9.9s %9.9s %9.9s \n",
			    "OpenOwner", "Opens", "LockOwner",
			    "Locks", "Delegs");
		printf("%9ju %9ju %9ju %9ju %9ju \n",
		    (uintmax_t)ext_nfsstats.srvopenowners,
		    (uintmax_t)ext_nfsstats.srvopens,
		    (uintmax_t)ext_nfsstats.srvlockowners,
		    (uintmax_t)ext_nfsstats.srvlocks,
		    (uintmax_t)ext_nfsstats.srvdelegates);
		if (printtitle) {
			printf("Server Cache Stats:\n");
			printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			    "Inprog", "Idem", "Non-idem", "Misses", 
			    "CacheSize", "TCPPeak");
		}
		printf("%9ju %9ju %9ju %9ju %9ju %9ju\n",
		    (uintmax_t)ext_nfsstats.srvcache_inproghits,
		    (uintmax_t)ext_nfsstats.srvcache_idemdonehits,
		    (uintmax_t)ext_nfsstats.srvcache_nonidemdonehits,
		    (uintmax_t)ext_nfsstats.srvcache_misses,
		    (uintmax_t)ext_nfsstats.srvcache_size,
		    (uintmax_t)ext_nfsstats.srvcache_tcppeak);
	}
}

static void
compute_totals(struct nfsstatsv1 *total_stats, struct nfsstatsv1 *cur_stats)
{
	int i;

	bzero(total_stats, sizeof(*total_stats));
	for (i = 0; i < (NFSV42_NOPS + NFSV4OP_FAKENOPS); i++) {
		total_stats->srvbytes[0] += cur_stats->srvbytes[i];
		total_stats->srvops[0] += cur_stats->srvops[i];
		bintime_add(&total_stats->srvduration[0],
			    &cur_stats->srvduration[i]);
		total_stats->srvrpccnt[i] = cur_stats->srvrpccnt[i];
	}
	total_stats->srvstartcnt = cur_stats->srvstartcnt;
	total_stats->srvdonecnt = cur_stats->srvdonecnt;
	total_stats->busytime = cur_stats->busytime;

}

/*
 * Print a running summary of nfs statistics for the experimental client and/or
 * server.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
static void
exp_sidewaysintpr(u_int interval, int clientOnly, int serverOnly,
    int newStats)
{
	struct nfsstatsv1 nfsstats, lastst, *ext_nfsstatsp;
	struct nfsstatsv1 curtotal, lasttotal;
	struct timespec ts, lastts;
	int hdrcnt = 1;

	ext_nfsstatsp = &lastst;
	ext_nfsstatsp->vers = NFSSTATS_V1;
	if (nfssvc(NFSSVC_GETSTATS | NFSSVC_NEWSTRUCT, ext_nfsstatsp) < 0)
		err(1, "Can't get stats");
	clock_gettime(CLOCK_MONOTONIC, &lastts);
	compute_totals(&lasttotal, ext_nfsstatsp);
	sleep(interval);

	for (;;) {
		ext_nfsstatsp = &nfsstats;
		ext_nfsstatsp->vers = NFSSTATS_V1;
		if (nfssvc(NFSSVC_GETSTATS | NFSSVC_NEWSTRUCT, ext_nfsstatsp)
		    < 0)
			err(1, "Can't get stats");
		clock_gettime(CLOCK_MONOTONIC, &ts);

		if (--hdrcnt == 0) {
			printhdr(clientOnly, serverOnly, newStats);
			if (newStats)
				hdrcnt = 20;
			else if (clientOnly && serverOnly)
				hdrcnt = 10;
			else
				hdrcnt = 20;
		}
		if (clientOnly && newStats == 0) {
		    printf("%s %6ju %6ju %6ju %6ju %6ju %6ju %6ju %6ju",
			((clientOnly && serverOnly) ? "Client:" : ""),
			(uintmax_t)DELTA(rpccnt[NFSPROC_GETATTR]),
			(uintmax_t)DELTA(rpccnt[NFSPROC_LOOKUP]),
			(uintmax_t)DELTA(rpccnt[NFSPROC_READLINK]),
			(uintmax_t)DELTA(rpccnt[NFSPROC_READ]),
			(uintmax_t)DELTA(rpccnt[NFSPROC_WRITE]),
			(uintmax_t)DELTA(rpccnt[NFSPROC_RENAME]),
			(uintmax_t)DELTA(rpccnt[NFSPROC_ACCESS]),
			(uintmax_t)(DELTA(rpccnt[NFSPROC_READDIR]) +
			DELTA(rpccnt[NFSPROC_READDIRPLUS]))
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

		if (serverOnly && newStats) {
			long double cur_secs, last_secs, etime;
			long double mbsec;
			long double kb_per_transfer;
			long double transfers_per_second;
			long double ms_per_transfer;
			uint64_t queue_len;
			long double busy_pct;
			int i;

			cur_secs = ts.tv_sec +
			    ((long double)ts.tv_nsec / 1000000000);
			last_secs = lastts.tv_sec +
			    ((long double)lastts.tv_nsec / 1000000000);
			etime = cur_secs - last_secs;

			compute_totals(&curtotal, &nfsstats);

			for (i = 0; i < NUM_STAT_TYPES; i++) {
				compute_new_stats(&nfsstats, &lastst,
				    STAT_TYPE_TO_NFS(i), etime, &mbsec,
				    &kb_per_transfer,
				    &transfers_per_second,
				    &ms_per_transfer, &queue_len,
				    &busy_pct);

				if (i == STAT_TYPE_COMMIT) {
					if (widemode == 0)
						continue;

					printf("%2.0Lf %7.2Lf ",
					    transfers_per_second,
					    ms_per_transfer);
				} else {
					printf("%5.2Lf %5.0Lf %7.2Lf ",
					    kb_per_transfer,
					    transfers_per_second, mbsec);
					if (widemode)
						printf("%5.2Lf ",
						    ms_per_transfer);
				}
			}

			compute_new_stats(&curtotal, &lasttotal, 0, etime,
			    &mbsec, &kb_per_transfer, &transfers_per_second,
			    &ms_per_transfer, &queue_len, &busy_pct);

			printf("%5.2Lf %5.0Lf %7.2Lf %5.2Lf %3ju %3.0Lf\n",
			    kb_per_transfer, transfers_per_second, mbsec,
			    ms_per_transfer, queue_len, busy_pct);
		} else if (serverOnly) {
		    printf("%s %6ju %6ju %6ju %6ju %6ju %6ju %6ju %6ju",
			((clientOnly && serverOnly) ? "Server:" : ""),
			(uintmax_t)DELTA(srvrpccnt[NFSV4OP_GETATTR]),
			(uintmax_t)DELTA(srvrpccnt[NFSV4OP_LOOKUP]),
			(uintmax_t)DELTA(srvrpccnt[NFSV4OP_READLINK]),
			(uintmax_t)DELTA(srvrpccnt[NFSV4OP_READ]),
			(uintmax_t)DELTA(srvrpccnt[NFSV4OP_WRITE]),
			(uintmax_t)DELTA(srvrpccnt[NFSV4OP_RENAME]),
			(uintmax_t)DELTA(srvrpccnt[NFSV4OP_ACCESS]),
			(uintmax_t)(DELTA(srvrpccnt[NFSV4OP_READDIR]) +
			DELTA(srvrpccnt[NFSV4OP_READDIRPLUS])));
		    printf("\n");
		}
		bcopy(&nfsstats, &lastst, sizeof(lastst));
		bcopy(&curtotal, &lasttotal, sizeof(lasttotal));
		lastts = ts;
		fflush(stdout);
		sleep(interval);
	}
	/*NOTREACHED*/
}
