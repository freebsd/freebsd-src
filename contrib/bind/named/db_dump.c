#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)db_dump.c	4.33 (Berkeley) 3/3/91";
static char rcsid[] = "$Id: db_dump.c,v 8.19 1996/10/08 04:51:03 vixie Exp $";
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <syslog.h>
#include <resolv.h>
#include <errno.h>

#include "named.h"

static int		scan_root __P((struct hashbuf *));
static const char	*MkCredStr __P((int));

#ifdef ALLOW_T_UNSPEC
static void		putbyte __P((int, char **));
#endif

/*
 * Dump current cache in a format similar to RFC 883.
 *
 * We try to be careful and determine whether the operation succeeded
 * so that the new cache file can be installed.
 */

void
doachkpt()
{
    FILE *fp;
    char tmpcheckfile[256];

    /* nowhere to checkpoint cache... */
    if (cache_file == NULL) {
	dprintf(3, (ddt, "skipping doachkpt (cache_file == NULL)\n"));
        return;
    }

    dprintf(3, (ddt, "doachkpt()\n"));

    (void) sprintf(tmpcheckfile, "%s.chk", cache_file);
    if ((fp = fopen(tmpcheckfile, "w")) == NULL) {
	dprintf(3, (ddt,
		    "doachkpt(can't open %s for write)\n", tmpcheckfile));
        return;
    }

    (void) gettime(&tt);
    fprintf(fp, "; Dumped at %s", ctimel(tt.tv_sec));
    fflush(fp);
    if (ferror(fp)) {
	dprintf(3, (ddt, "doachkpt(write to checkpoint file failed)\n"));
        return;
    }

    if (fcachetab != NULL) {
	int n = scan_root(hashtab);

	if (n < MINROOTS) {
	    syslog(LOG_NOTICE, "%d root hints... (too low)", n);
	    fprintf(fp, "; ---- Root hint cache dump ----\n");
	    (void) db_dump(fcachetab, fp, DB_Z_CACHE, "");
	}
    }

    if (hashtab != NULL) {
        fprintf(fp, "; ---- Cache dump ----\n");
        if (db_dump(hashtab, fp, DB_Z_CACHE, "") == NODBFILE) {
	    dprintf(3, (ddt, "doachkpt(checkpoint failed)\n"));
            (void) my_fclose(fp);
            return;
        }
    }

    if (my_fclose(fp) == EOF) {
        return;
    }

    if (rename(tmpcheckfile, cache_file)) {
	dprintf(3, (ddt, "doachkpt(install %s to %s failed, %d)\n",
		    tmpcheckfile, cache_file, errno));
    }
}

/*
 * What we do is scan the root hint cache to make sure there are at least
 * MINROOTS root pointers with non-0 TTL's so that the checkpoint will not
 * lose the root.  Failing this, all pointers are written out w/ TTL ~0
 * (root pointers timed out and prime_cache() not done or failed).
 */

static int
scan_root(htp)
	struct hashbuf *htp;
{
	register struct databuf *dp;
	register struct namebuf *np;
	struct timeval soon;
	int roots = 0;

	dprintf(1, (ddt, "scan_root(0x%lx)\n", (u_long)htp));

	/* metric by which we determine whether a root NS pointer is still */
	/* valid (will be written out if we do a dump).  we also add some */
	/* time buffer for safety... */
	(void) gettime(&soon);
	soon.tv_sec += TIMBUF;

	for (np = htp->h_tab[0]; np != NULL; np = np->n_next) {
		if (NAME(*np)[0] == '\0') {
			dp = np->n_data;
			while (dp != NULL) {
				if (dp->d_type == T_NS &&
				    dp->d_ttl > soon.tv_sec) {
					roots++;
					if (roots >= MINROOTS)
						return (roots);
				}
				dp = dp->d_next;
			}
		}
	}
	return (roots);
}

#ifdef notdef
mark_cache(htp, ttl)
    struct hashbuf *htp;
    int ttl;
{
    register struct databuf *dp;
    register struct namebuf *np;
    struct namebuf **npp, **nppend;
    struct timeval soon;

    dprintf(1, (ddt, "mark_cache()\n"));

    (void) gettime(&soon);
    soon.tv_sec += TIMBUF;

    npp = htp->h_tab;
    nppend = npp + htp->h_size;
    while (npp < nppend) {
        for (np = *npp++; np != NULL; np = np->n_next) {
            if (np->n_data == NULL)
                continue;
            for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
                if (dp->d_ttl < soon.tv_sec)
                    dp->d_ttl = ttl;
            }
        }
    }

    npp = htp->h_tab;
    nppend = npp + htp->h_size;
    while (npp < nppend) {
        for (np = *npp++; np != NULL; np = np->n_next) {
            if (np->n_hash == NULL)
                continue;
            mark_cache(np->n_hash, ttl);
        }
    }
}
#endif /* notdef */

/*
 * Dump current data base in a format similar to RFC 883.
 */

void
doadump()
{
	FILE	*fp;

	dprintf(3, (ddt, "doadump()\n"));
	syslog(LOG_NOTICE, "dumping nameserver data\n");

	if ((fp = fopen(dumpfile, "w")) == NULL)
		return;
	gettime(&tt);
	fprintf(fp, "; Dumped at %s", ctimel(tt.tv_sec));
	if (zones && nzones)
		zt_dump(fp);
	fputs(
"; Note: Cr=(auth,answer,addtnl,cache) tag only shown for non-auth RR's\n",
	      fp);
	fputs(
"; Note: NT=milliseconds for any A RR which we've used as a nameserver\n",
	      fp);
	fprintf(fp, "; --- Cache & Data ---\n");
	if (hashtab != NULL)
		(void) db_dump(hashtab, fp, DB_Z_ALL, "");
	fprintf(fp, "; --- Hints ---\n");
	if (fcachetab != NULL)
		(void) db_dump(fcachetab, fp, DB_Z_ALL, "");
	(void) my_fclose(fp);
	syslog(LOG_NOTICE, "finished dumping nameserver data\n");
}

int
zt_dump(fp)
	FILE *fp;
{
	register struct zoneinfo *zp;

	fprintf(fp, ";; ++zone table++\n");
        for (zp = &zones[1]; zp < &zones[nzones]; zp++) {
		char *pre, buf[64];
		u_int cnt;

		if (!zp->z_origin)
			continue;

		fprintf(fp, "; %s (type %d, class %d, source %s)\n",
			zp->z_origin
			  ? (*zp->z_origin ? zp->z_origin : ".")
			  : "Nil",
			zp->z_type, zp->z_class,
			zp->z_source ? zp->z_source : "Nil");
		fprintf(fp, ";\ttime=%lu, lastupdate=%lu, serial=%u,\n",
			(u_long)zp->z_time, (u_long)zp->z_lastupdate,
			zp->z_serial);
		fprintf(fp, ";\trefresh=%u, retry=%u, expire=%u, minimum=%u\n",
			zp->z_refresh, zp->z_retry,
			zp->z_expire, zp->z_minimum);
		fprintf(fp, ";\tftime=%lu, xaddr=[%s], state=%04x, pid=%d\n",
			(u_long)zp->z_ftime, inet_ntoa(zp->z_xaddr),
			zp->z_flags, (int)zp->z_xferpid);
		sprintf(buf, ";\tz_addr[%d]: ", zp->z_addrcnt);
		pre = buf;
		for (cnt = 0;  cnt < zp->z_addrcnt;  cnt++) {
			fprintf(fp, "%s[%s]", pre, inet_ntoa(zp->z_addr[cnt]));
			pre = ", ";
		}
		if (zp->z_addrcnt)
			fputc('\n', fp);
#ifdef BIND_NOTIFY
		if (zp->z_notifylist) {
			register struct notify *ap;

			for (ap = zp->z_notifylist; ap; ap = ap->next)
				fprintf(fp, ";\tNotify [%s] %s",
					inet_ntoa(ap->addr),
					ctime(&ap->last));
		}
#endif
	}
	fprintf(fp, ";; --zone table--\n");
	return (0);
}

int
db_dump(htp, fp, zone, origin)
	struct hashbuf *htp;
	FILE *fp;
	int zone;
	char *origin;
{
	register struct databuf *dp = NULL;
	register struct namebuf *np;
	struct namebuf **npp, **nppend;
	char dname[MAXDNAME];
	u_int32_t n;
	u_int32_t addr;
	int j, i;
	register u_char *cp;
	u_char *end;
	char *proto, *sep;
	int16_t type;
	int found_data = 0, tab, printed_origin = 0;
	u_int16_t keyflags;
	u_char *sigdata;
	u_char *savecp;
	char temp_base64[MAX_KEY_BASE64];

	npp = htp->h_tab;
	nppend = npp + htp->h_size;
	while (npp < nppend) {
	    for (np = *npp++; np != NULL; np = np->n_next) {
		if (np->n_data == NULL)
			continue;
		/* Blecch - can't tell if there is data here for the
		 * right zone, so can't print name yet
		 */
		found_data = 0;
		/* we want a snapshot in time... */
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			/* Is the data for this zone? */
			if (zone != DB_Z_ALL && dp->d_zone != zone)
			    continue;
			if (dp->d_zone == DB_Z_CACHE &&
			    dp->d_ttl <= tt.tv_sec &&
			    (dp->d_flags & DB_F_HINT) == 0)
				continue;
			if (!printed_origin) {
			    fprintf(fp, "$ORIGIN %s.\n", origin);
			    printed_origin++;
			}
			tab = 0;
#ifdef NCACHE
			if (dp->d_rcode == NXDOMAIN ||
			    dp->d_rcode == NOERROR_NODATA) {
				fputc(';', fp);
			} else if (found_data == 0 || found_data == 1) {
			    found_data = 2;
			}
#endif /*NCACHE*/
			if (found_data == 0 || found_data == 2) {
			    if (NAME(*np)[0] == '\0') {
				if (origin[0] == '\0')
				    fprintf(fp, ".\t");
				else
				    fprintf(fp, ".%s.\t", origin); /* ??? */
			    } else
				fprintf(fp, "%s\t", NAME(*np));
			    if (NAMELEN(*np) < (unsigned)8)
				tab = 1;
			    found_data++;
			} else {
			    (void) putc('\t', fp);
			    tab = 1;
			}
			if (dp->d_zone == DB_Z_CACHE) {
			    if (dp->d_flags & DB_F_HINT
				&& (int32_t)(dp->d_ttl - tt.tv_sec)
				    < DB_ROOT_TIMBUF)
				    fprintf(fp, "%d\t", DB_ROOT_TIMBUF);
			    else
				    fprintf(fp, "%d\t",
				        (int)(dp->d_ttl - tt.tv_sec));
			} else if (dp->d_ttl != USE_MINIMUM &&
			    dp->d_ttl != zones[dp->d_zone].z_minimum)
				fprintf(fp, "%d\t", (int)dp->d_ttl);
			else if (tab)
			    (void) putc('\t', fp);
			fprintf(fp, "%s\t%s\t",
				p_class(dp->d_class),
				p_type(dp->d_type));
			cp = (u_char *)dp->d_data;
			sep = "\t;";
			type = dp->d_type;
#ifdef NCACHE
			if (dp->d_rcode == NXDOMAIN ||
			    dp->d_rcode == NOERROR_NODATA) {
#ifdef RETURNSOA
				if (dp->d_size == 0) {
#endif
				fprintf(fp, "%s%s-$",
					(dp->d_rcode == NXDOMAIN)
						?"NXDOMAIN" :"NODATA",
					sep);
				goto eoln;
#ifdef RETURNSOA
				} else {
					type = T_SOA;
				}
#endif
			}
#endif
			/*
			 * Print type specific data
			 */
			switch (type) {
			case T_A:
				switch (dp->d_class) {
				case C_IN:
				case C_HS:
					GETLONG(n, cp);
					n = htonl(n);
					fputs(inet_ntoa(*(struct in_addr *)&n),
					      fp);
					break;
				}
				if (dp->d_nstime) {
					fprintf(fp, "%sNT=%d",
						sep, dp->d_nstime);
					sep = " ";
				}
				break;
			case T_CNAME:
			case T_MB:
			case T_MG:
			case T_MR:
			case T_PTR:
				fprintf(fp, "%s.", cp);
				break;

			case T_NS:
				cp = (u_char *)dp->d_data;
				if (cp[0] == '\0')
					fprintf(fp, ".\t");
				else
					fprintf(fp, "%s.", cp);
				break;

			case T_HINFO:
			case T_ISDN: {
				char buf[256];
				if ((n = *cp++) != '\0') {
					bcopy(cp, buf, n); buf[n] = '\0';
					fprintf(fp, "\"%.*s\"", (int)n, buf);
					cp += n;
				} else
					fprintf(fp, "\"\"");
				if ((n = *cp++) != '\0') {
					bcopy(cp, buf, n); buf[n] = '\0';
					fprintf(fp, " \"%.*s\"", (int)n, buf);
				} else
					fprintf(fp, " \"\"");
				break;
			}

			case T_SOA:
				fprintf(fp, "%s.", cp);
				cp += strlen((char *)cp) + 1;
				fprintf(fp, " %s. (\n", cp);
#if defined(RETURNSOA) && defined(NCACHE)
				if (dp->d_rcode)
					fputs(";", fp);
#endif
				cp += strlen((char *)cp) + 1;
				GETLONG(n, cp);
				fprintf(fp, "\t\t%lu", (u_long)n);
				GETLONG(n, cp);
				fprintf(fp, " %lu", (u_long)n);
				GETLONG(n, cp);
				fprintf(fp, " %lu", (u_long)n);
				GETLONG(n, cp);
				fprintf(fp, " %lu", (u_long)n);
				GETLONG(n, cp);
				fprintf(fp, " %lu )", (u_long)n);
#if defined(RETURNSOA) && defined(NCACHE)
				if (dp->d_rcode) {
					fprintf(fp,";%s.;%s%s-$",cp,
						(dp->d_rcode == NXDOMAIN) ?
						"NXDOMAIN" : "NODATA",
						sep);
				}
#endif
				break;

			case T_MX:
			case T_AFSDB:
			case T_RT:
				GETSHORT(n, cp);
				fprintf(fp, "%lu", (u_long)n);
				fprintf(fp, " %s.", cp);
				break;

			case T_PX:
				GETSHORT(n, cp);
				fprintf(fp, "%lu", (u_long)n);
				fprintf(fp, " %s.", cp);
				cp += strlen((char *)cp) + 1;
				fprintf(fp, " %s.", cp);
				break;

			case T_X25:
				if ((n = *cp++) != '\0')
					fprintf(fp, " \"%.*s\"", (int)n, cp);
				else
					fprintf(fp, " \"\"");
				break;

			case T_TXT:
				end = (u_char *)dp->d_data + dp->d_size;
				while (cp < end) {
					(void) putc('"', fp);
					if ((n = *cp++) != '\0') {
						for (j = n ; j > 0 && cp < end ; j--) {
							if (*cp == '\n' || *cp == '"' || *cp == '\\')
								(void) putc('\\', fp);
							(void) putc(*cp++, fp);
						}
					}
					(void) putc('"', fp);
					if (cp < end)
						(void) putc(' ', fp);
				}
				break;

			case T_NSAP:
				(void) fputs(inet_nsap_ntoa(dp->d_size,
							    dp->d_data, NULL),
					     fp);
				break;
			case T_AAAA: {
				char t[sizeof
				"ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"
				      ];

				(void) fputs(inet_ntop(AF_INET6, dp->d_data,
						       t, sizeof t),
					     fp);
				break;
			}
#ifdef LOC_RR
			case T_LOC: {
				char t[256];

				(void) fputs(loc_ntoa(dp->d_data, t), fp);
				break;
			}
#endif /* LOC_RR */

			case T_NAPTR: {
				u_int order, preference;

				GETSHORT(order, cp);
				fprintf(fp, "%lu", (u_long)order);
 
				GETSHORT(preference, cp);
				fprintf(fp, "%lu", (u_long)preference);

				if (n = *cp++) {
					fprintf(fp, "\"%.*s\"", (int)n, cp);
					cp += n;
				}
				if (n = *cp++) {
					fprintf(fp, "\"%.*s\"", (int)n, cp);
					cp += n;
				}
				if (n = *cp++) {
					fprintf(fp, " \"%.*s\"", (int)n, cp);
					cp += n;
				}
				fprintf(fp, " %s.", cp);

				break;
			}

			case T_SRV: {
				u_int priority, weight, port;

				GETSHORT(priority, cp);
				GETSHORT(weight, cp);
				GETSHORT(port, cp);
				fprintf(fp, "\t%u %u %u %s.",
					priority, weight, port, cp);
				break;
			}
			case T_UINFO:
				fprintf(fp, "\"%s\"", cp);
				break;

			case T_UID:
			case T_GID:
				if (dp->d_size == INT32SZ) {
					GETLONG(n, cp);
				} else {
					n = -2;		/* XXX - hack */
				}
				fprintf(fp, "%u", n);
				break;

			case T_WKS:
				GETLONG(addr, cp);	
				addr = htonl(addr);	
				fputs(inet_ntoa(*(struct in_addr *)&addr), fp);
				proto = protocolname(*cp);
				cp += sizeof(char); 
				fprintf(fp, " %s ", proto);
				i = 0;
				while(cp < (u_char *)dp->d_data + dp->d_size) {
					j = *cp++;
					do {
					    if (j & 0200)
						fprintf(fp, " %s",
							servicename(i, proto));
					    j <<= 1;
					} while (++i & 07);
				} 
				break;

			case T_MINFO:
			case T_RP:
				fprintf(fp, "%s.", cp);
				cp += strlen((char *)cp) + 1;
				fprintf(fp, " %s.", cp);
				break;

			case T_KEY:
			        savecp = cp;  /* save the beginning */
			/*>>> Flags (unsigned_16) */
				GETSHORT(keyflags,cp);
			        fprintf(fp, "0x%04x ", keyflags);
			/*>>> Protocol (8-bit decimal) */
				fprintf(fp, "%3u ", *cp++);
			/*>>> Algorithm id (8-bit decimal) */
				fprintf(fp, "%3u ", *cp++);
				
			/*>>> Public-Key Data (multidigit BASE64) */
			/*    containing ExponentLen, Exponent, and Modulus */
				i = b64_ntop(cp, dp->d_size - (cp - savecp),
				             temp_base64,
					     sizeof temp_base64);
				if (i < 0)
					fprintf(fp, "; BAD BASE64");
				else
					fprintf(fp, "%s", temp_base64);
			        break;

			case T_SIG:
			        sigdata = cp;
				/* RRtype (char *) */
			        GETSHORT(n,cp);
			        fprintf(fp, "%s ", p_type(n));
				/* Algorithm id (8-bit decimal) */
				fprintf(fp, "%d ", *cp++);
				/* Labels (8-bit decimal) (not saved in file) */
				/* FIXME -- check value and print err if bad */
				cp++;
				/* OTTL (u_long) */
				GETLONG(n, cp);
				fprintf(fp, "%lu ", n);
				/* Texp (u_long) */
				GETLONG(n, cp);
				fprintf(fp, "%s ", p_secstodate (n));
				/* Tsig (u_long) */
				GETLONG(n, cp);
				fprintf(fp, "%s ", p_secstodate (n));
				/* Kfootprint (unsigned_16) */
			        GETSHORT(n, cp);
				fprintf(fp, "%lu ", n);
				/* Signer's Name (char *)  */
				fprintf(fp, "%s ", cp);
				cp += strlen((char *)cp) + 1;
				/* Signature (base64 of any length) */
				i = b64_ntop(cp, dp->d_size - (cp - sigdata),
					     temp_base64,
					     sizeof temp_base64);
				if (i < 0)
					fprintf(fp, "; BAD BASE64");
				else
					fprintf(fp, "%s", temp_base64);
			        break;

#ifdef ALLOW_T_UNSPEC
			case T_UNSPEC:
				/* Dump binary data out in an ASCII-encoded
				   format */
				{
				  /* Allocate more than enough space:
				   *  actually need 5/4 size + 20 or so
				   */
				  int TmpSize = 2 * dp->d_size + 30;
				  char *TmpBuf = (char *) malloc(TmpSize);
				  if (TmpBuf == NULL) {
					TmpBuf = "BAD_MALLOC";
				  }
				  if (btoa(cp, dp->d_size, TmpBuf, TmpSize)
				      == CONV_OVERFLOW) {
					    TmpBuf = "OVERFLOW";
					}
					fprintf(fp, "%s", TmpBuf);
				}
				break;
#endif /* ALLOW_T_UNSPEC */
			default:
				fprintf(fp, "%s?d_type=%d?",
					sep, dp->d_type);
				sep = " ";
			}
			if (dp->d_cred < DB_C_ZONE) {
				fprintf(fp, "%sCr=%s",
					sep, MkCredStr(dp->d_cred));
				sep = " ";
			} else {
				fprintf(fp, "%sCl=%d",
					sep, dp->d_clev);
				sep = " ";
			}
eoln:
#ifdef STATS
			if (dp->d_ns) {
				fprintf(fp, "%s[%s]",
					sep, inet_ntoa(dp->d_ns->addr));
				sep = " ";
			}
#endif
			putc('\n', fp);
		}
	    }
	}
        if (ferror(fp))
		return(NODBFILE);

	npp = htp->h_tab;
	nppend = npp + htp->h_size;
	while (npp < nppend) {
	    for (np = *npp++; np != NULL; np = np->n_next) {
		if (np->n_hash == NULL)
			continue;
		getname(np, dname, sizeof(dname));
		if (db_dump(np->n_hash, fp, zone, dname) == NODBFILE)
		    return(NODBFILE);
	    }
	}
	return(OK);
}

static const char *
MkCredStr(cred)
	int cred;
{
	static char badness[20];

	switch (cred) {
	case DB_C_ZONE:		return "zone";
	case DB_C_AUTH:		return "auth";
	case DB_C_ANSWER:	return "answer";
	case DB_C_ADDITIONAL:	return "addtnl";
	case DB_C_CACHE:	return "cache";
	default:		break;
	}
	sprintf(badness, "?%d?", cred);
	return (badness);
}

#ifdef ALLOW_T_UNSPEC
/*
 * Subroutines to convert between 8 bit binary bytes and printable ASCII.
 * Computes the number of bytes, and three kinds of simple checksums.
 * Incoming bytes are collected into 32-bit words, then printed in base 85:
 *	exp(85,5) > exp(2,32)
 * The ASCII characters used are between '!' and 'u';
 * 'z' encodes 32-bit zero; 'x' is used to mark the end of encoded data.
 *
 * Originally by Paul Rutter (philabs!per) and Joe Orost (petsd!joe) for
 * the atob/btoa programs, released with the compress program, in mod.sources.
 * Modified by Mike Schwartz 8/19/86 for use in BIND.
 */

/* Make sure global variable names are unique */
#define Ceor T_UNSPEC_Ceor
#define Csum T_UNSPEC_Csum
#define Crot T_UNSPEC_Crot
#define word T_UNSPEC_word
#define bcount T_UNSPEC_bcount

static int32_t Ceor, Csum, Crot, word, bcount;

#define EN(c)	((int) ((c) + '!'))
#define DE(c) ((c) - '!')
#define AddToBuf(bufp, c) **bufp = c; (*bufp)++;
#define times85(x)	((((((x<<2)+x)<<2)+x)<<2)+x)

/* Decode ASCII-encoded byte c into binary representation and 
 * place into *bufp, advancing bufp 
 */
static int
byte_atob(c, bufp)
	register c;
	char **bufp;
{
	if (c == 'z') {
		if (bcount != 0)
			return(CONV_BADFMT);
		else {
			putbyte(0, bufp);
			putbyte(0, bufp);
			putbyte(0, bufp);
			putbyte(0, bufp);
		}
	} else if ((c >= '!') && (c < ('!' + 85))) {
		if (bcount == 0) {
			word = DE(c);
			++bcount;
		} else if (bcount < 4) {
			word = times85(word);
			word += DE(c);
			++bcount;
		} else {
			word = times85(word) + DE(c);
			putbyte((int)((word >> 24) & 255), bufp);
			putbyte((int)((word >> 16) & 255), bufp);
			putbyte((int)((word >> 8) & 255), bufp);
			putbyte((int)(word & 255), bufp);
			word = 0;
			bcount = 0;
		}
	} else
		return(CONV_BADFMT);
	return(CONV_SUCCESS);
}

/* Compute checksum info and place c into *bufp, advancing bufp */
static void
putbyte(c, bufp)
	register c;
	char **bufp;
{
	Ceor ^= c;
	Csum += c;
	Csum += 1;
	if ((Crot & 0x80000000)) {
		Crot <<= 1;
		Crot += 1;
	} else {
		Crot <<= 1;
	}
	Crot += c;
	AddToBuf(bufp, c);
}

/* Read the ASCII-encoded data from inbuf, of length inbuflen, and convert
   it into T_UNSPEC (binary data) in outbuf, not to exceed outbuflen bytes;
   outbuflen must be divisible by 4.  (Note: this is because outbuf is filled
   in 4 bytes at a time.  If the actual data doesn't end on an even 4-byte
   boundary, there will be no problem...it will be padded with 0 bytes, and
   numbytes will indicate the correct number of bytes.  The main point is
   that since the buffer is filled in 4 bytes at a time, even if there is
   not a full 4 bytes of data at the end, there has to be room to 0-pad the
   data, so the buffer must be of size divisible by 4).  Place the number of
   output bytes in numbytes, and return a failure/success status  */
int
atob(inbuf, inbuflen, outbuf, outbuflen, numbytes)
	char *inbuf;
	int inbuflen;
	char *outbuf;
	int outbuflen;
	int *numbytes;
{
	int inc, nb;
	int32_t oeor, osum, orot;
	char *inp, *outp = outbuf, *endoutp = &outbuf[outbuflen];

	if ( (outbuflen % 4) != 0)
		return(CONV_BADBUFLEN);
	Ceor = Csum = Crot = word = bcount = 0;
	for (inp = inbuf, inc = 0; inc < inbuflen; inp++, inc++) {
		if (outp > endoutp)
			return(CONV_OVERFLOW);
		if (*inp == 'x') {
			inp +=2;
			break;
		} else {
			if (byte_atob(*inp, &outp) == CONV_BADFMT)
				return(CONV_BADFMT);
		}
	}

	/* Get byte count and checksum information from end of buffer */
	if (sscanf(inp, "%d %lx %lx %lx", numbytes, &oeor, &osum, &orot) != 4)
		return(CONV_BADFMT);
	if ((oeor != Ceor) || (osum != Csum) || (orot != Crot))
		return(CONV_BADCKSUM);
	return(CONV_SUCCESS);
}

/* Encode binary byte c into ASCII representation and place into *bufp,
   advancing bufp */
static void
byte_btoa(c, bufp)
	register c;
	char **bufp;
{
	Ceor ^= c;
	Csum += c;
	Csum += 1;
	if ((Crot & 0x80000000)) {
		Crot <<= 1;
		Crot += 1;
	} else {
		Crot <<= 1;
	}
	Crot += c;

	word <<= 8;
	word |= c;
	if (bcount == 3) {
		if (word == 0) {
			AddToBuf(bufp, 'z');
		} else {
		    register int tmp = 0;
		    register int32_t tmpword = word;
			
		    if (tmpword < 0) {	
			   /* Because some don't support u_long */
		    	tmp = 32;
		    	tmpword -= (int32_t)(85 * 85 * 85 * 85 * 32);
		    }
		    if (tmpword < 0) {
		    	tmp = 64;
		    	tmpword -= (int32_t)(85 * 85 * 85 * 85 * 32);
		    }
		    AddToBuf(bufp,
		    	 EN((tmpword / (int32_t)(85 * 85 * 85 * 85)) + tmp));
		    tmpword %= (int32_t)(85 * 85 * 85 * 85);
		    AddToBuf(bufp, EN(tmpword / (85 * 85 * 85)));
		    tmpword %= (85 * 85 * 85);
		    AddToBuf(bufp, EN(tmpword / (85 * 85)));
		    tmpword %= (85 * 85);
		    AddToBuf(bufp, EN(tmpword / 85));
		    tmpword %= 85;
		    AddToBuf(bufp, EN(tmpword));
		}
		bcount = 0;
	} else {
		bcount += 1;
	}
}


/*
 * Encode the binary data from inbuf, of length inbuflen, into a
 * null-terminated ASCII representation in outbuf, not to exceed outbuflen
 * bytes. Return success/failure status
 */
static int
btoa(inbuf, inbuflen, outbuf, outbuflen)
	char *inbuf;
	int inbuflen;
	char *outbuf;
	int outbuflen;
{
	int32_t inc, nb;
	int32_t oeor, osum, orot;
	char *inp, *outp = outbuf, *endoutp = &outbuf[outbuflen -1];

	Ceor = Csum = Crot = word = bcount = 0;
	for (inp = inbuf, inc = 0; inc < inbuflen; inp++, inc++) {
		byte_btoa((unsigned char) (*inp), &outp);
		if (outp >= endoutp)
			return(CONV_OVERFLOW);
	}
	while (bcount != 0) {
		byte_btoa(0, &outp);
		if (outp >= endoutp)
			return(CONV_OVERFLOW);
	}
	/* Put byte count and checksum information at end of buffer, delimited
	   by 'x' */
	(void) sprintf(outp, "x %d %lx %lx %lx", inbuflen, Ceor, Csum, Crot);
	if (&outp[strlen(outp) - 1] >= endoutp)
		return(CONV_OVERFLOW);
	else
		return(CONV_SUCCESS);
}
#endif /* ALLOW_T_UNSPEC */
