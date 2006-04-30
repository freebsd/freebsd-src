/*
 * Copyright (c) 1996 by Internet Software Consortium.
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
 * Based on the Dynamic DNS reference implementation by Viraj Bais
 * <viraj_bais@ccm.fm.intel.com>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <limits.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "res_config.h"

static int getnum_str(u_char **, u_char *);
static int getword_str(char *, int, u_char **, u_char *);

#define ShrinkBuffer(x)  if ((buflen -= x) < 0) return (-2);

/*
 * Form update packets.
 * Returns the size of the resulting packet if no error
 * On error,
 *	returns -1 if error in reading a word/number in rdata
 *		   portion for update packets
 *		-2 if length of buffer passed is insufficient
 *		-3 if zone section is not the first section in
 *		   the linked list, or section order has a problem
 *		-4 on a number overflow
 *		-5 unknown operation or no records
 */
int
res_mkupdate(ns_updrec *rrecp_in, u_char *buf, int buflen) {
	ns_updrec *rrecp_start = rrecp_in;
	HEADER *hp;
	u_char *cp, *sp2, *startp, *endp;
	int n, i, soanum, multiline;
	ns_updrec *rrecp;
	struct in_addr ina;
        char buf2[MAXDNAME];
	int section, numrrs = 0, counts[ns_s_max];
	u_int16_t rtype, rclass;
	u_int32_t n1, rttl;
	u_char *dnptrs[20], **dpp, **lastdnptr;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NETDB_INTERNAL;
		return (-1);
	}

	/*
	 * Initialize header fields.
	 */
	if ((buf == NULL) || (buflen < HFIXEDSZ))
		return (-1);
	memset(buf, 0, HFIXEDSZ);
	hp = (HEADER *) buf;
	hp->id = htons(++_res.id);
	hp->opcode = ns_o_update;
	hp->rcode = NOERROR;
	cp = buf + HFIXEDSZ;
	buflen -= HFIXEDSZ;
	dpp = dnptrs;
	*dpp++ = buf;
	*dpp++ = NULL;
	lastdnptr = dnptrs + sizeof dnptrs / sizeof dnptrs[0];

	if (rrecp_start == NULL)
		return (-5);
	else if (rrecp_start->r_section != S_ZONE)
		return (-3);

	memset(counts, 0, sizeof counts);
	for (rrecp = rrecp_start; rrecp; rrecp = rrecp->r_grpnext) {
		numrrs++;
                section = rrecp->r_section;
		if (section < 0 || section >= ns_s_max)
			return (-1);
		counts[section]++;
		for (i = section + 1; i < ns_s_max; i++)
			if (counts[i])
				return (-3);
		rtype = rrecp->r_type;
		rclass = rrecp->r_class;
		rttl = rrecp->r_ttl;
		/* overload class and type */
		if (section == S_PREREQ) {
			rttl = 0;
			switch (rrecp->r_opcode) {
			case YXDOMAIN:
				rclass = C_ANY;
				rtype = T_ANY;
				rrecp->r_size = 0;
				break;
			case NXDOMAIN:
				rclass = C_NONE;
				rtype = T_ANY;
				rrecp->r_size = 0;
				break;
			case NXRRSET:
				rclass = C_NONE;
				rrecp->r_size = 0;
				break;
			case YXRRSET:
				if (rrecp->r_size == 0)
					rclass = C_ANY;
				break;
			default:
				fprintf(stderr,
					"res_mkupdate: incorrect opcode: %d\n",
					rrecp->r_opcode);
				fflush(stderr);
				return (-1);
			}
		} else if (section == S_UPDATE) {
			switch (rrecp->r_opcode) {
			case DELETE:
				rclass = rrecp->r_size == 0 ? C_ANY : C_NONE;
				break;
			case ADD:
				break;
			default:
				fprintf(stderr,
					"res_mkupdate: incorrect opcode: %d\n",
					rrecp->r_opcode);
				fflush(stderr);
				return (-1);
			}
		}

		/*
		 * XXX	appending default domain to owner name is omitted,
		 *	fqdn must be provided
		 */
		if ((n = dn_comp(rrecp->r_dname, cp, buflen, dnptrs,
				 lastdnptr)) < 0)
			return (-1);
		cp += n;
		ShrinkBuffer(n + 2*INT16SZ);
		PUTSHORT(rtype, cp);
		PUTSHORT(rclass, cp);
		if (section == S_ZONE) {
			if (numrrs != 1 || rrecp->r_type != T_SOA)
				return (-3);
			continue;
		}
		ShrinkBuffer(INT32SZ + INT16SZ);
		PUTLONG(rttl, cp);
		sp2 = cp;  /* save pointer to length byte */
		cp += INT16SZ;
		if (rrecp->r_size == 0) {
			if (section == S_UPDATE && rclass != C_ANY)
				return (-1);
			else {
				PUTSHORT(0, sp2);
				continue;
			}
		}
		startp = rrecp->r_data;
		endp = startp + rrecp->r_size - 1;
		/* XXX this should be done centrally. */
		switch (rrecp->r_type) {
		case T_A:
			if (!getword_str(buf2, sizeof buf2, &startp, endp))
				return (-1);
			if (!inet_aton(buf2, &ina))
				return (-1);
			n1 = ntohl(ina.s_addr);
			ShrinkBuffer(INT32SZ);
			PUTLONG(n1, cp);
			break;
		case T_CNAME:
		case T_MB:
		case T_MG:
		case T_MR:
		case T_NS:
		case T_PTR:
			if (!getword_str(buf2, sizeof buf2, &startp, endp))
				return (-1);
			n = dn_comp(buf2, cp, buflen, dnptrs, lastdnptr);
			if (n < 0)
				return (-1);
			cp += n;
			ShrinkBuffer(n);
			break;
		case T_MINFO:
		case T_SOA:
		case T_RP:
			for (i = 0; i < 2; i++) {
				if (!getword_str(buf2, sizeof buf2, &startp,
						 endp))
				return (-1);
				n = dn_comp(buf2, cp, buflen,
					    dnptrs, lastdnptr);
				if (n < 0)
					return (-1);
				cp += n;
				ShrinkBuffer(n);
			}
			if (rrecp->r_type == T_SOA) {
				ShrinkBuffer(5 * INT32SZ);
				while (isspace(*startp) || !*startp)
					startp++;
				if (*startp == '(') {
					multiline = 1;
					startp++;
				} else
					multiline = 0;
				/* serial, refresh, retry, expire, minimum */
				for (i = 0; i < 5; i++) {
					soanum = getnum_str(&startp, endp);
					if (soanum < 0)
						return (-1);
					PUTLONG(soanum, cp);
				}
				if (multiline) {
					while (isspace(*startp) || !*startp)
						startp++;
					if (*startp != ')')
						return (-1);
				}
			}
			break;
		case T_MX:
		case T_AFSDB:
		case T_RT:
			n = getnum_str(&startp, endp);
			if (n < 0)
				return (-1);
			PUTSHORT(n, cp);
			ShrinkBuffer(INT16SZ);
			if (!getword_str(buf2, sizeof buf2, &startp, endp))
				return (-1);
			n = dn_comp(buf2, cp, buflen, dnptrs, lastdnptr);
			if (n < 0)
				return (-1);
			cp += n;
			ShrinkBuffer(n);
			break;
		case T_PX:
			n = getnum_str(&startp, endp);
			if (n < 0)
				return (-1);
			PUTSHORT(n, cp);
			ShrinkBuffer(INT16SZ);
			for (i = 0; i < 2; i++) {
				if (!getword_str(buf2, sizeof buf2, &startp,
						 endp))
					return (-1);
				n = dn_comp(buf2, cp, buflen, dnptrs,
					    lastdnptr);
				if (n < 0)
					return (-1);
				cp += n;
				ShrinkBuffer(n);
			}
			break;
		case T_WKS:
		case T_HINFO:
		case T_TXT:
		case T_X25:
		case T_ISDN:
		case T_NSAP:
		case T_LOC:
			/* XXX - more fine tuning needed here */
			ShrinkBuffer(rrecp->r_size);
			memcpy(cp, rrecp->r_data, rrecp->r_size);
			cp += rrecp->r_size;
			break;
		default:
			return (-1);
		} /*switch*/
		n = (u_int16_t)((cp - sp2) - INT16SZ);
		PUTSHORT(n, sp2);
	} /*for*/
		
	hp->qdcount = htons(counts[0]);
	hp->ancount = htons(counts[1]);
	hp->nscount = htons(counts[2]);
	hp->arcount = htons(counts[3]);
	return (cp - buf);
}

/*
 * Get a whitespace delimited word from a string (not file)
 * into buf. modify the start pointer to point after the
 * word in the string.
 */
static int
getword_str(char *buf, int size, u_char **startpp, u_char *endp) {
        char *cp;
        int c;
 
        for (cp = buf; *startpp <= endp; ) {
                c = **startpp;
                if (isspace(c) || c == '\0') {
                        if (cp != buf) /* trailing whitespace */
                                break;
                        else { /* leading whitespace */
                                (*startpp)++;
                                continue;
                        }
                }
                (*startpp)++;
                if (cp >= buf+size-1)
                        break;
                *cp++ = (u_char)c;
        }
        *cp = '\0';
        return (cp != buf);
}

/*
 * Get a whitespace delimited number from a string (not file) into buf
 * update the start pointer to point after the number in the string.
 */
static int
getnum_str(u_char **startpp, u_char *endp) {
        int c, n;
        int seendigit = 0;
        int m = 0;

        for (n = 0; *startpp <= endp; ) {
                c = **startpp;
                if (isspace(c) || c == '\0') {
                        if (seendigit) /* trailing whitespace */
                                break;
                        else { /* leading whitespace */
                                (*startpp)++;
                                continue;
                        }
                }
                if (c == ';') {
                        while ((*startpp <= endp) &&
			       ((c = **startpp) != '\n'))
					(*startpp)++;
                        if (seendigit)
                                break;
                        continue;
                }
                if (!isdigit(c)) {
                        if (c == ')' && seendigit) {
                                (*startpp)--;
                                break;
                        }
			return (-1);
                }        
                (*startpp)++;
                n = n * 10 + (c - '0');
                seendigit = 1;
        }
        return (n + m);
}

/*
 * Allocate a resource record buffer & save rr info.
 */
ns_updrec *
res_mkupdrec(int section, const char *dname,
	     u_int class, u_int type, u_long ttl) {
	ns_updrec *rrecp = (ns_updrec *)calloc(1, sizeof(ns_updrec));

	if (!rrecp || !(rrecp->r_dname = strdup(dname)))
		return (NULL);
 	rrecp->r_class = class;
	rrecp->r_type = type;
	rrecp->r_ttl = ttl;
	rrecp->r_section = section;
	return (rrecp);
}

/*
 * Free a resource record buffer created by res_mkupdrec.
 */
void
res_freeupdrec(ns_updrec *rrecp) {
	/* Note: freeing r_dp is the caller's responsibility. */
	if (rrecp->r_dname != NULL)
		free(rrecp->r_dname);
	free(rrecp);
}
