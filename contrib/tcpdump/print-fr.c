/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static const char rcsid[] _U_ =
	"@(#)$Header: /tcpdump/master/tcpdump/print-fr.c,v 1.17.2.3 2003/12/15 03:37:45 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <string.h>
#include <pcap.h>

#include "addrtoname.h"
#include "interface.h"
#include "ethertype.h"
#include "extract.h"

static void lmi_print(const u_char *, u_int);

#define NLPID_LMI       0x08   /* ANSI T1.617 Annex D or ITU-T Q.933 Annex A */
#define NLPID_CISCO_LMI 0x09   /* The original, aka Cisco, aka Gang of Four */
#define NLPID_SNAP      0x80
#define NLPID_CLNP      0x81
#define NLPID_ESIS      0x82
#define NLPID_ISIS      0x83
#define NLPID_CONS      0x84
#define NLPID_IDRP      0x85
#define NLPID_X25_ESIS  0x8a
#define NLPID_IPV6      0x8e
#define NLPID_IP        0xcc

#define FR_EA_BIT	0x01


/* Finds out Q.922 address length, DLCI and flags. Returns 0 on success */
static int parse_q922_addr(const u_char *p, u_int *dlci, u_int *addr_len,
			   char **flags_ptr)
{
	static char flags[32];
	size_t len;

	if ((p[0] & FR_EA_BIT))
		return -1;

	*flags_ptr = flags;
	*addr_len = 2;
	*dlci = ((p[0] & 0xFC) << 2) | ((p[1] & 0xF0) >> 4);

	strcpy(flags, (p[0] & 0x02) ? "C!, " : "");
	if (p[1] & 0x08)
		strcat(flags, "FECN, ");
	if (p[1] & 0x04)
		strcat(flags, "BECN, ");
	if (p[1] & 0x02)
		strcat(flags, "DE, ");

	len = strlen(flags);
	if (len > 1)
		flags[len - 2] = '\x0';	/* delete trailing comma and space */

	if (p[1] & FR_EA_BIT)
		return 0;	/* 2-byte Q.922 address */

	p += 2;
	(*addr_len)++;		/* 3- or 4-byte Q.922 address */
	if ((p[0] & FR_EA_BIT) == 0) {
		*dlci = (*dlci << 7) | (p[0] >> 1);
		(*addr_len)++;	/* 4-byte Q.922 address */
		p++;
	}

	if ((p[0] & FR_EA_BIT) == 0)
		return -1; /* more than 4 bytes of Q.922 address? */

	if (p[0] & 0x02) {
		len = strlen(flags);
		snprintf(flags + len, sizeof(flags) - len,
			 "%sdlcore %x", len ? ", " : "", p[0] >> 2);
	} else
		*dlci = (*dlci << 6) | (p[0] >> 2);

	return 0;
}


static const char *fr_nlpids[256];

static void
init_fr_nlpids(void)
{
	int i;
	static int fr_nlpid_flag = 0;

	if (!fr_nlpid_flag) {
		for (i=0; i < 256; i++)
			fr_nlpids[i] = NULL;
		fr_nlpids[NLPID_LMI] = "LMI";
		fr_nlpids[NLPID_CISCO_LMI] = "Cisco LMI";
		fr_nlpids[NLPID_SNAP] = "SNAP";
		fr_nlpids[NLPID_CLNP] = "CLNP";
		fr_nlpids[NLPID_ESIS] = "ESIS";
		fr_nlpids[NLPID_ISIS] = "ISIS";
		fr_nlpids[NLPID_CONS] = "CONS";
		fr_nlpids[NLPID_IDRP] = "IDRP";
		fr_nlpids[NLPID_X25_ESIS] = "X25_ESIS";
		fr_nlpids[NLPID_IP] = "IP";
	}
	fr_nlpid_flag = 1;
}

/* Frame Relay packet structure, with flags and CRC removed

                  +---------------------------+
                  |       Q.922 Address*      |
                  +--                       --+
                  |                           |
                  +---------------------------+
                  | Control (UI = 0x03)       |
                  +---------------------------+
                  | Optional Pad      (0x00)  |
                  +---------------------------+
                  | NLPID                     |
                  +---------------------------+
                  |             .             |
                  |             .             |
                  |             .             |
                  |           Data            |
                  |             .             |
                  |             .             |
                  +---------------------------+

           * Q.922 addresses, as presently defined, are two octets and
             contain a 10-bit DLCI.  In some networks Q.922 addresses
             may optionally be increased to three or four octets.
*/

static u_int
fr_hdrlen(const u_char *p, u_int addr_len, u_int caplen)
{
	if ((caplen > addr_len + 1 /* UI */ + 1 /* pad */) &&
	    !p[addr_len + 1] /* pad exist */)
		return addr_len + 1 /* UI */ + 1 /* pad */ + 1 /* NLPID */;
	else 
		return addr_len + 1 /* UI */ + 1 /* NLPID */;
}

static const char *
fr_protostring(u_int8_t proto)
{
	static char buf[5+1+2+1];

	init_fr_nlpids();

	if (nflag || fr_nlpids[proto] == NULL) {
		snprintf(buf, sizeof(buf), "proto %02x", proto);
		return buf;
	}
	return fr_nlpids[proto];
}

static void
fr_hdr_print(int length, u_int dlci, char *flags, u_char nlpid)
{
	if (qflag)
		(void)printf("DLCI %u, %s%slength %d: ",
			     dlci, flags, *flags ? ", " : "", length);
	else
		(void)printf("DLCI %u, %s%s%s, length %d: ",
			     dlci, flags, *flags ? ", " : "",
			     fr_protostring(nlpid), length);
}

u_int
fr_if_print(const struct pcap_pkthdr *h, register const u_char *p)
{
	register u_int length = h->len;
	register u_int caplen = h->caplen;
	u_short extracted_ethertype;
	u_int32_t orgcode;
	register u_short et;
	u_int dlci;
	int addr_len;
	u_char nlpid;
	u_int hdr_len;
	char *flags;

	if (caplen < 4) {	/* minimum frame header length */
		printf("[|fr]");
		return caplen;
	}

	if (parse_q922_addr(p, &dlci, &addr_len, &flags)) {
		printf("Invalid Q.922 address");
		return caplen;
	}

	hdr_len = fr_hdrlen(p, addr_len, caplen);

	if (caplen < hdr_len) {
		printf("[|fr]");
		return caplen;
	}

	if (p[addr_len] != 0x03)
		printf("UI %02x! ", p[addr_len]);

	if (!p[addr_len + 1]) {	/* pad byte should be used with 3-byte Q.922 */
		if (addr_len != 3)
			printf("Pad! ");
	} else if (addr_len == 3)
		printf("No pad! ");

	nlpid = p[hdr_len - 1];

	p += hdr_len;
	length -= hdr_len;
	caplen -= hdr_len;

	if (eflag)
		fr_hdr_print(length, dlci, flags, nlpid);

	switch (nlpid) {
	case NLPID_IP:
		ip_print(p, length);
		break;

#ifdef INET6
	case NLPID_IPV6:
		ip6_print(p, length);
		break;
#endif
	case NLPID_CLNP:
	case NLPID_ESIS:
	case NLPID_ISIS:
		isoclns_print(p, length, caplen);
		break;

	case NLPID_SNAP:
		orgcode = EXTRACT_24BITS(p);
		et = EXTRACT_16BITS(p + 3);
		if (snap_print((const u_char *)(p + 5), length - 5,
			   caplen - 5, &extracted_ethertype, orgcode, et,
			   0) == 0) {
			/* ether_type not known, print raw packet */
			if (!eflag)
				fr_hdr_print(length + hdr_len,
					     dlci, flags, nlpid);
			if (extracted_ethertype) {
				printf("(SNAP %s) ",
			       etherproto_string(htons(extracted_ethertype)));
			}
			if (!xflag && !qflag)
				default_print(p - hdr_len, caplen + hdr_len);
		}
		break;

	case NLPID_LMI:
		lmi_print(p, length);
		break;

	default:
		if (!eflag)
			fr_hdr_print(length + hdr_len,
				     dlci, flags, nlpid);
		if (!xflag)
			default_print(p, caplen);
	}

	return hdr_len;
}

/*
 * Q.933 decoding portion for framerelay specific.
 */

/* Q.933 packet format
                      Format of Other Protocols   
                          using Q.933 NLPID
                  +-------------------------------+            
                  |        Q.922 Address          | 
                  +---------------+---------------+
                  |Control  0x03  | NLPID   0x08  |        
                  +---------------+---------------+        
                  |          L2 Protocol ID       |
                  | octet 1       |  octet 2      |
                  +-------------------------------+
                  |          L3 Protocol ID       |
                  | octet 2       |  octet 2      |
                  +-------------------------------+
                  |         Protocol Data         |
                  +-------------------------------+
                  | FCS                           |
                  +-------------------------------+
 */

/* L2 (Octet 1)- Call Reference Usually is 0x0 */

/*
 * L2 (Octet 2)- Message Types definition 1 byte long.
 */
/* Call Establish */
#define MSG_TYPE_ESC_TO_NATIONAL  0x00
#define MSG_TYPE_ALERT            0x01
#define MSG_TYPE_CALL_PROCEEDING  0x02
#define MSG_TYPE_CONNECT          0x07
#define MSG_TYPE_CONNECT_ACK      0x0F
#define MSG_TYPE_PROGRESS         0x03
#define MSG_TYPE_SETUP            0x05
/* Call Clear */
#define MSG_TYPE_DISCONNECT       0x45
#define MSG_TYPE_RELEASE          0x4D
#define MSG_TYPE_RELEASE_COMPLETE 0x5A
#define MSG_TYPE_RESTART          0x46
#define MSG_TYPE_RESTART_ACK      0x4E
/* Status */
#define MSG_TYPE_STATUS           0x7D
#define MSG_TYPE_STATUS_ENQ       0x75

#define MSG_ANSI_LOCKING_SHIFT	0x95
#define ONE_BYTE_IE_MASK	0xF0 /* details? */

#define ANSI_REPORT_TYPE_IE	0x01
#define ANSI_LINK_VERIFY_IE_91	0x19 /* details? */
#define ANSI_LINK_VERIFY_IE	0x03
#define ANSI_PVC_STATUS_IE	0x07

#define CCITT_REPORT_TYPE_IE	0x51
#define CCITT_LINK_VERIFY_IE	0x53
#define CCITT_PVC_STATUS_IE	0x57

struct common_ie_header {
    u_int8_t ie_id;
    u_int8_t ie_len;
};

#define FULL_STATUS 0
#define LINK_VERIFY 1
#define ASYNC_PVC   2


/* Parses DLCI information element. */
static const char * parse_dlci_ie(const u_char *p, u_int ie_len, char *buffer,
			    size_t buffer_len)
{
	u_int dlci;

	if ((ie_len < 3) ||
	    (p[0] & 0x80) ||
	    ((ie_len == 3) && !(p[1] & 0x80)) ||
	    ((ie_len == 4) && ((p[1] & 0x80) || !(p[2] & 0x80))) ||
	    ((ie_len == 5) && ((p[1] & 0x80) || (p[2] & 0x80) ||
			       !(p[3] & 0x80))) ||
	    (ie_len > 5) ||
	    !(p[ie_len - 1] & 0x80))
		return "Invalid DLCI IE";

	dlci = ((p[0] & 0x3F) << 4) | ((p[1] & 0x78) >> 3);
	if (ie_len == 4)
		dlci = (dlci << 6) | ((p[2] & 0x7E) >> 1);
	else if (ie_len == 5)
		dlci = (dlci << 13) | (p[2] & 0x7F) | ((p[3] & 0x7E) >> 1);

	snprintf(buffer, buffer_len, "DLCI %d: status %s%s", dlci,
		 p[ie_len - 1] & 0x8 ? "New, " : "",
		 p[ie_len - 1] & 0x2 ? "Active" : "Inactive");

	return buffer;
}


static void
lmi_print(const u_char *p, u_int length)
{
	const u_char *ptemp = p;
	const char *decode_str;
	char temp_str[255];
	struct common_ie_header *ie_p;
	int is_ansi = 0;

	if (length < 9) {	/* shortest: Q.933a LINK VERIFY */
		printf("[|lmi]");
		return;
	}

	if (p[2] == MSG_ANSI_LOCKING_SHIFT)
		is_ansi = 1;
    
	/* printing out header part */
	printf(is_ansi ? "ANSI" : "CCITT");
	if (p[0])
		printf(" Call Ref: %02x!", p[0]);

	switch(p[1]) {

	case MSG_TYPE_STATUS:
		printf(" STATUS REPLY\n");
		break;

	case MSG_TYPE_STATUS_ENQ:
		printf(" STATUS ENQUIRY\n");
		break;

	default:
		printf(" UNKNOWN MSG Type %02x\n", p[1]);
		break;
	}

	if (length < (u_int)(2 - is_ansi)) {
		printf("[|lmi]");
		return;
	}
	length -= 2 - is_ansi;
	ptemp += 2 + is_ansi;
	
	/* Loop through the rest of IE */
	while (length > 0) {
		ie_p = (struct common_ie_header *)ptemp;
		if (length < sizeof(struct common_ie_header) ||
		    length < sizeof(struct common_ie_header) + ie_p->ie_len) {
			printf("[|lmi]");
			return;
		}

		if ((is_ansi && ie_p->ie_id == ANSI_REPORT_TYPE_IE) ||
		    (!is_ansi && ie_p->ie_id == CCITT_REPORT_TYPE_IE)) {
			switch(ptemp[2]) {

			case FULL_STATUS:
				decode_str = "FULL STATUS";
				break;

			case LINK_VERIFY:
				decode_str = "LINK VERIFY";
				break;

			case ASYNC_PVC:
				decode_str = "Async PVC Status";
				break;

			default:
				decode_str = "Reserved Value";
				break;
			}
		} else if ((is_ansi && (ie_p->ie_id == ANSI_LINK_VERIFY_IE_91 ||
				        ie_p->ie_id == ANSI_LINK_VERIFY_IE)) ||
			  (!is_ansi && ie_p->ie_id == CCITT_LINK_VERIFY_IE)) {
			snprintf(temp_str, sizeof(temp_str),
			     "TX Seq: %3d, RX Seq: %3d",
			     ptemp[2], ptemp[3]);
			decode_str = temp_str;
		} else if ((is_ansi && ie_p->ie_id == ANSI_PVC_STATUS_IE) ||
			   (!is_ansi && ie_p->ie_id == CCITT_PVC_STATUS_IE)) {
			decode_str = parse_dlci_ie(ptemp + 2, ie_p->ie_len,
						   temp_str, sizeof(temp_str));
		} else
			decode_str = "Non-decoded Value";		    

		printf("\t\tIE: %02X Len: %d, %s\n",
		       ie_p->ie_id, ie_p->ie_len, decode_str);
		length = length - ie_p->ie_len - 2;
		ptemp = ptemp + ie_p->ie_len + 2;
	}
}
