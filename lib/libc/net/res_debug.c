/*
 * ++Copyright++ 1985, 1990, 1993
 * -
 * Copyright (c) 1985, 1990, 1993
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)res_debug.c	8.1 (Berkeley) 6/4/93";
static char rcsid[] = "$Id: res_debug.c,v 8.12 1996/08/05 08:31:35 vixie Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "res_config.h"
#include <arpa/nameser.h>

#include <stdio.h>
#include <netdb.h>
#include <resolv.h>
#include <ctype.h>
#include <string.h>


extern const char *_res_opcodes[];
extern const char *_res_resultcodes[];

/* XXX: we should use getservbyport() instead. */
static const char *
dewks(wks)
	int wks;
{
	static char nbuf[20];

	switch (wks) {
	case 5: return "rje";
	case 7: return "echo";
	case 9: return "discard";
	case 11: return "systat";
	case 13: return "daytime";
	case 15: return "netstat";
	case 17: return "qotd";
	case 19: return "chargen";
	case 20: return "ftp-data";
	case 21: return "ftp";
	case 23: return "telnet";
	case 25: return "smtp";
	case 37: return "time";
	case 39: return "rlp";
	case 42: return "name";
	case 43: return "whois";
	case 53: return "domain";
	case 57: return "apts";
	case 59: return "apfs";
	case 67: return "bootps";
	case 68: return "bootpc";
	case 69: return "tftp";
	case 77: return "rje";
	case 79: return "finger";
	case 87: return "link";
	case 95: return "supdup";
	case 100: return "newacct";
	case 101: return "hostnames";
	case 102: return "iso-tsap";
	case 103: return "x400";
	case 104: return "x400-snd";
	case 105: return "csnet-ns";
	case 109: return "pop-2";
	case 111: return "sunrpc";
	case 113: return "auth";
	case 115: return "sftp";
	case 117: return "uucp-path";
	case 119: return "nntp";
	case 121: return "erpc";
	case 123: return "ntp";
	case 133: return "statsrv";
	case 136: return "profile";
	case 144: return "NeWS";
	case 161: return "snmp";
	case 162: return "snmp-trap";
	case 170: return "print-srv";
	default: (void) sprintf(nbuf, "%d", wks); return (nbuf);
	}
}

/* XXX: we should use getprotobynumber() instead. */
static const char *
deproto(protonum)
	int protonum;
{
	static char nbuf[20];

	switch (protonum) {
	case 1: return "icmp";
	case 2: return "igmp";
	case 3: return "ggp";
	case 5: return "st";
	case 6: return "tcp";
	case 7: return "ucl";
	case 8: return "egp";
	case 9: return "igp";
	case 11: return "nvp-II";
	case 12: return "pup";
	case 16: return "chaos";
	case 17: return "udp";
	default: (void) sprintf(nbuf, "%d", protonum); return (nbuf);
	}
}

static const u_char *
do_rrset(msg, len, cp, cnt, pflag, file, hs)
	int cnt, pflag, len;
	const u_char *cp, *msg;
	const char *hs;
	FILE *file;
{
	int n;
	int sflag;

	/*
	 * Print answer records.
	 */
	sflag = (_res.pfcode & pflag);
	if ( (n = ntohs(cnt)) ) {
		if ((!_res.pfcode) ||
		    ((sflag) && (_res.pfcode & RES_PRF_HEAD1)))
			fprintf(file, hs);
		while (--n >= 0) {
			if ((!_res.pfcode) || sflag) {
				cp = p_rr(cp, msg, file);
			} else {
				unsigned int dlen;
				cp += __dn_skipname(cp, cp + MAXCDNAME);
				cp += INT16SZ;
				cp += INT16SZ;
				cp += INT32SZ;
				dlen = _getshort((u_char*)cp);
				cp += INT16SZ;
				cp += dlen;
			}
			if ((cp - msg) > len)
				return (NULL);
		}
		if ((!_res.pfcode) ||
		    ((sflag) && (_res.pfcode & RES_PRF_HEAD1)))
			putc('\n', file);
	}
	return (cp);
}

void
__p_query(msg)
	const u_char *msg;
{
	__fp_query(msg, stdout);
}

/*
 * Print the current options.
 * This is intended to be primarily a debugging routine.
 */
void
__fp_resstat(statp, file)
	struct __res_state *statp;
	FILE *file;
{
	register u_long mask;

	fprintf(file, ";; res options:");
	if (!statp)
		statp = &_res;
	for (mask = 1;  mask != 0;  mask <<= 1)
		if (statp->options & mask)
			fprintf(file, " %s", p_option(mask));
	putc('\n', file);
}

/*
 * Print the contents of a query.
 * This is intended to be primarily a debugging routine.
 */
void
__fp_nquery(msg, len, file)
	const u_char *msg;
	int len;
	FILE *file;
{
	register const u_char *cp, *endMark;
	register const HEADER *hp;
	register int n;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return;

#define TruncTest(x) if (x >= endMark) goto trunc
#define	ErrorTest(x) if (x == NULL) goto error

	/*
	 * Print header fields.
	 */
	hp = (HEADER *)msg;
	cp = msg + HFIXEDSZ;
	endMark = cp + len;
	if ((!_res.pfcode) || (_res.pfcode & RES_PRF_HEADX) || hp->rcode) {
		fprintf(file, ";; ->>HEADER<<- opcode: %s, status: %s, id: %d",
			_res_opcodes[hp->opcode],
			_res_resultcodes[hp->rcode],
			ntohs(hp->id));
		putc('\n', file);
	}
	if ((!_res.pfcode) || (_res.pfcode & RES_PRF_HEADX))
		putc(';', file);
	if ((!_res.pfcode) || (_res.pfcode & RES_PRF_HEAD2)) {
		fprintf(file, "; flags:");
		if (hp->qr)
			fprintf(file, " qr");
		if (hp->aa)
			fprintf(file, " aa");
		if (hp->tc)
			fprintf(file, " tc");
		if (hp->rd)
			fprintf(file, " rd");
		if (hp->ra)
			fprintf(file, " ra");
	}
	if ((!_res.pfcode) || (_res.pfcode & RES_PRF_HEAD1)) {
		fprintf(file, "; Ques: %d", ntohs(hp->qdcount));
		fprintf(file, ", Ans: %d", ntohs(hp->ancount));
		fprintf(file, ", Auth: %d", ntohs(hp->nscount));
		fprintf(file, ", Addit: %d", ntohs(hp->arcount));
	}
	if ((!_res.pfcode) || (_res.pfcode & 
		(RES_PRF_HEADX | RES_PRF_HEAD2 | RES_PRF_HEAD1))) {
		putc('\n',file);
	}
	/*
	 * Print question records.
	 */
	if ( (n = ntohs(hp->qdcount)) ) {
		if ((!_res.pfcode) || (_res.pfcode & RES_PRF_QUES))
			fprintf(file, ";; QUESTIONS:\n");
		while (--n >= 0) {
			if ((!_res.pfcode) || (_res.pfcode & RES_PRF_QUES))
				fprintf(file, ";;\t");
			TruncTest(cp);
			if ((!_res.pfcode) || (_res.pfcode & RES_PRF_QUES))
				cp = p_cdnname(cp, msg, len, file);
			else {
				int n;
				char name[MAXDNAME];

				if ((n = dn_expand(msg, msg+len, cp, name,
						sizeof name)) < 0)
					cp = NULL;
				else
					cp += n;
			}
			ErrorTest(cp);
			TruncTest(cp);
			if ((!_res.pfcode) || (_res.pfcode & RES_PRF_QUES))
				fprintf(file, ", type = %s",
					__p_type(_getshort((u_char*)cp)));
			cp += INT16SZ;
			TruncTest(cp);
			if ((!_res.pfcode) || (_res.pfcode & RES_PRF_QUES))
				fprintf(file, ", class = %s\n",
					__p_class(_getshort((u_char*)cp)));
			cp += INT16SZ;
			if ((!_res.pfcode) || (_res.pfcode & RES_PRF_QUES))
				putc('\n', file);
		}
	}
	/*
	 * Print authoritative answer records
	 */
	TruncTest(cp);
	cp = do_rrset(msg, len, cp, hp->ancount, RES_PRF_ANS, file,
		      ";; ANSWERS:\n");
	ErrorTest(cp);

	/*
	 * print name server records
	 */
	TruncTest(cp);
	cp = do_rrset(msg, len, cp, hp->nscount, RES_PRF_AUTH, file,
		      ";; AUTHORITY RECORDS:\n");
	ErrorTest(cp);

	TruncTest(cp);
	/*
	 * print additional records
	 */
	cp = do_rrset(msg, len, cp, hp->arcount, RES_PRF_ADD, file,
		      ";; ADDITIONAL RECORDS:\n");
	ErrorTest(cp);
	return;
 trunc:
	fprintf(file, "\n;; ...truncated\n");
	return;
 error:
	fprintf(file, "\n;; ...malformed\n");
}

void
__fp_query(msg, file)
	const u_char *msg;
	FILE *file;
{
	fp_nquery(msg, PACKETSZ, file);
}

const u_char *
__p_cdnname(cp, msg, len, file)
	const u_char *cp, *msg;
	int len;
	FILE *file;
{
	char name[MAXDNAME];
	int n;

	if ((n = dn_expand(msg, msg + len, cp, name, sizeof name)) < 0)
		return (NULL);
	if (name[0] == '\0')
		putc('.', file);
	else
		fputs(name, file);
	return (cp + n);
}

const u_char *
__p_cdname(cp, msg, file)
	const u_char *cp, *msg;
	FILE *file;
{
	return (p_cdnname(cp, msg, PACKETSZ, file));
}

/* XXX:	the rest of these functions need to become length-limited, too. (vix)
 */

const u_char *
__p_fqname(cp, msg, file)
	const u_char *cp, *msg;
	FILE *file;
{
	char name[MAXDNAME];
	int n;

	if ((n = dn_expand(msg, cp + MAXCDNAME, cp, name, sizeof name)) < 0)
		return (NULL);
	if (name[0] == '\0') {
		putc('.', file);
	} else {
		fputs(name, file);
		if (name[strlen(name) - 1] != '.')
			putc('.', file);
	}
	return (cp + n);
}

/*
 * Print resource record fields in human readable form.
 */
const u_char *
__p_rr(cp, msg, file)
	const u_char *cp, *msg;
	FILE *file;
{
	int type, class, dlen, n, c;
	struct in_addr inaddr;
	const u_char *cp1, *cp2;
	u_int32_t tmpttl, t;
	int lcnt;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
	if ((cp = p_fqname(cp, msg, file)) == NULL)
		return (NULL);			/* compression error */
	type = _getshort((u_char*)cp);
	cp += INT16SZ;
	class = _getshort((u_char*)cp);
	cp += INT16SZ;
	tmpttl = _getlong((u_char*)cp);
	cp += INT32SZ;
	dlen = _getshort((u_char*)cp);
	cp += INT16SZ;
	cp1 = cp;
	if ((!_res.pfcode) || (_res.pfcode & RES_PRF_TTLID))
		fprintf(file, "\t%lu", (u_long)tmpttl);
	if ((!_res.pfcode) || (_res.pfcode & RES_PRF_CLASS))
		fprintf(file, "\t%s", __p_class(class));
	fprintf(file, "\t%s", __p_type(type));
	/*
	 * Print type specific data, if appropriate
	 */
	switch (type) {
	case T_A:
		switch (class) {
		case C_IN:
		case C_HS:
			bcopy(cp, (char *)&inaddr, INADDRSZ);
			if (dlen == 4) {
				fprintf(file, "\t%s", inet_ntoa(inaddr));
				cp += dlen;
			} else if (dlen == 7) {
				char *address;
				u_char protocol;
				u_short port;

				address = inet_ntoa(inaddr);
				cp += INADDRSZ;
				protocol = *(u_char*)cp;
				cp += sizeof(u_char);
				port = _getshort((u_char*)cp);
				cp += INT16SZ;
				fprintf(file, "\t%s\t; proto %d, port %d",
					address, protocol, port);
			}
			break;
		default:
			cp += dlen;
		}
		break;
	case T_CNAME:
	case T_MB:
	case T_MG:
	case T_MR:
	case T_NS:
	case T_PTR:
		putc('\t', file);
		if ((cp = p_fqname(cp, msg, file)) == NULL)
			return (NULL);
		break;

	case T_HINFO:
	case T_ISDN:
		(void) fputs("\t\"", file);
		cp2 = cp + dlen;
		if ((n = (unsigned char) *cp++) != 0) {
			for (c = n; c > 0 && cp < cp2; c--) {
				if (strchr("\n\"\\", *cp))
					(void) putc('\\', file);
				(void) putc(*cp++, file);
			}
			putc('"', file);
		}
		if (cp < cp2 && (n = (unsigned char) *cp++) != 0) {
			(void) fputs ("\t\"", file);
			for (c = n; c > 0 && cp < cp2; c--) {
				if (strchr("\n\"\\", *cp))
					(void) putc('\\', file);
				(void) putc(*cp++, file);
			}
			putc('"', file);
		} else if (type == T_HINFO) {
			(void) fputs("\"?\"", file);
			fprintf(file, "\n;; *** Warning *** OS-type missing");
		}
		break;

	case T_SOA:
		putc('\t', file);
		if ((cp = p_fqname(cp, msg, file)) == NULL)
			return (NULL);
		putc(' ', file);
		if ((cp = p_fqname(cp, msg, file)) == NULL)
			return (NULL);
		fputs(" (\n", file);
		t = _getlong((u_char*)cp);  cp += INT32SZ;
		fprintf(file, "\t\t\t%lu\t; serial\n", (u_long)t);
		t = _getlong((u_char*)cp);  cp += INT32SZ;
		fprintf(file, "\t\t\t%lu\t; refresh (%s)\n",
			(u_long)t, __p_time(t));
		t = _getlong((u_char*)cp);  cp += INT32SZ;
		fprintf(file, "\t\t\t%lu\t; retry (%s)\n",
			(u_long)t, __p_time(t));
		t = _getlong((u_char*)cp);  cp += INT32SZ;
		fprintf(file, "\t\t\t%lu\t; expire (%s)\n",
			(u_long)t, __p_time(t));
		t = _getlong((u_char*)cp);  cp += INT32SZ;
		fprintf(file, "\t\t\t%lu )\t; minimum (%s)",
			(u_long)t, __p_time(t));
		break;

	case T_MX:
	case T_AFSDB:
	case T_RT:
		fprintf(file, "\t%d ", _getshort((u_char*)cp));
		cp += INT16SZ;
		if ((cp = p_fqname(cp, msg, file)) == NULL)
			return (NULL);
		break;

	case T_PX:
		fprintf(file, "\t%d ", _getshort((u_char*)cp));
		cp += INT16SZ;
		if ((cp = p_fqname(cp, msg, file)) == NULL)
			return (NULL);
		putc(' ', file);
		if ((cp = p_fqname(cp, msg, file)) == NULL)
			return (NULL);
		break;

	case T_TXT:
	case T_X25:
		(void) fputs("\t\"", file);
		cp2 = cp1 + dlen;
		while (cp < cp2) {
			if (n = (unsigned char) *cp++) {
				for (c = n; c > 0 && cp < cp2; c--) {
					if (strchr("\n\"\\", *cp))
						(void) putc('\\', file);
					(void) putc(*cp++, file);
				}
			}
		}
		putc('"', file);
		break;

	case T_NSAP:
		(void) fprintf(file, "\t%s", inet_nsap_ntoa(dlen, cp, NULL));
		cp += dlen;
		break;

	case T_AAAA: {
		char t[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];

		fprintf(file, "\t%s\n", inet_ntop(AF_INET6, cp, t, sizeof t));
		cp += dlen;
		break;
	}

	case T_LOC: {
		char t[255];

		(void) fprintf(file, "\t%s\n", loc_ntoa(cp, t));
		cp += dlen;
		break;
	}

	case T_MINFO:
	case T_RP:
		putc('\t', file);
		if ((cp = p_fqname(cp, msg, file)) == NULL)
			return (NULL);
		putc(' ', file);
		if ((cp = p_fqname(cp, msg, file)) == NULL)
			return (NULL);
		break;

	case T_UINFO:
		putc('\t', file);
		fputs((char *)cp, file);
		cp += dlen;
		break;

	case T_UID:
	case T_GID:
		if (dlen == 4) {
			fprintf(file, "\t%u", _getlong((u_char*)cp));
			cp += INT32SZ;
		}
		break;

	case T_WKS:
		if (dlen < INT32SZ + 1)
			break;
		bcopy(cp, (char *)&inaddr, INADDRSZ);
		cp += INT32SZ;
		fprintf(file, "\t%s %s ( ",
			inet_ntoa(inaddr),
			deproto((int) *cp));
		cp += sizeof(u_char);
		n = 0;
		lcnt = 0;
		while (cp < cp1 + dlen) {
			c = *cp++;
			do {
				if (c & 0200) {
					if (lcnt == 0) {
						fputs("\n\t\t\t", file);
						lcnt = 5;
					}
					fputs(dewks(n), file);
					putc(' ', file);
					lcnt--;
				}
				c <<= 1;
			} while (++n & 07);
		}
		putc(')', file);
		break;

#ifdef ALLOW_T_UNSPEC
	case T_UNSPEC:
		{
			int NumBytes = 8;
			u_char *DataPtr;
			int i;

			if (dlen < NumBytes) NumBytes = dlen;
			fprintf(file, "\tFirst %d bytes of hex data:",
				NumBytes);
			for (i = 0, DataPtr = cp; i < NumBytes; i++, DataPtr++)
				fprintf(file, " %x", *DataPtr);
			cp += dlen;
		}
		break;
#endif /* ALLOW_T_UNSPEC */

	default:
		fprintf(file, "\t?%d?", type);
		cp += dlen;
	}
#if 0
	fprintf(file, "\t; dlen=%d, ttl %s\n", dlen, __p_time(tmpttl));
#else
	putc('\n', file);
#endif
	if (cp - cp1 != dlen) {
		fprintf(file, ";; packet size error (found %d, dlen was %d)\n",
			cp - cp1, dlen);
		cp = NULL;
	}
	return (cp);
}

/*
 * Return a string for the type
 */
const char *
__p_type(type)
	int type;
{
	static char nbuf[20];

	switch (type) {
	case T_A:	return "A";
	case T_NS:	return "NS";
	case T_CNAME:	return "CNAME";
	case T_SOA:	return "SOA";
	case T_MB:	return "MB";
	case T_MG:	return "MG";
	case T_MR:	return "MR";
	case T_NULL:	return "NULL";
	case T_WKS:	return "WKS";
	case T_PTR:	return "PTR";
	case T_HINFO:	return "HINFO";
	case T_MINFO:	return "MINFO";
	case T_MX:	return "MX";
	case T_TXT:	return "TXT";
	case T_RP:	return "RP";
	case T_AFSDB:	return "AFSDB";
	case T_X25:	return "X25";
	case T_ISDN:	return "ISDN";
	case T_RT:	return "RT";
	case T_NSAP:	return "NSAP";
	case T_NSAP_PTR: return "NSAP_PTR";
	case T_SIG:	return "SIG";
	case T_KEY:	return "KEY";
	case T_PX:	return "PX";
	case T_GPOS:	return "GPOS";
	case T_AAAA:	return "AAAA";
	case T_LOC:	return "LOC";
	case T_AXFR:	return "AXFR";
	case T_MAILB:	return "MAILB";
	case T_MAILA:	return "MAILA";
	case T_ANY:	return "ANY";
	case T_UINFO:	return "UINFO";
	case T_UID:	return "UID";
	case T_GID:	return "GID";
#ifdef ALLOW_T_UNSPEC
	case T_UNSPEC:	return "UNSPEC";
#endif /* ALLOW_T_UNSPEC */
	default:	(void)sprintf(nbuf, "%d", type); return (nbuf);
	}
}

/*
 * Return a mnemonic for class
 */
const char *
__p_class(class)
	int class;
{
	static char nbuf[20];

	switch (class) {
	case C_IN:	return "IN";
	case C_HS:	return "HS";
	case C_ANY:	return "ANY";
	default:	(void)sprintf(nbuf, "%d", class); return (nbuf);
	}
}

/*
 * Return a mnemonic for an option
 */
const char *
__p_option(option)
	u_long option;
{
	static char nbuf[40];

	switch (option) {
	case RES_INIT:		return "init";
	case RES_DEBUG:		return "debug";
	case RES_AAONLY:	return "aaonly(unimpl)";
	case RES_USEVC:		return "usevc";
	case RES_PRIMARY:	return "primry(unimpl)";
	case RES_IGNTC:		return "igntc";
	case RES_RECURSE:	return "recurs";
	case RES_DEFNAMES:	return "defnam";
	case RES_STAYOPEN:	return "styopn";
	case RES_DNSRCH:	return "dnsrch";
	case RES_INSECURE1:	return "insecure1";
	case RES_INSECURE2:	return "insecure2";
	default:		sprintf(nbuf, "?0x%lx?", (u_long)option);
				return (nbuf);
	}
}

/*
 * Return a mnemonic for a time to live
 */
char *
__p_time(value)
	u_int32_t value;
{
	static char nbuf[40];
	int secs, mins, hours, days;
	register char *p;

	if (value == 0) {
		strcpy(nbuf, "0 secs");
		return (nbuf);
	}

	secs = value % 60;
	value /= 60;
	mins = value % 60;
	value /= 60;
	hours = value % 24;
	value /= 24;
	days = value;
	value = 0;

#define	PLURALIZE(x)	x, (x == 1) ? "" : "s"
	p = nbuf;
	if (days) {
		(void)sprintf(p, "%d day%s", PLURALIZE(days));
		while (*++p);
	}
	if (hours) {
		if (days)
			*p++ = ' ';
		(void)sprintf(p, "%d hour%s", PLURALIZE(hours));
		while (*++p);
	}
	if (mins) {
		if (days || hours)
			*p++ = ' ';
		(void)sprintf(p, "%d min%s", PLURALIZE(mins));
		while (*++p);
	}
	if (secs || ! (days || hours || mins)) {
		if (days || hours || mins)
			*p++ = ' ';
		(void)sprintf(p, "%d sec%s", PLURALIZE(secs));
	}
	return (nbuf);
}

/*
 * routines to convert between on-the-wire RR format and zone file format.
 * Does not contain conversion to/from decimal degrees; divide or multiply
 * by 60*60*1000 for that.
 */

static unsigned int poweroften[10] = {1, 10, 100, 1000, 10000, 100000,
				      1000000,10000000,100000000,1000000000};

/* takes an XeY precision/size value, returns a string representation. */
static const char *
precsize_ntoa(prec)
	u_int8_t prec;
{
	static char retbuf[sizeof("90000000.00")];
	unsigned long val;
	int mantissa, exponent;

	mantissa = (int)((prec >> 4) & 0x0f) % 10;
	exponent = (int)((prec >> 0) & 0x0f) % 10;

	val = mantissa * poweroften[exponent];

	(void) sprintf(retbuf, "%ld.%.2ld", val/100, val%100);
	return (retbuf);
}

/* converts ascii size/precision X * 10**Y(cm) to 0xXY.  moves pointer. */
static u_int8_t
precsize_aton(strptr)
	char **strptr;
{
	unsigned int mval = 0, cmval = 0;
	u_int8_t retval = 0;
	register char *cp;
	register int exponent;
	register int mantissa;

	cp = *strptr;

	while (isdigit(*cp))
		mval = mval * 10 + (*cp++ - '0');

	if (*cp == '.') {		/* centimeters */
		cp++;
		if (isdigit(*cp)) {
			cmval = (*cp++ - '0') * 10;
			if (isdigit(*cp)) {
				cmval += (*cp++ - '0');
			}
		}
	}
	cmval = (mval * 100) + cmval;

	for (exponent = 0; exponent < 9; exponent++)
		if (cmval < poweroften[exponent+1])
			break;

	mantissa = cmval / poweroften[exponent];
	if (mantissa > 9)
		mantissa = 9;

	retval = (mantissa << 4) | exponent;

	*strptr = cp;

	return (retval);
}

/* converts ascii lat/lon to unsigned encoded 32-bit number.  moves pointer. */
static u_int32_t
latlon2ul(latlonstrptr,which)
	char **latlonstrptr;
	int *which;
{
	register char *cp;
	u_int32_t retval;
	int deg = 0, min = 0, secs = 0, secsfrac = 0;

	cp = *latlonstrptr;

	while (isdigit(*cp))
		deg = deg * 10 + (*cp++ - '0');

	while (isspace(*cp))
		cp++;

	if (!(isdigit(*cp)))
		goto fndhemi;

	while (isdigit(*cp))
		min = min * 10 + (*cp++ - '0');

	while (isspace(*cp))
		cp++;

	if (!(isdigit(*cp)))
		goto fndhemi;

	while (isdigit(*cp))
		secs = secs * 10 + (*cp++ - '0');

	if (*cp == '.') {		/* decimal seconds */
		cp++;
		if (isdigit(*cp)) {
			secsfrac = (*cp++ - '0') * 100;
			if (isdigit(*cp)) {
				secsfrac += (*cp++ - '0') * 10;
				if (isdigit(*cp)) {
					secsfrac += (*cp++ - '0');
				}
			}
		}
	}

	while (!isspace(*cp))	/* if any trailing garbage */
		cp++;

	while (isspace(*cp))
		cp++;

 fndhemi:
	switch (*cp) {
	case 'N': case 'n':
	case 'E': case 'e':
		retval = ((unsigned)1<<31)
			+ (((((deg * 60) + min) * 60) + secs) * 1000)
			+ secsfrac;
		break;
	case 'S': case 's':
	case 'W': case 'w':
		retval = ((unsigned)1<<31)
			- (((((deg * 60) + min) * 60) + secs) * 1000)
			- secsfrac;
		break;
	default:
		retval = 0;	/* invalid value -- indicates error */
		break;
	}

	switch (*cp) {
	case 'N': case 'n':
	case 'S': case 's':
		*which = 1;	/* latitude */
		break;
	case 'E': case 'e':
	case 'W': case 'w':
		*which = 2;	/* longitude */
		break;
	default:
		*which = 0;	/* error */
		break;
	}

	cp++;			/* skip the hemisphere */

	while (!isspace(*cp))	/* if any trailing garbage */
		cp++;

	while (isspace(*cp))	/* move to next field */
		cp++;

	*latlonstrptr = cp;

	return (retval);
}

/* converts a zone file representation in a string to an RDATA on-the-wire
 * representation. */
int
loc_aton(ascii, binary)
	const char *ascii;
	u_char *binary;
{
	const char *cp, *maxcp;
	u_char *bcp;

	u_int32_t latit = 0, longit = 0, alt = 0;
	u_int32_t lltemp1 = 0, lltemp2 = 0;
	int altmeters = 0, altfrac = 0, altsign = 1;
	u_int8_t hp = 0x16;	/* default = 1e6 cm = 10000.00m = 10km */
	u_int8_t vp = 0x13;	/* default = 1e3 cm = 10.00m */
	u_int8_t siz = 0x12;	/* default = 1e2 cm = 1.00m */
	int which1 = 0, which2 = 0;

	cp = ascii;
	maxcp = cp + strlen(ascii);

	lltemp1 = latlon2ul(&cp, &which1);

	lltemp2 = latlon2ul(&cp, &which2);

	switch (which1 + which2) {
	case 3:			/* 1 + 2, the only valid combination */
		if ((which1 == 1) && (which2 == 2)) { /* normal case */
			latit = lltemp1;
			longit = lltemp2;
		} else if ((which1 == 2) && (which2 == 1)) { /* reversed */
			longit = lltemp1;
			latit = lltemp2;
		} else {	/* some kind of brokenness */
			return 0;
		}
		break;
	default:		/* we didn't get one of each */
		return 0;
	}

	/* altitude */
	if (*cp == '-') {
		altsign = -1;
		cp++;
	}
    
	if (*cp == '+')
		cp++;

	while (isdigit(*cp))
		altmeters = altmeters * 10 + (*cp++ - '0');

	if (*cp == '.') {		/* decimal meters */
		cp++;
		if (isdigit(*cp)) {
			altfrac = (*cp++ - '0') * 10;
			if (isdigit(*cp)) {
				altfrac += (*cp++ - '0');
			}
		}
	}

	alt = (10000000 + (altsign * (altmeters * 100 + altfrac)));

	while (!isspace(*cp) && (cp < maxcp)) /* if trailing garbage or m */
		cp++;

	while (isspace(*cp) && (cp < maxcp))
		cp++;

	if (cp >= maxcp)
		goto defaults;

	siz = precsize_aton(&cp);
	
	while (!isspace(*cp) && (cp < maxcp))	/* if trailing garbage or m */
		cp++;

	while (isspace(*cp) && (cp < maxcp))
		cp++;

	if (cp >= maxcp)
		goto defaults;

	hp = precsize_aton(&cp);

	while (!isspace(*cp) && (cp < maxcp))	/* if trailing garbage or m */
		cp++;

	while (isspace(*cp) && (cp < maxcp))
		cp++;

	if (cp >= maxcp)
		goto defaults;

	vp = precsize_aton(&cp);

 defaults:

	bcp = binary;
	*bcp++ = (u_int8_t) 0;	/* version byte */
	*bcp++ = siz;
	*bcp++ = hp;
	*bcp++ = vp;
	PUTLONG(latit,bcp);
	PUTLONG(longit,bcp);
	PUTLONG(alt,bcp);
    
	return (16);		/* size of RR in octets */
}

/* takes an on-the-wire LOC RR and formats it in a human readable format. */
char *
loc_ntoa(binary, ascii)
	const u_char *binary;
	char *ascii;
{
	static char *error = "?";
	register const u_char *cp = binary;

	int latdeg, latmin, latsec, latsecfrac;
	int longdeg, longmin, longsec, longsecfrac;
	char northsouth, eastwest;
	int altmeters, altfrac, altsign;

	const int referencealt = 100000 * 100;

	int32_t latval, longval, altval;
	u_int32_t templ;
	u_int8_t sizeval, hpval, vpval, versionval;
    
	char *sizestr, *hpstr, *vpstr;

	versionval = *cp++;

	if (versionval) {
		sprintf(ascii, "; error: unknown LOC RR version");
		return (ascii);
	}

	sizeval = *cp++;

	hpval = *cp++;
	vpval = *cp++;

	GETLONG(templ, cp);
	latval = (templ - ((unsigned)1<<31));

	GETLONG(templ, cp);
	longval = (templ - ((unsigned)1<<31));

	GETLONG(templ, cp);
	if (templ < referencealt) { /* below WGS 84 spheroid */
		altval = referencealt - templ;
		altsign = -1;
	} else {
		altval = templ - referencealt;
		altsign = 1;
	}

	if (latval < 0) {
		northsouth = 'S';
		latval = -latval;
	} else
		northsouth = 'N';

	latsecfrac = latval % 1000;
	latval = latval / 1000;
	latsec = latval % 60;
	latval = latval / 60;
	latmin = latval % 60;
	latval = latval / 60;
	latdeg = latval;

	if (longval < 0) {
		eastwest = 'W';
		longval = -longval;
	} else
		eastwest = 'E';

	longsecfrac = longval % 1000;
	longval = longval / 1000;
	longsec = longval % 60;
	longval = longval / 60;
	longmin = longval % 60;
	longval = longval / 60;
	longdeg = longval;

	altfrac = altval % 100;
	altmeters = (altval / 100) * altsign;

	if ((sizestr = strdup(precsize_ntoa(sizeval))) == NULL)
		sizestr = error;
	if ((hpstr = strdup(precsize_ntoa(hpval))) == NULL)
		hpstr = error;
	if ((vpstr = strdup(precsize_ntoa(vpval))) == NULL)
		vpstr = error;

	sprintf(ascii,
	      "%d %.2d %.2d.%.3d %c %d %.2d %.2d.%.3d %c %d.%.2dm %sm %sm %sm",
		latdeg, latmin, latsec, latsecfrac, northsouth,
		longdeg, longmin, longsec, longsecfrac, eastwest,
		altmeters, altfrac, sizestr, hpstr, vpstr);

	if (sizestr != error)
		free(sizestr);
	if (hpstr != error)
		free(hpstr);
	if (vpstr != error)
		free(vpstr);

	return (ascii);
}
