#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)db_load.c	4.38 (Berkeley) 3/2/91";
static char rcsid[] = "$Id: db_load.c,v 4.9.1.18 1994/07/23 23:23:56 vixie Exp $";
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
 * --Copyright--
 */

/*
 * Load data base from ascii backupfile.  Format similar to RFC 883.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <syslog.h>
#include <ctype.h>
#include <netdb.h>

#include "named.h"

static int		gettoken __P((register FILE *, char *)),
			getnonblank __P((FILE *, char *)),
			getprotocol __P((FILE *, char *)),
			getservices __P((int, char *, FILE *, char *));
static void		makename __P((char *, char *));
static int		empty_token = 0;

/*
 * Map class and type names to number
 */
struct map {
	char	token[8];
	int	val;
};

struct map m_class[] = {
	"in",		C_IN,
#ifdef notdef
	"any",		C_ANY,		/* any is a QCLASS, not CLASS */
#endif
	"chaos",	C_CHAOS,
	"hs",		C_HS,
};
#define NCLASS (sizeof(m_class) / sizeof(struct map))

struct map m_type[] = {
	"a",		T_A,
	"ns",		T_NS,
	"cname",	T_CNAME,
	"soa",		T_SOA,
	"mb",		T_MB,
	"mg",		T_MG,
	"mr",		T_MR,
	"null",		T_NULL,
	"wks",		T_WKS,
	"ptr",		T_PTR,
	"hinfo",	T_HINFO,
	"minfo",	T_MINFO,
	"mx",		T_MX,
	"uinfo",	T_UINFO,
	"txt",		T_TXT,
	"rp",		T_RP,
	"afsdb",	T_AFSDB,
	"x25",		T_X25,
	"isdn",		T_ISDN,
	"rt",		T_RT,
	"nsap",		T_NSAP,
	"uid",		T_UID,
	"gid",		T_GID,
#ifdef notdef
	"any",		T_ANY,		/* any is a QTYPE, not TYPE */
#endif
#ifdef ALLOW_T_UNSPEC
        "unspec",       T_UNSPEC,
#endif /* ALLOW_T_UNSPEC */
};
#define NTYPE (sizeof(m_type) / sizeof(struct map))

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

static int clev;	/* a zone deeper in a heirachy has more credability */

/* int
 * db_load(filename, in_origin, zp, doinginclude)
 *	load a database from `filename' into zone `zp'.  append `origin'
 *	to all nonterminal domain names in the file.  `doinginclude' is
 *	true if this is a $INCLUDE file.
 * returns:
 *	-1 = can't open file
 *	0 = success
 *	>0 = number of errors encountered
 */
int
db_load(filename, in_origin, zp, doinginclude)
	char *filename, *in_origin;
	struct zoneinfo *zp;
	int doinginclude;
{
	register char *cp;
	register struct map *mp;
	char domain[MAXDNAME];
	char origin[MAXDNAME];
	char tmporigin[MAXDNAME];
	char buf[MAXDATA];
	char data[MAXDATA];
	char *cp1;
	char *op;
	int c, class, type, ttl, dbflags, dataflags, multiline;
	static int read_soa;	/* number of soa's read */
	struct databuf *dp;
	FILE *fp;
	int slineno, i, errs = 0, didinclude = 0;
	register u_int32_t n;
	struct stat sb;
	struct in_addr ina;

	if (!doinginclude) {
	    read_soa = 0;
	    clev = db_getclev(in_origin);
	}

	dprintf(1, (ddt,"db_load(%s, %s, %d, %d)\n",
		    filename, in_origin, zp - zones, doinginclude));

	(void) strcpy(origin, in_origin);
	if ((fp = fopen(filename, "r")) == NULL) {
		syslog(LOG_ERR, "%s: %m", filename);
		dprintf(1, (ddt, "db_load: error opening file %s\n",
			    filename));
		return (-1);
	}
	if (zp->z_type == Z_CACHE) {
		dbflags = DB_NODATA | DB_NOHINTS;
		dataflags = DB_F_HINT;
	} else {
		dbflags = DB_NODATA;
		dataflags = 0;
	}
	gettime(&tt);
	if (fstat(fileno(fp), &sb) < 0) {
		syslog(LOG_ERR, "%s: %m", filename);
		sb.st_mtime = (int)tt.tv_sec;
	}
	slineno = lineno;
	lineno = 1;
	domain[0] = '\0';
	class = zp->z_class;
	zp->z_flags &= ~(Z_INCLUDE|Z_DB_BAD);
 	while ((c = gettoken(fp, filename)) != EOF) {
		switch (c) {
		case INCLUDE:
			if (!getword((char *)buf, sizeof(buf), fp))
				/* file name*/
				break;
			if (!getword(tmporigin, sizeof(tmporigin), fp))
				strcpy(tmporigin, origin);
			else {
				makename(tmporigin, origin);
				endline(fp);
			}
			didinclude = 1;
			errs += db_load((char *)buf, tmporigin, zp, 1);
			continue;

		case ORIGIN:
			(void) strcpy((char *)buf, origin);
			if (!getword(origin, sizeof(origin), fp))
				break;
			dprintf(3, (ddt, "db_load: origin %s, buf %s\n",
				    origin, buf));
			makename(origin, buf);
			dprintf(3, (ddt, "db_load: origin now %s\n", origin));
			continue;

		case DNAME:
			if (!getword(domain, sizeof(domain), fp))
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
			/* fall thru ... */
		case CURRENT:
		gotdomain:
			if (!getword((char *)buf, sizeof(buf), fp)) {
				if (c == CURRENT)
					continue;
				break;
			}
			cp = buf;
			ttl = 0;
			if (isdigit(*cp)) {
				n = 0;
				do {
				    if (n > (INT_MAX - (*cp - '0')) / 10) {
					syslog(LOG_ERR, 
					   "%s: line %d: number > %lu\n",
					   filename, lineno, INT_MAX);
					dprintf(1, (ddt,
						"%s: line %d: number > %lu\n",
						filename, lineno, INT_MAX));
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
				if (!getword((char *)buf, sizeof(buf), fp))
					break;
			}
			for (mp = m_class; mp < m_class+NCLASS; mp++)
				if (!strcasecmp((char *)buf, mp->token)) {
					class = mp->val;
					(void) getword((char *)buf,
						       sizeof(buf), fp);
					break;
				}
			for (mp = m_type; mp < m_type+NTYPE; mp++)
				if (!strcasecmp((char *)buf, mp->token)) {
					type = mp->val;
					goto fndtype;
				}
			dprintf(1, (ddt, "%s: Line %d: Unknown type: %s.\n",
				    filename, lineno, buf));
			errs++;
 			syslog(LOG_ERR, "%s: Line %d: Unknown type: %s.\n",
				filename, lineno, buf);
			break;
		fndtype:
#ifdef ALLOW_T_UNSPEC
			/* Don't do anything here for T_UNSPEC...
			 * read input separately later
			 */
                        if (type != T_UNSPEC) {
#endif
			    if (!getword((char *)buf, sizeof(buf), fp))
				break;
			    dprintf(3,
				    (ddt,
				     "d='%s', c=%d, t=%d, ttl=%d, data='%s'\n",
				     domain, class, type, ttl, buf));
#ifdef ALLOW_T_UNSPEC
                        }
#endif
			/*
			 * Convert the ascii data 'buf' to the proper format
			 * based on the type and pack into 'data'.
			 */
			switch (type) {
			case T_A:
				if (!inet_aton(buf, &ina))
					goto err;
				n = ntohl(ina.s_addr);
				cp = data;
				PUTLONG(n, cp);
				n = INT32SZ;
				break;

			case T_HINFO:
			case T_ISDN:
				n = strlen((char *)buf);
				if (n > 255) {
				    syslog(LOG_WARNING,
					"%s: line %d: CPU type too long",
					filename, lineno);
				    n = 255;
				}
				data[0] = n;
				bcopy(buf, (char *)data + 1, (int)n);
				if (n == 0)
					goto err;
				n++;
				if (!getword((char *)buf, sizeof(buf), fp))
					i = 0;
				else {
					endline(fp);
					i = strlen((char *)buf);
				}
				if (i == 0) {
					/* goto err; */
						/* XXX tolerate for now */
					data[n++] = 1;
					data[n++] = '?';
					break;
				}
				if (i > 255) {
				    syslog(LOG_WARNING,
					   "%s:%d: OS type too long",
					   filename, lineno);
				    i = 255;
				}
				data[n] = i;
				bcopy(buf, data + n + 1, i);
				n += i + 1;
				break;

			case T_SOA:
			case T_MINFO:
			case T_RP:
				(void) strcpy((char *)data, (char *)buf);
				makename(data, origin);
				cp = data + strlen((char *)data) + 1;
				if (!getword((char *)cp,
					     sizeof(data) - (cp - data), fp))
					goto err;
				makename(cp, origin);
				cp += strlen((char *)cp) + 1;
				if (type != T_SOA) {
					n = cp - data;
					break;
				}
				if (class != zp->z_class) {
					syslog(LOG_WARNING,
					       "%s:%d: %s",
					       filename, lineno,
					       "SOA class not same as zone's");
				}
				c = getnonblank(fp, filename);
				if (c == '(') {
					multiline = 1;
				} else {
					multiline = 0;
					ungetc(c, fp);
				}
				zp->z_serial = getnum(fp, filename, 1);
				n = (u_int32_t) zp->z_serial;
				PUTLONG(n, cp);
				zp->z_refresh = getnum(fp, filename, 0);
				n = (u_int32_t) zp->z_refresh;
				PUTLONG(n, cp);
				if (zp->z_type == Z_SECONDARY
#if defined(STUBS) 
				    || zp->z_type == Z_STUB
#endif
				    ) {
					zp->z_time = sb.st_mtime
						   + zp->z_refresh;
				}
				zp->z_retry = getnum(fp, filename, 0);
				n = (u_int32_t) zp->z_retry;
				PUTLONG(n, cp);
				zp->z_expire = getnum(fp, filename, 0);
				n = (u_int32_t) zp->z_expire;
				PUTLONG (n, cp);
				zp->z_minimum = getnum(fp, filename, 0);
				n = (u_int32_t) zp->z_minimum;
				PUTLONG (n, cp);
				n = cp - data;
				if (multiline) {
					if (getnonblank(fp, filename) != ')')
						goto err;
				}
                                read_soa++;
				endline(fp);
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
			case T_CNAME:
			case T_MB:
			case T_MG:
			case T_MR:
			case T_PTR:
				(void) strcpy((char *)data, (char *)buf);
				makename(data, origin);
				n = strlen((char *)data) + 1;
				break;

			case T_UINFO:
				cp = strchr((char *)buf, '&');
				bzero(data, sizeof(data));
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
			case T_MX:
			case T_AFSDB:
			case T_RT:
				n = 0;
				cp = buf;
				while (isdigit(*cp))
					n = n * 10 + (*cp++ - '0');
				/* catch bad values */
				if ((cp == buf) || (n > 65535))
					goto err;

				cp = data;
				PUTSHORT((u_int16_t)n, cp);

				if (!getword((char *)buf, sizeof(buf), fp))
					goto err;
				(void) strcpy((char *)cp, (char *)buf);
				makename(cp, origin);
				/* advance pointer to end of data */
				cp += strlen((char *)cp) +1;

				/* now save length */
				n = (cp - data);
				break;

			case T_TXT:
			case T_X25:
                                cp = buf + (n = strlen(buf));
				while ((i = getc(fp), *cp = i, i != EOF)
                                       && *cp != '\n'
                                       && (n < MAXDATA)) {
                                  cp++; n++;
                                }
                                if (*cp == '\n') /* leave \n for getword */
                                    ungetc(*cp, fp);
                                *cp = '\0';
                                /* now do normal processing */

				i = strlen((char *)buf);
				cp = data;
				cp1 = buf;
				/*
				 * there is expansion here so make sure we
				 * don't overflow data
				 */
				if (i > sizeof(data) * 255 / 256) {
				    syslog(LOG_WARNING,
					"%s: line %d: TXT record truncated",
					filename, lineno);
				    i = sizeof(data) * 255 / 256;
				}
				while (i > 255) {
				    *cp++ = 255;
				    bcopy(cp1, cp, 255);
				    cp += 255;
				    cp1 += 255;
				    i -= 255;
				}
				*cp++ = i;
				bcopy(cp1, cp, i);
				cp += i;
				n = cp - data;
				endline(fp);
				break;

			case T_NSAP:
				n = inet_nsap_addr(buf, data, MAXDATA);
				if (n == 0)
					goto err;
				endline(fp);
				break;
#ifdef ALLOW_T_UNSPEC
                        case T_UNSPEC:
                                {
                                    int rcode;
                                    fgets(buf, sizeof(buf), fp);
				    dprintf(1, (ddt, "loading T_UNSPEC\n"));
				    if (rcode = atob(buf,
						     strlen((char*)buf),
						     data, sizeof(data),
						     &n)) {
					if (rcode == CONV_OVERFLOW) {
						dprintf(1,
							(ddt,
				       "Load T_UNSPEC: input buffer overflow\n"
							 )
							);
						errs++;
						syslog(LOG_ERR,
				       "Load T_UNSPEC: input buffer overflow");
					} else {
						dprintf(1,
							(ddt,
				     "Load T_UNSPEC: Data in bad atob format\n"
							 )
							);
						errs++;
						syslog(LOG_ERR,
				     "Load T_UNSPEC: Data in bad atob format");
					}
                                    }
                                }
                                break;
#endif /* ALLOW_T_UNSPEC */

			default:
				goto err;
			}
#ifdef STUBS
			if (type == T_SOA && zp->z_type == Z_STUB)
				continue;
#endif
#ifdef NO_GLUE
			/*
			 * Ignore data outside the zone.
			 */
			if (zp->z_type != Z_CACHE &&
			    !samedomain(domain, zp->z_origin))
			{
				syslog(LOG_WARNING,
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
					   (zp->z_type == Z_CACHE)
					   ? fcachetab
					   : hashtab))
			    != OK) {
#ifdef DEBUG
				if (debug && (c != DATAEXISTS))
					fprintf(ddt, "update failed %s %d\n", 
						domain, type);
#endif
				free((char*) dp);
			}
			continue;

		case ERROR:
			break;
		}
	err:
		errs++;
		syslog(LOG_ERR, "%s: line %d: database format error (%s)",
			filename, empty_token ? (lineno - 1) : lineno, buf);
		dprintf(1, (ddt,
			    "%s: line %d: database format error ('%s', %d)\n",
			    filename, empty_token ? (lineno - 1) : lineno,
			    buf, n));
		while ((c = getc(fp)) != EOF && c != '\n')
			;
		if (c == '\n')
			lineno++;
	}
	(void) my_fclose(fp);
	lineno = slineno;
	if (doinginclude == 0) {
		if (didinclude) {
			zp->z_flags |= Z_INCLUDE;
			zp->z_ftime = 0;
		} else
			zp->z_ftime = sb.st_mtime;
		zp->z_lastupdate = sb.st_mtime;
		if (zp->z_type != Z_CACHE && read_soa != 1) {
			errs++;
			if (read_soa == 0)
				syslog(LOG_ERR, "%s: no SOA record", filename);
			else
				syslog(LOG_ERR, "%s: multiple SOA records",
				    filename);
		}
	}
#ifdef SECURE_ZONES
	build_secure_netlist(zp);
#endif
	if (errs)
		zp->z_flags |= Z_DB_BAD;
	return (errs);
}

static int
gettoken(fp, src)
	register FILE *fp;
	char *src;
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
			if (getword(op, sizeof(op), fp)) {
				if (!strcasecmp("include", op))
					return (INCLUDE);
				if (!strcasecmp("origin", op))
					return (ORIGIN);
			}
			dprintf(1, (ddt,
				    "%s: line %d: Unknown $ option: $%s\n", 
				    src, lineno, op));
			syslog(LOG_ERR,"%s: line %d: Unknown $ option: $%s\n", 
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
 * getword(buf, size, fp)
 *	get next word, skipping blanks & comments.
 * parameters:
 *	buf - destination
 *	size - of destination
 *	fp - file to read from
 * return value:
 *	0 = no word; perhaps EOL or EOF
 *	1 = word was read
 */
int
getword(buf, size, fp)
	char *buf;
	int size;
	FILE *fp;
{
	register char *cp;
	register int c;

	empty_token = 0;
	for (cp = buf; (c = getc(fp)) != EOF; ) {
		if (c == ';') {
			while ((c = getc(fp)) != EOF && c != '\n')
				;
			c = '\n';
		}
		if (c == '\n') {
			if (cp != buf)
				ungetc(c, fp);
			else
				lineno++;
			break;
		}
		if (isspace(c)) {
			while (isspace(c = getc(fp)) && c != '\n')
				;
			ungetc(c, fp);
			if (cp != buf)		/* Trailing whitespace */
				break;
			continue;		/* Leading whitespace */
		}
		if (c == '"') {
			while ((c = getc(fp)) != EOF && c != '"' && c != '\n') {
				if (c == '\\') {
					if ((c = getc(fp)) == EOF)
						c = '\\';
					if (c == '\n')
						lineno++;
				}
				if (cp >= buf+size-1)
					break;
				*cp++ = c;
			}
			if (c == '\n') {
				lineno++;
				break;
			}
			if ((c = getc(fp)) != EOF)
				ungetc(c, fp);
			if (c == EOF || isspace(c) || c == '\n') {
				*cp = '\0';
				return (1);
			}
			else
				continue;
		}
		if (c == '\\') {
			if ((c = getc(fp)) == EOF)
				c = '\\';
			if (c == '\n')
				lineno++;
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
getnum(fp, src, is_serial)
	FILE *fp;
	char *src;
	int is_serial;
{
	register int c, n;
	int seendigit = 0;
	int seendecimal = 0;
	int m = 0;
	int allow_dots = 0;

#ifdef DOTTED_SERIAL
	allow_dots += is_serial;
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
		if (!isdigit(c)) {
			if (c == ')' && seendigit) {
				(void) ungetc(c, fp);
				break;
			}
			if (seendecimal || c != '.' || !allow_dots) {
				syslog(LOG_ERR, "%s:%d: expected a number",
				       src, lineno);
				dprintf(1, (ddt, "%s:%d: expected a number",
					    src, lineno));
				exit(1);	/* XXX why exit here?? */
			} else {
				if (!seendigit)
					n = 1;
#ifdef SENSIBLE_DOTS
				n = n * 10000;
#else
				n = n * 1000;
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
	if (m > 9999) {
		syslog(LOG_ERR, 
		       "%s:%d: number after the decimal point exceeds 9999", 
		       src, lineno);
		dprintf(1, (ddt,
			"%s:%d: number after the decimal point exceeds 9999", 
			    src, lineno));
		exit(1);	/* XXX why exit here?? */
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
	char *src;
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
	syslog(LOG_ERR, "%s: line %d: unexpected EOF", src, lineno);
	dprintf(1, (ddt, "%s: line %d: unexpected EOF", src, lineno));
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
	char *name, *origin;
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

void
endline(fp)
	register FILE *fp;
{
	register int c;

	while (c = getc(fp)) {
		if (c == '\n') {
			(void) ungetc(c,fp);
			break;
		} else if (c == EOF) {
			break;
		}
	}
}

#define MAXPORT 256
#define MAXLEN 24

static int
getprotocol(fp, src)
	FILE *fp;
	char *src;
{
	int  k;
	char b[MAXLEN];

	(void) getword(b, sizeof(b), fp);
		
	k = protocolnumber(b);
	if(k == -1)
		syslog(LOG_ERR, "%s: line %d: unknown protocol: %s.",
			src, lineno, b);
	return(k);
}

static int
getservices(n, data, fp, src)
	int n;
	char *data, *src;
	FILE *fp;
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
	while (getword(b, sizeof(b), fp) || bracket) {
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
			syslog(LOG_WARNING,
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
			syslog(LOG_WARNING,
			    "%s: line %d: port no. (%d) too big\n",
				src, lineno, k);
			dprintf(1, (ddt,
				    "%s: line %d: port no. (%d) too big\n",
				    src, lineno, k));
		}
	}
	if (bracket)
		syslog(LOG_WARNING, "%s: line %d: missing close paren\n",
		    src, lineno);
	maxl = maxl/8+1;
	bcopy(bm, data+n, maxl);
	return(maxl+n);
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
	struct netinfo *ntp = NULL, **end = netlistp;
	char buf[BUFSIZ], *maskp;
	struct in_addr ina;

	dprintf(1, (ddt, "get_netlist(%s)", print_tag));
	while (getword(buf, sizeof(buf), fp)) {
		if (strlen(buf) == 0)
			break;
		if ((maskp = strchr(buf, '&')) != NULL)
			*maskp++ = '\0';
		dprintf(1, (ddt," %s", buf));
		if (ntp == NULL) {
			ntp = (struct netinfo *)malloc(sizeof(struct netinfo));
		}
		if (!inet_aton(buf, &ntp->my_addr)) {
			syslog(LOG_ERR, "%s contains bogus element (%s)",
			       print_tag, buf);
			continue;	
		}
		if (maskp) {
			if (!inet_aton(maskp, &ina)) {
				syslog(LOG_ERR,
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
			syslog(LOG_WARNING,
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
			fprintf(ddt, "ntp x%x addr x%x mask x%x",
				ntp, ntp->addr, ntp->mask);
			fprintf(ddt, " my_addr x%x", ntp->my_addr);
			fprintf(ddt, " %s", inet_ntoa(ntp->my_addr));
			fprintf(ddt, " next x%x\n", ntp->next);
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
