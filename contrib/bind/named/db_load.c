#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)db_load.c	4.38 (Berkeley) 3/2/91";
static char rcsid[] = "$Id: db_load.c,v 8.31 1996/12/18 04:09:48 vixie Exp $";
#endif /* not lint */

/*
 * ++Copyright++ 1986, 1988, 1990
 * -
 * Copyright (c) 1986, 1988, 1990
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
 * -
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
 * -
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
 * --Copyright--
 */

/*
 * Load data base from ascii backupfile.  Format similar to RFC 883.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <syslog.h>
#include <time.h>

#include "named.h"

#define ALLOW_LONG_TXT_RDATA

static int		gettoken __P((register FILE *, const char *)),
			getnonblank __P((FILE *, const char *)),
			getprotocol __P((FILE *, const char *)),
			getservices __P((int, char *, FILE *, const char *)),
			getcharstring __P((char *, char *, int, int, int, FILE *, const char *));
static void		makename __P((char *, const char *));
static int		makename_ok __P((char *name, const char *origin,
					 int class,
					 enum transport transport,
					 enum context context,
					 const char *owner,
					 const char *filename, int lineno));
static int		getmlword __P((char *, int, FILE *, int));
static int		getallwords __P((char *, int, FILE *, int));
static u_int32_t	wordtouint32 __P((char *));
static u_int32_t	datetosecs __P((char *, int *));

static int		wordtouint32_error = 0;
static int		empty_token = 0;
static int		getmlword_nesting = 0;

int	getnum_error;

/*
 * Parser token values
 */
#define CURRENT	1
#define DOT	2
#define AT	3
#define DNAME	4
#define INCLUDE	5
#define ORIGIN	6
#define ERROR	7

static int clev;	/* a zone deeper in a hierachy has more credability */

#define MAKENAME_OK(N)	if (!makename_ok(N, origin, class, \
					 transport, context, \
					 domain, filename, lineno)) { \
				errs++; \
				sprintf(buf, "bad name \"%s\"", N); \
				goto err; \
			}

/* int
 * db_load(filename, in_origin, zp, def_domain)
 *	load a database from `filename' into zone `zp'.  append `in_origin'
 *	to all nonterminal domain names in the file.  `def_domain' is the
 *	default domain for include files or NULL for zone base files.
 * returns:
 *	-1 = can't open file
 *	0 = success
 *	>0 = number of errors encountered
 */
int
db_load(filename, in_origin, zp, def_domain)
	const char *filename, *in_origin;
	struct zoneinfo *zp;
	const char *def_domain;
{
	static int read_soa, read_ns, rrcount;
	register char *cp;
	char domain[MAXDNAME];
	char origin[MAXDNAME];
	char tmporigin[MAXDNAME];
	char buf[MAXDATA];
	char data[MAXDATA];
	const char *op;
	int c, someclass, class, type, dbflags, dataflags, multiline;
	u_int32_t ttl;
	struct databuf *dp;
	FILE *fp;
	int slineno, i, errs, didinclude;
	register u_int32_t n;
	struct stat sb;
	struct in_addr ina;
	int escape;
	enum transport transport;
	enum context context;
	u_int32_t sig_type;
	u_int32_t keyflags;
	int success;
	int dateerror;
#ifdef DO_WARN_SERIAL
	u_int32_t serial;
#endif

/* Simple macro for setting error messages about RR's being parsed,
   before jumping to err label.  If no SETERR is done, the last word
   scanned into "buf" by getword will be printed.  */
#define SETERR(x)	strcpy (buf, x);

	switch (zp->z_type) {
	case Z_PRIMARY:
	case Z_CACHE:
		transport = primary_trans;
		break;
	case Z_SECONDARY:
	case Z_STUB:
		transport = secondary_trans;
		break;
	default:
		transport = response_trans; /*guessing*/
		break;
	}
	errs = 0;
	didinclude = 0;
	if (!def_domain) {
		/* This is not the result of a $INCLUDE. */
		rrcount = 0;
		read_soa = 0;
		read_ns = 0;
		clev = db_getclev(in_origin);
	}

	dprintf(1, (ddt,"db_load(%s, %s, %d, %s)\n",
		    filename, in_origin, zp - zones,
		    def_domain ? def_domain : "Nil"));

	(void) strcpy(origin, in_origin);
	if ((fp = fopen(filename, "r")) == NULL) {
		syslog(LOG_WARNING, "%s: %m", filename);
		dprintf(1, (ddt, "db_load: error opening file %s\n",
			    filename));
		return (-1);
	}
	if (zp->z_type == Z_CACHE) {
		dbflags = DB_NODATA | DB_NOHINTS;
		dataflags = DB_F_HINT;
#ifdef STUBS
	} else if (zp->z_type == Z_STUB && clev == 0) {
		dbflags = DB_NODATA | DB_NOHINTS;
		dataflags = DB_F_HINT;
#endif
	} else {
		dbflags = DB_NODATA;
		dataflags = 0;
	}
	gettime(&tt);
	if (fstat(fileno(fp), &sb) < 0) {
		syslog(LOG_WARNING, "%s: %m", filename);
		sb.st_mtime = (int)tt.tv_sec;
	}
	slineno = lineno;
	lineno = 1;
	if (def_domain)
		strcpy(domain, def_domain);
	else
		domain[0] = '\0';
	class = zp->z_class;
	zp->z_flags &= ~(Z_INCLUDE|Z_DB_BAD);
 	while ((c = gettoken(fp, filename)) != EOF) {
		switch (c) {
		case INCLUDE:
			if (!getword((char *)buf, sizeof(buf), fp, 0))
				/* file name*/
				break;
			if (!getword(tmporigin, sizeof(tmporigin), fp, 1))
				strcpy(tmporigin, origin);
			else {
				makename(tmporigin, origin);
				endline(fp);
			}
			didinclude = 1;
			errs += db_load((char *)buf, tmporigin, zp, domain);
			continue;

		case ORIGIN:
			(void) strcpy((char *)buf, origin);
			if (!getword(origin, sizeof(origin), fp, 1))
				break;
			dprintf(3, (ddt, "db_load: origin %s, buf %s\n",
				    origin, buf));
			makename(origin, buf);
			dprintf(3, (ddt, "db_load: origin now %s\n", origin));
			continue;

		case DNAME:
			if (!getword(domain, sizeof(domain), fp, 1))
				break;
			n = strlen(domain) - 1;
			if (domain[n] == '.')
				domain[n] = '\0';
			else if (*origin) {
				(void) strcat(domain, ".");
				(void) strcat(domain, origin);
			}
			goto gotdomain;

		case AT:
			(void) strcpy(domain, origin);
			goto gotdomain;

		case DOT:
			domain[0] = '\0';
			/* FALLTHROUGH */
		case CURRENT:
		gotdomain:
			if (!getword((char *)buf, sizeof(buf), fp, 0)) {
				if (c == CURRENT)
					continue;
				break;
			}
			cp = buf;
			ttl = USE_MINIMUM;
			if (isdigit(*cp)) {
				n = 0;
				do {
				    if (n > (INT_MAX - (*cp - '0')) / 10) {
					syslog(LOG_INFO, 
					   "%s: line %d: number > %lu\n",
					   filename, lineno, (u_long)INT_MAX);
					n = INT_MAX;
					cp++;
				    } else
  					n = n * 10 + (*cp++ - '0');
				}
				while (isdigit(*cp));
				if (zp->z_type == Z_CACHE) {
				    /* this allows the cache entry to age */
				    /* while sitting on disk (powered off) */
				    if (n > max_cache_ttl)
					n = max_cache_ttl;
				    n += sb.st_mtime;
				}
				ttl = n;
				if (!getword((char *)buf, sizeof(buf), fp, 0))
					break;
			}

			/* Parse class (IN, etc) */
			someclass = sym_ston(__p_class_syms,
					     (char *)buf, &success);
			if (success && someclass != C_ANY) {
				class = someclass;
				(void) getword((char *)buf,
					       sizeof(buf), fp, 0);
			}

			/* Parse RR type (A, MX, etc) */
			type = sym_ston(__p_type_syms,
					(char *)buf, &success);
			if ((!success) || type == T_ANY) {
			    dprintf(1, (ddt, "%s: Line %d: Unknown type: %s.\n",
					    filename, lineno, buf));
				errs++;
			    syslog(LOG_INFO, "%s: Line %d: Unknown type: %s.\n",
					filename, lineno, buf);
				break;
			}

			context = ns_ownercontext(type, transport);
			if (!ns_nameok(domain, class, transport, context,
				       domain, inaddr_any)) {
				errs++;
				syslog(LOG_NOTICE,
				       "%s:%d: owner name error\n",
				       filename, lineno);
				break;
			}

			context = domain_ctx;
			switch (type) {
#ifdef ALLOW_T_UNSPEC
			case T_UNSPEC:
#endif
			case T_KEY:
			case T_SIG:
				/* Don't do anything here for these types --
				   they read their own input separately later */
				goto dont_get_word;

			case T_SOA:
			case T_MINFO:
			case T_RP:
			case T_NS:
			case T_CNAME:
			case T_MB:
			case T_MG:
			case T_MR:
			case T_PTR:
				escape = 1;
				break;
			default:
				escape = 0;
			}
			    if (!getword((char *)buf, sizeof(buf), fp, escape))
				break;
			    dprintf(3,
				    (ddt,
				     "d='%s', c=%d, t=%d, ttl=%d, data='%s'\n",
				     domain, class, type, ttl, buf));
			/*
			 * Convert the ascii data 'buf' to the proper format
			 * based on the type and pack into 'data'.
			 */
	dont_get_word:
			switch (type) {
			case T_A:
				if (!inet_aton(buf, &ina))
					goto err;
				n = ntohl(ina.s_addr);
				cp = data;
				PUTLONG(n, cp);
				n = INT32SZ;
				break;

			case T_SOA:
				context = hostname_ctx;
				goto soa_rp_minfo;
			case T_RP:
			case T_MINFO:
				context = mailname_ctx;
				/* FALLTHROUGH */
			soa_rp_minfo:
				(void) strcpy((char *)data, (char *)buf);

				MAKENAME_OK(data);
				cp = data + strlen((char *)data) + 1;
				if (!getword((char *)cp,
					     (sizeof data) - 
					     (cp - (char *)data),
					     fp, 1))
					goto err;
				if (type == T_RP)
					context = domain_ctx;
				else
					context = mailname_ctx;
				MAKENAME_OK(cp);
				cp += strlen((char *)cp) + 1;
				if (type != T_SOA) {
					n = cp - (char *)data;
					break;
				}
				if (class != zp->z_class) {
					errs++;
					syslog(LOG_INFO,
					       "%s:%d: %s",
					       filename, lineno,
					       "SOA class not same as zone's");
				}
				if (strcasecmp(zp->z_origin, domain) != 0) {
					errs++;
					syslog(LOG_ERR,
			"%s: line %d: SOA for \"%s\" not at zone top \"%s\"",
					       filename, lineno, domain,
					       zp->z_origin);
				}
				c = getnonblank(fp, filename);
				if (c == '(') {
					multiline = 1;
				} else {
					multiline = 0;
					ungetc(c, fp);
				}
#ifdef DO_WARN_SERIAL
				serial = zp->z_serial;
#endif
				zp->z_serial = getnum(fp, filename,
						      GETNUM_SERIAL);
				if (getnum_error)
					errs++;
				n = (u_int32_t) zp->z_serial;
				PUTLONG(n, cp);
#ifdef DO_WARN_SERIAL
				if (serial && SEQ_GT(serial, zp->z_serial)) {
					syslog(LOG_NOTICE,
			"%s:%d: WARNING: new serial number < old (%lu < %lu)",
						filename , lineno,
						zp->z_serial, serial);
				}
#endif
				zp->z_refresh = getnum(fp, filename,
						       GETNUM_NONE);
				if (getnum_error) {
					errs++;
					zp->z_refresh = INIT_REFRESH;
				}
				n = (u_int32_t) zp->z_refresh;
				PUTLONG(n, cp);
				if (zp->z_type == Z_SECONDARY
#if defined(STUBS) 
				    || zp->z_type == Z_STUB
#endif
				    ) {
					ns_refreshtime(zp, MIN(sb.st_mtime,
							       tt.tv_sec));
				}
				zp->z_retry = getnum(fp, filename,
						     GETNUM_NONE);
				if (getnum_error) {
					errs++;
					zp->z_retry = INIT_REFRESH;
				}
				n = (u_int32_t) zp->z_retry;
				PUTLONG(n, cp);
				zp->z_expire = getnum(fp, filename,
						      GETNUM_NONE);
				if (getnum_error) {
					errs++;
					zp->z_expire = INIT_REFRESH;
				}
				n = (u_int32_t) zp->z_expire;
				PUTLONG (n, cp);
				zp->z_minimum = getnum(fp, filename,
						       GETNUM_NONE);
				if (getnum_error) {
					errs++;
					zp->z_minimum = 120;
				}
				n = (u_int32_t) zp->z_minimum;
				PUTLONG (n, cp);
				n = cp - (char *)data;
				if (multiline) {
					if (getnonblank(fp, filename) != ')')
						goto err;
					endline(fp);
				}
                                read_soa++;
				if (zp->z_type != Z_PRIMARY)
					break;
				/* sanity checks PRIMARY ONLY */
				/*
				 * sanity: give enough time for the
				 * zone to transfer (retry)
				 */
				if (zp->z_expire < 
					(zp->z_refresh+zp->z_retry)) {
				    syslog(LOG_NOTICE,
    "%s: WARNING SOA expire value is less then SOA refresh + retry (%lu < %lu + %lu)",
				    filename, zp->z_expire, zp->z_refresh,
				    zp->z_retry);
				}
				/* BIND specific */
				if (zp->z_expire < maint_interval) {
				    syslog(LOG_NOTICE,
    "%s: WARNING SOA expire value is less then maintainance interval (%lu < %lu)",
				    filename, zp->z_expire, maint_interval);
				}
				/* BIND Specific */
				if (zp->z_refresh < maint_interval) {
				    syslog(LOG_WARNING,
    "%s: WARNING SOA refresh value is less then maintainance interval (%lu < %lu)",
				    filename, zp->z_refresh, maint_interval);
				}
				/* BIND specific */
				if (zp->z_retry < maint_interval) {
				    syslog(LOG_WARNING,
    "%s: WARNING SOA retry value is less then maintainance interval (%lu < %lu)",
				    filename, zp->z_retry, maint_interval);
				}
				/* sanity */
				if (zp->z_expire < 
					(zp->z_refresh  + 10 * zp->z_retry)) {
				    syslog(LOG_WARNING,
    "%s: WARNING SOA expire value is less then refresh + 10 * retry (%lu < (%lu + 10 * %lu))",
				    filename, zp->z_expire, zp->z_refresh,
				    zp->z_retry);
				}
				/*
				 * sanity: most harware/telco faults are
				 * detected and fixed within a week,
				 * secondaries should continue to
				 * operate for this time.
				 * (minimum of 4 days for long weekends)
				 */
				if (zp->z_expire < (7 * 24 * 3600)) {
				    syslog(LOG_WARNING,
    "%s: WARNING SOA expire value is less then 7 days (%lu)",
				    filename, zp->z_expire);
				}
				/*
				 * sanity: maximum down time
				 * if we havn't talked for six months 
				 * war must have broken out
				 */
				if (zp->z_expire > ( 183 * 24 * 3600)) {
				    syslog(LOG_WARNING,
    "%s: WARNING SOA expire value is greater then 6 months (%lu)",
				    filename, zp->z_expire);
				}
				/* sanity */
				if (zp->z_refresh < (zp->z_retry * 2)) {
				    syslog(LOG_WARNING,
    "%s: WARNING SOA refresh value is less than 2 * retry (%lu < %lu * 2)",
				    filename, zp->z_refresh, zp->z_retry);
				}
				break;

			case T_UID:
			case T_GID:
				n = 0;
				cp = buf;
				while (isdigit(*cp))
					n = n * 10 + (*cp++ - '0');
				if (cp == buf)
					goto err;
				cp = data;
				PUTLONG(n, cp);
				n = INT32SZ;
				break;

			case T_WKS:
				/* Address */
				if (!inet_aton(buf, &ina))
					goto err;
				n = ntohl(ina.s_addr);
				cp = data;
				PUTLONG(n, cp);
				*cp = (char)getprotocol(fp, filename);
				/* Protocol */
				n = INT32SZ + sizeof(char);
				/* Services */
				n = getservices((int)n, data, fp, filename);
				break;

			case T_NS:
				if (strcasecmp(zp->z_origin, domain) == 0)
					read_ns++;
				context = hostname_ctx;
				goto cname_etc;
			case T_CNAME:
			case T_MB:
			case T_MG:
			case T_MR:
				context = domain_ctx;
				goto cname_etc;
			case T_PTR:
				context = ns_ptrcontext(domain);
			cname_etc:
				(void) strcpy((char *)data, (char *)buf);
				MAKENAME_OK(data);
				n = strlen((char *)data) + 1;
				break;

			case T_UINFO:
				cp = strchr((char *)buf, '&');
				bzero(data, sizeof data);
				if ( cp != NULL) {
					(void) strncpy((char *)data,
					    (char *)buf, cp - buf);
					op = strchr(domain, '.');
					if ( op != NULL)
					    (void) strncat((char *)data,
						domain,op-domain);
					else
						(void) strcat((char *)data,
						    domain);
					(void) strcat((char *)data,
					    (char *)++cp);
				} else
					(void) strcpy((char *)data,
					    (char *)buf);
				n = strlen((char *)data) + 1;
				break;

			case T_NAPTR:
			/* Order Preference Flags Service Replacement Regexp */
				n = 0;
				cp = buf;
				/* Order */
				while (isdigit(*cp))
					n = n * 10 + (*cp++ - '0');
				/* catch bad values */
				if ((cp == buf) || (n > 65535))
					goto err;
				cp = data;
				PUTSHORT((u_int16_t)n, cp);
				/* Preference */
				n = getnum(fp, filename, GETNUM_NONE);
				if (getnum_error || n > 65536)
					goto err;
				PUTSHORT((u_int16_t)n, cp);

                                /* Flags */
                                if (!getword((char *)buf, sizeof(buf), fp, 0))
                                        goto err;
                                n = strlen((char *)buf);
                                *cp++ = n;
                                bcopy(buf, (char *)cp, (int)n);
                                cp += n;
 
                                /* Service Classes */
                                if (!getword((char *)buf, sizeof(buf), fp, 0))
                                        goto err;
                                n = strlen((char *)buf);
                                *cp++ = n;
                                bcopy(buf, (char *)cp, (int)n);
                                cp += n;
 
                                /* Pattern */
                                if (!getword((char *)buf, sizeof(buf), fp, 0))
                                        goto err;
                                n = strlen((char *)buf);
                                *cp++ = n;
                                bcopy(buf, (char *)cp, (int)n);
                                cp += n;

				/* Replacement */
				if (!getword((char *)buf, sizeof(buf), fp, 1))
					goto err;
				(void) strcpy((char *)cp, (char *)buf);
				context = domain_ctx;
				MAKENAME_OK(cp);
				/* advance pointer to end of data */
				cp += strlen((char *)cp) +1;

				/* now save length */
				n = (cp - (char *)data);
				break;

			case T_MX:
			case T_AFSDB:
			case T_RT:
			case T_SRV:
				n = 0;
				cp = buf;
				while (isdigit(*cp))
					n = n * 10 + (*cp++ - '0');
				/* catch bad values */
				if ((cp == buf) || (n > 65535))
					goto err;
				cp = data;
				PUTSHORT((u_int16_t)n, cp);

				if (type == T_SRV) {
					n = getnum(fp, filename, GETNUM_NONE);
					if (getnum_error || n > 65536)
						goto err;
					PUTSHORT((u_int16_t)n, cp);

					n = getnum(fp, filename, GETNUM_NONE);
					if (getnum_error || n > 65536)
						goto err;
					PUTSHORT((u_int16_t)n, cp);
				}

				if (!getword((char *)buf, sizeof(buf), fp, 1))
					goto err;
				(void) strcpy((char *)cp, (char *)buf);
				context = hostname_ctx;
				MAKENAME_OK(cp);
				/* advance pointer to end of data */
				cp += strlen((char *)cp) +1;

				/* now save length */
				n = (cp - (char *)data);
				break;

			case T_PX:
				context = domain_ctx;
				n = 0;
				data[0] = '\0';
				cp = buf;
				while (isdigit(*cp))
					n = n * 10 + (*cp++ - '0');
				/* catch bad values */
				if ((cp == buf) || (n > 65535))
					goto err;
				cp = data;
				PUTSHORT((u_int16_t)n, cp);

				if (!getword((char *)buf, sizeof(buf), fp, 0))
					goto err;
				(void) strcpy((char *)cp, (char *)buf);
				MAKENAME_OK(cp);
				/* advance pointer to next field */
				cp += strlen((char *)cp) +1;
				if (!getword((char *)buf, sizeof(buf), fp, 0))
					goto err;
				(void) strcpy((char *)cp, (char *)buf);
				MAKENAME_OK(cp);
				/* advance pointer to end of data */
				cp += strlen((char *)cp) + 1;

				/* now save length */
				n = (cp - (char *)data);
				break;

			case T_HINFO:
				n = getcharstring(buf, data, type, 2, 2, fp, filename);
				if (n == 0)
					goto err;
				break;

			case T_ISDN:
				n = getcharstring(buf, data, type, 1, 2, fp, filename);
				if (n == 0)
					goto err;
				break;

			case T_TXT:
				n = getcharstring(buf, data, type, 1, 0, fp, filename);
				if (n == 0)
					goto err;
				break;

			case T_X25:
				n = getcharstring(buf, data, type, 1, 1, fp, filename);
				if (n == 0)
					goto err;
				break;

			case T_NSAP:
				n = inet_nsap_addr(buf, (u_char *)data,
						   sizeof data);
				if (n == 0)
					goto err;
				endline(fp);
				break;
			case T_AAAA:
				if (inet_pton(AF_INET6, buf, data) <= 0)
					goto err;
				n = IN6ADDRSZ;
				endline(fp);
				break;

			case T_KEY: {
	/* The KEY record looks like this in the db file:
	 *	Name  Cl KEY Flags  Proto  Algid  PublicKeyData
	 * where:
	 *	Name,Cl per usual
	 *	KEY	RR type
	 *	Flags	4 digit hex value (unsigned_16)
	 *	Proto	8 bit u_int
	 *	Algid	8 bit u_int
	 *	PublicKeyData
	 *		a string of base64 digits,
	 *		skipping any embedded whitespace.
	 */
				u_int32_t al, pr;
				int nk, klen;
				char *expstart;
				u_int expbytes, modbytes;

				i = 0;
				data[i] = '\0';
				cp = data;
				getmlword_nesting = 0; /* KLUDGE err recov. */
			/*>>> Flags (unsigned_16)  */
				if (!getmlword((char*)buf, sizeof(buf), fp, 0)
				    ) {
					SETERR("No flags field");
					goto err;
				}
				keyflags = wordtouint32(buf);
				if (wordtouint32_error || 0xFFFF < keyflags)
					goto err;
				if (keyflags & KEYFLAG_RESERVED_BITMASK) {
					SETERR("Reserved flag bits are set");
					goto err;
				}
				PUTSHORT(keyflags, cp);

			/*>>> Protocol (8-bit decimal) */
				if (!getmlword((char*)buf, sizeof(buf), fp, 0)
				    ) {
					SETERR("No protocol field");
					goto err;
				}
				pr = wordtouint32(buf);
				if (wordtouint32_error || 255 < pr)
					goto err;
				*cp++ = (u_char) pr;

			/*>>> Algorithm id (8-bit decimal) */
				if (!getmlword((char*)buf, sizeof(buf), fp, 0)
				    ) {
					SETERR("No algorithm ID")
					goto err;
				}
				al = wordtouint32(buf);
				if (wordtouint32_error ||
				    0 == al || 255 == al || 255 < al)
					goto err;
				*cp++ = (u_char) al;

			/*>>> Public Key data is in BASE64.
			 *	We don't care what algorithm it uses or what
			 *	the internal structure of the BASE64 data is.
			 */
				if (!getallwords((char *)buf, MAXDATA, fp, 0))
					klen = 0;
				else {
					/* Convert from BASE64 to binary. */
					klen = b64_pton(buf, (u_char*)cp,
						    sizeof data - 
						    (cp - (char *)data));
					if (klen < 0)
						goto err;
				}

				/* set total length */
				n = cp + klen - (char *)data;

			/*
			 * Now check for valid key flags & algs & etc,
			 * from the RFC.
			 */

				if (keyflags & (KEYFLAG_ZONEKEY | KEYFLAG_IPSEC
				    | KEYFLAG_EMAIL))
					pr |= 1;	/* A nonzero proto. */
				if (KEYFLAG_TYPE_NO_KEY ==
				    (keyflags & KEYFLAG_TYPEMASK))
					nk = 1;		/* No-key */
				else
					nk = 0;		/* have a key */
                              if ((keyflags & KEYFLAG_ZONEKEY) && 
                                  (KEYFLAG_TYPE_CONF_ONLY ==
                                   (keyflags & KEYFLAG_TYPEMASK))) {
                                      /* Zone key must have Authentication bit
                                       set  ogud@tis.com */ 
                                      SETERR("Zonekey needs authentication bit");
					goto err;
				}

				if (al == 0 && nk == 0) {
					SETERR("Key specified, but no alg");
					goto err;
				}
				if (al != 0 && pr == 0) {
					SETERR("Alg specified, but no protos");
					goto err;
				}

				if (nk == 1 && klen != 0) {
					SETERR("No-key flags set but key fnd");
					goto err;
				}

				if (nk == 0 && klen == 0) {
					SETERR("Key type spec'd, but no key");
					goto err;
				}

				/* Check algorithm-ID and key structure, for
				   the algorithm-ID's that we know about. */
				switch (al) {
				case ALGORITHM_MD5RSA:
					if (klen == 0)
						break;
					expstart = cp;
					expbytes = *expstart++;
					if (expbytes == 0)
						GETSHORT(expbytes, expstart);

					if (expbytes < 1) {
						SETERR("Exponent too short");
						goto err;
					}
					if (expbytes >
					    (MAX_MD5RSA_KEY_PART_BITS + 7) / 8
					    ) {
						SETERR("Exponent too long");
						goto err;
					}
					if (*expstart == 0) {
						SETERR("Exponent starts w/ 0");
						goto err;
					}

					modbytes = klen -
						(expbytes + (expstart - cp));
					if (modbytes < 
					    (MIN_MD5RSA_KEY_PART_BITS + 7) / 8
					    ) {
						SETERR("Modulus too short");
						goto err;
					}
					if (modbytes > 
					    (MAX_MD5RSA_KEY_PART_BITS + 7) / 8
					    ) {
						SETERR("Modulus too long");
						goto err;
					}
					if (*(expstart+expbytes) == 0) {
						SETERR("Modulus starts w/ 0");
						goto err;
					}
					break;

				case ALGORITHM_EXPIRE_ONLY:
					if (klen != 0) {
						SETERR(
				     "Key provided for expire-only algorithm");
						goto err;
					}
					break;
				case ALGORITHM_PRIVATE_OID:
					if (klen == 0) {
						SETERR("No ObjectID in key");
						goto err;
					}
					break;
				}

				endline(fp);  /* flush the rest of the line */
				break;
			    } /*T_KEY*/
		  
		        case T_SIG:
	{
		/* The SIG record looks like this in the db file:
		   Name Cl SIG RRtype Algid [OTTL] Texp Tsig Kfoot Signer Sig
		     
		   where:  Name and Cl are as usual
			   SIG     is a keyword
			   RRtype  is a char string 
			   ALGid   is  8 bit u_int
			   OTTL    is 32 bit u_int (optionally present)
			   Texp    is YYYYMMDDHHMMSS
			   Tsig    is YYYYMMDDHHMMSS
			   Kfoot   is 16-bit unsigned decimal integer
			   Signer  is a char string
			   Sig     is 64 to 319 base-64 digits
		   A missing OTTL is detected by the magnitude of the Texp value
		   that follows it, which is larger than any u_int.
		   The Labels field in the binary RR does not appear in the
		   text RR.

		   It's too crazy to run these pages of SIG code at the right
		   margin.  I'm exdenting them for readability.
		 */
		int siglen;
		u_int32_t al;
		u_int32_t signtime, exptime, timetilexp;
		u_int32_t origTTL;
		time_t now;

		/* The TTL gets checked against the Original TTL,
		   and bounded by the signature expiration time, which 
		   are both under the signature.  We can't let TTL drift
		   based on the SOA record.  If defaulted, fix it now. 
		   (It's not clear to me why USE_MINIMUM isn't eliminated
		   before putting ALL RR's into the database.  -gnu@toad.com) */
		if (ttl == USE_MINIMUM)
			ttl = zp->z_minimum;

		i = 0;
		data[i] = '\0';
		getmlword_nesting = 0; /* KLUDGE err recovery */

		/* RRtype (char *) */
		if (!getmlword((char*)buf, sizeof(buf), fp, 0)) {
			SETERR("SIG record doesn't specify type");
			goto err;
		}
		sig_type = sym_ston(__p_type_syms, (char *)buf, &success);
		if (!success || sig_type == T_ANY) {
			/*
			 * We'll also accept a numeric RR type,
			 * for signing RR types that this version
			 * of named doesn't yet understand.
			 * In the T_ANY case, we rely on wordtouint32
			 * to fail when scanning the string "ANY".
			 */
			sig_type = wordtouint32 (buf);
			if (wordtouint32_error || sig_type > 0xFFFF) {
				SETERR("Unknown RR type in SIG record");
				goto err;
			}
		}
		cp = &data[i];
		PUTSHORT((u_int16_t)sig_type, cp);
		i += 2;

		/* Algorithm id (8-bit decimal) */
		if (!getmlword((char *)buf, sizeof(buf), fp, 0)) {
			SETERR("Missing algorithm ID");
			goto err;
		}
		al = wordtouint32(buf);
		if (0 == al || wordtouint32_error || 255 <= al)
			goto err;
		data[i] = (u_char) al;
		i++;

		/*
		 * Labels (8-bit decimal)
		 *	Not given in the file.  Must compute.
		 */
		n = dn_count_labels(domain);
		if (0 >= n || 255 < n) {
			SETERR ("SIG label count invalid");
			goto err;
		}
		data[i] = (u_char) n;
		i++;

		/*
		 * OTTL (optional u_int32_t) and
		 * Texp (u_int32_t date)
		 */
		if (!getmlword((char *)buf, sizeof(buf), fp, 0)) {
			SETERR("OTTL and expiration time missing");
			goto err;
		}
		/*
		 * See if OTTL is missing and this is a date.
		 * This relies on good, silent error checking
		 * in datetosecs.
		 */
		exptime = datetosecs(buf, &dateerror);
		if (!dateerror) {
			/* Output TTL as OTTL */
			origTTL = ttl;
			cp = &data[i];
			PUTLONG (origTTL, cp);
			i += 4;
		} else {
			/* Parse and output OTTL; scan TEXP */
			origTTL = wordtouint32(buf);
			if (0 >= origTTL || wordtouint32_error ||
			    (origTTL > 0x7fffffff))
				goto err;
			cp = &data[i];
			PUTLONG(origTTL, cp);
			i += 4;
			if (!getmlword((char *)buf, sizeof(buf), fp, 0)) {
				SETERR ("Expiration time missing");
				goto err;
			}
			exptime = datetosecs(buf, &dateerror);
		}
 		if (dateerror || exptime > 0x7fffffff || exptime <= 0) {
			SETERR("Invalid expiration time");
			goto err;
		}
		cp = &data[i];
		PUTLONG(exptime, cp);
		i += 4;

		/* Tsig (u_int32_t) */
		if (!getmlword((char *)buf, sizeof(buf), fp, 0)) {
			SETERR("Missing signature time");
		 	goto err;
		}
		signtime = datetosecs(buf, &dateerror);
		if (0 == signtime || dateerror) {
			SETERR("Invalid signature time");
			goto err;
		}
		cp = &data[i];
		PUTLONG(signtime, cp);
		i += 4;

		/* Kfootprint (unsigned_16) */
		if (!getmlword((char *)buf, sizeof(buf), fp, 0)) {
			SETERR("Missing key footprint");
		 	goto err;
		}
		n = wordtouint32(buf);
		if (wordtouint32_error || n >= 0x0ffff) {
			SETERR("Invalid key footprint");
			goto err;
		}
		cp = &data[i];
		PUTSHORT((u_int16_t)n, cp);
		i += 2;

		/* Signer's Name */
		if (!getmlword((char*)buf, sizeof(buf), fp, 0)) {
			SETERR("Missing signer's name");
			goto err;
		}
		cp = &data[i];
		strcpy(cp,buf);
		context = domain_ctx;
		MAKENAME_OK(cp);
		i += strlen(cp) + 1;

		/*
		 * Signature (base64 of any length)
		 * We don't care what algorithm it uses or what
		 * the internal structure of the BASE64 data is.
		 */
		if (!getallwords((char *)buf, sizeof(buf), fp, 0)) {
			siglen = 0;
		} else {
			cp = &data[i];
			siglen = b64_pton(buf, (u_char*)cp, sizeof data - i);
			if (siglen < 0)
				goto err;
		}

		/* set total length and we're done! */
		n = i + siglen;

		/*
		 * Check signature time, expiration, and adjust TTL.  Note
		 * that all time values are in GMT (UTC), *not* local time.
		 */

		now = time (0);

		/* Don't let bogus name servers increase the signed TTL */
		if (ttl > origTTL) {
			SETERR("TTL is greater than signed original TTL");
			goto err;
		}

		/* Don't let bogus signers "sign" in the future.  */
		if (signtime > now) {
			SETERR("signature time is in the future");
			goto err;
		}
		
		/* Ignore received SIG RR's that are already expired.  */
		if (exptime <= now) {
			SETERR("expiration time is in the past");
			goto err;
		}

		/* Lop off the TTL at the expiration time.  */
		timetilexp = exptime - now;
		if (timetilexp < ttl) {
			dprintf(1, (ddt,
			       "shrinking expiring %s SIG TTL from %d to %d\n",
				    p_secstodate(exptime), ttl, timetilexp));
			ttl = timetilexp;
		}

		/*
		 * Check algorithm-ID and key structure, for
		 * the algorithm-ID's that we know about.
		 */
		switch (al) {
		case ALGORITHM_MD5RSA:
			if (siglen == 0) {
				SETERR("No key for RSA algorithm");
				goto err;
			}
			if (siglen < 1) {
				SETERR("Signature too short");
				goto err;
			}
			if (siglen > (MAX_MD5RSA_KEY_PART_BITS + 7) / 8) {
				SETERR("Signature too long");
				goto err;
			}
			/* We rely on  cp  from parse */
			if (*cp == 0) {
				SETERR("Signature starts with zeroes");
				goto err;
			}
			break;

		case ALGORITHM_EXPIRE_ONLY:
			if (siglen != 0) {
				SETERR(
				  "Signature supplied to expire-only algorithm"
				       );
				goto err;
			}
			break;
		case ALGORITHM_PRIVATE_OID:
			if (siglen == 0) {
				SETERR("No ObjectID in key");
				goto err;
			}
			break;
		}

		endline(fp);  /* flush the rest of the line */

		break;		/* Accept this RR. */
	}

#ifdef LOC_RR
			case T_LOC:
                                cp = buf + (n = strlen(buf));
				*cp = ' ';
				cp++;
				while ((i = getc(fp), *cp = i, i != EOF)
                                       && *cp != '\n'
                                       && (n < MAXDATA)) {
					cp++; n++;
                                }
                                if (*cp == '\n') /* leave \n for getword */
					ungetc(*cp, fp);
                                *cp = '\0';
				/* now process the whole line */
				n = loc_aton(buf, (u_char *)data);
				if (n == 0)
					goto err;
				endline(fp);
				break;
#endif /* LOC_RR */
#ifdef ALLOW_T_UNSPEC
                        case T_UNSPEC:
                                {
                                    int rcode;
                                    fgets(buf, sizeof(buf), fp);
				    dprintf(1, (ddt, "loading T_UNSPEC\n"));
				    if (rcode = atob(buf,
						     strlen((char*)buf),
						     data, sizeof data,
						     &n)) {
					if (rcode == CONV_OVERFLOW) {
						errs++;
						syslog(LOG_INFO,
				       "Load T_UNSPEC: input buffer overflow");
					} else {
						errs++;
						syslog(LOG_INFO,
				     "Load T_UNSPEC: Data in bad atob format");
					}
                                    }
                                }
                                break;
#endif /* ALLOW_T_UNSPEC */

			default:
				goto err;
			}
#ifndef PURGE_ZONE
#ifdef STUBS
			if (type == T_SOA && zp->z_type == Z_STUB)
				continue;
#endif
#endif
#ifdef NO_GLUE
			/*
			 * Ignore data outside the zone.
			 */
			if (zp->z_type != Z_CACHE &&
			    !samedomain(domain, zp->z_origin))
			{
				syslog(LOG_INFO,
			    "%s:%d: data \"%s\" outside zone \"%s\" (ignored)",
				       filename, lineno, domain, zp->z_origin);
				continue;
			}
#endif /*NO_GLUE*/
			dp = savedata(class, type, (u_int32_t)ttl,
				      (u_char *)data, (int)n);
			dp->d_zone = zp - zones;
			dp->d_flags = dataflags;
			dp->d_cred = DB_C_ZONE;
			dp->d_clev = clev;
			if ((c = db_update(domain, dp, dp, dbflags,
					   (dataflags & DB_F_HINT)
					   ? fcachetab
					   : hashtab))
			    != OK) {
#ifdef DEBUG
				if (debug && (c != DATAEXISTS))
					fprintf(ddt, "update failed %s %d\n", 
						domain, type);
#endif
				db_free(dp);
			} else {
				rrcount++;
			}
			continue;

		case ERROR:
			break;
		}
 err:
		errs++;
		syslog(LOG_NOTICE, "%s: line %d: database format error (%s)",
			filename, empty_token ? (lineno - 1) : lineno, buf);
		if (!empty_token)
			endline(fp);
	}
	(void) my_fclose(fp);
	lineno = slineno;
	if (!def_domain) {
		if (didinclude) {
			zp->z_flags |= Z_INCLUDE;
			zp->z_ftime = 0;
		} else
			zp->z_ftime = sb.st_mtime;
		zp->z_lastupdate = sb.st_mtime;
		if (zp->z_type != Z_CACHE) {
			const char *msg = NULL;

			if (read_soa == 0)
				msg = "no SOA RR found";
			else if (read_soa != 1)
				msg = "multiple SOA RRs found";
			else if (read_ns == 0)
				msg = "no NS RRs found at zone top";
			else if (!rrcount)
				msg = "no relevant RRs found";
			if (msg != NULL) {
				errs++;
				syslog(LOG_WARNING,
				       "Zone \"%s\" (file %s): %s",
				       zp->z_origin, filename, msg);
			}
		}
	}
#ifdef SECURE_ZONES
	build_secure_netlist(zp);
#endif
	if (!def_domain)
		syslog(errs ? LOG_WARNING : LOG_INFO,
		       "%s zone \"%s\" %s (serial %lu)",
		       zoneTypeString(zp), zp->z_origin,
		       errs ? "rejected due to errors" : "loaded",
		       (u_long)zp->z_serial);
	if (errs) {
		zp->z_flags |= Z_DB_BAD;
		zp->z_ftime = 0;
	}
#ifdef BIND_NOTIFY
	/* XXX: this needs to be delayed, both according to the spec, and
	 *	because the metadata needed by sysnotify() (and its sysquery())
	 *	could be in other zones that we (at startup) havn't loaded yet.
	 */
	if (!errs && !def_domain &&
	    (zp->z_type == Z_PRIMARY || zp->z_type == Z_SECONDARY))
		sysnotify(zp->z_origin, zp->z_class, T_SOA);
#endif
	return (errs);
}

static int
gettoken(fp, src)
	register FILE *fp;
	const char *src;
{
	register int c;
	char op[32];

	for (;;) {
		c = getc(fp);
	top:
		switch (c) {
		case EOF:
			return (EOF);

		case '$':
			if (getword(op, sizeof(op), fp, 0)) {
				if (!strcasecmp("include", op))
					return (INCLUDE);
				if (!strcasecmp("origin", op))
					return (ORIGIN);
			}
			syslog(LOG_NOTICE,
			       "%s: line %d: Unknown $ option: $%s\n", 
			       src, lineno, op);
			return (ERROR);

		case ';':
			while ((c = getc(fp)) != EOF && c != '\n')
				;
			goto top;

		case ' ':
		case '\t':
			return (CURRENT);

		case '.':
			return (DOT);

		case '@':
			return (AT);

		case '\n':
			lineno++;
			continue;

		default:
			(void) ungetc(c, fp);
			return (DNAME);
		}
	}
}

/* int
 * getword(buf, size, fp, preserve)
 *	get next word, skipping blanks & comments.
 *	'\' '\n' outside of "quotes" is considered a blank.
 * parameters:
 *	buf - destination
 *	size - of destination
 *	fp - file to read from
 *	preserve - should we preserve \ before \\ and \.?
 * return value:
 *	0 = no word; perhaps EOL or EOF; lineno was incremented.
 *	1 = word was read
 */
int
getword(buf, size, fp, preserve)
	char *buf;
	int size;
	FILE *fp;
	int preserve;
{
	register char *cp = buf;
	register int c, spaceok;

	empty_token = 0;	/* XXX global side effect. */
	while ((c = getc(fp)) != EOF) {
		if (c == ';') {
			/* Comment.  Skip to end of line. */
			while ((c = getc(fp)) != EOF && c != '\n')
				NULL;
			c = '\n';
		}
		if (c == '\n') {
			/*
			 * Unescaped newline.  It's a terminator unless we're
			 * already midway into a token.
			 */
			if (cp != buf)
				ungetc(c, fp);
			else
				lineno++;
			break;
		}
		if (c == '"') {
			/* "Quoted string."  Gather the whole string here. */
			while ((c = getc(fp)) != EOF && c!='"' && c!='\n') {
				if (c == '\\') {
					if ((c = getc(fp)) == EOF)
						c = '\\';
					if (preserve &&
					    (c == '\\' || c == '.')) {
						if (cp >= buf+size-1)
							break;
						*cp++ = '\\';
					}
					if (c == '\n')
						lineno++;
				}
				if (cp >= buf+size-1)
					break;
				*cp++ = c;
			}
			/*
			 * Newline string terminators are
			 * not token terminators.
			 */
			if (c == '\n') {
				lineno++;
				break;
			}
			/* Sample following character, check for terminator. */
			if ((c = getc(fp)) != EOF)
				ungetc(c, fp);
			if (c == EOF || isspace(c)) {
				*cp = '\0';
				return (1);
			}
			continue;
		}
		spaceok = 0;
		if (c == '\\') {
			/* Do escape processing. */
			if ((c = getc(fp)) == EOF)
				c = '\\';
			if (preserve && (c == '\\' || c == '.')) {
				if (cp >= buf+size-1)
					break;
				*cp++ = '\\';
			}
			if (c == ' ' || c == '\t')
				spaceok++;
		}
		if (isspace(c) && !spaceok) {
			/* Blank of some kind.  Skip run. */
			while (isspace(c = getc(fp)) && c != '\n')
				NULL;
			ungetc(c, fp);
			/* Blank means terminator if the token is nonempty. */
			if (cp != buf)		/* Trailing whitespace */
				break;
			continue;		/* Leading whitespace */
		}
		if (cp >= buf+size-1)
			break;
		*cp++ = (char)c;
	}
	*cp = '\0';
	if (cp == buf)
		empty_token = 1;
	return (cp != buf);
}

/* Get multiline words.  Same parameters as getword.  Handles any
   number of leading ('s or )'s in the words it sees.
   FIXME:  We kludge recognition of ( and ) for multiline input.
   Each paren must appear at the start of a (blank-separated) word,
   which is particularly counter-intuitive for ).  Good enough for now,
   until Paul rewrites the parser.
*/
static int
getmlword(buf, size, fp, preserve)
	char *buf;
	int size;
	FILE *fp;
	int preserve;
{
	char *p;
	
	do {
		while (!getword (buf, size, fp, preserve)) {
			/* No more words on this line.  See if doing the
			   multiline thing. */
			if (!getmlword_nesting) {	/* Nope... */
				ungetc('\n', fp);	/* Push back newline */
				lineno--;		/* Unbump the lineno */
				empty_token = 0;	/* Undo this botch */
				return 0;
			}
			if (feof(fp) || ferror(fp))
				return 0;	/* Error, no terminating ')' */
			/* Continue reading til we get a word... */
		}
		while ('(' == *buf) {
			/* Word starts with paren.  Multiline mode.
			   Move the rest of the word down over the paren.  */
			getmlword_nesting++;
			p = buf;
			while (0 != (p[0]=p[1]))  p++;
		}
		while (')' == *buf) {
			getmlword_nesting--;
			p = buf;
			while (0 != (p[0]=p[1]))  p++;
		}
	} while (buf[0] == 0);	/* loop til we get a non-( non-) word */

	return 1;		/* Got a word... */
}

/* Get all the remaining words on a line, concatenated into one big
   long (not too long!) string, with the whitespace squeezed out.
   This routine, like getword(), does not swallow the newline if words seen.
   This routine, unlike getword(), never swallows the newline if no words.
   Parameters are the same as getword().  Result is:
	 0	got no words at all
	 1 	got one or more words
	-1	got too many words, they don't all fit; or missing close paren
*/
static int
getallwords(buf, size, fp, preserve)
	char *buf;
	int size;
	FILE *fp;
	int preserve;
{
	char *runningbuf  = buf;
	int runningsize = size;
	int len;

	while (runningsize > 0) {
		if (!getmlword (runningbuf, runningsize, fp, preserve)) {
			return runningbuf!=buf;		/* 1 or 0 */
		}
		len = strlen(runningbuf);
		runningbuf += len;
		runningsize -= len;
	}
	return -1;			/* Error, String too long */
}

/*
From: kagotani@cs.titech.ac.jp
Message-Id: <9007040716.AA26646@saeko.cs.titech.ac.jp>
Subject: named bug report and fix
Date: Wed, 04 Jul 90 16:16:52 JST

I found a bug in the BIND source code. Named with this bug parses
the serial_no field of SOA records incorrectly. For example:
        expression      internal
        in files        expression      I expect
        1.              1000            10000
        1.2             10002           10002
        1.23            100023          10023
        2.3             20003           20003
Especially I can not accept that "2.3" is treated as if it is
smaller than "1.23" in their internal expressions.

[ if you define SENSIBLE_DOTS in ../conf/options.h, you get
  m. kagotani's expected behaviour.  this is NOT compatible
  with pre-4.9 versions of BIND.  --vix ]
*/

int
getnum(fp, src, opt)
	FILE *fp;
	const char *src;
	int opt;
{
	register int c, n;
	int seendigit = 0;
	int seendecimal = 0;
	int m = 0;
	int allow_dots = 0;

	getnum_error = 0;
#ifdef DOTTED_SERIAL
	if (opt & GETNUM_SERIAL)
		allow_dots++;
#endif
	for (n = 0; (c = getc(fp)) != EOF; ) {
		if (isspace(c)) {
			if (c == '\n')
				lineno++;
			if (seendigit)
				break;
			continue;
		}
		if (c == ';') {
			while ((c = getc(fp)) != EOF && c != '\n')
				;
			if (c == '\n')
				lineno++;
			if (seendigit)
				break;
			continue;
		}
		if (getnum_error)
			continue;
		if (!isdigit(c)) {
			if (c == ')' && seendigit) {
				(void) ungetc(c, fp);
				break;
			}
			if (seendigit && (opt & GETNUM_SCALED) &&
			    strchr("KkMmGg", c) != NULL) {
				switch (c) {
				case 'K': case 'k':
					n *= 1024;
					break;
				case 'M': case 'm':
					n *= (1024 * 1024);
					break;
				case 'G': case 'g':
					n *= (1024 * 1024 * 1024);
					break;
				}
				break;
			}
			if (seendecimal || c != '.' || !allow_dots) {
				syslog(LOG_NOTICE, "%s:%d: expected a number",
				       src, lineno);
				getnum_error = 1;
			} else {
				if (!seendigit)
					n = 1;
#ifdef SENSIBLE_DOTS
				n *= 10000;
#else
				n *= 1000;
#endif
				seendigit = 1;
				seendecimal = 1;
			}
			continue;
		}
#ifdef SENSIBLE_DOTS
		if (seendecimal)
			m = m * 10 + (c - '0');
		else
			n = n * 10 + (c - '0');
#else
		n = n * 10 + (c - '0');
#endif
		seendigit = 1;
	}
	if (getnum_error)
		return (0);
	if (m > 9999) {
		syslog(LOG_INFO, 
		       "%s:%d: number after the decimal point exceeds 9999", 
		       src, lineno);
		getnum_error = 1;
		return (0);
	}
	if (seendecimal) {
		syslog(LOG_INFO,
		       "%s:%d: decimal serial number interpreted as %d",
		       src, lineno, n+m);
	}
	return (n + m);
}

static int
getnonblank(fp, src)
	FILE *fp;
	const char *src;
{
	register int c;

	while ( (c = getc(fp)) != EOF ) {
		if (isspace(c)) {
			if (c == '\n')
				lineno++;
			continue;
		}
		if (c == ';') {
			while ((c = getc(fp)) != EOF && c != '\n')
				;
			if (c == '\n')
				lineno++;
			continue;
		}
		return(c);
	}
	syslog(LOG_INFO, "%s: line %d: unexpected EOF", src, lineno);
	return (EOF);
}

/*
 * Take name and fix it according to following rules:
 * "." means root.
 * "@" means current origin.
 * "name." means no changes.
 * "name" means append origin.
 */
static void
makename(name, origin)
	char *name;
	const char *origin;
{
	int n;

	if (origin[0] == '.')
		origin++;
	n = strlen(name);
	if (n == 1) {
		if (name[0] == '.') {
			name[0] = '\0';
			return;
		}
		if (name[0] == '@') {
			(void) strcpy(name, origin);
			return;
		}
	}
	if (n > 0) {
		if (name[n - 1] == '.')
			name[n - 1] = '\0';
		else if (origin[0] != '\0') {
			name[n] = '.';
			(void) strcpy(name + n + 1, origin);
		}
	}
}

static int
makename_ok(name, origin, class, transport, context, owner, filename, lineno)
	char *name;
	const char *origin;
	int class;
	enum transport transport;
	enum context context;
	const char *owner;
	const char *filename;
	int lineno;
{
	int ret = 1;

	makename(name, origin);
	if (!ns_nameok(name, class, transport, context, owner, inaddr_any)) {
		syslog(LOG_INFO, "%s:%d: database naming error\n",
		       filename, lineno);
		ret = 0;
	}
	return (ret);
}

void
endline(fp)
	register FILE *fp;
{
	register int c;

	while ((c = getc(fp)) != '\0') {
		if (c == '\n') {
			(void) ungetc(c,fp);
			break;
		} else if (c == EOF) {
			break;
		}
	}
}

#define MAXPORT 1024
#define MAXLEN 24

static int
getprotocol(fp, src)
	FILE *fp;
	const char *src;
{
	int  k;
	char b[MAXLEN];

	(void) getword(b, sizeof(b), fp, 0);

	k = protocolnumber(b);
	if (k == -1)
		syslog(LOG_INFO, "%s: line %d: unknown protocol: %s.",
		       src, lineno, b);
	return(k);
}

static int
getservices(n, data, fp, src)
	int n;
	char *data;
	FILE *fp;
	const char *src;
{
	int j, ch;
	int k;
	int maxl;
	int bracket;
	char b[MAXLEN];
	char bm[MAXPORT/8];

	for (j = 0; j < MAXPORT/8; j++)
		bm[j] = 0;
	maxl = 0;
	bracket = 0;
	while (getword(b, sizeof(b), fp, 0) || bracket) {
		if (feof(fp) || ferror(fp))
			break;
		if (strlen(b) == 0)
			continue;
		if ( b[0] == '(') {
			bracket++;
 			continue;
		}
		if ( b[0] == ')') {
			bracket = 0;
			while ((ch = getc(fp)) != EOF && ch != '\n')
				;
			if (ch == '\n')
				lineno++;
			break;
		}
		k = servicenumber(b);
		if (k == -1) {
			syslog(LOG_INFO,
			       "%s: line %d: Unknown service '%s'",
			       src, lineno, b);
			continue;
		}
		if ((k < MAXPORT) && (k)) {
			bm[k/8] |= (0x80>>(k%8));
			if (k > maxl)
				maxl=k;
		}
		else {
			syslog(LOG_INFO,
			       "%s: line %d: port no. (%d) too big",
			       src, lineno, k);
			dprintf(1, (ddt,
				    "%s: line %d: port no. (%d) too big\n",
				    src, lineno, k));
		}
	}
	if (bracket)
		syslog(LOG_INFO, "%s: line %d: missing close paren",
		       src, lineno);
	maxl = maxl/8+1;
	bcopy(bm, data+n, maxl);
	return (maxl+n);
}

/* get_netlist(fp, netlistp, allow)
 *	get list of nets from 'fp', put on *netlistp, 'allow' controls
 *	whether hosts, nets, or both shall be accepted without warnings.
 *	(note that they are always accepted; 'allow' just controls the
 *	warnings.)
 */
void
get_netlist(fp, netlistp, allow, print_tag)
	FILE *fp;
	struct netinfo **netlistp;
	int allow;
	char *print_tag;
{
	struct netinfo *ntp, **end;
	char buf[BUFSIZ], *maskp;
	struct in_addr ina;

	for (end = netlistp; *end; end = &(**end).next)
		;
	ntp = NULL;
	dprintf(1, (ddt, "get_netlist(%s)", print_tag));
	while (getword(buf, sizeof(buf), fp, 0)) {
		if (strlen(buf) == 0)
			break;
		if ((maskp = strchr(buf, '&')) != NULL)
			*maskp++ = '\0';
		dprintf(1, (ddt," %s", buf));
		if (!ntp) {
			ntp = (struct netinfo *)malloc(sizeof(struct netinfo));
			if (!ntp)
				panic(errno, "malloc(netinfo)");
		}
		if (!inet_aton(buf, &ntp->my_addr)) {
			syslog(LOG_INFO, "%s contains bogus element (%s)",
			       print_tag, buf);
			continue;
		}
		if (maskp) {
			if (!inet_aton(maskp, &ina)) {
				syslog(LOG_INFO,
				       "%s element %s has bad mask (%s)",
				       print_tag, buf, maskp);
				continue;
			}
		} else {
			if (allow & ALLOW_HOSTS)
				ina.s_addr = 0xffffffff;	/* "exact" */
			else
				ina.s_addr = net_mask(ntp->my_addr);
		}
		ntp->next = NULL;
		ntp->mask = ina.s_addr;
		ntp->addr = ntp->my_addr.s_addr & ntp->mask;

		/* Check for duplicates */
		if (addr_on_netlist(ntp->my_addr, *netlistp))
			continue;

		if (ntp->addr != ntp->my_addr.s_addr) {
			ina.s_addr = ntp->addr;
			syslog(LOG_INFO,
			       "%s element (%s) mask problem (%s)",
				print_tag, buf, inet_ntoa(ina));
		}

		*end = ntp;
		end = &ntp->next;
		ntp = NULL;
	}
	if (ntp)
		free((char *)ntp);

	dprintf(1, (ddt, "\n"));
#ifdef DEBUG
	if (debug > 2)
		for (ntp = *netlistp;  ntp != NULL;  ntp = ntp->next) {
			fprintf(ddt, "ntp x%lx addr x%lx mask x%lx",
				(u_long)ntp, (u_long)ntp->addr,
				(u_long)ntp->mask);
			fprintf(ddt, " my_addr x%lx",
				(u_long)ntp->my_addr.s_addr);
			fprintf(ddt, " %s", inet_ntoa(ntp->my_addr));
			fprintf(ddt, " next x%lx\n", (u_long)ntp->next);
		}
#endif
}

struct netinfo *
addr_on_netlist(addr, netlist)
	struct in_addr	addr;
	struct netinfo	*netlist;
{
	u_int32_t	a = addr.s_addr;
	struct netinfo	*t;

	for (t = netlist;  t != NULL;  t = t->next)
		if (t->addr == (a & t->mask))
			return t;
	return NULL;
}

int
position_on_netlist(addr, netlist)
	struct in_addr	addr;
	struct netinfo	*netlist;
{
	u_int32_t	a = addr.s_addr;
	struct netinfo	*t;
	int		position = 0;

	for (t = netlist;  t != NULL;  t = t->next)
		if (t->addr == (a & t->mask))
			break;
		else
			position++;
	return position;
}

void
free_netlist(netlistp)
	struct netinfo **netlistp;
{
	register struct netinfo *ntp, *next;

	for (ntp = *netlistp;  ntp != NULL;  ntp = next) {
		next = ntp->next;
		free((char *)ntp);
	}
	*netlistp = NULL;
}

/*
 * Converts a word to a u_int32_t.  Error if any non-numeric
 * characters in the word, except leading or trailing white space.
 */
static u_int32_t
wordtouint32(buf)
	char *buf;
{
	u_long result;
	u_int32_t res2;
	char *bufend;

	wordtouint32_error = 0;
	result = strtoul(buf, &bufend, 0);
	if (bufend == buf)
		wordtouint32_error = 1;
	else
		while ('\0' != *bufend) {
			if (isspace(*bufend))
				bufend++;
			else {
				wordtouint32_error = 1;
				break;
			}
		}
	/* Check for truncation between u_long and u_int32_t */
	res2 = result;
	if (res2 != result)		
		wordtouint32_error = 1;
	return (res2);
}


/*
 * Parse part of a date.  Set error flag if any error.
 * Don't reset the flag if there is no error.
 */
static int 
datepart(buf, size, min, max, errp)
	char *buf;
	int size, min, max, *errp;
{
	int result = 0;
	int i;

	for (i = 0; i < size; i++) {
		if (!isdigit(buf[i]))
			*errp = 1;
		result = (result * 10) + buf[i] - '0';
	}
	if (result < min)
		*errp = 1;
	if (result > max)
		*errp = 1;
	return (result);
}


/* Convert a date in ASCII into the number of seconds since
   1 January 1970 (GMT assumed).  Format is yyyymmddhhmmss, all
   digits required, no spaces allowed.  */

static u_int32_t
datetosecs(cp, errp)
	char *cp;
	int *errp;
{
	struct tm time;
	u_int32_t result;
	int mdays, i;
	static const int days_per_month[12] =
		{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	if (strlen(cp) != 14) {
		*errp = 1;
		return 0;
	}
	*errp = 0;

	bzero((char *)&time, sizeof time);
	time.tm_year  = datepart(cp +  0, 4, 1990, 9999, errp) - 1900;
	time.tm_mon   = datepart(cp +  4, 2,   01,   12, errp) - 1;
	time.tm_mday  = datepart(cp +  6, 2,   01,   31, errp);
	time.tm_hour  = datepart(cp +  8, 2,   00,   23, errp);
	time.tm_min   = datepart(cp + 10, 2,   00,   59, errp);
	time.tm_sec   = datepart(cp + 12, 2,   00,   59, errp);
	if (*errp)		/* Any parse errors? */
		return (0);

	/* 
	 * OK, now because timegm() is not available in all environments,
	 * we will do it by hand.  Roll up sleeves, curse the gods, begin!
	 */

#define	SECS_PER_DAY	((u_int32_t)24*60*60)
#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)

	result  = time.tm_sec;				/* Seconds */
	result += time.tm_min * 60;			/* Minutes */
	result += time.tm_hour * (60*60);		/* Hours */
	result += (time.tm_mday - 1) * SECS_PER_DAY;	/* Days */
	
	/* Months are trickier.  Look without leaping, then leap */
	mdays = 0;
	for (i = 0; i < time.tm_mon; i++)
		mdays += days_per_month[i];
	result += mdays * SECS_PER_DAY;			/* Months */
	if (time.tm_mon > 1 && isleap (1900+time.tm_year))
		result += SECS_PER_DAY;		/* Add leapday for this year */

	/* First figure years without leapdays, then add them in.  */
	/* The loop is slow, FIXME, but simple and accurate.  */
	result += (time.tm_year - 70) * (SECS_PER_DAY*365); /* Years */
	for (i = 70; i < time.tm_year; i++)
		if (isleap (1900+i)) 
			result += SECS_PER_DAY;	/* Add leapday for prev year */

	return (result);
}


#define MAXCHARSTRING 255

static int
getcharstring(buf, data, type, minfields, maxfields, fp, src)
	char *buf;
	char *data;
	int type;
	int minfields;
	int maxfields;
	FILE *fp;
	const char *src;
{
	int nfield = 0, done = 0, n = 0, i;
	char *b = buf;

	do {
		nfield++;
		i = strlen(buf);
#ifdef ALLOW_LONG_TXT_RDATA
		b = buf;
		if (type == T_TXT || type == T_X25) {
			while (i > MAXCHARSTRING
			       && n + MAXCHARSTRING + 1 < MAXDATA) {
				data[n] = MAXCHARSTRING;
				bcopy(b, data + n + 1, MAXCHARSTRING);
				n += MAXCHARSTRING + 1;
				b += MAXCHARSTRING;
				i -= MAXCHARSTRING;
			}
		}
#endif /* ALLOW_LONG_TXT_RDATA */
		if (i > MAXCHARSTRING) {
			syslog(LOG_INFO,
			       "%s: line %d: RDATA field %d too long",
			       src, lineno, nfield);
			return (0);
		}
		if (n + i + 1 > MAXDATA) {
			syslog(LOG_INFO,
			       "%s: line %d: total RDATA too long",
			       src, lineno);
			return (0);
		}
		data[n] = i;
		bcopy(b, data + n + 1, (int)i);
		n += i + 1;
		done = (maxfields && nfield >= maxfields);
	} while (!done && getword(buf, MAXDATA, fp, 0));

	if (nfield < minfields) {
		syslog(LOG_INFO,
		       "%s: line %d: expected %d RDATA fields, only saw %d",
		       src, lineno, minfields, nfield);
		return (0);
	}

	if (done)
		endline(fp);

	return (n);
}
