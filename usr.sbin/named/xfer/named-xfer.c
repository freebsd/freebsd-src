/*-
 * Copyright (c) 1988, 1990 The Regents of the University of California.
 * All rights reserved.
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
 *
 * The original version of xfer by Kevin Dunlap.
 * Completed and integrated with named by David Waitzman
 *	(dwaitzman@bbn.com) 3/14/88.
 * Modified by M. Karels and O. Kure 10-88.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1988, 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)named-xfer.c	4.18 (Berkeley) 3/7/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/signal.h>

#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <resolv.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>

#define XFER			/* modifies the ns.h include file */
#include "ns.h"
#include "pathnames.h"

char	*savestr();

/* max length of data in RR data field */
#define MAXDATA		2048	/* from db.h */
 
int	debug = 0; 
int	quiet = 0;
int	read_interrupted = 0;
struct	zoneinfo zones;		/* zone information */
struct	timeval	tt;

static	char ddtfilename[] = _PATH_TMPXFER;
static	char *ddtfile = ddtfilename;
static	char *tmpname;
FILE	*fp = 0, *ddt, *dbfp;
char	*domain;			/* domain being xfered */
int	domain_len;			/* strlen(domain) */

extern	int errno;

main(argc, argv)
	int argc;
	char *argv[];
{
	register struct zoneinfo *zp;
	register struct hostent *hp;
 	char *dbfile = NULL, *tracefile = NULL, *tm = NULL;
	int dbfd, ddtd, result, c;
	u_long serial_no = 0;
	extern char *optarg;
	extern int optind, getopt();
	u_short port = htons(NAMESERVER_PORT);

	(void) umask(022);
#ifdef LOG_DAEMON
	openlog("named-xfer", LOG_PID|LOG_CONS, LOG_DAEMON);
#else
	openlog("named-xfer", LOG_PID);
#endif
	while ((c = getopt(argc, argv, "d:l:s:t:z:f:p:P:q")) != EOF)
	    switch (c) {
		case 'd':
			debug = atoi(optarg);
			break;
		case 'l':
			ddtfile = (char *)malloc(strlen(optarg) +
			    sizeof(".XXXXXX") + 1);
			(void) strcpy(ddtfile, optarg);
			(void) strcat(ddtfile, ".XXXXXX");
			break;
		case 's':
			serial_no = (u_long) atol(optarg);
			break;
		case 't':
			tracefile = optarg;
			break;
		case 'z':		/* zone == domain */
			domain = optarg;
			domain_len = strlen(domain);
			break;
		case 'f':
			dbfile = optarg;
			tmpname = (char *)malloc((unsigned)strlen(optarg) +
			    sizeof(".XXXXXX") + 1);
			(void) strcpy(tmpname, optarg);
			break;
		case 'p':
			port = htons((u_short)atoi(optarg));
			break;
		case 'P':
			port = (u_short)atoi(optarg);
			break;
		case 'q':
			quiet++;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
	    }

	if (!domain || !dbfile || optind >= argc) {
		usage();
		/* NOTREACHED */
	}	    
	if (tracefile && (fp = fopen(tracefile, "w")) == NULL)
		perror(tracefile);
	(void) strcat(tmpname, ".XXXXXX");
	/* tmpname is now something like "/etc/named/named.bu.db.XXXXXX" */
	if ((dbfd = mkstemp(tmpname)) == -1) {
		perror(tmpname);
		if (!quiet)
			syslog(LOG_ERR, "can't make tmpfile (%s): %m\n",
			    tmpname);
		exit(XFER_FAIL);
	}
	if (fchmod(dbfd, 0644) == -1) {
		perror(tmpname);
		if (!quiet)
			syslog(LOG_ERR, "can't fchmod tmpfile (%s): %m\n",
			    tmpname);
		exit(XFER_FAIL);
	}
	if ((dbfp = fdopen(dbfd, "r+")) == NULL) {
		perror(tmpname);
		if (!quiet)
			syslog(LOG_ERR, "can't fdopen tmpfile (%s)", tmpname);
		exit(XFER_FAIL);
	}
#ifdef DEBUG
	if (debug) {
		/* ddtfile is now something like "/usr/tmp/xfer.ddt.XXXXXX" */
		if ((ddtd = mkstemp(ddtfile)) == -1) {
			perror(ddtfile);
			debug = 0;
		} else if (fchmod(ddtd, 0644) == -1) {
			perror(ddtfile);
			debug = 0;
		} else if ((ddt = fdopen(ddtd, "w")) == NULL) {
			perror(ddtfile);
			debug = 0;
		} else {
#if defined(SYSV)
			setvbuf(ddt, NULL, _IOLBF, BUFSIZ);
#else
			setlinebuf(ddt);
#endif
		}
	}
#endif
	/*
	 * Ignore many types of signals that named (assumed to be our parent)
	 * considers important- if not, the user controlling named with
	 * signals usually kills us.
	 */
	(void) signal(SIGHUP, SIG_IGN);
	(void) signal(SIGSYS, SIG_IGN);
	if (debug == 0) {
		(void) signal(SIGINT, SIG_IGN);
		(void) signal(SIGQUIT, SIG_IGN);
	}
	(void) signal(SIGIOT, SIG_IGN);

#if defined(SIGUSR1) && defined(SIGUSR2)
	(void) signal(SIGUSR1, SIG_IGN);
	(void) signal(SIGUSR2, SIG_IGN);
#else	SIGUSR1&&SIGUSR2
	(void) signal(SIGEMT, SIG_IGN);
	(void) signal(SIGFPE, SIG_IGN);
#endif SIGUSR1&&SIGUSR2

#ifdef DEBUG
	if (debug) (void)fprintf(ddt, "domain `%s' file `%s' ser no %lu \n", 
	      domain, dbfile,serial_no);
#endif
	buildservicelist();
	buildprotolist();

	/* init zone data */
 
	zp = &zones;
	zp->z_type = Z_SECONDARY;
	zp->z_origin = domain;
	zp->z_source = dbfile;
	zp->z_addrcnt = 0;
#ifdef DEBUG
	if (debug) {
		(void)fprintf(ddt,"zone found (%d): ", zp->z_type);
		if (zp->z_origin[0] == '\0')
			(void)fprintf(ddt,"'.'");
		else
			(void)fprintf(ddt,"'%s'", zp->z_origin);
		(void)fprintf(ddt,", source = %s\n", zp->z_source);
	}
#endif
	for (; optind != argc; optind++,zp->z_addrcnt++) {
		tm = argv[optind];
		zp->z_addr[zp->z_addrcnt].s_addr = inet_addr(tm);
 
		if (zp->z_addr[zp->z_addrcnt].s_addr == (unsigned)-1) {
			hp = gethostbyname(tm);
			if (hp == NULL) {
				syslog(LOG_ERR, "uninterpretable server %s\n",
					tm);
				continue;
			}
			bcopy(hp->h_addr,
			    (char *)&zp->z_addr[zp->z_addrcnt].s_addr,
			    sizeof(zp->z_addr[zp->z_addrcnt].s_addr));
#ifdef DEBUG
			if (debug)
				(void)fprintf(ddt,", %s",tm);
#endif
		}
		if (zp->z_addrcnt >= NSMAX) {
			zp->z_addrcnt = NSMAX;
#ifdef DEBUG
			if (debug)
			    (void)fprintf(ddt, "\nns.h NSMAX reached\n");
#endif
			break;
		}
	}
#ifdef DEBUG
	if (debug) (void)fprintf(ddt," (addrcnt) = %d\n", zp->z_addrcnt);
#endif

	_res.options &= ~(RES_DEFNAMES | RES_DNSRCH | RES_RECURSE);
	result = getzone(zp, serial_no, port);
	(void) fclose(dbfp);
	switch (result) {

	case XFER_SUCCESS:			/* ok exit */
		if (rename(tmpname, dbfile) == -1) {
			perror("rename");
			if (!quiet)
			    syslog(LOG_ERR, "rename %s to %s: %m",
				tmpname, dbfile);
			exit(XFER_FAIL);
		}
		exit(XFER_SUCCESS);

	case XFER_UPTODATE:		/* the zone was already uptodate */
		(void) unlink(tmpname);
		exit(XFER_UPTODATE);

	case XFER_TIMEOUT:
#ifdef DEBUG
		if (!debug)
#endif
		    (void) unlink(tmpname);
		exit(XFER_TIMEOUT);		/* servers not reachable exit */

	case XFER_FAIL:
	default:
#ifdef DEBUG
		if (!debug)
#endif
		    (void) unlink(tmpname);
		exit(XFER_FAIL);		/* yuck exit */
	}
}
 
usage()
{
	(void)fprintf(stderr,
"Usage: xfer\n\
\t-z zone_to_transfer\n\
\t-f db_file\n\
\t-s serial_no\n\
\t[-d debug_level]\n\
\t[-l debug_log_file (default %s)]\n\
\t[-t trace_file]\n\
\t[-p port]\n\
\tservers...\n", ddtfile);
	exit(XFER_FAIL);
}

int	minimum_ttl = 0, got_soa = 0;
char	prev_origin[MAXDNAME];
char	prev_dname[MAXDNAME];

getzone(zp, serial_no, port)
	struct zoneinfo *zp;
	u_long serial_no;
	u_short port;
{
	HEADER *hp;
	u_short len;
	u_long serial;
	int s, n, l, cnt, soacnt, error = 0;
 	u_char *cp, *nmp, *eom, *tmp ;
	u_char *buf = NULL;
	int bufsize;
	u_char name[MAXDNAME], name2[MAXDNAME];
	struct sockaddr_in sin;
	struct zoneinfo zp_start, zp_finish;
	struct itimerval ival, zeroival;
	extern SIG_FN read_alarm();
	struct sigvec sv, osv;
	int ancount, aucount;
#ifdef DEBUG
	if (debug)
		(void)fprintf(ddt,"getzone() %s\n", zp->z_origin);
#endif
	bzero((char *)&zeroival, sizeof(zeroival));
	ival = zeroival;
	ival.it_value.tv_sec = 120;
	sv.sv_handler = read_alarm;
	sv.sv_onstack = 0;
	sv.sv_mask = ~0;
	(void) sigvec(SIGALRM, &sv, &osv);

	strcpy(prev_origin, zp->z_origin);

	for (cnt = 0; cnt < zp->z_addrcnt; cnt++) {
		error = 0;
		if (buf == NULL) {
			if ((buf = (u_char *)malloc(2 * PACKETSZ)) == NULL) {
				syslog(LOG_ERR, "malloc(%u) failed",
				    2 * PACKETSZ);
				error++;
				break;
			}
			bufsize = 2 * PACKETSZ;
		}
		bzero((char *)&sin, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = port;
		sin.sin_addr = zp->z_addr[cnt];
		if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			syslog(LOG_ERR, "socket: %m");
			error++;
			break;
		}	
#ifdef DEBUG
		if (debug >= 2) {
			(void)fprintf(ddt,"connecting to server #%d %s, %d\n",
			   cnt+1, inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
		}
#endif
		if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
			(void) close(s);
			error++;
#ifdef DEBUG
			if (debug >= 2)
				(void)fprintf(ddt, "connect failed, %s\n",
					strerror(errno));
#endif
			continue;
		}	
		if ((n = res_mkquery(QUERY, zp->z_origin, C_IN,
		    T_SOA, (char *)NULL, 0, NULL, (char *)buf, bufsize)) < 0) {
			if (!quiet)
			    syslog(LOG_ERR, "zone %s: res_mkquery T_SOA failed",
				zp->z_origin);
			(void) close(s);
			(void) sigvec(SIGALRM, &osv, (struct sigvec *)0);
			return XFER_FAIL;
		}
		/*
		 * Send length & message for zone transfer
		 */
		if (writemsg(s, buf, n) < 0) {
			(void) close(s);
			error++;
#ifdef DEBUG
			if (debug >= 2)
				(void)fprintf(ddt,"writemsg failed\n");
#endif
			continue;	
		}
		/*
		 * Get out your butterfly net and catch the SOA
		 */
		cp = buf;
		l = sizeof(u_short);
		read_interrupted = 0;
		while (l > 0) {
#ifdef DEBUG
			if (debug > 10) (void)fprintf(ddt,"Before setitimer\n");
#endif
			(void) setitimer(ITIMER_REAL, &ival,
			    (struct itimerval *)NULL);
#ifdef DEBUG
			if (debug > 10) (void)fprintf(ddt,"Before recv(l = %d)\n",n);
#endif
			errno = 0;
			if ((n = recv(s, (char *)cp, l, 0)) > 0) {
				cp += n;
				l -= n;
			} else {
#ifdef DEBUG
				if (debug > 10)
				(void)fprintf(ddt,
"bad recv->%d, errno= %d, read_interrupt=%d\n", n, errno, read_interrupted);
#endif
				if (n == -1 && errno == EINTR 
				    && !read_interrupted)
					continue;
				error++;
				break;
			}
		}

		(void) setitimer(ITIMER_REAL, &zeroival,
		    (struct itimerval *)NULL);
		if (error) {
			(void) close(s);
			continue;
		}
		if ((len = htons(*(u_short *)buf)) == 0) {
			(void) close(s);
			continue;
		}
		if (len > bufsize) {
			if ((buf = (u_char *)realloc(buf, len)) == NULL) {
				syslog(LOG_ERR,
			  "malloc(%u) failed for SOA from server %s, zone %s\n",
				    len, inet_ntoa(sin.sin_addr), zp->z_origin);
				(void) close(s);
				continue;
			}
			bufsize = len;
		}
		l = len;
		cp = buf;
		while (l > 0) {
			(void) setitimer(ITIMER_REAL, &ival,
			    (struct itimerval *)NULL);
			errno = 0;
			if ((n = recv(s, (char *)cp, l, 0)) > 0) {
				cp += n;
				l -= n;
			} else {
				if (errno == EINTR && !read_interrupted)
					continue;
				error++;
#ifdef DEBUG
				if (debug > 10)
				    (void)fprintf(ddt,
					    "recv failed: n= %d, errno = %d\n",
					    n, errno);
#endif
				break;
			}
		}
		(void) setitimer(ITIMER_REAL, &zeroival,
		    (struct itimerval *)NULL);
		if (error) {
			(void) close(s);
			continue;
		}
#ifdef DEBUG
		if (debug >= 3) {
			(void)fprintf(ddt,"len = %d\n", len);
			fp_query(buf, ddt);
		}
#endif DEBUG
		hp = (HEADER *) buf;
		ancount = ntohs(hp->ancount);
		aucount = ntohs(hp->nscount);

		/*
		 * close socket if:
		 *  1) rcode != NOERROR
		 *  2) not an authority response
		 *  3) both the number of answers and authority count < 1)
		 */
		if (hp->rcode != NOERROR || !(hp->aa) || 
		    (ancount < 1 && aucount < 1)) {
			if (!quiet)
				syslog(LOG_ERR,
	    "%s from %s, zone %s: rcode %d, aa %d, ancount %d, aucount %d\n",
				    "bad response to SOA query",
				    inet_ntoa(sin.sin_addr), zp->z_origin,
				    hp->rcode, hp->aa, ancount, aucount);
#ifdef DEBUG
			if (debug)
				fprintf(ddt,
	    "%s from %s, zone %s: rcode %d, aa %d, ancount %d, aucount %d\n",
				    "bad response to SOA query",
				    inet_ntoa(sin.sin_addr), zp->z_origin,
				    hp->rcode, hp->aa, ancount, aucount);
#endif DEBUG
			(void) close(s);
			error++;
			continue;
		}
		zp_start = *zp;
		if (len < sizeof(HEADER) + QFIXEDSZ) {
	badsoa:
			if (!quiet)
				syslog(LOG_ERR,
				  "malformed SOA from %s, zone %s: too short\n",
				    inet_ntoa(sin.sin_addr), zp->z_origin);
#ifdef DEBUG
			if (debug)
				fprintf(ddt,
				    "malformed SOA from %s: too short\n",
				    inet_ntoa(sin.sin_addr));
#endif DEBUG
			(void) close(s);
			error++;
			continue;
		}
		tmp = buf + sizeof(HEADER);
		eom = buf + len;
		if ((n = dn_skipname(tmp, eom)) == -1)
			goto badsoa;
		tmp += n + QFIXEDSZ;
		if ((n = dn_skipname(tmp, eom)) == -1)
			goto badsoa;
		tmp += n;
		if (soa_zinfo(&zp_start, tmp, eom) == -1)
			goto badsoa;
		if (zp_start.z_serial > serial_no || serial_no == 0) {
#ifdef DEBUG
		    if (debug)
			(void)fprintf(ddt, "need update, serial %d\n",
			    zp_start.z_serial);
#endif DEBUG
		    hp = (HEADER *) buf;
		    soacnt = 0;
		    for (;;) {
			if (soacnt == 0) {
			    if ((n = res_mkquery(QUERY, zp->z_origin, C_IN,
			      T_AXFR, (char *)NULL, 0, NULL,
			      (char *)buf, bufsize)) < 0) {
				if (!quiet)
				    syslog(LOG_ERR,
					"zone %s: res_mkquery T_AXFR failed",
					zp->z_origin);
				(void) close(s);
				(void) sigvec(SIGALRM, &osv,
				    (struct sigvec *)0);
				return XFER_FAIL;
			    }
			    /*
			     * Send length & message for zone transfer
			     */
			    if (writemsg(s, buf, n) < 0) {
				    (void) close(s);
				    error++;
#ifdef DEBUG
				    if (debug >= 2)
					    (void)fprintf(ddt,"writemsg failed\n");
#endif
				    break;	
			    }
			}
			/*
			 * Receive length & response
			 */
			cp = buf;
			l = sizeof(u_short);
			/* allow extra time for the fork on first read */
			if (soacnt == 0)
				ival.it_value.tv_sec = 300;
			while (l > 0) {
				(void) setitimer(ITIMER_REAL, &ival,
				    (struct itimerval *)NULL);
				errno = 0;
				if ((n = recv(s, (char *)cp, l, 0)) > 0) {
					cp += n;
					l -= n;
				} else {
					if (errno == EINTR && !read_interrupted)
						continue;
					error++;
#ifdef DEBUG
					if (debug >= 2)
					  (void)fprintf(ddt,
					    "recv failed: n= %d, errno = %d\n",
					    n, errno);
#endif
					break;
				}
			}
			if (soacnt == 0)
				ival.it_value.tv_sec = 120;
			(void) setitimer(ITIMER_REAL, &zeroival,
			    (struct itimerval *)NULL);
			if (error)
				break;
			if ((len = htons(*(u_short *)buf)) == 0)
				break;
			l = len;
			cp = buf;
			eom = buf + len;
			while (l > 0) {
				(void) setitimer(ITIMER_REAL, &ival,
				    (struct itimerval *)NULL);
				errno = 0;
				if ((n = recv(s, (char *)cp, l, 0)) > 0) {
					cp += n;
					l -= n;
				} else {
					if (errno == EINTR && !read_interrupted)
						continue;
					error++;
#ifdef DEBUG
					if (debug >= 2)
						(void)fprintf(ddt,"recv failed\n");
#endif
					break;
				}
			}
			(void) setitimer(ITIMER_REAL, &zeroival,
			    (struct itimerval *)NULL);
			if (error)
				break;
#ifdef DEBUG
			if (debug >= 3) {
				(void)fprintf(ddt,"len = %d\n", len);
				fp_query(buf, ddt);
			}
			if (fp) fp_query(buf,fp);
#endif
			if (len < sizeof(HEADER)) {
		badrec:
				error++;
				if (!quiet)
					syslog(LOG_ERR,
					  "record too short from %s, zone %s\n",
					  inet_ntoa(sin.sin_addr),
					  zp->z_source);
#ifdef DEBUG
				if (debug)
					fprintf(ddt,
					    "record too short from %s\n",
				    inet_ntoa(sin.sin_addr));
#endif DEBUG
				break;
			}
			cp = buf + sizeof(HEADER);
			if (hp->qdcount) {
				if ((n = dn_skipname(cp, eom)) == -1 ||
				    n + QFIXEDSZ >= eom - cp)
					goto badrec;
				cp += n + QFIXEDSZ;
			}
			nmp = cp;
			if ((n = dn_skipname(cp, eom)) == -1)
				goto badrec;
			tmp = cp + n;

			n = print_output(buf, bufsize, cp);
			if (cp + n != eom) {
#ifdef DEBUG
			   if (debug)
			   (void)fprintf(ddt,
				     "getzone: print_update failed (%d, %d)\n",
					cp - buf, n);
#endif
				error++;
				break;
			}
			GETSHORT(n, tmp);
			if (n == T_SOA) {
				if (soacnt == 0) {
					soacnt++;
					if (dn_expand(buf, buf + 512, nmp,
					    name, sizeof(name)) == -1)
						goto badsoa;
					if (eom - tmp <= 2 * sizeof(u_short) +
					    sizeof(u_long))
						goto badsoa;
					tmp += 2 * sizeof(u_short)
						+ sizeof(u_long);
					if ((n = dn_skipname(tmp, eom)) == -1)
						goto badsoa;
					tmp += n;
					if ((n = dn_skipname(tmp, eom)) == -1)
						goto badsoa;
					tmp += n;
					if (eom - tmp <= sizeof(u_long))
						goto badsoa;
					GETLONG(serial, tmp);
#ifdef DEBUG
					if (debug > 2)
					    (void)fprintf(ddt,
					        "first SOA for %s, serial %d\n",
					        name, serial);
#endif DEBUG
					continue;
				}
				if (dn_expand(buf, buf + 512, nmp, name2, 
				    sizeof(name2)) == -1)
					goto badsoa;
				if (strcasecmp((char *)name, (char *)name2) != 0) {
#ifdef DEBUG
					if (debug > 1)
					    (void)fprintf(ddt,
					      "extraneous SOA for %s\n",
					      name2);
#endif DEBUG
					continue;
				}
				tmp -= sizeof(u_short);
				if (soa_zinfo(&zp_finish, tmp, eom) == -1)
					goto badsoa;
#ifdef DEBUG
				if (debug > 1)
				    (void)fprintf(ddt,
				      "SOA, serial %d\n", zp_finish.z_serial);
#endif DEBUG
				if (serial != zp_finish.z_serial) {
					soacnt = 0;
					got_soa = 0;
					minimum_ttl = 0;
					strcpy(prev_origin, zp->z_origin);
					prev_dname[0] = 0;
#ifdef DEBUG
					if (debug)
					    (void)fprintf(ddt,
						"serial changed, restart\n");
#endif DEBUG
					/*
					 * Flush buffer, truncate file
					 * and seek to beginning to restart.
					 */
					fflush(dbfp);
					if (ftruncate(fileno(dbfp), 0) != 0) {
						if (!quiet)
						    syslog(LOG_ERR,
							"ftruncate %s: %m\n",
							tmpname);
						return(XFER_FAIL);
					}
					fseek(dbfp, 0L, 0);
				} else
					break;
			}
		}
		(void) close(s);
		if (error == 0) {
			(void) sigvec(SIGALRM, &osv, (struct sigvec *)0);
			return XFER_SUCCESS;
		}
#ifdef DEBUG
		if (debug >= 2)
		  (void)fprintf(ddt,"error receiving zone transfer\n");
#endif
	    } else {
		(void) close(s);
#ifdef DEBUG
		if (debug)
		    (void)fprintf(ddt,
		      "zone up-to-date, serial %d\n", zp_start.z_serial);
#endif DEBUG
		return XFER_UPTODATE;
	    }
	}
	(void) sigvec(SIGALRM, &osv, (struct sigvec *)0);
	if (error)
		return XFER_TIMEOUT;
	return XFER_FAIL;
}

/*
 * Set flag saying to read was interrupted
 * used for a read timer
 */
SIG_FN
read_alarm()
{
	extern int read_interrupted;
	read_interrupted = 1;
}
 
writemsg(rfd, msg, msglen)
	int rfd;
	u_char *msg;
	int msglen;
{
	struct iovec iov[2];
	u_short len = htons((u_short)msglen);
 
	iov[0].iov_base = (caddr_t)&len;
	iov[0].iov_len = sizeof(len);
	iov[1].iov_base = (caddr_t)msg;
	iov[1].iov_len = msglen;
	if (writev(rfd, iov, 2) != sizeof(len) + msglen) {
#ifdef DEBUG
	    if (debug)
	      (void)fprintf(ddt,"write failed %d\n", errno);
#endif
	    return (-1);
	}
	return (0);
}


soa_zinfo(zp, cp, eom)
	register struct zoneinfo *zp;
	register u_char *cp;
	u_char *eom;
{
	register int n;

	if (eom - cp < 3 * sizeof(u_short) + sizeof(u_long))
		return (-1);
	cp += 3 * sizeof(u_short) + sizeof(u_long);
	if ((n = dn_skipname(cp, eom)) == -1)
		return (-1);
	cp += n;
	if ((n = dn_skipname(cp, eom)) == -1)
		return (-1);
	cp += n;
	if (eom - cp < 5 * sizeof(u_long))
		return (-1);
	GETLONG(zp->z_serial, cp);
	GETLONG(zp->z_refresh, cp);
	gettime(&tt);
	zp->z_time = tt.tv_sec + zp->z_refresh;
	GETLONG(zp->z_retry, cp);
	GETLONG(zp->z_expire, cp);
	GETLONG(zp->z_minimum, cp);
	return (0);
}

gettime(ttp)
struct timeval *ttp;
{
	if (gettimeofday(ttp, (struct timezone *)0) < 0)
		syslog(LOG_ERR, "gettimeofday failed: %m");
}

/*
 * Parse the message, determine if it should be printed, and if so, print it
 * in .db file form.
 * Does minimal error checking on the message content.
 */
print_output(msg, msglen, rrp)
	u_char *msg;
	int msglen;
	u_char *rrp;
{
	register u_char *cp;
	register HEADER *hp = (HEADER *) msg;
	u_long addr, ttl;
	int i, j, tab, result, class, type, dlen, n1;
	long n;
	u_char *cp1, data[BUFSIZ];
	u_char *temp_ptr;	/* used to get ttl for RR */
	char *cdata, *origin, *proto, dname[MAXDNAME];
	extern char *inet_ntoa(), *protocolname(), *servicename();

	cp = rrp;
	if ((n = dn_expand(msg, msg + msglen, cp, (u_char *) dname,
		    sizeof(dname))) < 0) {
		hp->rcode = FORMERR;
		return (-1);
	}
	cp += n;
	GETSHORT(type, cp);
	GETSHORT(class, cp);
	GETLONG(ttl, cp);
	GETSHORT(dlen, cp);

	origin = index(dname, '.');
	if (origin == NULL)
		origin = "";
	else
		origin++;	/* move past the '.' */
#ifdef DEBUG
	if (debug > 2)
		(void) fprintf(ddt, "print_output: dname %s type %d class %d ttl %d\n",
		    dname, type, class, ttl);
#endif
	/*
	 * Convert the resource record data into the internal database format.
	 */
	switch (type) {
	case T_A:
	case T_WKS:
	case T_HINFO:
	case T_UINFO:
	case T_TXT:
	case T_UID:
	case T_GID:
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
		if ((n = dn_expand(msg, msg + msglen, cp, data,
			    sizeof(data))) < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 = data;
		n = strlen((char *) data) + 1;
		break;

	case T_MINFO:
	case T_SOA:
		if ((n = dn_expand(msg, msg + msglen, cp, data,
			    sizeof(data))) < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 = data + (n = strlen((char *) data) + 1);
		n1 = sizeof(data) - n;
		if (type == T_SOA)
			n1 -= 5 * sizeof(u_long);
		if ((n = dn_expand(msg, msg + msglen, cp, cp1, n1)) < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += strlen((char *) cp1) + 1;
		if (type == T_SOA) {
			temp_ptr = cp + 4 * sizeof(u_long);
			GETLONG(minimum_ttl, temp_ptr);
			bcopy((char *) cp, (char *) cp1,
			    n = 5 * sizeof(u_long));
			cp += n;
			cp1 += n;
		}
		n = cp1 - data;
		cp1 = data;
		break;

	case T_MX:
		/* grab preference */
		bcopy((char *) cp, (char *) data, sizeof(u_short));
		cp1 = data + sizeof(u_short);
		cp += sizeof(u_short);

		/* get name */
		if ((n = dn_expand(msg, msg + msglen, cp, cp1,
			    sizeof(data) - sizeof(u_short))) < 0)
			return (-1);
		cp += n;

		/* compute end of data */
		cp1 += strlen((char *) cp1) + 1;
		/* compute size of data */
		n = cp1 - data;
		cp1 = data;
		break;

	default:
#ifdef DEBUG
		if (debug >= 3)
			(void) fprintf(ddt, "unknown type %d\n", type);
#endif
		return ((cp - rrp) + dlen);
	}
	if (n > MAXDATA) {
#ifdef DEBUG
		if (debug)
			(void) fprintf(ddt,
			    "update type %d: %d bytes is too much data\n",
			    type, n);
#endif
		hp->rcode = NOCHANGE;	/* XXX - FORMERR ??? */
		return (-1);
	}
	cdata = (char *) cp1;
	result = cp - rrp;

	/*
	 * Only print one SOA per db file
	 */
	if (type == T_SOA) {
		if (got_soa)
			return result;
		else
			got_soa++;
	}
	/*
	 * If the origin has changed, print the new origin
	 */
	if (strcasecmp(prev_origin, origin)) {
		(void) strcpy(prev_origin, origin);
		(void) fprintf(dbfp, "$ORIGIN %s.\n", origin);
	}
	tab = 0;

	if (strcasecmp(prev_dname, dname)) {
		/*
		 * set the prev_dname to be the current dname, then cut off all
		 * characters of dname after (and including) the first '.'
		 */
		char *cutp = index(dname, '.');

		(void) strcpy(prev_dname, dname);
		if (cutp)
			*cutp = NULL;

		if (dname[0] == 0) {
			if (origin[0] == 0)
				(void) fprintf(dbfp, ".\t");
			else
				(void) fprintf(dbfp, ".%s.\t", origin);	/* ??? */
		} else
			(void) fprintf(dbfp, "%s\t", dname);
		if (strlen(dname) < 8)
			tab = 1;
	} else {
		(void) putc('\t', dbfp);
		tab = 1;
	}

	if (ttl != 0 && ttl != minimum_ttl)
		(void) fprintf(dbfp, "%d\t", (int) ttl);
	else if (tab)
		(void) putc('\t', dbfp);

	(void) fprintf(dbfp, "%s\t%s\t", p_class(class), p_type(type));
	cp = (u_char *) cdata;

	/*
	 * Print type specific data
	 */
	switch (type) {

	case T_A:
		switch (class) {
		case C_IN:
		case C_HS:
			GETLONG(n, cp);
			n = htonl(n);
			(void) fprintf(dbfp, "%s",
			    inet_ntoa(*(struct in_addr *) & n));
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
		cp = (u_char *) cdata;
		if (cp[0] == '\0')
			(void) fprintf(dbfp, ".\t");
		else
			(void) fprintf(dbfp, "%s.", cp);
		(void) fprintf(dbfp, "\n");
		break;

	case T_HINFO:
		if (n = *cp++) {
			(void) fprintf(dbfp, "\"%.*s\"", (int) n, cp);
			cp += n;
		} else
			(void) fprintf(dbfp, "\"\"");
		if (n = *cp++)
			(void) fprintf(dbfp, " \"%.*s\"", (int) n, cp);
		else
			(void) fprintf(dbfp, "\"\"");
		(void) putc('\n', dbfp);
		break;

	case T_SOA:
		(void) fprintf(dbfp, "%s.", cp);
		cp += strlen((char *) cp) + 1;
		(void) fprintf(dbfp, " %s. (\n", cp);
		cp += strlen((char *) cp) + 1;
		GETLONG(n, cp);
		(void) fprintf(dbfp, "\t\t%lu", n);
		GETLONG(n, cp);
		(void) fprintf(dbfp, " %lu", n);
		GETLONG(n, cp);
		(void) fprintf(dbfp, " %lu", n);
		GETLONG(n, cp);
		(void) fprintf(dbfp, " %lu", n);
		GETLONG(n, cp);
		(void) fprintf(dbfp, " %lu )\n", n);
		break;

	case T_MX:
		GETSHORT(n, cp);
		(void) fprintf(dbfp, "%lu", n);
		(void) fprintf(dbfp, " %s.\n", cp);
		break;

	case T_TXT:
		cp1 = cp + n;
		(void) putc('"', dbfp);
		while (cp < cp1) {
			if (i = *cp++) {
				for (j = i ; j > 0 && cp < cp1 ; j--)
					if (*cp == '\n') {
						(void) putc('\\', dbfp);
						(void) putc(*cp++, dbfp);
					} else
						(void) putc(*cp++, dbfp);
			}
		}
		(void) fputs("\"\n", dbfp);
		break;

	case T_UINFO:
		(void) fprintf(dbfp, "\"%s\"\n", cp);
		break;

	case T_UID:
	case T_GID:
		if (n == sizeof(u_long)) {
			GETLONG(n, cp);
			(void) fprintf(dbfp, "%lu\n", n);
		}
		break;

	case T_WKS:
		GETLONG(addr, cp);
		addr = htonl(addr);
		(void) fprintf(dbfp, "%s ",
		    inet_ntoa(*(struct in_addr *) & addr));
		proto = protocolname(*cp);
		cp += sizeof(char);
		(void) fprintf(dbfp, "%s ", proto);
		i = 0;
		while (cp < (u_char *) cdata + n) {
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
		(void) fprintf(dbfp, "%s.", cp);
		cp += strlen((char *) cp) + 1;
		(void) fprintf(dbfp, " %s.\n", cp);
		break;

	default:
		(void) fprintf(dbfp, "???\n");
	}
	if (ferror(dbfp)) {
		syslog(LOG_ERR, "%s: %m", tmpname);
		exit(XFER_FAIL);
	}
	return result;
}

/*
 * Make a copy of a string and return a pointer to it.
 */
char *
savestr(str)
	char *str;
{
	char *cp;

	cp = (char *)malloc((unsigned)strlen(str) + 1);
	if (cp == NULL) {
		syslog(LOG_ERR, "savestr: %m");
		exit(XFER_FAIL);
	}
	(void) strcpy(cp, str);
	return (cp);
}
