#if !defined(lint) && !defined(SABER)
static const char sccsid[] = "@(#)db_dump.c	4.33 (Berkeley) 3/3/91";
static const char rcsid[] = "$Id: db_dump.c,v 8.49 2001/02/06 06:42:19 marka Exp $";
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
 * Portions Copyright (c) 1996-2000 by Internet Software Consortium.
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

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>

#include "port_after.h"

#include "named.h"

#ifdef HITCOUNTS
u_int32_t	db_total_hits;
#endif /* HITCOUNTS */

static const char	*MkCredStr(int);

static int fwd_dump(FILE *fp);

/*
 * Dump current data base in a format similar to RFC 883.
 */

void
doadump(void) {
	FILE *fp;

	ns_notice(ns_log_db, "dumping nameserver data");

	if ((fp = write_open(server_options->dump_filename)) == NULL)
		return;
	gettime(&tt);
#ifdef HITCOUNTS
	if (NS_OPTION_P(OPTION_HITCOUNT))
		fprintf(fp, "; Total hits: %d\n",db_total_hits);
#endif /* HITCOUNTS */
	fprintf(fp, "; Dumped at %s", ctimel(tt.tv_sec));
	if (zones != NULL && nzones != 0)
		zt_dump(fp);
	if (fwddata != NULL && fwddata_count != 0)
		fwd_dump(fp);
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
	ns_notice(ns_log_db, "finished dumping nameserver data");
}

int
zt_dump(FILE *fp) {
	struct zoneinfo *zp;

	fprintf(fp, ";; ++zone table++\n");
	for (zp = &zones[0]; zp < &zones[nzones]; zp++) {
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
		fprintf(fp, ";\tftime=%lu, xaddrcnt=%d, state=%04x, pid=%d\n",
			(u_long)zp->z_ftime, zp->z_xaddrcnt,
			zp->z_flags, (int)zp->z_xferpid);
		sprintf(buf, ";\tz_addr[%d]: ", zp->z_addrcnt);
		pre = buf;
		for (cnt = 0;  cnt < zp->z_addrcnt;  cnt++) {
			fprintf(fp, "%s[%s]", pre, inet_ntoa(zp->z_addr[cnt]));
			pre = ", ";
		}
		if (zp->z_addrcnt)
			fputc('\n', fp);
		if (zp->z_axfr_src.s_addr != 0)
			fprintf(fp, ";\tupdate source [%s]\n",
				inet_ntoa(zp->z_axfr_src));
	}
	fprintf(fp, ";; --zone table--\n");
	return (0);
}

static int
fwd_dump(FILE *fp) {
	int i;

	fprintf(fp, ";; ++forwarders table++\n");
	for (i = 0; i < fwddata_count; i++) {
		if (fwddata[i] != NULL)
			fprintf(fp,"; %s rtt=%d\n",
				inet_ntoa(fwddata[i]->fwdaddr.sin_addr),
				fwddata[i]->nsdata->d_nstime);
	}
	fprintf(fp, ";; --forwarders table--\n");
	return (0);
}

int
db_dump(struct hashbuf *htp, FILE *fp, int zone, char *origin) {
	struct databuf *dp = NULL;
	struct namebuf *np;
	struct namebuf **npp, **nppend;
	char dname[MAXDNAME];
	u_int32_t n;
	int j, i, found_data, tab, printed_origin;
	u_char *cp, *end;
	const char *proto, *sep;
	int16_t type;
	u_int16_t keyflags;
	u_char *sigdata, *certdata;
	u_char *savecp;
	char temp_base64[NS_MD5RSA_MAX_BASE64];

	found_data = 0;
	printed_origin = 0;
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
			/* XXX why are we not calling stale() here? */
			if (dp->d_zone == DB_Z_CACHE &&
			    dp->d_ttl <= (u_int32_t)tt.tv_sec &&
			    (dp->d_flags & DB_F_HINT) == 0)
				continue;
			if (!printed_origin) {
			    fprintf(fp, "$ORIGIN %s.\n", origin);
			    printed_origin++;
			}
			tab = 0;
			if (dp->d_rcode == NXDOMAIN ||
			    dp->d_rcode == NOERROR_NODATA) {
				fputc(';', fp);
			} else if (found_data == 0 || found_data == 1) {
			    found_data = 2;
			}
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
			    if (dp->d_flags & DB_F_HINT &&
				(int32_t)(dp->d_ttl - tt.tv_sec)
				    < DB_ROOT_TIMBUF)
				    fprintf(fp, "%d\t", DB_ROOT_TIMBUF);
			    else
				    fprintf(fp, "%d\t",
					(int)(dp->d_ttl - tt.tv_sec));
			} else if (dp->d_ttl != USE_MINIMUM)
				fprintf(fp, "%u\t", dp->d_ttl);
			else
				fprintf(fp, "%u\t",
				        zones[dp->d_zone].z_minimum);
			fprintf(fp, "%s\t%s\t",
				p_class(dp->d_class),
				p_type(dp->d_type));
			cp = (u_char *)dp->d_data;
			sep = "\t;";
			type = dp->d_type;
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
			/*
			 * Print type specific data
			 */
			/* XXX why are we not using ns_sprintrr() here? */
			switch (type) {
			case T_A:
				switch (dp->d_class) {
				case C_IN:
				case C_HS:
					fputs(inet_ntoa(ina_get(cp)), fp);
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
					memcpy(buf, cp, n); buf[n] = '\0';
					fprintf(fp, "\"%.*s\"", (int)n, buf);
 					cp += n;
				} else
					fprintf(fp, "\"\"");
				if ((n = *cp++) != '\0') {
					memcpy(buf, cp, n); buf[n] = '\0';
					fprintf(fp, " \"%.*s\"", (int)n, buf);
				} else
					fprintf(fp, " \"\"");
				break;
			}

			case T_SOA:
				fprintf(fp, "%s.", cp);
				cp += strlen((char *)cp) + 1;
				fprintf(fp, " %s. (\n", cp);
#if defined(RETURNSOA)
				if (dp->d_rcode)
					fputs(";", fp);
#endif
				cp += strlen((char *)cp) + 1;
				NS_GET32(n, cp);
				fprintf(fp, "\t\t%u", n);
				NS_GET32(n, cp);
				fprintf(fp, " %u", n);
				NS_GET32(n, cp);
				fprintf(fp, " %u", n);
				NS_GET32(n, cp);
				fprintf(fp, " %u", n);
				NS_GET32(n, cp);
				fprintf(fp, " %u )", n);
#if defined(RETURNSOA)
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
				NS_GET16(n, cp);
				fprintf(fp, "%u", n);
				fprintf(fp, " %s.", cp);
				break;

			case T_PX:
				NS_GET16(n, cp);
				fprintf(fp, "%u", n);
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

			case T_LOC: {
				char t[256];

				(void) fputs(loc_ntoa(dp->d_data, t), fp);
 				break;
			}

			case T_NAPTR: {
				u_int32_t order, preference;

				NS_GET16(order, cp);
				fprintf(fp, "%u", order);
 
				NS_GET16(preference, cp);
				fprintf(fp, "%u", preference);

				n = *cp++;
				fprintf(fp, "\"%.*s\"", (int)n, cp);
				cp += n;

				n = *cp++;
				fprintf(fp, "\"%.*s\"", (int)n, cp);
				cp += n;

				n = *cp++;
				fprintf(fp, " \"%.*s\"", (int)n, cp);
				cp += n;

				fprintf(fp, " %s.", cp);

				break;
			}

			case T_SRV: {
				u_int priority, weight, port;

				NS_GET16(priority, cp);
				NS_GET16(weight, cp);
				NS_GET16(port, cp);
				fprintf(fp, "\t%u %u %u %s.",
					priority, weight, port, cp);
				break;
			}

			case T_WKS:
				fputs(inet_ntoa(ina_get(cp)), fp);
				cp += INADDRSZ;
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
				NS_GET16(keyflags,cp);
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
				NS_GET16(n,cp);
				fprintf(fp, "%s ", p_type(n));
				/* Algorithm id (8-bit decimal) */
				fprintf(fp, "%d ", *cp++);
				/* Labels (8-bit decimal) */
				fprintf(fp, "%d ", *cp++);
				/* OTTL (u_long) */
				NS_GET32(n, cp);
				fprintf(fp, "%u ", n);
				/* Texp (u_long) */
				NS_GET32(n, cp);
				fprintf(fp, "%s ", p_secstodate (n));
				/* Tsig (u_long) */
				NS_GET32(n, cp);
				fprintf(fp, "%s ", p_secstodate (n));
				/* Kfootprint (unsigned_16) */
				NS_GET16(n, cp);
				fprintf(fp, "%u ", n);
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

			case T_NXT:
				fprintf(fp, "%s.", cp);
				n = strlen ((char *)cp) + 1;
				cp += n;
				i = 8 * (dp->d_size - n);  /* How many bits? */
				for (n = 0; n < (u_int32_t)i; n++) {
					if (NS_NXT_BIT_ISSET(n, cp)) 
						fprintf(fp," %s", p_type(n));
				}
				break;

			case ns_t_cert:
				certdata = cp;
				NS_GET16(n,cp);
				fprintf(fp, "%d ", n); /* cert type */

				NS_GET16(n,cp);
				fprintf(fp, "%d %d ", n, *cp++); /* tag & alg */

				/* Certificate (base64 of any length) */
				i = b64_ntop(cp,
					     dp->d_size - (cp - certdata),
					     temp_base64, sizeof(temp_base64));
				if (i < 0)
					fprintf(fp, "; BAD BASE64");
				else
					fprintf(fp, "%s", temp_base64);
				break;

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
			if ((dp->d_flags & DB_F_LAME) != 0) {
				time_t when;
				getname(np, dname, sizeof(dname));
				when = db_lame_find(dname, dp);
				if (when != 0 && when > tt.tv_sec) {
					fprintf(fp, "%sLAME=%ld",
						sep, when - tt.tv_sec);
					sep = " ";
				}
			}

 eoln:
			if (dp->d_addr.s_addr != htonl(0)) {
				fprintf(fp, "%s[%s]",
					sep, inet_ntoa(dp->d_addr));
				sep = " ";
			}
#ifdef HITCOUNTS
			if (NS_OPTION_P(OPTION_HITCOUNT)) {
				fprintf(fp, "%shits=%d", sep, dp->d_hitcnt);
				sep=" ";
			}
#endif /* HITCOUNTS */
			putc('\n', fp);
		}
	    }
	}
	if (ferror(fp))
		return (NODBFILE);

	npp = htp->h_tab;
	nppend = npp + htp->h_size;
	while (npp < nppend) {
		for (np = *npp++; np != NULL; np = np->n_next) {
			if (np->n_hash == NULL)
				continue;
			getname(np, dname, sizeof(dname));
			if (db_dump(np->n_hash, fp, zone, dname) == NODBFILE)
				return (NODBFILE);
		}
	}
	return (OK);
}

static const char *
MkCredStr(int cred) {
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
