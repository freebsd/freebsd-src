#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)db_load.c	4.38 (Berkeley) 3/2/91";
static char rcsid[] = "$Id: db_load.c,v 8.41 1998/02/13 20:02:28 halley Exp $";
#endif /* not lint */

/*
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
 * Portions Copyright (c) 1996, 1997 by Internet Software Consortium.
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
 * Load zone from ASCII file on local host.  Format similar to RFC 883.
 */

/* Import. */

#include "port_before.h"

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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>

#include "port_after.h"

#include "named.h"

/* Forward. */

static int        	gettoken(FILE *, const char *);
static int		getttl(FILE *, const char *, int, u_int32_t *, int *);
static int		getcharstring(char *, char *, int, int, int, FILE *,
				      const char *);
static int		makename_ok(char *name, const char *origin, int class,
				    struct zoneinfo *zp,
				    enum transport transport,
				    enum context context,
				    const char *owner, const char *filename,
				    int lineno, int size);
static int		getmlword(char *, size_t, FILE *, int);
static int		getallwords(char *, size_t, FILE *, int);
static u_int32_t	wordtouint32(char *);
static int		datepart(const char *, int, int, int, int *);
static u_int32_t	datetosecs(const char *, int *);
static int		get_nxt_types(u_char *, FILE *, const char *);
static void		fixup_soa(const char *fn, struct zoneinfo *zp);
#ifdef BIND_NOTIFY
static void		notify_after_delay(evContext ctx, void *uap,
					   struct timespec due,
					   struct timespec inter);
#endif
static int		wordtouint32_error = 0;
static int		empty_token = 0;
static int		getmlword_nesting = 0;

/* Global. */

static int clev;	/* a zone deeper in a hierarchy has more credibility */

#ifdef BIND_NOTIFY
static notify_info_list	pending_notifies;
#endif

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

#define MAKENAME_OK(N) \
	do { \
		if (!makename_ok(N, origin, class, zp, \
				 transport, context, \
				 domain, filename, lineno, \
				 sizeof(data) - ((u_char*)N - data))) { \
			errs++; \
			sprintf(buf, "bad name \"%s\"", N); \
		        goto err; \
		} \
	} while (0)

/* Public. */

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
db_load(const char *filename, const char *in_origin,
	struct zoneinfo *zp, const char *def_domain)
{
	static int read_soa, read_ns, rrcount;

	const char *errtype = "Database";
	char *cp;
	char domain[MAXDNAME], origin[MAXDNAME], tmporigin[MAXDNAME];
	char buf[MAXDATA];
	u_char data[MAXDATA];
	const char *op;
	int c, someclass, class, type, dbflags, dataflags, multiline;
	int slineno, i, errs, didinclude, escape, success, dateerror;
	u_int32_t ttl, n, serial;
	u_long tmplong;
	struct databuf *dp;
	FILE *fp;
	struct stat sb;
	struct in_addr ina;
	enum transport transport;
	enum context context;
	u_int32_t sig_type;
	u_int32_t keyflags;
	struct sockaddr_in empty_from;

	empty_from.sin_family = AF_INET;
	empty_from.sin_addr.s_addr = htonl(INADDR_ANY);
	empty_from.sin_port = htons(0);

/*
 * We use an 'if' inside of the 'do' below because otherwise the Solaris
 * compiler detects that the 'while' is never executed because of the 'goto'
 * and complains.
 */
#define	ERRTO(msg)  do { if (1) { errtype = msg; goto err; } } while (0)

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
		clev = nlabels(in_origin);
	}

	ns_debug(ns_log_load, 1, "db_load(%s, %s, %d, %s)",
		 filename, in_origin, zp - zones,
		 def_domain ? def_domain : "Nil");

	strcpy(origin, in_origin);
	if ((fp = fopen(filename, "r")) == NULL) {
		ns_warning(ns_log_load, "%s: %s", filename, strerror(errno));
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
		ns_warning(ns_log_load, "%s: %s", filename, strerror(errno));
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
			if (!getword(buf, sizeof buf, fp, 0))
				/* file name*/
				break;
			if (!getword(tmporigin, sizeof(tmporigin), fp, 1))
				strcpy(tmporigin, origin);
			else {
				if (makename(tmporigin, origin,
					     sizeof(tmporigin)) == -1) 
					ERRTO("$INCLUDE makename failed");
				endline(fp);
			}
			didinclude = 1;
			errs += db_load(buf, tmporigin, zp, domain);
			continue;

		case ORIGIN:
			(void) strcpy(buf, origin);
			if (!getword(origin, sizeof(origin), fp, 1))
				break;
			ns_debug(ns_log_load, 3, "db_load: origin %s, buf %s",
				 origin, buf);
			if (makename(origin, buf, sizeof(origin)) == -1)
				ERRTO("$ORIGIN makename failed");
			ns_debug(ns_log_load, 3, "db_load: origin now %s",
				 origin);
			continue;

		case DNAME:
			if (!getword(domain, sizeof(domain), fp, 1))
				break;
			if (makename(domain, origin, sizeof(domain)) == -1)
				ERRTO("ownername makename failed");
			goto gotdomain;

		case AT:
			(void) strcpy(domain, origin);
			goto gotdomain;

		case DOT:
			domain[0] = '\0';
			/* FALLTHROUGH */
		case CURRENT:
		gotdomain:
			if (!getword(buf, sizeof buf, fp, 0)) {
				if (c == CURRENT)
					continue;
				break;
			}
			if (ns_parse_ttl(buf, &tmplong) < 0)
				ttl = USE_MINIMUM;
			else {
				ttl = (u_int32_t)tmplong;
				if (ttl > MAXIMUM_TTL) {
					ns_info(ns_log_load,
				      "%s: Line %d: TTL > %u; converted to 0",
						filename, lineno, MAXIMUM_TTL);
					ttl = 0;
				}
				if (zp->z_type == Z_CACHE) {
					/*
					 * This allows the cache entry to age
					 * while sitting on disk (powered off).
					 */
					if (ttl > max_cache_ttl)
						ttl = max_cache_ttl;
					ttl += sb.st_mtime;
				}
				if (!getword(buf, sizeof buf, fp, 0))
					break;
			}

			/* Parse class (IN, etc) */
			someclass = sym_ston(__p_class_syms, buf, &success);
			if (success && someclass != C_ANY) {
				class = someclass;
				(void) getword(buf, sizeof buf, fp, 0);
			}

			/* Parse RR type (A, MX, etc) */
			type = sym_ston(__p_type_syms, buf, &success);
			if (success == 0 || type == ns_t_any) {
				ns_info(ns_log_load,
					"%s: Line %d: Unknown type: %s.",
					filename, lineno, buf);
				errs++;
				break;
			}

			context = ns_ownercontext(type, transport);
			if (!ns_nameok(domain, class, zp, transport, context,
				       domain, inaddr_any)) {
				errs++;
				ns_notice(ns_log_load,
					  "%s:%d: owner name error",
					  filename, lineno);
				break;
			}
			context = domain_ctx;
			switch (type) {
			case ns_t_key:
			case ns_t_sig:
			case ns_t_nxt:
				/*
				 * Don't do anything here for these types --
				 * they read their own input separately later.
				 */
				goto dont_get_word;

			case ns_t_soa:
			case ns_t_minfo:
			case ns_t_rp:
			case ns_t_ns:
			case ns_t_cname:
			case ns_t_mb:
			case ns_t_mg:
			case ns_t_mr:
			case ns_t_ptr:
				escape = 1;
				break;
			default:
				escape = 0;
			}
			if (!getword(buf, sizeof buf, fp, escape))
				break;
			ns_debug(ns_log_load, 3,
				 "d='%s', c=%d, t=%d, ttl=%u, data='%s'",
				 domain, class, type, ttl, buf);
			/*
			 * Convert the ascii data 'buf' to the proper format
			 * based on the type and pack into 'data'.
			 */
		dont_get_word:
			switch (type) {
			case ns_t_a:
				if (!inet_aton(buf, &ina))
					ERRTO("IP Address");
				(void) ina_put(ina, data);
				n = NS_INT32SZ;
				break;

			case ns_t_soa:
				context = hostname_ctx;
				goto soa_rp_minfo;
			case ns_t_rp:
			case ns_t_minfo:
				context = mailname_ctx;
				/* FALLTHROUGH */
			soa_rp_minfo:
				(void) strcpy((char *)data, buf);
			        
			        MAKENAME_OK((char *)data);
				cp = (char *)(data + strlen((char *)data) + 1);
				if (!getword(cp,
					     (sizeof data) -
					     (cp - (char*)data),
					     fp, 1))
					ERRTO("Domain Name");
				if (type == ns_t_rp)
					context = domain_ctx;
				else
					context = mailname_ctx;
				MAKENAME_OK(cp);
				cp += strlen((char *)cp) + 1;
				if (type != ns_t_soa) {
					n = cp - (char *)data;
					break;
				}
				if (class != zp->z_class) {
					errs++;
					ns_info(ns_log_load, "%s:%d: %s",
						filename, lineno,
					       "SOA class not same as zone's");
				}
				if (strcasecmp(zp->z_origin, domain) != 0) {
					errs++;
					ns_error(ns_log_load,
				"%s:%d: SOA for \"%s\" not at zone top \"%s\"",
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
				serial = zp->z_serial;
				zp->z_serial = getnum(fp, filename,
						      GETNUM_SERIAL);
				if (getnum_error)
					errs++;
				n = (u_int32_t) zp->z_serial;
				PUTLONG(n, cp);
				if (serial != 0 &&
				    SEQ_GT(serial, zp->z_serial)) {
					ns_notice(ns_log_load,
			 "%s:%d: WARNING: new serial number < old (%lu < %lu)",
						  filename , lineno,
						  zp->z_serial, serial);
				}
				if (getttl(fp, filename, lineno, &n,
					   &multiline) <= 0) {
					errs++;
					n = INIT_REFRESH;
				}
				PUTLONG(n, cp);
				zp->z_refresh = MAX(n, MIN_REFRESH);
				if (zp->z_type == Z_SECONDARY
#if defined(STUBS) 
				    || zp->z_type == Z_STUB
#endif
				    ) {
					ns_refreshtime(zp, MIN(sb.st_mtime,
							       tt.tv_sec));
					sched_zone_maint(zp);
				}
#ifdef BIND_UPDATE
                                if ((zp->z_type == Z_PRIMARY) && 
				    (zp->z_flags & Z_DYNAMIC))
                                        if ((u_int32_t)zp->z_soaincrintvl >
					    zp->z_refresh/3) {
                                                ns_info(ns_log_load,
		    "zone soa update time truncated to 1/3rd of refresh time");
                                                zp->z_soaincrintvl =
							zp->z_refresh / 3;
                                        }
#endif

				if (getttl(fp, filename, lineno, &n,
					   &multiline) <= 0) {
					errs++;
					n = INIT_REFRESH;
				}
				PUTLONG(n, cp);
				zp->z_retry = MAX(n, MIN_RETRY);
				if (getttl(fp, filename, lineno,
					   &zp->z_expire, &multiline) <= 0) {
					errs++;
					zp->z_expire = INIT_REFRESH;
				}
				n = zp->z_expire;
				PUTLONG(n, cp);
				if (getttl(fp, filename, lineno, &n,
					   &multiline) <= 0) {
					errs++;
					n = 120;
				}
				PUTLONG(n, cp);
				if (n > MAXIMUM_TTL) {
					ns_info(ns_log_load,
			   "%s: Line %d: SOA minimum TTL > %u; converted to 0",
						filename, lineno, MAXIMUM_TTL);
					zp->z_minimum = 0;
				} else 
					zp->z_minimum = n;
				n = cp - (char *)data;
				if (multiline) {
					buf[0] = getnonblank(fp, filename);
					buf[1] = '\0';
					if (buf[0] != ')')
						ERRTO("SOA \")\"");
					endline(fp);
				}
                                read_soa++;
				if (zp->z_type == Z_PRIMARY)
					fixup_soa(filename, zp);
				break;

			case ns_t_wks:
				/* Address */
				if (!inet_aton(buf, &ina))
					ERRTO("WKS IP Address");
				(void) ina_put(ina, data);
				/* Protocol */
				data[INADDRSZ] = getprotocol(fp, filename);
				/* Services */
				n = getservices(NS_INT32SZ + sizeof(char),
						(char *)data, fp, filename);
				break;

			case ns_t_ns:
				if (strcasecmp(zp->z_origin, domain) == 0)
					read_ns++;
				context = hostname_ctx;
				goto cname_etc;
			case ns_t_cname:
			case ns_t_mb:
			case ns_t_mg:
			case ns_t_mr:
				context = domain_ctx;
				goto cname_etc;
			case ns_t_ptr:
				context = ns_ptrcontext(domain);
			cname_etc:
				(void) strcpy((char *)data, buf);
				MAKENAME_OK((char *)data);
				n = strlen((char *)data) + 1;
				break;

			case ns_t_naptr:
			/* Order Preference Flags Service Replacement Regexp */
				n = 0;
				cp = buf;
				/* Order */
				while (isdigit(*cp))
					n = n * 10 + (*cp++ - '0');
				/* catch bad values */
				if (cp == buf || n > 65535)
					ERRTO("NAPTR Order");
				cp = (char *)data;
				PUTSHORT((u_int16_t)n, cp);

				/* Preference */
				n = getnum(fp, filename, GETNUM_NONE);
				if (getnum_error || n > 65536)
					ERRTO("NAPTR Preference");
				PUTSHORT((u_int16_t)n, cp);

                                /* Flags */
                                if (!getword(buf, sizeof buf, fp, 0))
					ERRTO("NAPTR Flags");
                                n = strlen(buf);
                                *cp++ = n;
                                memcpy(cp, buf, (int)n);
                                cp += n;
 
                                /* Service Classes */
                                if (!getword(buf, sizeof buf, fp, 0))
					ERRTO("NAPTR Service Classes");
                                n = strlen(buf);
                                *cp++ = n;
                                memcpy(cp, buf, (int)n);
                                cp += n;
 
                                /* Pattern */
                                if (!getword(buf, sizeof buf, fp, 0))
					ERRTO("NAPTR Pattern");
                                n = strlen(buf);
                                *cp++ = n;
                                memcpy(cp, buf, (int)n);
                                cp += n;

				/* Replacement */
				if (!getword(buf, sizeof buf, fp, 1))
					ERRTO("NAPTR Replacement");
				(void) strcpy((char *)cp, buf);
				context = domain_ctx;
				MAKENAME_OK(cp);
				/* advance pointer to end of data */
				cp += strlen((char *)cp) +1;

				/* now save length */
				n = (cp - (char *)data);
				break;


			case ns_t_mx:
			case ns_t_afsdb:
			case ns_t_rt:
			case ns_t_srv:
				n = 0;
				cp = buf;
				while (isdigit(*cp))
					n = n * 10 + (*cp++ - '0');
				/* catch bad values */
				if ((cp == buf) || (n > 65535))
					ERRTO("Priority");
				cp = (char *)data;
				PUTSHORT((u_int16_t)n, cp);

				if (type == ns_t_srv) {
					n = getnum(fp, filename, GETNUM_NONE);
					if (getnum_error || n > 65536)
						ERRTO("SRV RR");
					PUTSHORT((u_int16_t)n, cp);

					n = getnum(fp, filename, GETNUM_NONE);
					if (getnum_error || n > 65536)
						ERRTO("SRV RR");
					PUTSHORT((u_int16_t)n, cp);
				}

				if (!getword(buf, sizeof buf, fp, 1))
					ERRTO("Domain Name");
				(void) strcpy((char *)cp, buf);
				context = hostname_ctx;
				MAKENAME_OK(cp);
				/* advance pointer to end of data */
				cp += strlen((char *)cp) +1;

				/* now save length */
				n = (cp - (char *)data);
				break;

			case ns_t_px:
				context = domain_ctx;
				n = 0;
				data[0] = '\0';
				cp = buf;
				while (isdigit(*cp))
					n = n * 10 + (*cp++ - '0');
				/* catch bad values */
				if ((cp == buf) || (n > 65535))
					ERRTO("PX Priority");
				cp = (char *)data;
				PUTSHORT((u_int16_t)n, cp);

				if (!getword(buf, sizeof buf, fp, 0))
					ERRTO("PX Domain1");
				(void) strcpy((char *)cp, buf);
				MAKENAME_OK(cp);
				/* advance pointer to next field */
				cp += strlen((char *)cp) + 1;
				if (!getword(buf, sizeof buf, fp, 0))
					ERRTO("PX Domain2");
				(void) strcpy((char *)cp, buf);
				MAKENAME_OK(cp);
				/* advance pointer to end of data */
				cp += strlen((char *)cp) + 1;

				/* now save length */
				n = (cp - (char *)data);
				break;

			case ns_t_hinfo:
				n = getcharstring(buf, (char *)data, type,
						  2, 2, fp, filename);
				if (n == 0)
					ERRTO("HINFO RR");
				break;

			case ns_t_isdn:
				n = getcharstring(buf, (char *)data, type,
						  1, 2, fp, filename);
				if (n == 0)
					ERRTO("ISDN RR");
				break;

 			case ns_t_txt:
				n = getcharstring(buf, (char *)data, type,
						  1, 0, fp, filename);
				if (n == 0)
					ERRTO("TXT RR");
				break;


			case ns_t_x25:
				n = getcharstring(buf, (char *)data, type,
						  1, 1, fp, filename);
				if (n == 0)
					ERRTO("X25 RR");
				break;

			case ns_t_nsap:
				n = inet_nsap_addr(buf, (u_char *)data,
						   sizeof data);
				if (n == 0)
					ERRTO("NSAP RR");
				endline(fp);
				break;

			case ns_t_aaaa:
				if (inet_pton(AF_INET6, buf, data) <= 0)
					ERRTO("IPv4 Address");
				n = NS_IN6ADDRSZ;
				endline(fp);
				break;

			case ns_t_key: {
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
				cp = (char *)data;
				getmlword_nesting = 0; /* KLUDGE err recov. */
			/*>>> Flags (unsigned_16)  */
				if (!getmlword((char*)buf, sizeof buf, fp, 0))
					ERRTO("KEY Flags Field");
				keyflags = wordtouint32(buf);
				if (wordtouint32_error || 0xFFFF < keyflags)
					goto err;
				if (keyflags & NS_KEY_RESERVED_BITMASK)
					ERRTO("KEY Reserved Flag Bit");
				PUTSHORT(keyflags, cp);

			/*>>> Protocol (8-bit decimal) */
				if (!getmlword((char*)buf, sizeof buf, fp, 0))
					ERRTO("KEY Protocol Field");
				pr = wordtouint32(buf);
				if (wordtouint32_error || 255 < pr)
					ERRTO("KEY Protocol Field");
				*cp++ = (u_char) pr;

			/*>>> Algorithm id (8-bit decimal) */
				if (!getmlword((char*)buf, sizeof buf, fp, 0))
					ERRTO("KEY Algorithm ID");
				al = wordtouint32(buf);
				if (wordtouint32_error ||
				    0 == al || 255 == al || 255 < al)
					ERRTO("KEY Algorithm ID");
				*cp++ = (u_char) al;

			/*>>> Public Key data is in BASE64.
			 *	We don't care what algorithm it uses or what
			 *	the internal structure of the BASE64 data is.
			 */
				if (!getallwords(buf, MAXDATA, fp, 0))
					klen = 0;
				else {
					/* Convert from BASE64 to binary. */
					klen = b64_pton(buf, (u_char*)cp,
							sizeof data - 
							(cp - (char *)data));
					if (klen < 0)
						ERRTO("KEY Public Key");
				}

				/* set total length */
				n = cp + klen - (char *)data;

			/*
			 * Now check for valid key flags & algs & etc,
			 * from the RFC.
			 */

				if (keyflags & (NS_KEY_ZONEKEY | NS_KEY_IPSEC
				    | NS_KEY_EMAIL))
					pr |= 1;	/* A nonzero proto. */
				if (NS_KEY_TYPE_NO_KEY ==
				    (keyflags & NS_KEY_TYPEMASK))
					nk = 1;		/* No-key */
				else
					nk = 0;		/* have a key */

				if ((keyflags & NS_KEY_ZONEKEY) && 
				    (NS_KEY_TYPE_CONF_ONLY ==
				     (keyflags & NS_KEY_TYPEMASK)))
					/* Zone key must have Auth bit set. */
					ERRTO("KEY Zone Key Auth. bit");

				if (al == 0 && nk == 0)
					ERRTO("KEY Algorithm");
				if (al != 0 && pr == 0)
					ERRTO("KEY Protocols");

				if (nk == 1 && klen != 0)
					ERRTO("KEY No-Key Flags Set");

				if (nk == 0 && klen == 0)
					ERRTO("KEY Type Spec'd");

				/* Check algorithm-ID and key structure, for
				   the algorithm-ID's that we know about. */
				switch (al) {
				case NS_ALG_MD5RSA:
					if (klen == 0)
						break;
					expstart = cp;
					expbytes = *expstart++;
					if (expbytes == 0)
						GETSHORT(expbytes, expstart);

					if (expbytes < 1)
						ERRTO("Exponent too short");
					if (expbytes >
					    (NS_MD5RSA_MAX_BITS + 7) / 8
					    )
						ERRTO("Exponent too long");
					if (*expstart == 0)
						ERRTO("Exponent w/ 0");

					modbytes = klen -
						(expbytes + (expstart - cp));
					if (modbytes < 
					    (NS_MD5RSA_MIN_BITS + 7) / 8
					    )
						ERRTO("Modulus too short");
					if (modbytes > 
					    (NS_MD5RSA_MAX_BITS + 7) / 8
					    )
						ERRTO("Modulus too long");
					if (*(expstart+expbytes) == 0)
						ERRTO("Modulus starts w/ 0");
					break;

				case NS_ALG_EXPIRE_ONLY:
					if (klen != 0)
						ERRTO(
				       "Key provided for expire-only algorithm"
						      );
					break;
				case NS_ALG_PRIVATE_OID:
					if (klen == 0)
						ERRTO("No ObjectID in key");
					break;
				}

				endline(fp);  /* flush the rest of the line */
				break;
			    } /*T_KEY*/
		  
		        case ns_t_sig:
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
		if (!getmlword((char*)buf, sizeof buf, fp, 0))
			ERRTO("SIG record doesn't specify type");
		sig_type = sym_ston(__p_type_syms, buf, &success);
		if (!success || sig_type == ns_t_any) {
			/*
			 * We'll also accept a numeric RR type,
			 * for signing RR types that this version
			 * of named doesn't yet understand.
			 * In the ns_t_any case, we rely on wordtouint32
			 * to fail when scanning the string "ANY".
			 */
			sig_type = wordtouint32 (buf);
			if (wordtouint32_error || sig_type > 0xFFFF)
				ERRTO("Unknown RR type in SIG record");
		}
		cp = (char *)&data[i];
		PUTSHORT((u_int16_t)sig_type, cp);
		i += 2;

		/* Algorithm id (8-bit decimal) */
		if (!getmlword(buf, sizeof buf, fp, 0))
			ERRTO("Missing algorithm ID");
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
		if (0 >= n || 255 < n)
			ERRTO("SIG label count invalid");
		data[i] = (u_char) n;
		i++;

		/*
		 * OTTL (optional u_int32_t) and
		 * Texp (u_int32_t date)
		 */
		if (!getmlword(buf, sizeof buf, fp, 0))
			ERRTO("OTTL and expiration time missing");
		/*
		 * See if OTTL is missing and this is a date.
		 * This relies on good, silent error checking
		 * in datetosecs.
		 */
		exptime = datetosecs(buf, &dateerror);
		if (!dateerror) {
			/* Output TTL as OTTL */
			origTTL = ttl;
			cp = (char *)&data[i];
			PUTLONG (origTTL, cp);
			i += 4;
		} else {
			/* Parse and output OTTL; scan TEXP */
			origTTL = wordtouint32(buf);
			if (0 >= origTTL || wordtouint32_error ||
			    (origTTL > 0x7fffffff))
				goto err;
			cp = (char *)&data[i];
			PUTLONG(origTTL, cp);
			i += 4;
			if (!getmlword(buf, sizeof buf, fp, 0))
				ERRTO("Expiration time missing");
			exptime = datetosecs(buf, &dateerror);
		}
 		if (dateerror || exptime > 0x7fffffff || exptime <= 0)
			ERRTO("Invalid expiration time");
		cp = (char *)&data[i];
		PUTLONG(exptime, cp);
		i += 4;

		/* Tsig (u_int32_t) */
		if (!getmlword(buf, sizeof buf, fp, 0))
			ERRTO("Missing signature time");
		signtime = datetosecs(buf, &dateerror);
		if (0 == signtime || dateerror)
			ERRTO("Invalid signature time");
		cp = (char *)&data[i];
		PUTLONG(signtime, cp);
		i += 4;

		/* Kfootprint (unsigned_16) */
		if (!getmlword(buf, sizeof buf, fp, 0))
			ERRTO("Missing key footprint");
		n = wordtouint32(buf);
		if (wordtouint32_error || n >= 0x0ffff)
			ERRTO("Invalid key footprint");
		cp = (char *)&data[i];
		PUTSHORT((u_int16_t)n, cp);
		i += 2;

		/* Signer's Name */
		if (!getmlword((char*)buf, sizeof buf, fp, 0))
			ERRTO("Missing signer's name");
		cp = (char *)&data[i];
		strcpy(cp,buf);
		context = domain_ctx;
		MAKENAME_OK(cp);
		i += strlen(cp) + 1;

		/*
		 * Signature (base64 of any length)
		 * We don't care what algorithm it uses or what
		 * the internal structure of the BASE64 data is.
		 */
		if (!getallwords(buf, sizeof buf, fp, 0)) {
			siglen = 0;
		} else {
			cp = (char *)&data[i];
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
		if (ttl > origTTL)
			ERRTO("TTL is greater than signed original TTL");

		/* Don't let bogus signers "sign" in the future.  */
		if (signtime > (u_int32_t)now)
			ERRTO("signature time is in the future");
		
		/* Ignore received SIG RR's that are already expired.  */
		if (exptime <= (u_int32_t)now)
			ERRTO("expiration time is in the past");

		/* Lop off the TTL at the expiration time.  */
		timetilexp = exptime - now;
		if (timetilexp < ttl) {
			ns_debug(ns_log_load, 1,
				 "shrinking expiring %s SIG TTL from %d to %d",
				 p_secstodate(exptime), ttl, timetilexp);
			ttl = timetilexp;
		}

		/*
		 * Check algorithm-ID and key structure, for
		 * the algorithm-ID's that we know about.
		 */
		switch (al) {
		case NS_ALG_MD5RSA:
			if (siglen == 0)
				ERRTO("No key for RSA algorithm");
			if (siglen < 1)
				ERRTO("Signature too short");
			if (siglen > (NS_MD5RSA_MAX_BITS + 7) / 8)
				ERRTO("Signature too long");
			/* We rely on  cp  from parse */
			if (*cp == 0)
				ERRTO("Signature starts with zeroes");
			break;

		case NS_ALG_EXPIRE_ONLY:
			if (siglen != 0)
				ERRTO(
				"Signature supplied to expire-only algorithm");
			break;
		case NS_ALG_PRIVATE_OID:
			if (siglen == 0)
				ERRTO("No ObjectID in key");
			break;
		}

		/* Should we complain about algorithm-ID's that we	
		   don't understand?  It may help debug some obscure
		   cases, but in general we should accept any RR whether
		   we could cryptographically process it or not; it
		   may be being published for some newer DNS clients
		   to validate themselves.  */

		endline(fp);  /* flush the rest of the line */

		break;		/* Accept this RR. */
	}

			case ns_t_nxt:
				/* The NXT record looks like:
				   Name Cl NXT nextname RRT1 RRT2 MX A SOA ...
				    
				   where:  Name and Cl are as usual
					   NXT     is a keyword
					   nextname is the next valid name in
						   the zone after "Name".  All
						   names between the two are
						   known to be nonexistent.
					   RRT's... are a series of RR type
						   names, which indicate that
						   RR's of these types are
						   published for "Name", and
						   that no RR's of any other
						   types are published for
						   "Name".

				   When a NXT record is cryptographically
				   signed, it proves the nonexistence of an
				   RR (actually a whole set of RR's).  */

				getmlword_nesting = 0; /* KLUDGE err recov. */
				if (!getmlword(buf, sizeof buf, fp, 1))
					goto err;
				(void) strcpy((char *)data, buf);
			        MAKENAME_OK((char *)data);
				n = strlen((char *)data) + 1;
				cp = n + (char *)data;
				n += get_nxt_types((u_char *)cp, fp, filename);
				break;

			case ns_t_loc:
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

			default:
				goto err;
			}
			/*
			 * Ignore data outside the zone.
			 */
			if (zp->z_type != Z_CACHE &&
			    !samedomain(domain, zp->z_origin))
			{
				ns_info(ns_log_load,
			    "%s:%d: data \"%s\" outside zone \"%s\" (ignored)",
					filename, lineno, domain,
					zp->z_origin);
				continue;
			}
			dp = savedata(class, type, (u_int32_t)ttl,
				      (u_char *)data, (int)n);
			dp->d_zone = zp - zones;
			dp->d_flags = dataflags;
			dp->d_cred = DB_C_ZONE;
			dp->d_clev = clev;
			if ((c = db_update(domain, dp, dp, NULL, dbflags,
					   (dataflags & DB_F_HINT)
					   ? fcachetab
					   : hashtab, empty_from))
			    != OK) {
				if (c != DATAEXISTS)
					ns_debug(ns_log_load, 1,
						 "update failed %s %d", 
						 domain, type);
				db_freedata(dp);
			} else {
				rrcount++;
			}
			continue;

		case ERROR:
			break;
		}
 err:
		errs++;
		ns_notice(ns_log_load, "%s:%d: %s error (%s)",
			  filename, empty_token ? (lineno - 1) : lineno,
			  errtype, buf);
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
				ns_warning(ns_log_load,
					   "Zone \"%s\" (file %s): %s",
					   zp->z_origin, filename, msg);
			}
		}
	}
	if (!def_domain) {
		if (errs)
			ns_warning(ns_log_load,
		   "%s zone \"%s\" (%s) rejected due to errors (serial %u)",
				   zoneTypeString(zp), zp->z_origin,
				   p_class(zp->z_class), zp->z_serial);
		else
			ns_info(ns_log_load,
				"%s zone \"%s\" (%s) loaded (serial %u)",
				zoneTypeString(zp), zp->z_origin,
				p_class(zp->z_class), zp->z_serial);
	}
	if (errs) {
		zp->z_flags |= Z_DB_BAD;
		zp->z_ftime = 0;
	}
#ifdef BIND_NOTIFY
	if (!errs && !def_domain &&
	    (zp->z_type == z_master || zp->z_type == z_slave)) {
		static const char no_room[] =
		       "%s failed, cannot notify for zone %s";
		notify_info ni;

		ni = memget(sizeof *ni);
		if (ni == NULL)
			ns_info(ns_log_load, no_room, "memget", zp->z_origin);
		else {
			ni->name = savestr(zp->z_origin, 0);
			if (ni->name == NULL) {
				memput(ni, sizeof *ni);
				ns_info(ns_log_load, no_room,
					"memget", zp->z_origin);
			} else {
				ni->class = zp->z_class;
				ni->state = notify_info_waitfor;
				if (evWaitFor(ev,
					      (const void *)notify_after_load,
					      notify_after_load, ni,
					      &ni->wait_id) < 0) {
					ns_error(ns_log_load,
						 "evWaitFor() failed: %s",
						 strerror(errno));
					freestr(ni->name);
					memput(ni, sizeof *ni);
				} else {
					APPEND(pending_notifies, ni, link);
					ns_need(MAIN_NEED_NOTIFY);
				}
			}
		}
	}
#endif
	return (errs);
}

static int
gettoken(FILE *fp, const char *src) {
	int c;
	char op[32];

	for (;;) {
		c = getc(fp);
 top:
		switch (c) {
		case EOF:
			return (EOF);

		case '$':
			if (getword(op, sizeof op, fp, 0)) {
				if (!strcasecmp("include", op))
					return (INCLUDE);
				if (!strcasecmp("origin", op))
					return (ORIGIN);
			}
			ns_notice(ns_log_db,
				  "%s:%d: Unknown $ option: $%s", 
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
getword(char *buf, size_t size, FILE *fp, int preserve) {
	char *cp = buf;
	int c, spaceok;

	empty_token = 0;	/* XXX global side effect. */
	while ((c = getc(fp)) != EOF) {
		if (c == ';') {
			/* Comment.  Skip to end of line. */
			while ((c = getc(fp)) != EOF && c != '\n')
				(void)NULL;
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
					if (preserve)
						switch (c) {
						case '\\':
						case '.':
						case '0':
						case '1':
						case '2':
						case '3':
						case '4':
						case '5':
						case '6':
						case '7':
						case '8':
						case '9':
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
			if (preserve)
				switch (c) {
				case '\\':
				case '.':
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
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
				(void)NULL;
			ungetc(c, fp);
			/* Blank means terminator if the token is nonempty. */
			if (cp != buf)		/* Trailing whitespace */
				break;
			continue;		/* Leading whitespace */
		}
		if (cp >= buf + size - 1)
			break;
		*cp++ = (char)c;
	}
	*cp = '\0';
	if (cp == buf)
		empty_token = 1;
	return (cp != buf);
}

/*
 * int
 * getttl(fp, fn, ln, ttl, multiline)
 *	read a word from the file and parse it as a TTL.
 * return:
 *	1 ttl found
 *	0 word not read (EOF or EOL?)
 *	-1 word read but it wasn't a ttl
 * side effects:
 *	*ttl is written if the return value is to be 1.
 */
static int
getttl(FILE *fp, const char *fn, int lineno, u_int32_t *ttl, int *multiline) {
	char buf[MAXDATA];
	u_long tmp;
	int ch;
	int len;

	while (!feof(fp) && !getword(buf, sizeof buf, fp, 0) && *multiline)
		(void)NULL;
	len = strlen(buf);
	if (*multiline && len && buf[len-1] == ')') {
		buf[len-1] = '\0';
		*multiline = 0;
	}
	if (ns_parse_ttl(buf, &tmp) < 0) {
		ns_notice(ns_log_db, "%s:%d: expected a TTL, got \"%s\"",
			  fn, lineno, buf);
		return (-1);
	}
	if (*multiline) {
		ch = getnonblank(fp, fn);
		if (ch == EOF)
			return (-1);
		if (ch == ';')
			endline(fp);
		else
			ungetc(ch, fp);
	}
	*ttl = (u_int32_t)tmp;
	return (1);
}

/* Get multiline words.  Same parameters as getword.  Handles any
   number of leading ('s or )'s in the words it sees.
   FIXME:  We kludge recognition of ( and ) for multiline input.
   Each paren must appear at the start of a (blank-separated) word,
   which is particularly counter-intuitive for ).  Good enough for now,
   until Paul rewrites the parser.  (gnu@toad.com, oct96)
*/
static int
getmlword(char *buf, size_t size, FILE *fp, int preserve) {
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
getallwords(char *buf, size_t size, FILE *fp, int preserve) {
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

int
getnum(FILE *fp, const char *src, int opt) {
	int c, n;
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
				ns_notice(ns_log_db,
					  "%s:%d: expected a number",
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
		ns_info(ns_log_db, 
			"%s:%d: number after the decimal point exceeds 9999", 
			src, lineno);
		getnum_error = 1;
		return (0);
	}
	if (seendecimal) {
		ns_info(ns_log_db,
			"%s:%d: decimal serial number interpreted as %d",
			src, lineno, n+m);
	}
	return (n + m);
}

#ifndef BIND_UPDATE
static
#endif
int
getnonblank(FILE *fp, const char *src) {
	int c;

	while ((c = getc(fp)) != EOF) {
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
		return (c);
	}
	ns_info(ns_log_db, "%s:%d: unexpected EOF", src, lineno);
	return (EOF);
}

/*
 * Take name and fix it according to following rules:
 * "." means root.
 * "@" means current origin.
 * "name." means no changes.
 * "name" means append origin.
 */
int
makename(char *name, const char *origin, int size) {
	int n;
	u_char domain[MAXCDNAME];

	switch (ns_name_pton(name, domain, sizeof(domain))) {
	case -1:
		return (-1);
	case 1:		/* FULLY QUALIFIED */
		break;
	case 0: 	/* UNQUALIFIED */
		if (strcmp(name, "@") == 0) /* must test raw name */
			domain[0] = 0;
		if ((n = dn_skipname(domain, domain+sizeof(domain))) == -1)
			return (-1);
		/* step back over root, append origin */
		switch (ns_name_pton(origin, domain+n-1, sizeof(domain)-n+1)) {
		case -1:
			return (-1);
		case 0:
		case 1:
			break;
		}
		break;
	}
	if (ns_name_ntop(domain, name, size) == -1)
		return (-1);
	if (name[0] == '.')	/* root */
		name[0] = '\0';
	return (0);
}

static int
makename_ok(char *name, const char *origin, int class, struct zoneinfo *zp,
	    enum transport transport, enum context context,
	    const char *owner, const char *filename, int lineno, int size)
{
	int ret = 1;

	if (makename(name, origin, size) == -1) {
		ns_info(ns_log_db, "%s:%d: makename failed",
			filename, lineno);
		return (0);
	}
	if (!ns_nameok(name, class, zp, transport, context, owner,
		       inaddr_any)) {
		ns_info(ns_log_db, "%s:%d: database naming error",
			filename, lineno);
		ret = 0;
	}
	return (ret);
}

void
endline(FILE *fp) {
	int c;

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

#ifndef BIND_UPDATE
static
#endif
char
getprotocol(FILE *fp, const char *src) {
	int k;
	char b[MAXLEN];

	(void) getword(b, sizeof(b), fp, 0);
		
	k = protocolnumber(b);
	if (k == -1)
		ns_info(ns_log_db, "%s:%d: unknown protocol: %s.",
			src, lineno, b);
	return ((char) k);
}

#ifndef BIND_UPDATE
static
#endif
int
getservices(int offset, char *data, FILE *fp, const char *src) {
	int j, ch, k, maxl, bracket;
	char bm[MAXPORT/8];
	char b[MAXLEN];

	for (j = 0; j < MAXPORT/8; j++)
		bm[j] = 0;
	maxl = 0;
	bracket = 0;
	while (getword(b, sizeof(b), fp, 0) || bracket) {
		if (feof(fp) || ferror(fp))
			break;
		if (strlen(b) == 0)
			continue;
		if (b[0] == '(') {
			bracket++;
 			continue;
		}
		if (b[0] == ')') {
			bracket = 0;
			while ((ch = getc(fp)) != EOF && ch != '\n')
				(void)NULL;
			if (ch == '\n')
				lineno++;
			break;
		}
		k = servicenumber(b);
		if (k == -1) {
			ns_info(ns_log_db,
				"%s:%d: Unknown service '%s'",
				src, lineno, b);
			continue;
		}
		if ((k < MAXPORT) && (k)) {
			bm[k/8] |= (0x80>>(k%8));
			if (k > maxl)
				maxl = k;
		} else {
			ns_info(ns_log_db,
				"%s:%d: port no. (%d) too big",
				src, lineno, k);
		}
	}
	if (bracket)
		ns_info(ns_log_db, "%s:%d: missing close paren",
			src, lineno);
	maxl = maxl/8+1;
	memcpy(data+offset, bm, maxl);
	return (maxl+offset);
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
datepart(const char *buf, int size, int min, int max, int *errp) {
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
datetosecs(const char *cp, int *errp) {
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

	memset(&time, 0, sizeof time);
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
getcharstring(char *buf, char *data, int type,
	      int minfields, int maxfields,
	      FILE *fp, const char *src)
{
	int nfield = 0, done = 0, n = 0, i;
	char *b = buf;

	do {
		nfield++;
		i = strlen(buf);
#ifdef ALLOW_LONG_TXT_RDATA
		b = buf;
		if (type == ns_t_txt || type == ns_t_x25) {
			while (i > MAXCHARSTRING
			       && n + MAXCHARSTRING + 1 < MAXDATA) {
				data[n] = MAXCHARSTRING;
				memmove(data + n + 1, b, MAXCHARSTRING);
				n += MAXCHARSTRING + 1;
				b += MAXCHARSTRING;
				i -= MAXCHARSTRING;
			}
		}
#endif /* ALLOW_LONG_TXT_RDATA */
		if (i > MAXCHARSTRING) {
			ns_info(ns_log_db,
				"%s:%d: RDATA field %d too long",
				src, lineno, nfield);
			return (0);
		}
		if (n + i + 1 > MAXDATA) {
			ns_info(ns_log_db,
				"%s:%d: total RDATA too long",
				src, lineno);
			return (0);
		}
		data[n] = i;
		memmove(data + n + 1, b, (int)i);
		n += i + 1;
		done = (maxfields && nfield >= maxfields);
	} while (!done && getword(buf, MAXDATA, fp, 0));

	if (nfield < minfields) {
		ns_info(ns_log_db,
			"%s:%d: expected %d RDATA fields, only saw %d",
			src, lineno, minfields, nfield);
		return (0);
	}

	if (done)
		endline(fp);

	return (n);
}


/* 
 * get_nxt_types(): Read the list of types in the NXT record.
 *
 * Data is the array where the bit flags are stored; it must
 * contain at least ns_t_any/NS_NXT_BITS bytes. 
 * FP is the input FILE *.
 * Filename is the sourcefile 
 *
 * The result is how many bytes are significant in the result.
 * ogud@tis.com 1995
 */
static int
get_nxt_types(u_char *data, FILE *fp, const char *filename) {
	char b[MAXLABEL];	/* Not quite the right size, but good enough */
	int maxtype=0; 
	int success;
	int type;
	int errs = 0;

	memset(data, 0, ns_t_any/NS_NXT_BITS+1); 

	while (getmlword(b, sizeof(b), fp, 0)) {
		if (feof(fp) || ferror(fp))
			break; 
		if (strlen(b) == 0 || b[0] == '\n')
			continue; 

		/* Parse RR type (A, MX, etc) */
		type = sym_ston(__p_type_syms, (char *)b, &success);
		if ((!success) || type == ns_t_any) {
			errs++;
			ns_info(ns_log_db,
				"%s: Line %d: Unknown type: %s in NXT record.",
				filename, lineno, b);
			continue;
		}
		NS_NXT_BIT_SET(type, data);
		if (type > maxtype) 
			maxtype = type;
	}
	if (errs)
		return (0);
	else
		return (maxtype/NS_NXT_BITS+1);
}

/* sanity checks PRIMARY ONLY */
static void
fixup_soa(const char *fn, struct zoneinfo *zp) {
	/* Sanity: give enough time for the zone to transfer (retry). */
	if (zp->z_expire < (zp->z_refresh + zp->z_retry))
		ns_notice(ns_log_db,
 "%s: WARNING SOA expire value is less than SOA refresh+retry (%u < %u+%u)",
			  fn, zp->z_expire, zp->z_refresh, zp->z_retry);

	/* Sanity. */
	if (zp->z_expire < (zp->z_refresh  + 10 * zp->z_retry))
		ns_warning(ns_log_db,
"%s: WARNING SOA expire value is less than refresh + 10 * retry \
(%u < (%u + 10 * %u))",
			   fn, zp->z_expire, zp->z_refresh, zp->z_retry);

	/*
	 * Sanity: most hardware/telco faults are detected and fixed within
 	 * a week, secondaries should continue to operate for this time.
	 * (minimum of 4 days for long weekends)
	 */
	if (zp->z_expire < (7 * 24 * 3600))
		ns_warning(ns_log_db,
		      "%s: WARNING SOA expire value is less than 7 days (%u)",
			   fn, zp->z_expire);

	/*
	 * Sanity: maximum down time if we havn't talked for six months 
	 * war must have broken out.
	 */
	if (zp->z_expire > ( 183 * 24 * 3600))
		ns_warning(ns_log_db,
	         "%s: WARNING SOA expire value is greater than 6 months (%u)",
			   fn, zp->z_expire);

	/* Sanity. */
	if (zp->z_refresh < (zp->z_retry * 2))
		ns_warning(ns_log_db,
        "%s: WARNING SOA refresh value is less than 2 * retry (%u < %u * 2)",
			   fn, zp->z_refresh, zp->z_retry);
}

#ifdef BIND_NOTIFY
static void
free_notify_info(notify_info ni) {
	if (ni->state == notify_info_waitfor)
		evUnwait(ev, ni->wait_id);
	else if (ni->state == notify_info_delay)
		evClearTimer(ev, ni->timer_id);
	freestr(ni->name);
	memput(ni, sizeof *ni);
}

void
notify_after_load(evContext ctx, void *uap, const void *tag) {
	int delay, max_delay;
	notify_info ni = uap;
	
	INSIST(tag == (const void *)notify_after_load);

	/* delay notification for from five seconds up to fifteen minutes */
	max_delay = MIN(nzones/5, 895);
	max_delay = MAX(max_delay, 25);
	delay = 5 + (rand() % max_delay);
	ns_debug(ns_log_notify, 3, "notify_after_load: uap %p tag %p delay %d",
		 uap, tag, delay);
	if (evSetTimer(ctx, notify_after_delay, ni,
		       evAddTime(evNowTime(), evConsTime(delay, 0)),
		       evConsTime(0, 0), &ni->timer_id) < 0) {
		ns_error(ns_log_notify, "evSetTimer() failed: %s",
			 strerror(errno));
		UNLINK(pending_notifies, ni, link);
		ni->state = notify_info_error;
		free_notify_info(ni);
	}
	ni->state = notify_info_delay;
}

static void
notify_after_delay(evContext ctx, void *uap,
		   struct timespec due,
		   struct timespec inter)
{
	notify_info ni = uap;

	UNLINK(pending_notifies, ni, link);
	ni->state = notify_info_done;
	sysnotify(ni->name, ni->class, ns_t_soa);
	free_notify_info(ni);
}

void
db_cancel_pending_notifies(void) {
	notify_info ni, ni_next;
	for (ni = HEAD(pending_notifies); ni != NULL; ni = ni_next) {
		ni_next = NEXT(ni, link);
		free_notify_info(ni);
	}
	INIT_LIST(pending_notifies);
}
#endif
