/*
 * ++Copyright++ 1985, 1989
 * -
 * Copyright (c) 1985, 1989
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

#ifndef lint
static char sccsid[] = "@(#)debug.c	5.26 (Berkeley) 3/21/91";
static char rcsid[] = "$Id: debug.c,v 8.10 1996/12/18 04:09:50 vixie Exp $";
#endif /* not lint */

/*
 *******************************************************************************
 *
 *  debug.c --
 *
 *	Routines to print out packets received from a name server query.
 *
 *      Modified version of 4.3BSD BIND res_debug.c 5.30 6/27/90
 *
 *******************************************************************************
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <netdb.h>
#include <stdio.h>
#include "res.h"
#include "../../conf/portability.h"

/*
 *  Imported from res_debug.c
 */
extern char *_res_resultcodes[];
extern char *_res_opcodes[];

/*
 *  Used to highlight the start of a record when printing it.
 */
#define INDENT "    ->  "



/*
 * Print the contents of a query.
 * This is intended to be primarily a debugging routine.
 */

Print_query(msg, eom, printHeader)
	char *msg, *eom;
	int printHeader;
{
	Fprint_query(msg, eom, printHeader,stdout);
}

Fprint_query(msg, eom, printHeader,file)
	u_char *msg, *eom;
	int printHeader;
	FILE *file;
{
	register u_char *cp;
	register HEADER *hp;
	register int n;
	short class;
	short type;

	/*
	 * Print header fields.
	 */
	hp = (HEADER *)msg;
	cp = msg + HFIXEDSZ;
	if (printHeader || (_res.options & RES_DEBUG2)) {
	    fprintf(file,"    HEADER:\n");
	    fprintf(file,"\topcode = %s", _res_opcodes[hp->opcode]);
	    fprintf(file,", id = %d", ntohs(hp->id));
	    fprintf(file,", rcode = %s\n", _res_resultcodes[hp->rcode]);
	    fprintf(file,"\theader flags: ");
	    if (hp->qr) {
		    fprintf(file," response");
	    } else {
		    fprintf(file," query");
	    }
	    if (hp->aa)
		    fprintf(file,", auth. answer");
	    if (hp->tc)
		    fprintf(file,", truncation");
	    if (hp->rd)
		    fprintf(file,", want recursion");
	    if (hp->ra)
		    fprintf(file,", recursion avail.");
	    if (hp->unused)
		    fprintf(file,", UNUSED-QUERY_BIT");
	    if (hp->ad)
		    fprintf(file,", authentic data");
	    if (hp->cd)
		    fprintf(file,", checking disabled");
	    fprintf(file,"\n\tquestions = %d", ntohs(hp->qdcount));
	    fprintf(file,",  answers = %d", ntohs(hp->ancount));
	    fprintf(file,",  authority records = %d", ntohs(hp->nscount));
	    fprintf(file,",  additional = %d\n\n", ntohs(hp->arcount));
	}

	/*
	 * Print question records.
	 */
	if (n = ntohs(hp->qdcount)) {
		fprintf(file,"    QUESTIONS:\n");
		while (--n >= 0) {
			fprintf(file,"\t");
			cp = Print_cdname(cp, msg, eom, file);
			if (cp == NULL)
				return;
			type = _getshort((u_char*)cp);
			cp += INT16SZ;
			class = _getshort((u_char*)cp);
			cp += INT16SZ;
			fprintf(file,", type = %s", p_type(type));
			fprintf(file,", class = %s\n", p_class(class));
		}
	}
	/*
	 * Print authoritative answer records
	 */
	if (n = ntohs(hp->ancount)) {
		fprintf(file,"    ANSWERS:\n");
		if (type == T_A && n > MAXADDRS) {
			printf("Limiting response to MAX Addrs = %d \n",
			       MAXADDRS);
			n = MAXADDRS;
		}
		while (--n >= 0) {
			fprintf(file, INDENT);
			cp = Print_rr(cp, msg, eom, file);
			if (cp == NULL)
				return;
		}
	}
	/*
	 * print name server records
	 */
	if (n = ntohs(hp->nscount)) {
		fprintf(file,"    AUTHORITY RECORDS:\n");
		while (--n >= 0) {
			fprintf(file, INDENT);
			cp = Print_rr(cp, msg, eom, file);
			if (cp == NULL)
				return;
		}
	}
	/*
	 * print additional records
	 */
	if (n = ntohs(hp->arcount)) {
		fprintf(file,"    ADDITIONAL RECORDS:\n");
		while (--n >= 0) {
			fprintf(file, INDENT);
			cp = Print_rr(cp, msg, eom, file);
			if (cp == NULL)
				return;
		}
	}
	fprintf(file,"\n------------\n");
}


u_char *
Print_cdname_sub(cp, msg, eom, file, format)
	u_char *cp, *msg, *eom;
	FILE *file;
	int format;
{
	int n;
	char name[MAXDNAME];

	n = dn_expand(msg, eom, cp, name, sizeof name);
	if (n < 0)
		return (NULL);
	if (name[0] == '\0') {
	    (void) strcpy(name, "(root)");
	}
	if (format) {
	    fprintf(file, "%-30s", name);
	} else {
	    fputs(name, file);
	}
	return (cp + n);
}

u_char *
Print_cdname(cp, msg, eom, file)
	u_char *cp, *msg, *eom;
	FILE *file;
{
	return (Print_cdname_sub(cp, msg, eom, file, 0));
}

u_char *
Print_cdname2(cp, msg, eom, file)
	u_char *cp, *msg, *eom;
	FILE *file;
{
	return (Print_cdname_sub(cp, msg, eom, file, 1));
}

/*
 * Print resource record fields in human readable form.
 */
u_char *
Print_rr(ocp, msg, eom, file)
	u_char *ocp, *msg, *eom;
	FILE *file;
{
	int type, class, dlen, n, c;
	u_int32_t rrttl, ttl;
	struct in_addr inaddr;
	u_char *cp, *cp1, *cp2;
	int debug;

	if ((cp = Print_cdname(ocp, msg, eom, file)) == NULL) {
		fprintf(file, "(name truncated?)\n");
		return (NULL);			/* compression error */
	}

	type = _getshort((u_char*)cp);
	cp += INT16SZ;
	class = _getshort((u_char*)cp);
	cp += INT16SZ;
	rrttl = _getlong((u_char*)cp);
	cp += INT32SZ;
	dlen = _getshort((u_char*)cp);
	cp += INT16SZ;

	debug = _res.options & (RES_DEBUG|RES_DEBUG2);
	if (debug) {
	    if (_res.options & RES_DEBUG2) {
		fprintf(file,"\n\ttype = %s, class = %s, dlen = %d",
			    p_type(type), p_class(class), dlen);
	    }
	    if (type == T_SOA) {
		fprintf(file,"\n\tttl = %lu (%s)", rrttl, p_time(rrttl));
	    }
	    (void) putc('\n', file);
	} 

	cp1 = cp;

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
				fprintf(file,"\tinternet address = %s\n",
					inet_ntoa(inaddr));
				cp += dlen;
			} else if (dlen == 7) {
				fprintf(file,"\tinternet address = %s",
					inet_ntoa(inaddr));
				fprintf(file,", protocol = %d", cp[4]);
				fprintf(file,", port = %d\n",
					(cp[5] << 8) + cp[6]);
				cp += dlen;
			}
			break;
		default:
			fprintf(file,"\taddress, class = %d, len = %d\n",
			    class, dlen);
			cp += dlen;
		}
		break;

	case T_CNAME:
		fprintf(file,"\tcanonical name = ");
		goto doname;

	case T_MG:
		fprintf(file,"\tmail group member = ");
		goto doname;
	case T_MB:
		fprintf(file,"\tmail box = ");
		goto doname;
	case T_MR:
		fprintf(file,"\tmailbox rename = ");
		goto doname;
	case T_MX:
		fprintf(file,"\tpreference = %u",_getshort((u_char*)cp));
		cp += INT16SZ;
		fprintf(file,", mail exchanger = ");
		goto doname;

	case T_NAPTR: 
		fprintf(file, "\torder = %u",_getshort((u_char*)cp));
		cp += INT16SZ;
		fprintf(file,", preference = %u\n", _getshort((u_char*)cp));
		cp += INT16SZ;
		/* Flags */
		n = *cp++;
		fprintf(file,"\tflags = \"%.*s\"\n", (int)n, cp);
		cp += n;
		/* Service */
		n = *cp++;
		fprintf(file,"\tservices = \"%.*s\"\n", (int)n, cp);
		cp += n;
		/* Regexp */
		n = *cp++;
		fprintf(file,"\trule = \"%.*s\"\n", (int)n, cp);
		cp += n;
		/* Replacement */
		fprintf(file,"\treplacement = ");
                cp = Print_cdname(cp, msg, eom, file);
		if (cp == NULL) {
			fprintf(file, "(replacement truncated?)\n");
			return (NULL);			/* compression error */
		}
                (void) putc('\n', file);

		break;
	case T_SRV: 
		fprintf(file, "\tpriority = %u",_getshort((u_char*)cp));
		cp += INT16SZ;
		fprintf(file,", weight = %u", _getshort((u_char*)cp));
		cp += INT16SZ;
		fprintf(file,", port= %u\n", _getshort((u_char*)cp));
		cp += INT16SZ;

		fprintf(file,"\thost = ");
		goto doname;

        case T_PX:
                fprintf(file,"\tpreference = %u",_getshort((u_char*)cp));
                cp += INT16SZ;
                fprintf(file,", RFC 822 = ");
                cp = Print_cdname(cp, msg, eom, file);
		if (cp == NULL) {
			fprintf(file, "(name truncated?)\n");
			return (NULL);			/* compression error */
		}
                fprintf(file,"\nX.400 = ");
                cp = Print_cdname(cp, msg, eom, file);
		if (cp == NULL) {
			fprintf(file, "(name truncated?)\n");
			return (NULL);			/* compression error */
		}
                (void) putc('\n', file);
                break;
	case T_RT:
		fprintf(file,"\tpreference = %u",_getshort((u_char*)cp));
		cp += INT16SZ;
		fprintf(file,", router = ");
		goto doname;
	case T_AFSDB:
		fprintf(file,"\tsubtype = %d",_getshort((u_char*)cp));
		cp += INT16SZ;
		fprintf(file,", DCE/AFS server = ");
		goto doname;
	case T_NS:
		fprintf(file,"\tnameserver = ");
		goto doname;
	case T_PTR:
		fprintf(file,"\tname = ");
doname:
		cp = Print_cdname(cp, msg, eom, file);
		if (cp == NULL) {
			fprintf(file, "(name truncated?)\n");
			return (NULL);			/* compression error */
		}
		(void) putc('\n', file);
		break;

	case T_HINFO:
		cp2 = cp + dlen;
		if (n = *cp++) {
			fprintf(file,"\tCPU = %.*s", n, cp);
			cp += n;
		}
		if ((cp < cp2) && (n = *cp++)) {
			fprintf(file,"\tOS = %.*s\n", n, cp);
			cp += n;
		} else fprintf(file, "\n*** Warning *** OS-type missing\n");
		break;

	case T_ISDN:
		cp2 = cp + dlen;
		if (n = *cp++) {
			fprintf(file,"\tISDN = \"%.*s", n, cp);
			cp += n;
		}
		if ((cp < cp2) && (n = *cp++)) {
			fprintf(file,"-%.*s\"\n", n, cp);
			cp += n;
		} else fprintf(file,"\"\n");
		break;


	case T_SOA:
		if (!debug)
		    (void) putc('\n', file);
		fprintf(file,"\torigin = ");
		cp = Print_cdname(cp, msg, eom, file);
		if (cp == NULL) {
			fprintf(file, "(name truncated?)\n");
			return (NULL);			/* compression error */
		}
		fprintf(file,"\n\tmail addr = ");
		cp = Print_cdname(cp, msg, eom, file);
		if (cp == NULL) {
			fprintf(file, "(name truncated?)\n");
			return (NULL);			/* compression error */
		}
		fprintf(file,"\n\tserial = %lu", _getlong((u_char*)cp));
		cp += INT32SZ;
		ttl = _getlong((u_char*)cp);
		fprintf(file,"\n\trefresh = %lu (%s)", ttl, p_time(ttl));
		cp += INT32SZ;
		ttl = _getlong((u_char*)cp);
		fprintf(file,"\n\tretry   = %lu (%s)", ttl, p_time(ttl));
		cp += INT32SZ;
		ttl = _getlong((u_char*)cp);
		fprintf(file,"\n\texpire  = %lu (%s)", ttl, p_time(ttl));
		cp += INT32SZ;
		ttl = _getlong((u_char*)cp);
		fprintf(file,
		    "\n\tminimum ttl = %lu (%s)\n", ttl, p_time(ttl));
		cp += INT32SZ;
		break;

	case T_MINFO:
		if (!debug)
		    (void) putc('\n', file);
		fprintf(file,"\trequests = ");
		cp = Print_cdname(cp, msg, eom, file);
		if (cp == NULL) {
			fprintf(file, "(name truncated?)\n");
			return (NULL);			/* compression error */
		}
		fprintf(file,"\n\terrors = ");
		cp = Print_cdname(cp, msg, eom, file);
		if (cp == NULL) {
			fprintf(file, "(name truncated?)\n");
			return (NULL);			/* compression error */
		}
		(void) putc('\n', file);
		break;
	case T_RP:
		if (!debug)
		    (void) putc('\n', file);
		fprintf(file,"\tmailbox = ");
		cp = Print_cdname(cp, msg, eom, file);
		if (cp == NULL) {
			fprintf(file, "(name truncated?)\n");
			return (NULL);			/* compression error */
		}
		fprintf(file,"\n\ttext = ");
		cp = Print_cdname(cp, msg, eom, file);
		if (cp == NULL) {
			fprintf(file, "(name truncated?)\n");
			return (NULL);			/* compression error */
		}
		(void) putc('\n', file);
		break;

	case T_TXT:
		(void) fputs("\ttext = ", file);
		cp2 = cp1 + dlen;
		while (cp < cp2) {
			(void) putc('"', file);
			if (n = (unsigned char) *cp++) {
				for (c = n; c > 0 && cp < cp2; c--) {
					if ((*cp == '\n') || (*cp == '"') || (*cp == '\\'))
						(void) putc('\\', file);
					(void) putc(*cp++, file);
				}
			}
			(void) putc('"', file);
			if (cp < cp2)
				(void) putc(' ', file);
		}
		(void) putc('\n', file);
  		break;

	case T_X25:
		(void) fputs("\tX25 = \"", file);
		cp2 = cp1 + dlen;
		while (cp < cp2) {
			if (n = (unsigned char) *cp++) {
				for (c = n; c > 0 && cp < cp2; c--)
					if (*cp == '\n') {
					    (void) putc('\\', file);
					    (void) putc(*cp++, file);
					} else
					    (void) putc(*cp++, file);
			}
		}
		(void) fputs("\"\n", file);
  		break;

	case T_NSAP:
		fprintf(file, "\tnsap = %s\n", inet_nsap_ntoa(dlen, cp, NULL));
		cp += dlen;
  		break;

	case T_AAAA: {
		char t[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];

		fprintf(file, "\tIPv6 address = %s\n",
			inet_ntop(AF_INET6, cp, t, sizeof t));
		cp += IN6ADDRSZ;
		break;
	}

	case T_UINFO:
		fprintf(file,"\tuser info = %s\n", cp);
		cp += dlen;
		break;

	case T_UID:
	case T_GID:
		if (dlen == 4) {
			fprintf(file,"\t%cid = %u\n",type == T_UID ? 'u' : 'g',
			    _getlong((u_char*)cp));
			cp += INT32SZ;
		} else {
			fprintf(file,"\t%cid of length %d?\n",
			    type == T_UID ? 'u' : 'g', dlen);
			cp += dlen;
		}
		break;

	case T_WKS: {
		struct protoent *protoPtr;

		if (dlen < INT32SZ + 1)
			break;
		if (!debug)
		    (void) putc('\n', file);
		bcopy(cp, (char *)&inaddr, INADDRSZ);
		cp += INT32SZ;
		if ((protoPtr = getprotobynumber(*cp)) != NULL) {
		    fprintf(file,"\tinet address = %s, protocol = %s\n\t",
			inet_ntoa(inaddr), protoPtr->p_name);
		} else {
		    fprintf(file,"\tinet address = %s, protocol = %d\n\t",
			inet_ntoa(inaddr), *cp);
		}
		cp++;
		n = 0;
		while (cp < cp1 + dlen) {
			c = *cp++;
			do {
				struct servent *s;

 				if (c & 0200) {
					s = getservbyport((int)htons(n),
					    protoPtr ? protoPtr->p_name : NULL);
					if (s != NULL) {
					    fprintf(file,"  %s", s->s_name);
					} else {
					    fprintf(file," #%d", n);
					}
				}
 				c <<= 1;
			} while (++n & 07);
		}
		putc('\n',file);
	    }
	    break;

	case T_NULL:
		fprintf(file, "\tNULL (dlen %d)\n", dlen);
		cp += dlen;
		break;

	default:
		fprintf(file,"\trecord type %d, interpreted as:\n", type);
		/* Let resolver library try to print it */
		p_rr(ocp, msg, file);
		cp += dlen;
	}
	if (_res.options & RES_DEBUG && type != T_SOA) {
	    fprintf(file,"\tttl = %lu (%s)\n", rrttl, p_time(rrttl));
	}
	if (cp != cp1 + dlen) {
		fprintf(file,
			"\n*** Error: record size incorrect (%d != %d)\n\n",
			cp - cp1, dlen);
		cp = NULL;
	}
	return (cp);
}
