/* $FreeBSD: src/contrib/bind/bin/named-xfer/named-xfer.c,v 1.3.2.1 2000/07/03 22:47:04 ps Exp $ */
/*
 * The original version of named-xfer by Kevin Dunlap.
 * Completed and integrated with named by David Waitzman
 *	(dwaitzman@bbn.com) 3/14/88.
 * Modified by M. Karels and O. Kure 10-88.
 * Modified extensively since then by just about everybody.
 */

/*
 * Copyright (c) 1988, 1990
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
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

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1995 by International Business Machines, Inc.
 *
 * International Business Machines, Inc. (hereinafter called IBM) grants
 * permission under its copyrights to use, copy, modify, and distribute this
 * Software with or without fee, provided that the above copyright notice and
 * all paragraphs of this notice appear in all copies, and that the name of IBM
 * not be used in connection with the marketing of any product incorporating
 * the Software or modifications thereof, without specific, written prior
 * permission.
 *
 * To the extent it has a right to do so, IBM grants an immunity from suit
 * under its patents, if any, for the use, sale or manufacture of products to
 * the extent that such products are used for performing Domain Name System
 * dynamic updates in TCP/IP networks by means of the Software.  No immunity is
 * granted for any product per se or for any other function of any product.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", AND IBM DISCLAIMS ALL WARRANTIES,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL IBM BE LIABLE FOR ANY SPECIAL,
 * DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE, EVEN
 * IF IBM IS APPRISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

/*
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1998 by MetaInfo, Incorporated.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of MetaInfo Incorporated not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND METAINFO INCORPORATED DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL METAINFO INCORPRATED
 * BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR 
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT 
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#if !defined(lint) && !defined(SABER)
char copyright[] =
"@(#) Copyright (c) 1988, 1990 The Regents of the University of California.\n\
 portions Copyright (c) 1993 Digital Equipment Corporation\n\
 portions Copyright (c) 1998 MetaInfo, Inc.\n\
 portions Copyright (c) 1995, 1996 Internet Software Consorium\n\
 All rights reserved.\n";
#endif /* not lint */

#if !defined(lint) && !defined(SABER)
static const char sccsid[] = "@(#)named-xfer.c	4.18 (Berkeley) 3/7/91";
static const char rcsid[] = "$Id: named-xfer.c,v 8.93 2000/04/20 07:33:47 vixie Exp $";
#endif /* not lint */

#include "port_before.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <stdarg.h>

#include <isc/eventlib.h>
#include <isc/list.h>
#include <isc/logging.h>
/* This still uses malloc/free, but the tsig routines allocate memory with
 * memget, and we free it with memput.
 */
#include <isc/memcluster.h>

#include <isc/dst.h>

#include "port_after.h"


#define MAIN_PROGRAM
#include "../named/named.h"
#undef MAIN_PROGRAM

#define MAX_XFER_RESTARTS 2

#define ENABLE_IXFR 1

# ifdef SHORT_FNAMES
extern long pathconf __P((const char *path, int name));	/* XXX */
# endif


static	struct zoneinfo	zone;		/* zone information */

static	char		*ddtfilename = NULL,
			*ddtfile = NULL;

static char   *tmpname = NULL, 
			*tmpiname = NULL, 	  /* temporary file name for ixfr transaction file */
			*domain;	/* domain being xfered */

static	int		quiet = 0,
			read_interrupted = 0,
			curclass,
			domain_len;		/* strlen(domain) */

static	FILE		*fp = NULL,
			*dbfp = NULL,
			*ixfp = NULL;

static	char		*ProgName;

static	void		usage(const char *),
			tsig_init(const char *);
static	int		getzone(struct zoneinfo *, u_int32_t, int),
			print_output(struct zoneinfo *, u_int32_t, 
				     u_char *, int, u_char *, int),
			netread(int, char *, int, int),
			writemsg(int, const u_char *, int);
static int		ixfr_log(const u_char *msg, int len, int *delete,
				 FILE *file, struct sockaddr_in *sin,
				 char *domain, u_int32_t *serial_no, int *);
static	SIG_FN		read_alarm(void);
static	SIG_FN		term_handler(void);
static	const char	*soa_zinfo(struct zoneinfo *, u_char *, u_char*),
			*tsig_rcode(int);

struct zoneinfo		zp_start, zp_finish;

static int		restarts = 0;
static int		check_serial = 0;
static int		xfr_qtype = T_AXFR;

FILE			*ddt = NULL;
int 			servermethode[NSMAX];
char			*soa_buf;

typedef struct _tsig_node {
	struct in_addr addr;
	DST_KEY *dst_key;
	LINK(struct _tsig_node) link;
} tsig_node;

LIST(tsig_node)		tsig_list;

/*
 * Debugging printf.
 */
void
dprintf(int level, const char *format, ...) {
	va_list ap;

	va_start(ap, format);
	if (ddt != NULL && debug >= level)
		(void) vfprintf(ddt, format, ap);
	va_end(ap);
}

static int
init_xfer_logging() {
	log_channel chan;

	if (log_new_context(ns_log_max_category, NULL, &log_ctx) < 0) {
		perror("log_new_context");
		return (0);
	}
	log_option(log_ctx, LOG_OPTION_DEBUG, debug);
	log_option(log_ctx, LOG_OPTION_LEVEL, debug);

	log_ctx_valid = 1;

	chan = log_new_syslog_channel(0, 0, LOG_DAEMON);
	if (chan == NULL)
		return (0);
	if (log_add_channel(log_ctx, ns_log_default, chan) < 0) {
		perror("log_add_channel syslog");
		return (0);
	}

	if (debug) {
		unsigned int flags = LOG_USE_CONTEXT_LEVEL|LOG_REQUIRE_DEBUG;

		chan = log_new_file_channel(flags, 0, NULL, ddt, 0, ULONG_MAX);
		if (chan == NULL)
			return (0);
		if (log_add_channel(log_ctx, ns_log_default, chan) < 0) {
			perror("log_add_channel debug");
			return (0);
		}
	}

	return (1);
}

void cleanup_for_exit(void) {
#ifdef DEBUG
	if (!debug)
#endif
	{
		(void) unlink(tmpname);
		if (tmpiname != NULL)
			(void) unlink(tmpiname);
	}
	if(tmpiname)
		free(tmpiname);
	tmpiname = NULL;
	if (ddtfilename != NULL) {
		free(ddtfilename);
		if (ddtfilename == ddtfile)
			ddtfile = NULL;
		ddtfilename = NULL;
	}
	if(tmpname)
		free(tmpname);
	tmpname = NULL;
	if(ddtfile)
		free(ddtfile);
	ddtfile = NULL;
}

	
int
main(int argc, char *argv[]) {
	struct zoneinfo *zp;
	struct hostent *hp;
	struct in_addr axfr_src;
 	char *dbfile = NULL, *tracefile = NULL, *tm = NULL, *tsigfile = NULL;
 	char *ixfrfile = NULL;
	u_int32_t new_serial_no = 0;
	int dbfd, ddtd, result, c, fd, ixfd;
	u_int32_t serial_no = 0;
	u_int port = htons(NAMESERVER_PORT);
	struct stat statbuf;
	int stub_only = 0;
	int class = C_IN;
	int n;
	long num_files;

#ifdef _AUX_SOURCE
	set42sig();
#endif
	memset(&axfr_src, 0, sizeof axfr_src);
	ProgName = strrchr(argv[0], '/');
	if (ProgName != NULL)
		ProgName++;
	else
		ProgName = argv[0];

	(void) umask(022);

	ddtfilename = (char *)malloc(strlen(_PATH_TMPXFER) + 1);
	strcpy(ddtfilename, _PATH_TMPXFER);
	ddtfile = ddtfilename;

#ifdef RENICE
	nice(-40);	/* this is the recommended procedure to        */
	nice(20);	/*   reset the priority of the current process */
	nice(0);	/*   to "normal" (== 0) - see nice(3)          */
#endif

	n = LOG_PID;
#ifdef LOG_PERROR
	n |= LOG_PERROR;
#endif
#if defined(LOG_CONS) && defined(USE_LOG_CONS)
	n |= LOG_CONS;
#endif
#ifdef SYSLOG_42BSD
	openlog(ProgName, n);
#else
	openlog(ProgName, n, LOG_DAEMON);
#endif
	while ((c = getopt(argc, argv, "C:d:l:s:t:z:f:i:p:P:qx:ST:Z")) != -1)
		switch (c) {
		case 'C':
			class = get_class(optarg);
			break;
		case 'd':
#ifdef DEBUG
			debug = atoi(optarg);
#endif
			break;
		case 'l':
			ddtfile = (char *)malloc(strlen(optarg) +
						 sizeof(".XXXXXX") + 1);
			if (!ddtfile)
				panic("malloc(ddtfile)", NULL);
#ifdef SHORT_FNAMES
			filenamecpy(ddtfile, optarg);
#else
			(void) strcpy(ddtfile, optarg);
#endif /* SHORT_FNAMES */
			(void) strcat(ddtfile, ".XXXXXX");
			break;
		case 's':
			serial_no = strtoul(optarg, (char **)NULL, 10);
			check_serial++;
			break;
		case 't':
			tracefile = optarg;
			break;
		case 'z':		/* zone == domain */
			domain = optarg;
			domain_len = strlen(domain);
			while ((domain_len > 0) && 
					(domain[domain_len-1] == '.'))
				domain[--domain_len] = '\0';
			break;
		case 'f':
			dbfile = optarg;
			tmpname = (char *)malloc((unsigned)strlen(optarg) +
						 sizeof(".XXXXXX") + 1);
			if (!tmpname)
				panic("malloc(tmpname)", NULL);
#ifdef SHORT_FNAMES
			filenamecpy(tmpname, optarg);
#else
			(void) strcpy(tmpname, optarg);
#endif /* SHORT_FNAMES */
			break;
		case 'i':
#if ENABLE_IXFR
			ixfrfile = optarg;
			tmpiname = (char *) malloc(strlen(optarg) +
						  sizeof(".XXXXXX") + 1);
			if (!tmpiname)
				panic("malloc(tmpiname)", NULL);
#ifdef SHORT_FNAMES
			filenamecpy(tmpiname, optarg);
#else
			(void) strcpy(tmpiname, optarg);
#endif /* SHORT_FNAMES */
#endif /* ENABLE_IXFR */
			break;
		case 'p':
			port = htons((u_int16_t)atoi(optarg));
			break;
		case 'P':
			port = (u_int16_t)atoi(optarg);
			break;
		case 'S':
			stub_only = 1;
			break;
		case 'q':
			quiet++;
			break;
		case 'x':
			if (!inet_aton(optarg, &axfr_src))
				panic("bad -x addr: %s", optarg);
			break;
		case 'T':
			tsigfile = optarg;
			break;
		case 'Z':
			xfr_qtype = ns_t_zxfr;
			break;
		case '?':
		default:
			usage("unrecognized argument");
			/* NOTREACHED */
		}
	if (!domain || ((!dbfile) && (!ixfrfile)) || optind >= argc) {
		if (!domain)
			usage("no domain");
		if (!dbfile)
			usage("no dbfile");
		if (optind >= argc)
			usage("not enough arguments");
		/* NOTREACHED */
	}
	if (stat(dbfile, &statbuf) != -1 &&
	    !S_ISREG(statbuf.st_mode) &&
	    !S_ISFIFO(statbuf.st_mode))
		usage("dbfile must be a regular file or FIFO");
	if (ixfrfile && (stat(ixfrfile, &statbuf) != -1 &&
			!S_ISREG(statbuf.st_mode) &&
			!S_ISFIFO(statbuf.st_mode)))
		usage("ixfrfile must be a regular file or FIFO");
	if (tsigfile && stat(tsigfile, &statbuf) != -1 &&
	    !S_ISREG(statbuf.st_mode) &&
	    !S_ISFIFO(statbuf.st_mode))
		usage("tsigfile must be a regular file or FIFO");
	if (tracefile && (fp = fopen(tracefile, "w")) == NULL)
		perror(tracefile);
	(void) strcat(tmpname, ".XXXXXX");
	/* tmpname is now something like "/etc/named/named.bu.db.XXXXXX" */
	if ((dbfd = mkstemp(tmpname)) == -1) {
		perror(tmpname);
		if (!quiet)
			syslog(LOG_ERR, "can't make tmpfile (%s): %s\n",
			       tmpname, strerror(errno));
		exit(XFER_FAIL);
	}
#ifdef HAVE_FCHMOD	/* XXX */
	if (fchmod(dbfd, 0644) == -1)
#else
	if (chmod(tmpname, 0644) == -1)
#endif
	{
		perror(tmpname);
		if (!quiet)
			syslog(LOG_ERR, "can't [f]chmod tmpfile (%s): %s\n",
			       tmpname, strerror(errno));
		exit(XFER_FAIL);
	}
	if ((dbfp = fdopen(dbfd, "r+")) == NULL) {
		perror(tmpname);
		if (!quiet)
			syslog(LOG_ERR, "can't fdopen tmpfile (%s)", tmpname);
		exit(XFER_FAIL);
	}
	if (ixfrfile) {
		(void) strcat(tmpiname, ".XXXXXX");
		if ((ixfd = mkstemp(tmpiname)) == -1) {
			perror(tmpiname);
			if (!quiet)
				syslog(LOG_ERR,
				       "can't make tmpifile (%s): %s\n",
				       tmpiname, strerror(errno));
			(void) fclose(dbfp);
			(void) close(dbfd);
			exit(XFER_FAIL);
		}
#ifdef HAVE_FCHMOD    /* XXX */
		if (fchmod(ixfd, 0644) == -1)
#else
		if (chmod(tmpiname, 0644) == -1)
#endif
		{
			perror(tmpiname);
			if (!quiet)
				syslog(LOG_ERR,
				       "can't [f]chmod tmpifile (%s): %s\n",
				       tmpiname, strerror(errno));
			(void) fclose(dbfp);
			(void) close(dbfd);
			(void) close(ixfd);
			exit(XFER_FAIL);
		}
		close(ixfd);
	}
#ifdef DEBUG
	if (debug) {
		/* ddtfile is now something like "/usr/tmp/xfer.ddt.XXXXXX" */
		if ((ddtd = mkstemp(ddtfile)) == -1) {
			perror(ddtfile);
			debug = 0;
		}
#ifdef HAVE_FCHMOD
		else if (fchmod(ddtd, 0644) == -1)
#else
		else if (chmod(ddtfile, 0644) == -1)
#endif
		{
			perror(ddtfile);
			debug = 0;
		} else if ((ddt = fdopen(ddtd, "w")) == NULL) {
			perror(ddtfile);
			debug = 0;
		} else
			setvbuf(ddt, NULL, _IOLBF, 0);
	}
#endif
	if (!init_xfer_logging()) {
		cleanup_for_exit();
		perror("init_xfer_logging");
	}

	/*
	 * Ignore many types of signals that named (assumed to be our parent)
	 * considers important- if not, the user controlling named with
	 * signals usually kills us.
	 */
	(void) signal(SIGHUP, SIG_IGN);
#ifdef SIGSYS
	(void) signal(SIGSYS, SIG_IGN);
#endif
#ifdef DEBUG
	if (debug == 0)
#endif
	{
		(void) signal(SIGINT, SIG_IGN);
		(void) signal(SIGQUIT, SIG_IGN);
	}
	(void) signal(SIGILL, SIG_IGN);

#if defined(SIGUSR1) && defined(SIGUSR2)
	(void) signal(SIGUSR1, SIG_IGN);
	(void) signal(SIGUSR2, SIG_IGN);
#else	/* SIGUSR1&&SIGUSR2 */
	(void) signal(SIGEMT, SIG_IGN);
	(void) signal(SIGFPE, SIG_IGN);
#endif /* SIGUSR1&&SIGUSR2 */

	if (dbfile)
		dprintf(1, "domain `%s'; file `%s'; serial %u\n",
			domain, dbfile, serial_no);

	if (ixfrfile) 
		dprintf(1, "domain `%s'; ixfrfile `%s'; serial %u\n",
			domain, ixfrfile, serial_no);

	buildservicelist();
	buildprotolist();

	tsig_init(tsigfile);

	/* init zone data */
 
	zp = &zone;
	if (stub_only)
		zp->z_type = Z_STUB;
	else
		zp->z_type = Z_SECONDARY;
	zp->z_class = class;
	zp->z_origin = domain;
	zp->z_source = dbfile;
	zp->z_axfr_src = axfr_src;
	zp->z_addrcnt = 0;
	dprintf(1, "zone found (%d): \"%s\", source = %s\n",
		zp->z_type,
		(zp->z_origin[0] == '\0') ? "." : zp->z_origin,
		zp->z_source);

	for (; optind != argc; optind++) {
		int tmpsupportixfr;

		tm = argv[optind];
		tmpsupportixfr = ISNOTIXFR;
		if ((optind+1) != argc) {
			if (strcasecmp("ixfr", argv[optind+1]) == 0) {
#if ENABLE_IXFR
				tmpsupportixfr = ISIXFR;	
				servermethode[zp->z_addrcnt] = tmpsupportixfr;
#endif
				optind++;
			} else if (strcasecmp("axfr", argv[optind+1]) == 0) {
				tmpsupportixfr = ISNOTIXFR;
				optind++;
			}
		}
		if (!inet_aton(tm, &zp->z_addr[zp->z_addrcnt])) {
			if (strcmp("-ixfr",tm)==0) {
#if ENABLE_IXFR
				tmpsupportixfr = ISIXFR;	
				servermethode[zp->z_addrcnt-1] = tmpsupportixfr;
#endif
				continue;
			} else
				if (strcmp("-axfr",tm)==0) {
					tmpsupportixfr = ISNOTIXFR;
					continue;
				}
			hp = gethostbyname(tm);
			if (hp == NULL) {
				syslog(LOG_NOTICE,
				       "uninterpretable server (%s) for %s\n",
				       tm, zp->z_origin);
				continue;
			}
			memcpy(&zp->z_addr[zp->z_addrcnt],
			       hp->h_addr,
			       INADDRSZ);
			dprintf(1, "Arg: \"%s\" %s\n", tm,((tmpsupportixfr) ? "IXFR":"AXFR"));
		}
		if (++zp->z_addrcnt >= NSMAX) {
			zp->z_addrcnt = NSMAX;
			dprintf(1, "NSMAX reached\n");
			break;
		}
	}
	dprintf(1, "addrcnt = %d\n", zp->z_addrcnt);

	res_ninit(&res);
	res.options &= ~(RES_DEFNAMES | RES_DNSRCH | RES_RECURSE);
	result = getzone(zp, serial_no, port);
	(void) fclose(dbfp);
	(void) close(dbfd);
	
	if (ixfp)
		(void) my_fclose(ixfp);
	else
		close(ixfd);

	switch (result) {

	case XFER_SUCCESSAXFR:			/* ok exit */
		if (tmpiname != NULL)
			unlink(tmpiname);
		if (ixfrfile) {
			/*
			 * An IXFR was requested but we performed an
			 * AXFR.  Rename the temporary file to the IXFR
			 * name, named will rename it again to the dbname.
			 */
			if (movefile(tmpname, ixfrfile) == -1) {
				perror("movefile");
#ifdef DEBUG
				if (debug)
					(void) unlink(ddtfile);
#endif
				if (!quiet)
					syslog(LOG_ERR,
					       "rename %s to %s: %s",
					       tmpname, ixfrfile, strerror(errno));
				cleanup_for_exit();
				exit(XFER_FAIL);
			};
			exit(XFER_SUCCESSAXFRIXFRFILE);	
		}
		if (movefile(tmpname, dbfile) == -1) {
			perror("movefile");
			if (!quiet)
			    syslog(LOG_ERR, "movefile %s to %s: %m",
				   tmpname, dbfile);
			cleanup_for_exit();
			exit(XFER_FAIL);
		}
		exit(XFER_SUCCESSAXFR);

	case XFER_SUCCESSIXFR:
		unlink(tmpname);
		if (movefile(tmpiname, ixfrfile) == -1) {
			perror("movefile");
			if (!quiet)
				syslog(LOG_ERR, "movefile %s to %s: %m",
				       tmpiname, ixfrfile);
			cleanup_for_exit();
			exit(XFER_FAIL);
		}
		cleanup_for_exit();
		exit(XFER_SUCCESSIXFR);

	case XFER_UPTODATE:		/* the zone was already uptodate */
		(void) unlink(tmpname);
		if (tmpiname != NULL)
			(void) unlink(tmpiname);
		cleanup_for_exit();
		exit(XFER_UPTODATE);

	default:
		result = XFER_FAIL;
		/* fall through */
	case XFER_TIMEOUT:
	case XFER_FAIL:
		(void) unlink(tmpname);
		cleanup_for_exit();
		exit(result);	/* error or timeout */
	}
	/*NOTREACHED*/
	return (0);		/* Make gcc happy. */
}
 
static char *UsageText[] = {
	"\t-z zone_to_transfer\n",
	"\t-f db_file\n",
	"\t[-i ixfr_file]\n",
	"\t[-s serial_no]\n",
	"\t[-d debug_level]\n",
	"\t[-l debug_log_file]\n",
	"\t[-t trace_file]\n",
	"\t[-p port]\n",
	"\t[-S] [-Z]\n",
	"\t[-C class]\n",
	"\t[-x axfr-src]\n",
	"\t[-T tsig_info_file]\n",
	"\tservers [-ixfr|-axfr]...\n",
	NULL
};

static void
usage(const char *msg) {
	char * const *line;

	fprintf(stderr, "Usage error: %s\n", msg);
	fprintf(stderr, "Usage: %s\n", ProgName);
	for (line = UsageText;  *line;  line++)
		fputs(*line, stderr);
	exit(XFER_FAIL);
}

static void
tsig_init(const char *file) {
	char buf[1024];
	int n;
	FILE *fp;
	char *s;

	if (file == NULL)
		return;
	fp = fopen(file, "r");
	if (fp == NULL)
		return;
	dst_init();
	INIT_LIST(tsig_list);
	while (1) {
		tsig_node *n = malloc(sizeof(tsig_node));
		int alg, secret_len;
		char *address, *name;
		char *cp;
		u_char secret[128];

		s = fgets(buf, sizeof(buf), fp);
		if (s == NULL)
			break;
		buf[strlen(buf)-1] = 0;
		inet_aton(buf, &n->addr);

		fgets(buf, sizeof(buf), fp);
		buf[strlen(buf)-1] = 0;
		name = strdup(buf);

		fscanf(fp, "%d", &alg);
		fgets(buf, sizeof(buf), fp);

		fgets(buf, sizeof(buf), fp);
		buf[strlen(buf)-1] = 0;
		cp = buf;
		while (isspace(*cp))
			cp++;

		secret_len = b64_pton(cp, secret, sizeof(secret));
		n->dst_key = dst_buffer_to_key(name, alg, 0, 0,
					       secret, secret_len);

		free(name);
		APPEND(tsig_list, n, link);
	}
	fclose(fp);
	unlink(file);
}

#define DEF_DNAME	'\001'		/* '\0' means the root domain */
/* XXX: The following variables should probably all be "static" */
u_int32_t	minimum_ttl = 0;
int	soa_cnt = 0, scdsoa = 0, methode = ISNOTIXFR;
int     delete_soa = 1;
u_int32_t final_serial = 0;
int	ixfr_soa = 0;
int	ns_cnt = 0;
int	query_type = 0;
int	prev_comment = 0;	/* was previous record a comment? */
char	zone_top[MAXDNAME];	/* the top of the zone */
char	prev_origin[MAXDNAME];	/* from most recent $ORIGIN line */
char	prev_dname[MAXDNAME] = { DEF_DNAME }; /* from previous record */
char	prev_ns_dname[MAXDNAME] = { DEF_DNAME }; /* from most recent NS record */

static int
getzone(struct zoneinfo *zp, u_int32_t serial_no, int port) {
	HEADER *hp;
	u_int len;
	u_int32_t serial;
	int s, n, l, error = 0;
	int was_ixfr = 0;
	u_int cnt;
 	u_char *cp, *nmp, *eom, *tmp ;
	u_char *buf = NULL, *cpp = NULL;
	u_char *bp;
	u_int bufsize = 0;
	char name[MAXDNAME], name2[MAXDNAME];
	struct sockaddr_in sin;
	struct sockaddr_in local;
	int locallen;
#ifdef POSIX_SIGNALS
	struct sigaction sv, osv;
#else
	struct sigvec sv, osv;
#endif
	int qdcount, ancount, aucount, arcount, class, type;
	int first_serial;
	const char *badsoa_msg = "Nil";
	struct sockaddr_in my_addr;
	char my_addr_text[30];
	int alen, ret, tsig_req;
	DST_KEY *tsig_key;
	ns_tcp_tsig_state tsig_state;
	int tsig_signed = 0;
	u_char sig[64];
	int siglen;
	int ixfr_first = 1;
	int loop_cnt = 0;
	time_t timesigned;
	u_int32_t query_serial = serial_no;

#ifdef DEBUG
	if (debug) {
		(void)fprintf(ddt,"getzone() %s ", zp->z_origin);
		switch (zp->z_type) {
		case Z_STUB:
			fprintf(ddt,"stub\n");
			break;
		case Z_SECONDARY:
			fprintf(ddt,"secondary\n");
			break;
		default:
			fprintf(ddt,"unknown type\n");
		}
	}
#endif
#ifdef POSIX_SIGNALS
	memset(&sv, 0, sizeof sv);
	sv.sa_handler = (SIG_FN (*)()) read_alarm;
	/* SA_ONSTACK isn't recommended for strict POSIX code */
	/* is it absolutely necessary? */
	/* sv.sa_flags = SA_ONSTACK; */
	sigfillset(&sv.sa_mask);
	(void) sigaction(SIGALRM, &sv, &osv);
	memset(&sv, 0, sizeof sv);
	sv.sa_handler = (SIG_FN (*)()) term_handler;
	sigfillset(&sv.sa_mask);
	(void) sigaction(SIGTERM, &sv, &osv);
#else
	memset(&sv, 0, sizeof sv);
	sv.sv_handler = read_alarm;
	sv.sv_mask = ~0;
	(void) sigvec(SIGALRM, &sv, &osv);
	memset(&sv, 0, sizeof sv);
	sv.sv_handler = term_handler;
	sv.sv_mask = ~0;
	(void) sigvec(SIGTERM, &sv, &osv);
#endif

	strcpy(zone_top, zp->z_origin);
	if ((l = strlen(zone_top)) != 0 && zone_top[l - 1] == '.')
		zone_top[l - 1] = '\0';
	strcpy(prev_origin, zone_top);
	for (cnt = 0; cnt < zp->z_addrcnt; cnt++) {
		methode = servermethode[cnt];
		sin.sin_addr = zp->z_addr[cnt];
		dprintf(3, "address [%s] %s\n",
			inet_ntoa(sin.sin_addr),
			(methode == ISIXFR) ? "IXFR":"AXFR");

	}
	for (cnt = 0; cnt < zp->z_addrcnt; cnt++) {
		methode = ISNOTIXFR;
		curclass = zp->z_class;
		/*
		 * If we have been given a serial number and a ixfr log
		 * file name then set methode.
		 */
		if (check_serial && tmpiname != NULL)
			methode = servermethode[cnt];
		error = 0;
		if (buf == NULL) {
			if ((buf = (u_char *)malloc(2 * PACKETSZ)) == NULL) {
				syslog(LOG_INFO, "malloc(%u) failed",
				       2 * PACKETSZ);
				error++;
				break;
			}
			bufsize = 2 * PACKETSZ;
		}
 try_again:
		if ((s = socket(AF_INET, SOCK_STREAM, PF_UNSPEC)) < 0) {
			syslog(LOG_INFO, "socket: %m");
			error++;
			break;
		}	
		if (zp->z_axfr_src.s_addr != 0) {
			memset(&sin, 0, sizeof sin);
			sin.sin_family = AF_INET;
			sin.sin_port = 0;		/* "ANY" */
			sin.sin_addr = zp->z_axfr_src;
			dprintf(2, "binding to address [%s]\n",
				inet_ntoa(sin.sin_addr));
			if (bind(s, (struct sockaddr *)&sin, sizeof sin) < 0)
				syslog(LOG_INFO, "warning: bind(%s) failed",
				       inet_ntoa(zp->z_axfr_src));
		}
		memset(&sin, 0, sizeof sin);
		sin.sin_family = AF_INET;
		sin.sin_port = port;
		sin.sin_addr = zp->z_addr[cnt];
		dprintf(2, "connecting to server #%d [%s].%d\n",
			cnt+1, inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
		if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
			if (zp->z_axfr_src.s_addr != 0) {
				dprintf(2, "connect failed, trying w/o -x");
				zp->z_axfr_src.s_addr = 0;
				(void) my_close(s);
				goto try_again;
			}
			if (!quiet)
				syslog(LOG_INFO,
				       "connect(%s) for zone %s failed: %s",
				       inet_ntoa(sin.sin_addr), zp->z_origin, strerror(errno));
			error++;
			(void) my_close(s);
			continue;
		}	
		if (methode == ISIXFR && was_ixfr == 0) {
			hp = (HEADER *) buf;
			cpp = buf;
			n = res_nmkquery(&res, QUERY, zp->z_origin, curclass,
				 T_IXFR, NULL, 0, NULL, buf, bufsize);
			dprintf(1, "len = %d\n", n);
			if (n < 0) {
				if (!quiet)
					syslog(LOG_INFO,
					    "zone %s: dn_comp for ixfr failed",
						zp->z_origin);
					(void) my_close(s);
#ifdef POSIX_SIGNALS
					sigaction(SIGALRM,
						  &osv,
						 (struct sigaction*)0);
#else
					sigvec(SIGALRM,
						       &osv,
						   (struct sigvec *)0);
#endif
					return (XFER_FAIL);
			}
			hp->nscount = htons(1+ntohs(hp->nscount));
			cpp += n;
			n = dn_comp(zp->z_origin, cpp, bufsize-(cpp-buf),
				    NULL, NULL);
			if (n > 0)
				cpp += n;
			PUTSHORT(T_SOA, cpp); /* type */
			PUTSHORT(C_IN, cpp);  /* class */
			PUTLONG(0, cpp);      /* ttl */
			PUTSHORT(22, cpp);    /* dlen */
			*cpp++ = 0;           /* mname */
			*cpp++ = 0;           /* rname */
			PUTLONG(serial_no, cpp);
			PUTLONG(0xDEAD, cpp); /* Refresh */
			PUTLONG(0xBEEF, cpp); /* Retry */
			PUTLONG(0xABCD, cpp); /* Expire */
			PUTLONG(0x1776, cpp); /* Min TTL */
			n = cpp-buf;
			dprintf(1, "len = %d\n", cpp-buf);
			if (debug)
				res_pquery(&res, buf, n, ddt);
		}
		else {
		n = res_nmkquery(&res, QUERY, zp->z_origin, curclass,
				 T_SOA, NULL, 0, NULL, buf, bufsize);
		if (n < 0) {
			if (!quiet)
				syslog(LOG_INFO,
				       "zone %s: res_nmkquery T_SOA failed",
				       zp->z_origin);
			(void) my_close(s);
#ifdef POSIX_SIGNALS
			(void) sigaction(SIGALRM, &osv, (struct sigaction *)0);
#else
			(void) sigvec(SIGALRM, &osv, (struct sigvec *)0);
#endif
			return (XFER_FAIL);
		}
		}
		/*
		 * Append TSIG to SOA query if desired
		 */
		tsig_key = tsig_key_from_addr(sin.sin_addr);
		if (tsig_key != NULL) {
			siglen = sizeof(sig);
			ret = ns_sign(buf, &n, bufsize, NOERROR, tsig_key,
				      NULL, 0, sig, &siglen, timesigned);
			if (ret == 0)
				tsig_signed = 1;
		}

		/*
		 * Send length & message for SOA query
		 */
		if (writemsg(s, buf, n) < 0) {
			syslog(LOG_INFO, "writemsg: %m");
			error++;
			(void) my_close(s);
			continue;	
		}
		/*
		 * Get out your butterfly net and catch the SOA
		 */
		
		if (netread(s, (char *)buf, INT16SZ, 
			(soa_cnt == 0) ?400 :XFER_TIMER) < 0) {
			(void) my_close(s);
			error++;
			continue;
		}
		if ((len = ns_get16(buf)) == 0) {
			(void) my_close(s);
			continue;
		}
		if (len > bufsize) {
			if ((buf = (u_char *)realloc(buf, len)) == NULL) {
				syslog(LOG_INFO,
		       "malloc(%u) failed for SOA from server [%s], zone %s\n",
				       len,
				       inet_ntoa(sin.sin_addr),
				       zp->z_origin);
				(void) my_close(s);
				continue;
			}
			bufsize = len;
		}
		if (netread(s, (char *)buf, len, XFER_TIMER) < 0) {
			error++;
			(void) my_close(s);
			continue;
		}
		/*
		 * Verify the TSIG if expected
		 */
		if (tsig_signed != 0) {
			ret = ns_verify(buf, (int *)&len, tsig_key, sig, siglen,
					NULL, NULL, &timesigned, 0);
			if (ret != 0) {
				syslog(LOG_NOTICE,
		"SOA TSIG verification from server [%s], zone %s: %s (%d)\n",
				       inet_ntoa(sin.sin_addr), zp->z_origin,
				       tsig_rcode(ret), ret);
				error++;
				continue;
			}
		}
		
#ifdef DEBUG
		if (debug >= 3) {
			(void)fprintf(ddt,"len = %d\n", len);
			res_pquery(&res, buf, len, ddt);
		}
#endif
		if (((methode == ISIXFR) && (ixfp == NULL)) && was_ixfr == 0) {
			delete_soa = 1;	
			ixfr_soa = 0;
			if ((ixfp = fopen(tmpiname, "w+")) == NULL) {
				perror(tmpiname);
				if (!quiet)
					syslog(LOG_ERR,
					       "can't fdopen ixfr log (%s)",
					       tmpname);
				exit(XFER_FAIL);
			}
		}

		hp = (HEADER *) buf;
		qdcount = ntohs(hp->qdcount);
		ancount = ntohs(hp->ancount);
		aucount = ntohs(hp->nscount);
		arcount = ntohs(hp->arcount);
		/*
		 * close socket if any of these apply:
		 *  1) rcode != NOERROR
		 *  2) not an authority response
		 *  3) not an answer to our question
		 *  4) both the number of answers and authority count < 1)
		 */
		if (hp->rcode != NOERROR || !hp->aa || qdcount != 1 ||
		    (ancount < 1 && aucount < 1)) {
#ifndef SYSLOG_42BSD
			syslog(LOG_NOTICE,
       "[%s] %s for %s, SOA query got rcode %d, aa %d, ancount %d, aucount %d",
			       inet_ntoa(sin.sin_addr),
			       (hp->aa
				? (qdcount==1 ?"no SOA found" :"bad response")
				: "not authoritative"),
			       zp->z_origin[0] != '\0' ? zp->z_origin : ".",
			       hp->rcode, hp->aa, ancount, aucount);
#endif
			error++;
			(void) my_close(s);
			continue;
		}
		zp_start = *zp;
		if ((int)len < HFIXEDSZ + QFIXEDSZ) {
			badsoa_msg = "too short";
 badsoa:
			syslog(LOG_INFO,
			       "malformed SOA from [%s], zone %s: %s",
			       inet_ntoa(sin.sin_addr), zp->z_origin,
			       badsoa_msg);
			error++;
			(void) my_close(s);
			continue;
		}
		/*
		 * Step through response.
		 */
		tmp = buf + HFIXEDSZ;
		eom = buf + len;
		/* Query Section. */
		if (qdcount > 1) {
			badsoa_msg = "question error";
			goto badsoa;
		}
		if (qdcount < 1)
			goto no_question;
		n = dn_expand(buf, eom, tmp, name2, sizeof name2);
		if (n < 0) {
			badsoa_msg = "qname error";
			goto badsoa;
		}
		tmp += n;
		if (tmp + 2 * INT16SZ > eom) {
			badsoa_msg = "query error";
			goto badsoa;
		}
		NS_GET16(type, tmp);
		NS_GET16(class, tmp);
		if (class != curclass || 
			((type != T_SOA) && (type != T_IXFR) && (type != T_AXFR)) ||
		    ns_samename(zp->z_origin, name2) != 1) 
		{
			syslog(LOG_INFO,
			"wrong query in resp from [%s], zone %s: [%s %s %s]\n",
			       inet_ntoa(sin.sin_addr), zp->z_origin,
			       name2, p_class(class), p_type(type));
			error++;
			(void) my_close(s);
			continue;
		}
 no_question:
		/* ... Answer Section.
		 * We may have to loop a little, to bypass SIG SOA's in
		 * the response.
		 */
		loop_cnt = 0;
		do {
			u_char *cp4;
			u_short type, class, dlen;
			u_int32_t ttl;
			n = dn_expand(buf, eom, tmp, name2, sizeof name2);
			if (n < 0) {
				badsoa_msg = "aname error";
				goto badsoa;
			}
			tmp += n;

			/* Are type, class, and ttl OK? */
			cp4 = tmp;	/* Leave tmp pointing to type field */
			if (eom - cp4 < 3 * INT16SZ + INT32SZ) {
				badsoa_msg = "zinfo too short";
				goto badsoa;
			}
			NS_GET16(type, cp4);
			NS_GET16(class, cp4);
			NS_GET32(ttl, cp4);
			NS_GET16(dlen, cp4);
			if (cp4 + dlen > eom) {
				badsoa_msg = "zinfo dlen too big";
				goto badsoa;
			}
			if (type == T_SOA) {
			  if (was_ixfr)  {
				methode = ISNOTIXFR;
				break;
			  }
			  if ((methode == ISIXFR) && (loop_cnt == 0)) {
				bp = tmp;
			  	soa_cnt++;
				badsoa_msg = soa_zinfo(&zp_finish, tmp, eom);
				if (badsoa_msg)
					goto badsoa;
				if (ixfp)
					  if (ixfr_log(buf, len, &delete_soa, ixfp,
						 &sin, domain, &serial_no,
						 &ixfr_first) < 0) {
						 error++;
						 break;
					  }
			  } else { 
				if (methode == ISIXFR) {
					check_serial = 0;
			 		soa_cnt++;
					break;
				}
				break;
			   }
			}
			if ((loop_cnt >= 1) && (soa_cnt < 2)) {
					dprintf(1,
				      	"server %s %d rejected IXFR and responded with AXFR\n",
					       inet_ntoa(sin.sin_addr), soa_cnt);
					methode = ISNOTIXFR;
					check_serial = 0;
					was_ixfr++;
					tmp = bp;
				break;
			}
			/* Skip to next record, if any.  */
			dprintf(1, "skipping %s %s RR in response\n",
				name2, p_type(type));
			tmp = cp4 + dlen;
			loop_cnt++;
			if (loop_cnt >= ancount) {
				tmp = bp;
				check_serial = 0;
				break;
			}
		} while (1);

		if (ns_samename(zp->z_origin, name2) != 1) {
			syslog(LOG_INFO,
		       "wrong answer in resp from [%s], zone %s: [%s %s %s]\n",
			       inet_ntoa(sin.sin_addr), zp->z_origin,
			       name2, p_class(class), p_type(type));
			error++;
			(void) my_close(s);
			continue;
		}
		badsoa_msg = soa_zinfo(&zp_start, tmp, eom);
		if (badsoa_msg)
			goto badsoa;
		if (methode == ISNOTIXFR) {
		if (SEQ_GT(zp_start.z_serial, serial_no) || !check_serial) {
			const char *l, *nl, *t;

			if (soa_cnt)  {
				goto axfr_response;
			}
			dprintf(1, "need update, serial %u\n",
				zp_start.z_serial);
			soa_cnt = 0;
			hp = (HEADER *) buf;
			ns_cnt = 0;
			gettime(&tt);
			locallen = sizeof local;
			if (getsockname(s, (struct sockaddr *)&local,
					&locallen) < 0) {
				memset(&local, 0, sizeof local);
			}
			for (l = Version; l; l = nl) {
				size_t len;
				if ((nl = strchr(l, '\n')) != NULL) {
					len = nl - l;
					nl = nl + 1;
				} else {
					len = strlen(l);
					nl = NULL;
				}
				while (isspace((unsigned char) *l))
					l++;
				if (*l)
					fprintf(dbfp, "; BIND version %.*s\n",
						(int)len, l);
			}
			fprintf(dbfp, check_serial?
				"; zone '%s'   last serial %u\n":
				"; zone '%s'   first transfer\n",
				domain, serial_no);
			t = strdup(inet_ntoa(sin.sin_addr));
			fprintf(dbfp, "; from %s:%d (local %s) using %s at %s",
				t, ntohs(sin.sin_port),
				inet_ntoa(local.sin_addr),
				(methode == ISIXFR) ? "IXFR":"AXFR", 
				ctimel(tt.tv_sec));
			free((void *)t);
			for (;;) {
				if ((soa_cnt == 0) || (zp->z_type == Z_STUB)) {
					if (zp->z_type == Z_STUB) {
						if (soa_cnt == 1 &&
						    ns_cnt == 0)
							query_type = T_NS;
						else
							query_type = T_SOA;
					} else if (methode == ISIXFR)
						query_type = T_IXFR;
					else
						query_type = xfr_qtype;
					n = res_nmkquery(&res, QUERY,
							 zp->z_origin,
							 curclass, query_type,
							 NULL, 0,
							 NULL, buf, bufsize);
					syslog(LOG_INFO,
					       "send %s query %d to %s",
				      (query_type == T_IXFR) ? "IXFR" :
				      (query_type == T_AXFR) ? "AXFR" :
				      (query_type == ns_t_zxfr) ? "ZXFR" :
				      (query_type == T_SOA) ? "SOA" : "NS",
					       cnt, inet_ntoa(sin.sin_addr));
					dprintf(1,
					"send %s query to %s\n",
					(query_type == T_IXFR) ? "IXFR" :
					(query_type == T_AXFR) ? "AXFR" :
					(query_type == ns_t_zxfr) ? "ZXFR" :
					(query_type == T_SOA) ? "SOA" : "NS",
						inet_ntoa(sin.sin_addr));
					dprintf(1,"bufsize = %d\n", bufsize);
					if (n < 0) {
						if (!quiet) {
						    if (zp->z_type == Z_STUB)
							syslog(LOG_INFO,
						       (query_type == T_SOA)
					? "zone %s: res_nmkquery T_SOA failed"
					: "zone %s: res_nmkquery T_NS failed",
							       zp->z_origin);
						    else
							syslog(LOG_INFO,
					     "zone %s: res_nmkquery %s failed",
						       zp->z_origin,
						       p_type(query_type));
						}
						(void) my_close(s);
#ifdef POSIX_SIGNALS
						sigaction(SIGALRM, &osv,
							(struct sigaction *)0);
#else
						sigvec(SIGALRM, &osv,
						       (struct sigvec *)0);
#endif
						return (XFER_FAIL);
					}
					cpp = buf + n;
					/*
					 * Append TSIG to AXFR query if desired
					 */
					if (tsig_signed != 0) {
						siglen = sizeof(sig);
						ns_sign(buf, &n, bufsize,
							NOERROR, tsig_key,
							NULL, 0, sig, &siglen,
							timesigned);
						cpp = buf + n;
						ns_verify_tcp_init(tsig_key,
								   sig, siglen,
								   &tsig_state);
					}
					/*
					 * Send length & msg for zone transfer
					 */
					if (writemsg(s, buf, cpp - buf) < 0) {
						syslog(LOG_INFO,
						       "writemsg: %m");
						error++;
						(void) my_close(s);
						break;	
					}
				}
/*XXX ZXFR*/
receive:
				/*
				 * Receive length & response
				 */
				if (netread(s, (char *)buf, INT16SZ,
					    (soa_cnt == 0) ?300 :XFER_TIMER)
				    < 0) {
					error++;
					break;
				}
				if ((len = ns_get16(buf)) == 0)
					break;
				if (len > bufsize) {
					buf = (u_char *)realloc(buf, len);
					if (buf == NULL) {
						syslog(LOG_INFO,
		   	 "malloc(%u) failed for packet from server [%s], zone %s\n",
						       len,
						       inet_ntoa(sin.sin_addr),
						       zp->z_origin);
						error++;
						break;
					}
					bufsize = len;
				}
				hp = (HEADER *)buf;
				eom = buf + len;
				if (netread(s, (char *)buf, len, XFER_TIMER)
				    < 0) {
					error++;
					break;
				}
#ifdef DEBUG
				if (debug >= 3) {
					(void)fprintf(ddt,"len = %d\n", len);
					res_pquery(&res, buf, len, ddt);
				}
				if (fp)
					res_pquery(&res, buf, len, fp);
#endif
				/*
				 * Verify the TSIG if expected
				 */
				if (tsig_signed != 0) {
					tsig_req = (soa_cnt == 0);
					ret = ns_verify_tcp(buf, (int *)&len,
							    &tsig_state,
							    tsig_req);
					eom = buf + len;

					if (ret != 0) {
						syslog(LOG_NOTICE,
		"TSIG verification from server [%s], zone %s: %s (%d)\n",
							inet_ntoa(sin.sin_addr),
							zp->z_origin,
							tsig_rcode(ret), ret);
						error++;
						break;
					}
				}
				if (len < HFIXEDSZ) {
 badrec:
					error++;
					alen = sizeof my_addr;
					if (getsockname(s, (struct sockaddr *)
							&my_addr, &alen) < 0)
						sprintf(my_addr_text,
							"[errno %d]", errno);
					else
						sprintf(my_addr_text,
							"[%s].%u",
							inet_ntoa(my_addr.
								  sin_addr),
							ntohs(my_addr.sin_port)
							);
					if ((hp->rcode == REFUSED) &&
					    (len >= HFIXEDSZ)) {
						syslog(LOG_INFO,
				  "[%s] transfer refused from [%s], zone %s\n",
						       my_addr_text,
						       inet_ntoa(sin.sin_addr),
						       zp->z_origin);
					} else {
						syslog(LOG_INFO,
				  "[%s] record too short from [%s], zone %s\n",
						       my_addr_text,
						       inet_ntoa(sin.sin_addr),
						       zp->z_origin);
					}
					break;
				}
axfr_response:
				if (query_type == T_IXFR) 
					if (hp->rcode != NOERROR) {
					    dprintf(1,
					    "server %s did not support IXFR\n",
						    inet_ntoa(sin.sin_addr));
						methode = ISNOTIXFR;
						continue;
					};
				cp = buf + HFIXEDSZ;
				if (ntohs(hp->qdcount) == 1) {
					if ((query_type == T_IXFR) && (methode == ISIXFR)) {
							dprintf(1,
				      "server %s rejected IXFR and responded with AXFR\n",
						       inet_ntoa(sin.sin_addr));
							methode = ISNOTIXFR;
					}
					n = dn_skipname(cp, eom);
					if ((n  == -1) ||
					    ((n + QFIXEDSZ) >= (eom - cp)))
						goto badrec;
					cp += n + QFIXEDSZ;
				}
				nmp = cp;
				if ((n = dn_skipname(cp, eom)) == -1)
					goto badrec;
				tmp = cp + n;
				if (zp->z_type == Z_STUB) {
					ancount = ntohs(hp->ancount);
					n = 0;
					for (cnt = 0;
					     cnt < (u_int)ancount;
					     cnt++) {
						n = print_output(zp,
								 serial_no,
								 buf, len, cp,
								 was_ixfr);
						if (n < 0)
							break;
						cp += n;
					}
					/*
					 * If we've processed the answer
					 * section and didn't get any useful
					 * answers, bail out.
					 */
					if (query_type == T_SOA &&
					    soa_cnt == 0) {
						syslog(LOG_ERR,
						    "stubs: no SOA in answer");
						error++;
						break;
					}
					if (query_type == T_NS &&
					    ns_cnt == 0) {
						syslog(LOG_ERR,
						     "stubs: no NS in answer");
						error++;
						break;
					}
					if (n >= 0 && hp->nscount) {
						ancount = ntohs(hp->nscount);
						for (cnt = 0;
						     cnt < (u_int)ancount;
						     cnt++) {
							n = print_output(zp,
								     serial_no,
									 buf,
									 len,
									 cp,
								     was_ixfr);
							if (n < 0)
								break;
							cp += n;
						}
					}
					ancount = ntohs(hp->arcount);
					for (cnt = 0;
					     n > 0 && cnt < (u_int)ancount;
					     cnt++) {
						n = print_output(zp, serial_no,
								 buf, len, cp,
								 was_ixfr);
						cp += n;
					}
					if (n < 0) {
						syslog(LOG_INFO,
			      "print_output: unparseable answer (%d), zone %s",
						       hp->rcode,
						       zp->z_origin);
						error++;
						break;
					}
					if (cp != eom) {
						syslog(LOG_INFO,
				"print_output: short answer (%d, %d), zone %s",
						       cp - buf, eom - buf,
						       zp->z_origin);
						error++;
						break;
					}
				} else {
					ancount = ntohs(hp->ancount);
					if (query_type == T_IXFR &&
					    methode == ISIXFR) {
						if (ixfr_log(buf, len,
							     &delete_soa, ixfp,
							     &sin, domain,
							     &serial_no,
							     &ixfr_first) < 0){
								error++;
								break;
							}
					}
					for (n = cnt = 0;
					     cnt < (u_int)ancount;
					     cnt++) {
						n = print_output(zp, serial_no,
								 buf, len, cp,
								 was_ixfr);
						if (n < 0)
							break;
						cp += n;
					}
					if (n < 0) {
						syslog(LOG_INFO,
			"print_output: unparseable answer (%d), zone %s",
						       hp->rcode,
						       zp->z_origin);
						error++;
						break;
					}
					if (cp != eom) {
						syslog(LOG_INFO,
				"print_output: short answer (%d, %d), zone %s",
						       cp - buf, eom - buf,
						       zp->z_origin);
						error++;
						break;
					}
				}
				if ((soa_cnt >= 2) && (methode == ISNOTIXFR))
					break;
				if ((soa_cnt == -1) && (methode == ISIXFR))
					break;
			}
			(void) my_close(s);
			if (error == 0) {
#ifdef POSIX_SIGNALS
				(void) sigaction(SIGALRM, &osv,
						 (struct sigaction *)0);
#else
				(void) sigvec(SIGALRM, &osv,
					      (struct sigvec *)0);
#endif
				if (ixfp) {
					(void) fclose(ixfp);
					ixfp = NULL;
				}
				return (XFER_SUCCESSAXFR);
			}
			if (ixfp) {
				(void) fclose(ixfp);
				ixfp = NULL;
			}
			dprintf(2, "error receiving zone transfer\n");
		} else if (zp_start.z_serial == serial_no) {
			(void) my_close(s);
			dprintf(1, "zone up-to-date, serial %u\n",
				zp_start.z_serial);
			if (ixfp) {
				(void) unlink (tmpiname);
				(void) fclose(ixfp);
				ixfp = NULL;
			}
			return (XFER_UPTODATE);
		} else {
			(void) my_close(s);
			if (!quiet)
				syslog(LOG_NOTICE,
		      "serial from [%s], zone %s: %u lower than current: %u\n",
				       inet_ntoa(sin.sin_addr), zp->z_origin,
				       zp_start.z_serial, serial_no);
			return (XFER_FAIL);
		}
		} else {
			if (zp_finish.z_serial == query_serial) {
				(void) my_close(s);
				dprintf(1, "zone up-to-date, serial %u\n",
					zp_start.z_serial);
				if (ixfp) {
					(void) unlink (tmpiname);
					(void) fclose(ixfp);
					ixfp = NULL;
				}
				return (XFER_UPTODATE);
			}
			if (SEQ_GT(query_serial, zp_finish.z_serial)) {
				if (!quiet)
				syslog(LOG_NOTICE,
		             "serial from [%s], zone %s: %u lower than current: %u\n",
				       inet_ntoa(sin.sin_addr), zp->z_origin,
				       zp_finish.z_serial, query_serial);
				dprintf(1, 
		             "serial from [%s], zone %s: %u lower than current: %u\n",
					inet_ntoa(sin.sin_addr), zp->z_origin,
					zp_finish.z_serial, query_serial);
				if (ixfp) {
					(void) fclose(ixfp);
					ixfp = NULL;
					(void) unlink (tmpiname);
				}
				if (was_ixfr == 0) {
					was_ixfr++;
					n = res_nmkquery(&res, QUERY,
							 zp->z_origin,
							 curclass, T_AXFR,
							 NULL, 0,
							 NULL, buf, bufsize);
					if (n < 0) {
						if (!quiet)
							syslog(LOG_INFO,
		    		               "zone %s: res_nmkquery T_SOA failed",
							       zp->z_origin);
						(void) my_close(s);
#ifdef POSIX_SIGNALS
						(void) sigaction(SIGALRM, &osv,
							(struct sigaction *)0);
#else
						(void) sigvec(SIGALRM, &osv,
							   (struct sigvec *)0);
#endif
						return (XFER_FAIL);
					}
					/*
					 * Append TSIG to SOA query if desired
					 */
					tsig_key = tsig_key_from_addr(sin.sin_addr);
					if (tsig_key != NULL) {
						siglen = sizeof(sig);
						ret = ns_sign(buf, &n, bufsize,
							      NOERROR,
							      tsig_key, NULL,
							      0, sig, &siglen,
							      timesigned);
						if (ret == 0)
							tsig_signed = 1;
					}

				/*
				 * Send length & message for AXFR query
				 */
					if (writemsg(s, buf, n) < 0)
						syslog(LOG_INFO,
						       "writemsg: %m");
					else {
						methode = ISNOTIXFR;
						check_serial = 0;
						soa_cnt = 0;
						was_ixfr = 0;
						goto receive;
					}
				}
				(void) my_close(s);
				return (XFER_FAIL);
			}
			if (ancount == 1) {
				methode = ISNOTIXFR;
				check_serial = 0;
				soa_cnt = 0;
				goto axfr_response;
			}
			dprintf(1, "We have an IXFR\n");
			while (SEQ_GT(zp_finish.z_serial, serial_no)) {
				/*
				 * Receive length & response
				 */
				if (netread(s, (char *)buf, INT16SZ,
					    (soa_cnt == 0) ?300 :XFER_TIMER)
				    < 0) {
					error++;
					break;
				}
				if ((len = ns_get16(buf)) == 0)
					break;
				if (len > bufsize) {
					buf = (u_char *)realloc(buf, len);
					if (buf == NULL) {
						syslog(LOG_INFO,
		   	 "malloc(%u) failed for packet from server [%s], zone %s\n",
						       len,
						       inet_ntoa(sin.sin_addr),
						       zp->z_origin);
						error++;
						break;
					}
					bufsize = len;
				}
				hp = (HEADER *)buf;
				eom = buf + len;
				if (netread(s, (char *)buf, len, XFER_TIMER)
				    < 0) {
					error++;
					break;
				}
#ifdef DEBUG
				if (debug >= 3) {
					(void)fprintf(ddt,"len = %d\n", len);
					res_pquery(&res, buf, len, ddt);
				}
				if (fp)
					res_pquery(&res, buf, len, fp);
#endif
				/*
				 * Verify the TSIG if expected
				 */
				if (tsig_signed != 0) {
					tsig_req = (soa_cnt == 0);
					ret = ns_verify_tcp(buf, (int *)&len,
							    &tsig_state,
							    tsig_req);
					eom = buf + len;

					if (ret != 0) {
						syslog(LOG_NOTICE,
		"TSIG verification from server [%s], zone %s: %s (%d)\n",
							inet_ntoa(sin.sin_addr),
							zp->z_origin,
							tsig_rcode(ret), ret);
						error++;
						break;
					}
				}
				if (len < HFIXEDSZ) {
					error++;
					alen = sizeof my_addr;
					if (getsockname(s, (struct sockaddr *)
							&my_addr, &alen) < 0)
						sprintf(my_addr_text, "[errno %d]", errno);
					else
						sprintf(my_addr_text, "[%s].%u",
							inet_ntoa(my_addr.  sin_addr),
							ntohs(my_addr.sin_port));
					if ((hp->rcode == REFUSED) &&
					    (len >= HFIXEDSZ)) {
						syslog(LOG_INFO,
				  			"[%s] transfer refused from [%s], zone %s\n",
						       my_addr_text,
						       inet_ntoa(sin.sin_addr),
						       zp->z_origin);
					} else {
						syslog(LOG_INFO,
				  "[%s] record too short from [%s], zone %s\n",
						       my_addr_text,
						       inet_ntoa(sin.sin_addr),
						       zp->z_origin);
					}
					break;
				}
				if (ixfp)
				  if (ixfr_log(buf, len, &delete_soa, ixfp,
						 &sin, domain, &serial_no,
						 &ixfr_first) < 0) {
							error++;
							break;
				  }
			}
			(void) my_close(s);
			if (!error) {
				fprintf(ixfp, "update:\t{add} ");
				if (soa_buf)
				  fputs(soa_buf, ixfp);
				fprintf(ixfp, "[END_DELTA]\n");
				return (XFER_SUCCESSIXFR);
			}
		}
	}
#ifdef POSIX_SIGNALS
	(void) sigaction(SIGALRM, &osv, (struct sigaction *)0);
#else
	(void) sigvec(SIGALRM, &osv, (struct sigvec *)0);
#endif
	if (ixfp) {
			(void) unlink (tmpiname);
			(void) my_fclose(ixfp);
			ixfp = 0;
	}
	if (!error)
		return (XFER_TIMEOUT);
	return (XFER_FAIL);
}

static SIG_FN
term_handler() {
	cleanup_for_exit();
	_exit(XFER_FAIL);  /* not safe to call exit() from a signal handler */
}

/*
 * Set flag saying to read was interrupted
 * used for a read timer
 */
static SIG_FN
read_alarm() {
	read_interrupted = 1;
}
 
static int
netread(int fd, char *buf, int len, int timeout) {
	static const char setitimerStr[] = "setitimer: %m";
	struct itimerval ival, zeroival;
	struct sockaddr_in sa;
	int n, salen;
#if defined(NETREAD_BROKEN)
	int retries = 0;
#endif

	memset(&zeroival, 0, sizeof zeroival);
	ival = zeroival;
	ival.it_value.tv_sec = timeout;
	while (len > 0) {
#ifndef WINNT
		if (setitimer(ITIMER_REAL, &ival, NULL) < 0) {
			syslog(LOG_INFO, setitimerStr);
			return (-1);
		}
#endif 
		errno = 0;
		salen = sizeof sa;
		n = recvfrom(fd, buf, len, 0, (struct sockaddr *)&sa, &salen);
		if (n == 0 && errno == 0) {
#if defined(NETREAD_BROKEN)
			if (++retries < 42)	/* doug adams */
				continue;
#endif
			syslog(LOG_INFO, "premature EOF, fetching \"%s\"",
			       domain);
			return (-1);
		}
		if (n < 0) {
			if (errno == 0) {
#if defined(NETREAD_BROKEN)
				if (++retries < 42)	/* doug adams */
					continue;
#endif
				syslog(LOG_INFO,
				       "recv(len=%d): n=%d && !errno",
				       len, n);
				return (-1);
			}
			if (errno == EINTR) {
				if (!read_interrupted) {
					/* It wasn't a timeout; ignore it. */
					continue;
				}
				errno = ETIMEDOUT;
			}
			syslog(LOG_INFO, "recv(len=%d): %m", len);
			return (-1);
		}
		buf += n;
		len -= n;
	}
#ifndef WINNT
	if (setitimer(ITIMER_REAL, &zeroival, NULL) < 0) {
		syslog(LOG_INFO, setitimerStr);
		return (-1);
	}
#endif
	return (0);
}

/*
 * Write a counted buffer to a file descriptor preceded by a length word.
 */
static int
writemsg(int rfd, const u_char *msg, int msglen) {
	struct iovec iov[2];
	u_char len[INT16SZ];
	int ret;

	__putshort(msglen, len);
	iov[0].iov_base = (char *)len;
	iov[0].iov_len = INT16SZ;
	iov[1].iov_base = (char *)msg;
	iov[1].iov_len = msglen;
	ret = writev(rfd, iov, 2);
	if (ret != INT16SZ + msglen) {
		syslog(LOG_DEBUG, "writemsg(%d,%p,%d) failed: %s",
		       rfd, (void *)msg, msglen, strerror(errno));
		return (-1);
	}
	return (ret);
}

static const char *
soa_zinfo(struct zoneinfo *zp, u_char *cp, u_char *eom) {
	int n, type, class;
	u_int32_t ttl;
	u_int16_t dlen;
	u_char *rdatap;

	/* Are type, class, and ttl OK? */
	if (eom - cp < 3 * INT16SZ + INT32SZ)
		return ("zinfo too short");
	NS_GET16(type, cp);
	NS_GET16(class, cp);
	NS_GET32(ttl, cp);
	NS_GET16(dlen, cp);
	rdatap = cp;
	if (type != T_SOA || class != curclass)
		return ("zinfo wrong typ/cla/ttl");
	/* Skip master name and contact name, we can't validate them. */
	if ((n = dn_skipname(cp, eom)) == -1)
		return ("zinfo mname");
	cp += n;
	if ((n = dn_skipname(cp, eom)) == -1)
		return ("zinfo hname");
	cp += n;
	/* Grab the data fields. */
	if (eom - cp < 5 * INT32SZ)
		return ("zinfo dlen");
	NS_GET32(zp->z_serial, cp);
	NS_GET32(zp->z_refresh, cp);
	NS_GET32(zp->z_retry, cp);
	NS_GET32(zp->z_expire, cp);
	NS_GET32(zp->z_minimum, cp);
	if (cp != rdatap + dlen)
		return ("bad soa dlen");
	return (NULL);
}

#define BOUNDS_CHECK(ptr, count) \
	do { \
		if ((ptr) + (count) > eom) { \
			hp->rcode = FORMERR; \
			return (-1); \
		} \
	} while (0)

/*
 * Parse the message, determine if it should be printed, and if so, print it
 * in .db file form.  Does minimal error checking on the message content.
 *
 * XXX why aren't we using ns_sprintrr() ?
 */
static int
print_output(struct zoneinfo *zp, u_int32_t serial_no, u_char *msg,
	     int msglen, u_char *rrp, int was_ixfr) {
	u_char *cp;
	HEADER *hp = (HEADER *) msg;
	u_int32_t addr, ttl, tmpnum;
	int i, j, tab, result, n1, n;
	u_int class, type, dlen;
	char data[MAXDATA];
	u_char *cp1, *cp2, *temp_ptr, *eom, *rr_type_ptr;
	u_char *cdata, *rdatap;
	char *origin, dname[MAXDNAME];
	const char *proto;
	const char *ignore = "";
	const char *badsoa_msg;
	int escaped = 0;

	eom = msg + msglen;
	cp = rrp;
	n = dn_expand(msg, msg + msglen, cp, dname, sizeof dname);
	if (n < 0) {
		hp->rcode = FORMERR;
		return (-1);
	}
	cp += n;
	BOUNDS_CHECK(cp, 3 * INT16SZ + INT32SZ);
	rr_type_ptr = cp;
	NS_GET16(type, cp);
	NS_GET16(class, cp);
	NS_GET32(ttl, cp);
	/*
	 * Following the Clarification draft's direction, we treat TTLs with
	 * the MSB set as if they were 0.
	 */
	if (ttl > MAXIMUM_TTL) {
		syslog(LOG_INFO, "%s: TTL > %u, converted to 0", dname,
		       MAXIMUM_TTL);
		ttl = 0;
	}
	NS_GET16(dlen, cp);
	BOUNDS_CHECK(cp, dlen);
	rdatap = cp;

	origin = dname;
	while (*origin) {
		if (!escaped && *origin == '.') {
			origin++;	/* skip over '.' */
			break;
		}
		escaped = (*origin++ == '\\') && !escaped;
	}
	dprintf(3, "print_output: dname %s type %d class %d ttl %u\n",
		dname, type, class, ttl);
	/*
	 * Convert the resource record data into the internal database format.
	 * CP points to the raw resource record.
	 * After this switch:
	 *	CP has been updated to point past the RR.
	 *	CP1 points to the internal database version.
 	 *	N is the length of the internal database version.
	 */
	switch (type) {
	case T_A:
	case T_WKS:
	case T_HINFO:
	case T_TXT:
	case T_X25:
	case T_ISDN:
	case T_LOC:
	case T_NSAP:
	case T_AAAA:
	case T_KEY:
	case ns_t_cert:
		cp1 = cp;
		n = dlen;
		cp += n;
		break;

	case T_CNAME:
	case T_MB:
	case T_MG:
	case T_MR:
	case T_NS:
	case T_PTR:
		n = dn_expand(msg, msg + msglen, cp, data, sizeof data);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 = (u_char *)data;
		n = strlen(data) + 1;
		break;

	case T_MINFO:
	case T_SOA:
	case T_RP:
		n = dn_expand(msg, msg + msglen, cp, data, sizeof data);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		n = strlen(data) + 1;
		cp1 = (u_char *)data + n;
		n1 = sizeof data - n;
		if (type == T_SOA)
			n1 -= 5 * INT32SZ;
		n = dn_expand(msg, msg + msglen, cp, (char *)cp1, n1);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += strlen((char *) cp1) + 1;
		if (type == T_SOA) {
			BOUNDS_CHECK(cp, 5 * INT32SZ);
			temp_ptr = cp + 4 * INT32SZ;
			NS_GET32(minimum_ttl, temp_ptr);
			/*
			 * Following the Clarification draft's direction,
			 * we treat TTLs with the MSB set as if they were 0.
			 */
			if (minimum_ttl > MAXIMUM_TTL) {
				syslog(LOG_INFO,
			       "%s: SOA minimum TTL > %u, converted to 0",
				       dname, MAXIMUM_TTL);
				minimum_ttl = 0;
			}
			n = 5 * INT32SZ;
			memcpy(cp1, cp, n);
			cp += n;
			cp1 += n;
		}
		n = cp1 - (u_char *)data;
		cp1 = (u_char *)data;
		break;

	case T_NAPTR:
		/* Grab weight and port. */
		BOUNDS_CHECK(cp, INT16SZ*2);
		memcpy(data, cp, INT16SZ*2);
		cp1 = (u_char *)data + INT16SZ*2;
		cp += INT16SZ*2;

		/* Flags */
		BOUNDS_CHECK(cp, 1);
		n = *cp++;
		BOUNDS_CHECK(cp, n);
		*cp1++ = n;
		memcpy(cp1, cp, n);
		cp += n; cp1 += n;

		/* Service */
		BOUNDS_CHECK(cp, 1);
		n = *cp++;
		BOUNDS_CHECK(cp, n);
		*cp1++ = n;
		memcpy(cp1, cp, n);
		cp += n; cp1 += n;

		/* Regexp */
		BOUNDS_CHECK(cp, 1);
		n = *cp++;
		BOUNDS_CHECK(cp, n);
		*cp1++ = n;
		memcpy(cp1, cp, n);
		cp += n; cp1 += n;

		/* Replacement */
		n = dn_expand(msg, msg + msglen, cp, (char *)cp1,
			      sizeof data - ((char *)cp1 - data));
		if (n < 0)
			return (-1);
		cp += n;

		/* compute end of data */
		cp1 += strlen((char *)cp1) + 1;
		/* compute size of data */
		n = cp1 - (u_char *)data;
		cp1 = (u_char *)data;
		break;

	case T_MX:
	case T_AFSDB:
	case T_RT:
	case T_SRV:
		/* grab preference */
		BOUNDS_CHECK(cp, INT16SZ);
		memcpy(data, cp, INT16SZ);
		cp1 = (u_char *)data + INT16SZ;
		cp += INT16SZ;

		if (type == T_SRV) {
			BOUNDS_CHECK(cp, INT16SZ*2);
			memcpy(cp1, cp, INT16SZ*2);
			cp1 += INT16SZ*2;
			cp += INT16SZ*2;
		}

		/* get name */
		n = dn_expand(msg, msg + msglen, cp,
			      (char *)cp1,
			      sizeof data - (cp1 - (u_char *)data));
		if (n < 0)
			return (-1);
		cp += n;

		/* compute end of data */
		cp1 += strlen((char *) cp1) + 1;
		/* compute size of data */
		n = cp1 - (u_char *)data;
		cp1 = (u_char *)data;
		break;

	case T_PX:
		/* grab preference */
		BOUNDS_CHECK(cp, INT16SZ);
		memcpy(data, cp, INT16SZ);
		cp1 = (u_char *)data + INT16SZ;
		cp += INT16SZ;

		/* get MAP822 name */
		n = dn_expand(msg, msg + msglen, cp,
			      (char *)cp1, sizeof data - INT16SZ);
		if (n < 0)
			return (-1);
		cp += n;
		cp1 += (n = (strlen((char *) cp1) + 1));
		n1 = sizeof data - n;

		/* get MAPX400 name */
		n = dn_expand(msg, msg + msglen, cp, (char *)cp1, n1);
		if (n < 0)
			return (-1);
		cp += n;
		cp1 += strlen((char *) cp1) + 1;
		n = cp1 - (u_char *)data;
		cp1 = (u_char *)data;
		break;

	case T_SIG:
	 /* CP is the raw resource record as it arrived.
	  * CP1, after this switch, points to the internal database version. */
		cp1 = (u_char *)data;

		/* first just copy over the type_covered, algorithm, */
		/* labels, orig ttl, two timestamps, and the footprint */
		BOUNDS_CHECK(cp, NS_SIG_SIGNER);
		memcpy(cp1, cp, NS_SIG_SIGNER);
		cp  += NS_SIG_SIGNER;
		cp1 += NS_SIG_SIGNER;

		/* then the signer's name */
		n = dn_expand(msg, msg + msglen, cp,
			      (char *)cp1, (sizeof data) - 18);
		if (n < 0)
			return (-1);
		cp += n;
		cp1 += strlen((char*)cp1)+1;

		/* finally, we copy over the variable-length signature.
		   Its size is the total data length, minus what we copied. */
		n = dlen - (NS_SIG_SIGNER + n);
		if (n > ((int)(sizeof data) - (int)(cp1 - (u_char *)data))) {
			hp->rcode = FORMERR;
			return (-1);  /* out of room! */
		}
		memcpy(cp1, cp, n);
		cp += n;
		cp1 += n;
		
		/* compute size of data */
		n = cp1 - (u_char *)data;
		cp1 = (u_char *)data;
		break;

	case T_NXT:
		n = dn_expand(msg, msg + msglen, cp,
			      (char *)data, sizeof data);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 = (u_char *)data + strlen(data) + 1;
		n  = dlen - n;
		if (n > ((int)(sizeof data) - (int)(cp1 - (u_char *)data))) {
			hp->rcode = FORMERR;
			return (-1);  /* out of room! */
		}
		if (n > 0) { /* Actually, n should never be less than 4 */ 
			memcpy(cp1, cp, n);
			cp += n;
		} else {  
			hp->rcode = FORMERR; 
			return (-1);
		}
		n += cp1 - (u_char *)data;
		cp1 = (u_char *)data; 
		break;

	default:
		syslog(LOG_INFO, "\"%s %s %s\" - unknown type (%d)",
		       dname, p_class(class), p_type(type), type);
		hp->rcode = NOTIMP;
		return (-1);
	}

	if (n > MAXDATA) {
		dprintf(1, "update type %d: %d bytes is too much data\n",
			type, n);
		hp->rcode = FORMERR;
		return (-1);
	}
	if (cp != rdatap + dlen) {
		dprintf(1,
		    "encoded rdata length is %u, but actual length was %u\n",
			dlen, (u_int)(cp - rdatap));
		hp->rcode = FORMERR;
		return (-1);
	}

	cdata = cp1;
	result = cp - rrp;

	/*
	 * Special handling for SOA records.
	 */

	if (type == T_SOA) {
		if (ns_samename(dname, zp->z_origin) != 1) {
			syslog(LOG_INFO,
			 "wrong zone name in XFR (wanted \"%s\", got \"%s\")",
			       zp->z_origin, dname);
			hp->rcode = FORMERR;
			return (-1);
		}
		if (soa_cnt == 0) {
			badsoa_msg = soa_zinfo(&zp_start, rr_type_ptr, eom);
			if (badsoa_msg) {
				syslog(LOG_INFO,
				       "malformed SOA for zone %s: %s",
				       zp->z_origin, badsoa_msg);
				hp->rcode = FORMERR;
				return (-1);
			}
			if (SEQ_GT(zp_start.z_serial, serial_no) ||
			    !check_serial) {
				soa_cnt++;
			} else {
				syslog(LOG_INFO,
			       "serial went backwards after transfer started");
				return (-1);
			}
		} else if (soa_cnt == 1) {
			badsoa_msg = soa_zinfo(&zp_finish, rr_type_ptr, eom);
			if (badsoa_msg) {
				syslog(LOG_INFO,
				       "malformed SOA for zone %s: %s",
				       zp->z_origin, badsoa_msg);
				hp->rcode = FORMERR;
				return (-1);
			}
			if (zp_start.z_serial == zp_finish.z_serial) {
				methode = ISNOTIXFR;
			} else if (zp_finish.z_serial != serial_no) {
				syslog(LOG_INFO,
			       "Unexpected serial number for zone %s: %u",
					zp->z_origin, zp_finish.z_serial);
			}
			soa_cnt++;
			if (methode == ISIXFR)
				return (result);
		} else {
			badsoa_msg = soa_zinfo(&zp_finish, rr_type_ptr, eom);
			if (badsoa_msg) {
				syslog(LOG_INFO,
				       "malformed SOA for zone %s: %s",
				       zp->z_origin, badsoa_msg);
				hp->rcode = FORMERR;
				return (-1);
			}
			if (methode == ISIXFR) {
				if (zp_start.z_serial == zp_finish.z_serial) {
					if (scdsoa) {
						soa_cnt = -1;
						return (result);
					} else {
						scdsoa = 1;
						soa_cnt++;
					};
				} else 
					soa_cnt++;
			} else {
				dprintf(2, "SOA, serial %u\n",
					zp_finish.z_serial);
				if (zp_start.z_serial != zp_finish.z_serial) {
					dprintf(1, "serial changed, restart\n");
					restarts++;
					if (restarts > MAX_XFER_RESTARTS) {
						syslog(LOG_INFO,
				      "too many transfer restarts for zone %s",
						       zp->z_origin);
						hp->rcode = FORMERR;
						return (-1);
					}
					soa_cnt = 0;
					ns_cnt = 0;
					minimum_ttl = 0;
					strcpy(prev_origin, zp->z_origin);
					prev_dname[0] = DEF_DNAME;
					/*
					 * Flush buffer, truncate file
					 * and seek to beginning to restart.
					 */
					fflush(dbfp);
					if (ftruncate(fileno(dbfp), 0) != 0) {
						if (!quiet)
							syslog(LOG_INFO,
							  "ftruncate %s: %m\n",
							       tmpname);
						return (-1);
					}
					fseek(dbfp, 0L, 0);
					return (result);
				}
				soa_cnt++;
				return (result);
			}
		}	
		if ((soa_cnt == 2) && (was_ixfr == 0))	
			return (result);

	}

	if (zp->z_type == Z_STUB) {
		if (query_type == T_NS && type == T_NS)
			ns_cnt++;
		/*
		 * If we're processing a response to an SOA query, we don't
		 * want to print anything from the response except for the SOA.
		 * We do want to check everything in the packet, which is
		 * why we do this check now instead of earlier.
		 */
		if (query_type == T_SOA && type != T_SOA)
			return (result);
	}

	if ((!soa_cnt || soa_cnt > 2) && methode == ISNOTIXFR) {
		char *gripe;

		if (!soa_cnt)
			gripe = "got RR before first SOA";
		else
			gripe = "got RR after second SOA";
		syslog(LOG_INFO, "%s in zone %s", gripe, zp->z_origin);
		hp->rcode = FORMERR;
		return (-1);
	}

	/*
	 * If they are trying to tell us info about something that is
	 * not in the zone that we are transfering, then ignore it!
	 * They don't have the authority to tell us this info.
	 *
	 * We have to do a bit of checking here - the name that we are
	 * checking is is fully qualified & may be in a subdomain of the
	 * zone in question.  We also need to ignore any final dots.
	 *
	 * If a domain has both NS records and non-NS records, (for
	 * example, NS and MX records), then we should ignore the non-NS
	 * records (except that we should not ignore glue A records).
	 * XXX: It is difficult to do this properly, so we just compare
	 * the current dname with that in the most recent NS record.
	 * This defends against the most common error case,
	 * where the remote server sends MX records soon after the
	 * NS records for a particular domain.  If sent earlier, we lose. XXX
	 */
	if (!ns_samedomain(dname, domain)) {
		(void) fprintf(dbfp, "; Ignoring info about %s, not in zone %s.\n",
			dname, domain);
		ignore = "; ";
	} else if (type != T_NS && type != T_A &&
		   ns_samename(zone_top, dname) != 1 &&
		   ns_samename(prev_ns_dname, dname) == 1)
	{
		(void) fprintf(dbfp, "; Ignoring extra info about %s, invalid after NS delegation.\n",
			dname);
		ignore = "; ";
	} else if (class != zp->z_class) {
		(void) fprintf(dbfp, "; Ignoring info about %s, not class %s\n",
		dname, p_class(zp->z_class));
		ignore = "; ";
	}

	/*
	 * If the current record is not being ignored, but the
	 * previous record was ignored, then we invalidate information
	 * that might have been altered by ignored records.
	 * (This means that we sometimes output unnecessary $ORIGIN
	 * lines, but that is harmless.)
	 * 
	 * Also update prev_comment now.
	 */
	if (prev_comment && ignore[0] == '\0') {
		prev_dname[0] = DEF_DNAME;
		prev_origin[0] = DEF_DNAME;
	}
	prev_comment = (ignore[0] != '\0');

	/*
	 * set prev_ns_dname if necessary
	 */
	if (type == T_NS) {
		(void) strcpy(prev_ns_dname, dname);
	}

	/*
	 * If the origin has changed, print the new origin
	 */
	if (ns_samename(prev_origin, origin) != 1) {
		(void) strcpy(prev_origin, origin);
		(void) fprintf(dbfp, "%s$ORIGIN %s.\n", ignore, origin);
	}
	tab = 0;

	if (ns_samename(prev_dname, dname) != 1) {
		/*
		 * set the prev_dname to be the current dname, then cut off all
		 * characters of dname after (and including) the first '.'
		 */
		char *cutp;

		(void) strcpy(prev_dname, dname);
		escaped = 0;
		cutp = dname;
		while (*cutp) {
			if (!escaped && *cutp == '.')
				break;
			escaped = (*cutp++ == '\\') && !escaped;
		}
		*cutp = '\0';

		if (dname[0] == 0) {
			if (origin[0] == 0)
				(void) fprintf(dbfp, "%s.\t", ignore);
			else
				(void) fprintf(dbfp, "%s.%s.\t",
					ignore, origin);	/* ??? */
		} else {
			char *backslash;
			backslash = (*dname == '@' || *dname == '$') ?
				"\\" : "";
			(void) fprintf(dbfp, "%s%s%s\t", ignore,
				       backslash, dname);
		}
		if (strlen(dname) < (size_t)8)
			tab = 1;
	} else {
		(void) fprintf(dbfp, "%s\t", ignore);
		tab = 1;
	}

	(void) fprintf(dbfp, "%d\t", (int) ttl);

	(void) fprintf(dbfp, "%s\t%s\t", p_class(class), p_type(type));
	cp = cdata;

	/*
	 * Print type specific data
	 */
	switch (type) {

	case T_A:
		switch (class) {
		case C_IN:
		case C_HS:
			fputs(inet_ntoa(ina_get(cp)), dbfp);
			break;
		}
		(void) fprintf(dbfp, "\n");
		break;

	case T_CNAME:
	case T_MB:
	case T_MG:
	case T_MR:
	case T_PTR:
		if (cp[0] == '\0')
			(void) fprintf(dbfp, ".\n");
		else
			(void) fprintf(dbfp, "%s.\n", cp);
		break;

	case T_NS:
		cp = cdata;
		if (cp[0] == '\0')
			(void) fprintf(dbfp, ".\t");
		else
			(void) fprintf(dbfp, "%s.", cp);
		(void) fprintf(dbfp, "\n");
		break;

	case T_HINFO:
	case T_ISDN:
		cp2 = cp + n;
		for (i = 0; i < 2; i++) {
			if (i != 0)
				(void) putc(' ', dbfp);
			n = *cp++;
			cp1 = cp + n;
			if (cp1 > cp2)
				cp1 = cp2;
			(void) putc('"', dbfp);
			j = 0;
			while (cp < cp1) {
				if (*cp == '\0') {
					cp = cp1;
					break;
				}
				if (strchr("\n\"\\", *cp))
					(void) putc('\\', dbfp);
				(void) putc(*cp++, dbfp);
				j++;
			}
			if (j == 0 && (type != T_ISDN || i == 0))
				(void) putc('?', dbfp);
			(void) putc('"', dbfp);
		}
		(void) putc('\n', dbfp);
		break;

	case T_SOA:
		(void) fprintf(dbfp, "%s.", cp);
		cp += strlen((char *) cp) + 1;
		(void) fprintf(dbfp, " %s. (\n", cp);
		cp += strlen((char *) cp) + 1;
		NS_GET32(tmpnum, cp);
		(void) fprintf(dbfp, "%s\t\t%u", ignore, tmpnum);
		NS_GET32(tmpnum, cp);
		(void) fprintf(dbfp, " %u", tmpnum);
		NS_GET32(tmpnum, cp);
		(void) fprintf(dbfp, " %u", tmpnum);
		NS_GET32(tmpnum, cp);
		(void) fprintf(dbfp, " %u", tmpnum);
		NS_GET32(tmpnum, cp);
		(void) fprintf(dbfp, " %u )\n", tmpnum);
		break;

	case T_MX:
	case T_AFSDB:
	case T_RT:
		NS_GET16(tmpnum, cp);
		(void) fprintf(dbfp, "%u", tmpnum);
		(void) fprintf(dbfp, " %s.\n", cp);
		break;

	case T_PX:
		NS_GET16(tmpnum, cp);
		(void) fprintf(dbfp, "%u", tmpnum);
		(void) fprintf(dbfp, " %s.", cp);
		cp += strlen((char *) cp) + 1;
		(void) fprintf(dbfp, " %s.\n", cp);
		break;

	case T_TXT:
	case T_X25:
		cp1 = cp + n;
		while (cp < cp1) {
			(void) putc('"', dbfp);
			if ((i = *cp++) != 0) {
				for (j = i; j > 0 && cp < cp1; j--) {
					if (strchr("\n\"\\", *cp))
						(void) putc('\\', dbfp);
					(void) putc(*cp++, dbfp);
				}
			}
			(void) putc('"', dbfp);
			if (cp < cp1)
				(void) putc(' ', dbfp);
		}
		(void) putc('\n', dbfp);
		break;

	case T_NSAP:
		fprintf(dbfp, "%s\n", inet_nsap_ntoa(n, cp, NULL));
		break;

	case T_AAAA: {
		char t[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];

		fprintf(dbfp, "%s\n", inet_ntop(AF_INET6, cp, t, sizeof t));
		break;
	    }

	case T_LOC: {
		char t[255];

		(void) fprintf(dbfp, "%s\n", loc_ntoa(cp, t));
		break;
	    }

	case T_NAPTR: {
		u_int32_t order, preference;

		/* Order */
		NS_GET16(order, cp);
		fprintf(dbfp, "%u", order);

		/* Preference */
		NS_GET16(preference, cp);
		fprintf(dbfp, " %u", preference);

		/* Flags */
		if ((n = *cp++) != 0) {
			fprintf(dbfp, " \"%.*s\"", (int)n, cp);
			cp += n;
		}

		/* Service */
		if ((n = *cp++) != 0) {
			fprintf(dbfp, " \"%.*s\"", (int)n, cp);
			cp += n;
		}

		/* Regexp */
		if ((n = *cp++) != 0) {
			fprintf(dbfp, " \"%.*s\"", (int)n, cp);
			cp += n;
		}

		/* Replacement */
		fprintf(dbfp, " %s.\n", cp);

		break;
	    }
	case T_SRV: {
		u_int priority, weight, port;

		NS_GET16(priority, cp);
		NS_GET16(weight, cp);
		NS_GET16(port, cp);
		fprintf(dbfp, "\t%u %u %u %s.\n",
			priority, weight, port, cp);
		break;
	    }

	case T_WKS:
		fputs(inet_ntoa(ina_get(cp)), dbfp);
		cp += INADDRSZ;
		fputc(' ', dbfp);
		proto = protocolname(*cp);
		cp += sizeof(char);
		(void) fprintf(dbfp, "%s ", proto);
		i = 0;
		while (cp < cdata + n) {
			j = *cp++;
			do {
				if (j & 0200)
					(void) fprintf(dbfp, " %s",
						       servicename(i, proto));
				j <<= 1;
			} while (++i & 07);
		}
		(void) fprintf(dbfp, "\n");
		break;

	case T_MINFO:
	case T_RP:
		(void) fprintf(dbfp, "%s.", cp);
		cp += strlen((char *) cp) + 1;
		(void) fprintf(dbfp, " %s.\n", cp);
		break;

	case T_KEY: {
		char databuf[16+NS_MD5RSA_MAX_BASE64];  /* 16 for slop */
		u_int keyflags;

		/* get & format key flags */
		keyflags = ns_get16(cp);
		(void) fprintf(dbfp, "0x%04x ", keyflags);
		cp += INT16SZ;

		/* protocol id */
		(void) fprintf(dbfp, " %u", *cp++);

		/* algorithm id */
		(void) fprintf(dbfp, " %u ", *cp++);

		/* key itself (which may have zero length) */
		n = b64_ntop(cp, (cp1 + n) - cp, databuf, sizeof databuf);
		if (n < 0)
			fprintf(dbfp, "; BAD BASE64\n");
		else
			fprintf(dbfp, "%s\n", databuf);
		break;
	    }

	case T_SIG: {
		char databuf[16+NS_MD5RSA_MAX_BASE64];  /* 16 for slop */

		/* get & format rr type which signature covers */
		(void) fprintf(dbfp,"%s", p_type(ns_get16((u_char*)cp)));
		cp += INT16SZ;

		/* algorithm id */
		(void) fprintf(dbfp," %d",*cp++);

		/* labels (# of labels in name) */
		(void) fprintf(dbfp," %d",*cp++);

		/* orig time to live (TTL)) */
		(void) fprintf(dbfp," %u", (u_int32_t)ns_get32((u_char*)cp));
		cp += INT32SZ;

		/* expiration time */
		(void) fprintf(dbfp," %s", p_secstodate(ns_get32((u_char*)cp)));
		cp += INT32SZ;

		/* time signed */
		(void) fprintf(dbfp," %s", p_secstodate(ns_get32((u_char*)cp)));
		cp += INT32SZ;
		
		/* Key footprint */
		(void) fprintf(dbfp," %d", ns_get16((u_char*)cp));
		cp += INT16SZ;
		
		/* signer's name */
		(void) fprintf(dbfp, " %s. ", cp);
		cp += strlen((char *) cp) + 1;

		/* signature itself */
		n = b64_ntop(cp, (cdata + n) - cp, databuf, sizeof databuf);
		if (n < 0)
			fprintf (dbfp, "; BAD BASE64\n");
		else
			fprintf (dbfp, "%s\n", databuf);
		break;
	   }

	case T_NXT:
		fprintf(dbfp, "%s.", (char *)cp);
		i = strlen((char *)cp)+1;
		cp += i;
		n -= i;
		for (i=0; i < n*NS_NXT_BITS; i++) 
			if (NS_NXT_BIT_ISSET (i, cp))
				fprintf(dbfp, " %s", p_type(i));
		fprintf(dbfp,"\n");
		break;

	case ns_t_cert: {
		int databufsize = n * 4 / 3 + 4;
		char *databuf = malloc(databufsize);

		if (databuf == NULL)
			panic("cert malloc failed", NULL);

		/* Object id */
		(void) fprintf(dbfp,"%d ", ns_get16((u_char*)cp));
		cp += INT16SZ;
		
		/* Key tag */
		(void) fprintf(dbfp,"%d ", ns_get16((u_char*)cp));
		cp += INT16SZ;
		
		/* Algorithm id */
		(void) fprintf(dbfp,"%d ", (u_char)*cp);
		cp += 1;
		
		n = b64_ntop(cp, n - 2 * INT16SZ - 1, databuf, databufsize);
		if (n < 0)
			panic ("cert b64_ntop failed", NULL);
		fprintf (dbfp, "%s\n", databuf);
		free(databuf);
		break;
	}

	default:
		cp1 = cp + n;
		while (cp < cp1)
			fprintf(dbfp, "0x%02.2X ", *cp++ & 0xFF);
		(void) fprintf(dbfp, "???\n");
	}
	if (ferror(dbfp)) {
		syslog(LOG_ERR, "%s: %m", tmpname);
		cleanup_for_exit();
		exit(XFER_FAIL);
	}
	return (result);
}

#ifdef SHORT_FNAMES
/*
** This routine handles creating temporary files with mkstemp
** in the presence of a 14 char filename system.  Pathconf()
** does not work over NFS.
*/
filenamecpy(char *ddtfile, char *optarg) {
	int namelen, extra, len;
	char *dirname, *filename;

	/* determine the length of filename allowed */
	if((dirname = strrchr(optarg, '/')) == NULL){
		filename = optarg;
	} else {
		*dirname++ = '\0';
		filename = dirname;
	}
	namelen = pathconf(dirname == NULL? "." : optarg, _PC_NAME_MAX);
	if(namelen <= 0)
		namelen = 255;  /* length could not be determined */
	if(dirname != NULL)
		*--dirname = '/';

	/* copy a shorter name if it will be longer than allowed */
	extra = (strlen(filename)+strlen(".XXXXXX")) - namelen;
	if(extra > 0){
		len = strlen(optarg) - extra;
		(void) strncpy(ddtfile, optarg, len);
		ddtfile[len] = '\0';
	} else
		(void) strcpy(ddtfile, optarg);
}
#endif /* SHORT_FNAMES */

DST_KEY *
tsig_key_from_addr(struct in_addr addr) {
	tsig_node *n;
	for (n = HEAD(tsig_list); n != NULL; n = NEXT(n, link))
		if (memcpy(&addr, &n->addr, sizeof(struct in_addr)))
			return n->dst_key;
	return NULL;
}

static u_int32_t 
do_section(ns_msg *handle, ns_sect section, int pflag, FILE *file, int *delete) {
	int n, sflag, rrnum;
	char buf[2048];	/* XXX need to malloc */
	ns_opcode opcode;
	ns_rr rr;
	const unsigned char *cp;
	const unsigned char *eom;
	u_int32_t serial;
	time_t now;

	time(&now);

	/*
	 * Print answer records.
	 */
	sflag = (_res.pfcode & pflag);
	if (_res.pfcode && !sflag)
		return (-1);

	opcode = (ns_opcode)ns_msg_getflag(*handle, ns_f_opcode);
	rrnum = 0;
	serial = -1;
	for (;;) {
		if (ns_parserr(handle, section, rrnum, &rr)) {
			if (errno != ENODEV) {
				fprintf(file, ";; ns_parserr: %s\n",
					strerror(errno));
				return (-1);
			} else if (rrnum > 0 && sflag != 0 &&
				 (_res.pfcode & RES_PRF_HEAD1))
				putc('\n', file);
			return (serial);
		}
		if (rrnum == 0 && sflag != 0 && (_res.pfcode & RES_PRF_HEAD1))
			fprintf(file, ";; %s SECTION:\n",
				p_section(section, opcode));
		if (section == ns_s_qd)
			fprintf(file, ";;\t%s, type = %s, class = %s\n",
				ns_rr_name(rr),
				p_type(ns_rr_type(rr)),
				p_class(ns_rr_class(rr)));
		else {
			int print_record = 1;
			if (rr.type == ns_t_soa) {
			print_record = 0;
				*delete = !*delete;
				cp = ns_rr_rdata(rr);
				eom = cp + ns_rr_rdlen(rr);
				if ((n = dn_skipname(cp, eom)) < 0) {
					rrnum++;
					continue;
				}
				cp += n;
				if ((n = dn_skipname(cp, eom)) < 0) {
					rrnum++;
					continue;
				}
				cp += n;
				NS_GET32(serial, cp);
				switch (++ixfr_soa) {
				case 1:
					final_serial = serial;
					if (soa_buf == NULL) {
						if ((soa_buf = (char *)malloc(2 * PACKETSZ)) == NULL) {
							syslog(LOG_INFO, "malloc(%u) failed", 2 * PACKETSZ);
							return(-1);
						}
						n = ns_sprintrr(handle, &rr, NULL, NULL,
								soa_buf, 2*PACKETSZ);
						if (n < 0) {
							fprintf(file, ";; ns_sprintrr: %s\n",
								strerror(errno));
							return (-1);
						}
					}
					print_record = 0;
					break;
				case 2:
						fprintf(file,
				"zone:\torigin %s class %s serial %u\n",
						ns_rr_name(rr),
						p_class(ns_rr_class(rr)),
						serial);
					print_record = 0;
					break;
				default:
					print_record = 0;
					break;
				}

			}

			if (print_record) {
				if (rr.type != ns_t_soa) {
				fprintf(file, "update:\t{%s} ",
					*delete ? "delete" : "add");

				n = ns_sprintrr(handle, &rr, NULL, NULL,
						buf, sizeof buf);
				if (n < 0) {
					fprintf(file, ";; ns_sprintrr: %s\n",
						strerror(errno));
					return(-1);
				}
				fputs(buf, file);
					fputc('\n', file);
					}
			}

		}
		rrnum++;
	}
	return (serial);
}

static int
ixfr_log(const u_char *msg, int len, int  *delete, FILE *file,
	 struct sockaddr_in *sin, char *domain, u_int32_t *serial_no,
	 int *first_rr)
{
	ns_msg handle;
	ns_type type;
	ns_class class;
	ns_opcode opcode;
	ns_rcode rcode;
	u_int id, n;
	char time[25];
	ns_rr rr;
	char *cp;
	HEADER *hp;

 	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		dprintf(1, "ixfr_log() failed\n");
 		return (-1);
	}	

	if (ns_initparse(msg, len, &handle) < 0) {
		fprintf(file, ";; ns_initparse: %s\n", strerror(errno));
		dprintf(1, "ixfr_log() failed\n");
		return (-1);
	}
	opcode = (ns_opcode) ns_msg_getflag(handle, ns_f_opcode);
	rcode = (ns_rcode) ns_msg_getflag(handle, ns_f_rcode);
	id = ns_msg_id(handle);

	if (ns_parserr(&handle, ns_s_an, 0, &rr))
	{

		(void) fprintf(file,"ns_parserr() failed");
		dprintf(1, "ixfr_log() failed\n");
		return (-1);
	}
	type = (ns_type)rr.type;
	class = (ns_class)rr.rr_class;

	if (*first_rr == 1) {
		gettime(&tt);
		(void) fprintf(file,"%s", LogSignature);
		sprintf(time, "at %lu", (u_long)tt.tv_sec);
		fprintf(file,
	       "[IXFR_UPDATE] id %u from [%s].%d %s (named-xfer pid %ld):\n",
			id, inet_ntoa(sin->sin_addr),
			ntohs(sin->sin_port), time, (long)getpid());
		(*first_rr)++;
	}
	*serial_no = do_section(&handle, ns_s_an, RES_PRF_ANS, file, delete);
	return (1);
}

static const char *
tsig_rcode(int rcode) {
	static char buffer[64];

	switch (rcode) {
	case ns_r_badkey:
	case ns_r_badsig:
	case ns_r_badtime:
		sprintf(buffer, "message had %s set", p_rcode(rcode));
		return (buffer);
	case -ns_r_badkey:
	case -ns_r_badsig:
	case -ns_r_badtime:
		return (p_rcode(-rcode));
	case NS_TSIG_ERROR_NO_TSIG:
		return ("no TSIG present");
	default:
		break;
	}
	return ("FORMERR");
}

