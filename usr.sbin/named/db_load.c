/*
 * Copyright (c) 1986, 1988, 1990 Regents of the University of California.
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
 */

#ifndef lint
static char sccsid[] = "@(#)db_load.c	4.38 (Berkeley) 3/2/91";
#endif /* not lint */

/*
 * Load data base from ascii backupfile.  Format similar to RFC 883.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <stdio.h>
#include <syslog.h>
#include <ctype.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include "ns.h"
#include "db.h"

extern char *index();
extern int max_cache_ttl;

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
#define NCLASS (sizeof(m_class)/sizeof(struct map))

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
	"uid",		T_UID,
	"gid",		T_GID,
#ifdef notdef
	"any",		T_ANY,		/* any is a QTYPE, not TYPE */
#endif
#ifdef ALLOW_T_UNSPEC
        "unspec",       T_UNSPEC,
#endif ALLOW_T_UNSPEC
};
#define NTYPE (sizeof(m_type)/sizeof(struct map))

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

int	lineno;		/* current line number */

/*
 * Load the database from 'filename'. Origin is appended to all domain
 * names in the file.
 */
db_load(filename, in_origin, zp, doinginclude)
	char *filename, *in_origin;
	struct zoneinfo *zp;
{
	register u_char *cp;
	register struct map *mp;
	char domain[MAXDNAME];
	char origin[MAXDNAME];
	char tmporigin[MAXDNAME];
	u_char buf[MAXDATA];
	u_char data[MAXDATA];
	u_char *cp1;
	char *op;
	int c;
	int class, type, ttl, dbflags, dataflags;
        int read_soa = 0;	/* number of soa's read */
	struct databuf *dp;
	FILE *fp;
	int slineno, i, errs = 0, didinclude = 0;
	register u_long n;
	struct stat sb;

#ifdef DEBUG
	if (debug)
		fprintf(ddt,"db_load(%s, %s, %d, %d)\n",
		    filename, in_origin, zp - zones, doinginclude);
#endif

	(void) strcpy(origin, in_origin);
	if ((fp = fopen(filename, "r")) == NULL) {
		syslog(LOG_ERR, "%s: %m", filename);
#ifdef DEBUG
		if (debug)
		    fprintf(ddt,"db_load: error opening file %s\n", filename);
#endif
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
	class = C_IN;
	zp->z_state &= ~(Z_INCLUDE|Z_DB_BAD);
	while ((c = gettoken(fp)) != EOF) {
		switch (c) {
		case INCLUDE:
			if (!getword(buf, sizeof(buf), fp)) /* file name */
				break;
			if (!getword(tmporigin, sizeof(tmporigin), fp))
				strcpy(tmporigin, origin);
			else {
				makename(tmporigin, origin);
				endline(fp);
			}
			didinclude = 1;
			errs += db_load(buf, tmporigin, zp, 1);
			continue;

		case ORIGIN:
			(void) strcpy((char *)buf, origin);
			if (!getword(origin, sizeof(origin), fp))
				break;
#ifdef DEBUG
			if (debug > 3)
				fprintf(ddt,"db_load: origin %s, buf %s\n",
				    origin, buf);
#endif
			makename(origin, buf);
#ifdef DEBUG
			if (debug > 3)
				fprintf(ddt,"db_load: origin now %s\n", origin);
#endif
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
			if (!getword(buf, sizeof(buf), fp)) {
				if (c == CURRENT)
					continue;
				break;
			}
			cp = buf;
			ttl = 0;
			if (isdigit(*cp)) {
				n = 0;
				do
					n = n * 10 + (*cp++ - '0');
				while (isdigit(*cp));
				if (zp->z_type == Z_CACHE) {
				    /* this allows the cache entry to age */
				    /* while sitting on disk (powered off) */
				    if (n > max_cache_ttl)
					n = max_cache_ttl;
				    n += sb.st_mtime;
				}
				ttl = n;
				if (!getword(buf, sizeof(buf), fp))
					break;
			}
			for (mp = m_class; mp < m_class+NCLASS; mp++)
				if (!strcasecmp((char *)buf, mp->token)) {
					class = mp->val;
					(void) getword(buf, sizeof(buf), fp);
					break;
				}
			for (mp = m_type; mp < m_type+NTYPE; mp++)
				if (!strcasecmp((char *)buf, mp->token)) {
					type = mp->val;
					goto fndtype;
				}
#ifdef DEBUG
			if (debug)
				fprintf(ddt,"Line %d: Unknown type: %s.\n",
					lineno, buf);
#endif
			errs++;
 			syslog(LOG_ERR, "Line %d: Unknown type: %s.\n",
				lineno, buf);
			break;
		fndtype:
#ifdef ALLOW_T_UNSPEC
			/* Don't do anything here for T_UNSPEC...
			 * read input separately later
			 */
                        if (type != T_UNSPEC) {
#endif ALLOW_T_UNSPEC
			    if (!getword(buf, sizeof(buf), fp))
				break;
#ifdef DEBUG
			    if (debug >= 3)
			        fprintf(ddt,
				    "d='%s', c=%d, t=%d, ttl=%d, data='%s'\n",
				    domain, class, type, ttl, buf);
#endif
#ifdef ALLOW_T_UNSPEC
                        }
#endif ALLOW_T_UNSPEC
			/*
			 * Convert the ascii data 'buf' to the proper format
			 * based on the type and pack into 'data'.
			 */
			switch (type) {
			case T_A:
				n = ntohl((u_long)inet_addr((char *)buf));
				cp = data;
				PUTLONG(n, cp);
				n = sizeof(u_long);
				break;

			case T_HINFO:
				n = strlen((char *)buf);
				if (n > 255) {
				    syslog(LOG_WARNING,
					"%s: line %d: CPU type too long",
					filename, lineno);
				    n = 255;
				}
				data[0] = n;
				bcopy(buf, (char *)data + 1, (int)n);
				n++;
				if (!getword(buf, sizeof(buf), fp))
					break;
				i = strlen((char *)buf);
				if (i > 255) {
				    syslog(LOG_WARNING,
					"%s: line %d: OS type too long",
					filename, lineno);
				    i = 255;
				}
				data[n] = i;
				bcopy(buf, data + n + 1, i);
				n += i + 1;
				endline(fp);
				break;

			case T_SOA:
			case T_MINFO:
				(void) strcpy((char *)data, (char *)buf);
				makename(data, origin);
				cp = data + strlen((char *)data) + 1;
				if (!getword(cp,
				    sizeof(data) - (cp - data),fp)) {
					n = cp - data;
					break;
				}
				makename(cp, origin);
				cp += strlen((char *)cp) + 1;
				if (type == T_MINFO) {
					n = cp - data;
					break;
				}
				if (getnonblank(fp) != '(')
					goto err;
				zp->z_serial = getnum(fp);
				n = (u_long) zp->z_serial;
				PUTLONG(n, cp);
				zp->z_refresh = getnum(fp);
				n = (u_long) zp->z_refresh;
				PUTLONG(n, cp);
				if (zp->z_type == Z_SECONDARY)
					zp->z_time = sb.st_mtime + zp->z_refresh;
				zp->z_retry = getnum(fp);
				n = (u_long) zp->z_retry;
				PUTLONG(n, cp);
				zp->z_expire = getnum(fp);
				n = (u_long) zp->z_expire;
				PUTLONG (n, cp);
				zp->z_minimum = getnum(fp);
				n = (u_long) zp->z_minimum;
				PUTLONG (n, cp);
				n = cp - data;
				if (getnonblank(fp) != ')')
					goto err;
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
				n = sizeof(long);
				break;

			case T_WKS:
				/* Address */
				n = ntohl((u_long)inet_addr((char *)buf));
				cp = data;
				PUTLONG(n, cp);
				*cp = getprotocol(fp, filename);
				/* Protocol */
				n = sizeof(u_long) + sizeof(char);
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
				cp = (u_char *)index((char *)buf, '&');
				bzero(data, sizeof(data));
				if ( cp != NULL) {
					(void) strncpy((char *)data,
					    (char *)buf, cp - buf);
					op = index(domain, '.');
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
				n = 0;
				cp = buf;
				while (isdigit(*cp))
					n = n * 10 + (*cp++ - '0');
				/* catch bad values */
				if ((cp == buf) || (n > 65535))
					goto err;

				cp = data;
				PUTSHORT((u_short)n, cp);

				if (!getword(buf, sizeof(buf), fp))
					    break;
				(void) strcpy((char *)cp, (char *)buf);
				makename(cp, origin);
				/* get pointer to place in data */
				cp += strlen((char *)cp) +1;

				/* now save length */
				n = (cp - data);
				break;

			case T_TXT:
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
#ifdef ALLOW_T_UNSPEC
                        case T_UNSPEC:
                                {
                                    int rcode;
                                    fgets(buf, sizeof(buf), fp);
#ifdef DEBUG
				    if (debug)
                                    	fprintf(ddt, "loading T_UNSPEC\n");
#endif DEBUG
                                    if (rcode = atob(buf,
					strlen((char *)buf), data,
					sizeof(data), &n)) {
                                        if (rcode == CONV_OVERFLOW) {
#ifdef DEBUG
                                            if (debug)
                                               fprintf(ddt,
		       				   "Load T_UNSPEC: input buffer overflow\n");
#endif DEBUG
					    errs++;
                                            syslog(LOG_ERR,
						 "Load T_UNSPEC: input buffer overflow");
                                         } else {
#ifdef DEBUG
                                            if (debug)
                                                fprintf(ddt,
						   "Load T_UNSPEC: Data in bad atob format\n");
#endif DEBUG
					    errs++;
                                            syslog(LOG_ERR,
						   "Load T_UNSPEC: Data in bad atob format");
                                         }
                                    }
                                }
                                break;
#endif ALLOW_T_UNSPEC

			default:
				goto err;
			}
			dp = savedata(class, type, (u_long)ttl, data, (int)n);
			dp->d_zone = zp - zones;
			dp->d_flags = dataflags;
			if ((c = db_update(domain, dp, dp, dbflags,
			   (zp->z_type == Z_CACHE)? fcachetab : hashtab)) < 0) {
#ifdef DEBUG
				if (debug && (c != DATAEXISTS))
					fprintf(ddt,"update failed\n");
#endif
			}
			continue;

		case ERROR:
			break;
		}
	err:
		errs++;
		syslog(LOG_ERR, "%s: line %d: database format error (%s)",
			filename, lineno, buf);
#ifdef DEBUG
		if (debug)
			fprintf(ddt,"%s: line %d: database format error ('%s', %d)\n",
				filename, lineno, buf, n);
#endif
		while ((c = getc(fp)) != EOF && c != '\n')
			;
		if (c == '\n')
			lineno++;
	}
	(void) fclose(fp);
	lineno = slineno;
	if (doinginclude == 0) {
		if (didinclude) {
			zp->z_state |= Z_INCLUDE;
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
	if (errs)
		zp->z_state |= Z_DB_BAD;
	return (errs);
}

int gettoken(fp)
	register FILE *fp;
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
#ifdef DEBUG
			if (debug)
				fprintf(ddt,"Line %d: Unknown $ option: $%s\n", 
				    lineno, op);
#endif
			syslog(LOG_ERR,"Line %d: Unknown $ option: $%s\n", 
			    lineno, op);
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

/*
 * Get next word, skipping blanks & comments.
 */
getword(buf, size, fp)
	char *buf;
	int size;
	FILE *fp;
{
	register char *cp;
	register int c;

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
		*cp++ = c;
	}
	*cp = '\0';
	return (cp != buf);
}

getnum(fp)
	FILE *fp;
{
	register int c, n;
	int seendigit = 0;
	int seendecimal = 0;

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
			if (seendecimal || c != '.') {
				syslog(LOG_ERR, "line %d: expected a number",
				lineno);
#ifdef DEBUG
				if (debug)
				    fprintf(ddt,"line %d: expected a number",
				        lineno);
#endif
				exit(1);	/* XXX why exit */
			} else {
				if (!seendigit)
					n = 1;
				n = n * 1000 ;
				seendigit = 1;
				seendecimal = 1;
			}
			continue;
		}
		n = n * 10 + (c - '0');
		seendigit = 1;
	}
	return (n);
}

getnonblank(fp)
	FILE *fp;
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
	syslog(LOG_ERR, "line %d: unexpected EOF", lineno);
#ifdef DEBUG
	if (debug)
		fprintf(ddt, "line %d: unexpected EOF", lineno);
#endif
	return (EOF);
}

/*
 * Take name and fix it according to following rules:
 * "." means root.
 * "@" means current origin.
 * "name." means no changes.
 * "name" means append origin.
 */
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

endline(fp)
	register FILE *fp;
{
     register int c;
     while (c = getc(fp))
	if (c == '\n') {
	    (void) ungetc(c,fp);
	    break;
	} else if (c == EOF)
	    break;
}

#define MAXPORT 256
#define MAXLEN 24

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

int
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
			syslog(LOG_WARNING, "%s: line %d: Unknown service '%s'",
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
#ifdef DEBUG
			if (debug)
				fprintf(ddt,
				    "%s: line %d: port no. (%d) too big\n",
					src, lineno, k);
#endif
		}
	}
	if (bracket)
		syslog(LOG_WARNING, "%s: line %d: missing close paren\n",
		    src, lineno);
	maxl = maxl/8+1;
	bcopy(bm, data+n, maxl);
	return(maxl+n);
}

get_sort_list(fp)
	FILE *fp;
{
	extern struct netinfo **enettab;
	register struct netinfo *ntp, **end = enettab;
	extern struct netinfo *findnetinfo();
	struct in_addr addr;
	char buf[BUFSIZ];

#ifdef DEBUG
	if (debug)
		fprintf(ddt,"sortlist ");
#endif
	while (getword(buf, sizeof(buf), fp)) {
		if (strlen(buf) == 0)
			break;
#ifdef DEBUG
		if (debug)
			fprintf(ddt," %s",buf);
#endif
		addr.s_addr = inet_addr(buf);
		if (addr.s_addr == (unsigned)-1) {
			/* resolve name to address - XXX */
			continue;	
		}
		/* Check for duplicates, then add to linked list */
		if (findnetinfo(addr))
			continue;
		ntp = (struct netinfo *)malloc(sizeof(struct netinfo));
		ntp->my_addr = addr;
		ntp->next = NULL;
		ntp->mask = net_mask(ntp->my_addr);
		ntp->net = ntp->my_addr.s_addr & ntp->mask;
		if (ntp->net != addr.s_addr) {
			struct in_addr tmpaddr;

			tmpaddr.s_addr = ntp->net;
			syslog(LOG_WARNING, "sortlist: addr %s != %s", buf,
				inet_ntoa(tmpaddr));
#ifdef DEBUG
			if (debug)
				fprintf(ddt, "\nsortlist: addr %s != %s\n", buf,
					inet_ntoa(tmpaddr));
#endif
		}

		*end = ntp;
		end = &ntp->next;
	}
	
#ifdef DEBUG
	if (debug) 
		fprintf(ddt,"\n");
	if (debug > 2)
		printnetinfo(*enettab);
	if (debug > 4) {
		extern struct netinfo *nettab;

		fprintf(ddt, "\nFull sort list:\n");
		printnetinfo(nettab);
	}
#endif
}

free_sort_list()
{
	extern struct netinfo **enettab;
	register struct netinfo *ntp, *next;

	for (ntp = *enettab; ntp != NULL; ntp = next) {
		next = ntp->next;
		free((char *)ntp);
	}
	*enettab = NULL;
}
