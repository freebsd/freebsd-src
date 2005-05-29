/* 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * Original code by Hannes Gredler (hannes@juniper.net)
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-juniper.c,v 1.8 2005/04/06 21:32:41 mcr Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <pcap.h>
#include <stdio.h>

#include "interface.h"
#include "extract.h"
#include "ppp.h"
#include "llc.h"
#include "nlpid.h"

#define JUNIPER_BPF_OUT           0       /* Outgoing packet */
#define JUNIPER_BPF_IN            1       /* Incoming packet */
#define JUNIPER_BPF_PKT_IN        0x1     /* Incoming packet */
#define JUNIPER_BPF_NO_L2         0x2     /* L2 header stripped */

#define LS_COOKIE_ID            0x54
#define LS_MLFR_LEN		4
#define ML_MLFR_LEN		2

#define ATM2_PKT_TYPE_MASK  0x70
#define ATM2_GAP_COUNT_MASK 0x3F

int ip_heuristic_guess(register const u_char *, u_int);
int juniper_ppp_heuristic_guess(register const u_char *, u_int);
static int juniper_parse_header (const u_char *, u_int8_t *, u_int);

u_int
juniper_mlppp_print(const struct pcap_pkthdr *h, register const u_char *p)
{
	register u_int length = h->len;
	register u_int caplen = h->caplen;
        u_int8_t direction,bundle,cookie_len;
        u_int32_t cookie,proto;
        
        if(juniper_parse_header(p, &direction,length) == 0)
            return 0;

        p+=4;
        length-=4;
        caplen-=4;

        if (p[0] == LS_COOKIE_ID) {
            cookie=EXTRACT_32BITS(p);
            if (eflag) printf("LSPIC-MLPPP cookie 0x%08x, ",cookie);
            cookie_len = LS_MLFR_LEN;
            bundle = cookie & 0xff;
        } else {
            cookie=EXTRACT_16BITS(p);
            if (eflag) printf("MLPIC-MLPPP cookie 0x%04x, ",cookie);
            cookie_len = ML_MLFR_LEN;
            bundle = (cookie >> 8) & 0xff;
        }

        proto = EXTRACT_16BITS(p+cookie_len);        
        p += cookie_len;
        length-= cookie_len;
        caplen-= cookie_len;

        /* suppress Bundle-ID if frame was captured on a child-link
         * this may be the case if the cookie looks like a proto */
        if (eflag &&
            cookie != PPP_OSI &&
            cookie !=  (PPP_ADDRESS << 8 | PPP_CONTROL))
            printf("Bundle-ID %u, ",bundle);

        switch (cookie) {
        case PPP_OSI:
            ppp_print(p-2,length+2);
            break;
        case (PPP_ADDRESS << 8 | PPP_CONTROL): /* fall through */
        default:
            ppp_print(p,length);
            break;
        }

        return cookie_len;
}


u_int
juniper_mlfr_print(const struct pcap_pkthdr *h, register const u_char *p)
{
	register u_int length = h->len;
	register u_int caplen = h->caplen;
        u_int8_t direction,bundle,cookie_len;
        u_int32_t cookie,proto,frelay_len = 0;
        
        if(juniper_parse_header(p, &direction,length) == 0)
            return 0;

        p+=4;
        length-=4;
        caplen-=4;

        if (p[0] == LS_COOKIE_ID) {
            cookie=EXTRACT_32BITS(p);
            if (eflag) printf("LSPIC-MLFR cookie 0x%08x, ",cookie);
            cookie_len = LS_MLFR_LEN;
            bundle = cookie & 0xff;
        } else {
            cookie=EXTRACT_16BITS(p);
            if (eflag) printf("MLPIC-MLFR cookie 0x%04x, ",cookie);
            cookie_len = ML_MLFR_LEN;
            bundle = (cookie >> 8) & 0xff;
        }

        proto = EXTRACT_16BITS(p+cookie_len);        
        p += cookie_len+2;
        length-= cookie_len+2;
        caplen-= cookie_len+2;

        /* suppress Bundle-ID if frame was captured on a child-link */
        if (eflag && cookie != 1) printf("Bundle-ID %u, ",bundle);

        switch (proto) {
        case (LLC_UI):
        case (LLC_UI<<8):
            isoclns_print(p, length, caplen);
            break;
        case (LLC_UI<<8 | NLPID_Q933):
        case (LLC_UI<<8 | NLPID_IP):
        case (LLC_UI<<8 | NLPID_IP6):
            isoclns_print(p-1, length+1, caplen+1); /* pass IP{4,6} to the OSI layer for proper link-layer printing */
            break;
        default:
            printf("unknown protocol 0x%04x, length %u",proto, length);
        }

        return cookie_len + frelay_len;
}

/*
 *     ATM1 PIC cookie format
 *
 *     +-----+-------------------------+-------------------------------+
 *     |fmtid|     vc index            |  channel  ID                  |
 *     +-----+-------------------------+-------------------------------+
 */

u_int
juniper_atm1_print(const struct pcap_pkthdr *h, register const u_char *p)
{
	register u_int length = h->len;
	register u_int caplen = h->caplen;
        u_int16_t extracted_ethertype;
        u_int8_t direction;
        u_int32_t cookie1;

        if(juniper_parse_header(p, &direction,length) == 0)
            return 0;

        p+=4;
        length-=4;
        caplen-=4;

        cookie1=EXTRACT_32BITS(p);

        if (eflag) {
            /* FIXME decode channel-id, vc-index, fmt-id
               for once lets just hexdump the cookie */

            printf("ATM1 cookie 0x%08x, ", cookie1);
        }

        p+=4;
        length-=4;
        caplen-=4;

        if ((cookie1 >> 24) == 0x80) { /* OAM cell ? */
            oam_print(p,length);
            return 0;
        }

        if (EXTRACT_24BITS(p) == 0xfefe03 || /* NLPID encaps ? */
            EXTRACT_24BITS(p) == 0xaaaa03) { /* SNAP encaps ? */

            if (llc_print(p, length, caplen, NULL, NULL,
                          &extracted_ethertype) != 0)
                return 8;
        }

        if (p[0] == 0x03) { /* Cisco style NLPID encaps ? */
            isoclns_print(p + 1, length - 1, caplen - 1);
            /* FIXME check if frame was recognized */
            return 8;
        }

        if(ip_heuristic_guess(p, length) != 0) /* last try - vcmux encaps ? */
            return 0;

	return (8);
}

/*
 *     ATM2 PIC cookie format
 *
 *     +-------------------------------+---------+---+-----+-----------+
 *     |     channel ID                |  reserv |AAL| CCRQ| gap cnt   |
 *     +-------------------------------+---------+---+-----+-----------+
 */

u_int
juniper_atm2_print(const struct pcap_pkthdr *h, register const u_char *p)
{
	register u_int length = h->len;
	register u_int caplen = h->caplen;
        u_int16_t extracted_ethertype;
        u_int8_t direction;
        u_int32_t cookie1,cookie2;

        if(juniper_parse_header(p, &direction,length) == 0)
            return 0;

        p+=4;
        length-=4;
        caplen-=4;

        cookie1=EXTRACT_32BITS(p);
        cookie2=EXTRACT_32BITS(p+4);

        if (eflag) {
            /* FIXME decode channel, fmt-id, ccrq, aal, gap cnt
               for once lets just hexdump the cookie */

            printf("ATM2 cookie 0x%08x%08x, ",
                   EXTRACT_32BITS(p),
                   EXTRACT_32BITS(p+4));
        }

        p+=8;
        length-=8;
        caplen-=8;

        if (cookie2 & ATM2_PKT_TYPE_MASK) { /* OAM cell ? */
            oam_print(p,length);
            return 12;
        }

        if (EXTRACT_24BITS(p) == 0xfefe03 || /* NLPID encaps ? */
            EXTRACT_24BITS(p) == 0xaaaa03) { /* SNAP encaps ? */

            if (llc_print(p, length, caplen, NULL, NULL,
                          &extracted_ethertype) != 0)
                return 12;
        }

        if (direction != JUNIPER_BPF_PKT_IN && /* ether-over-1483 encaps ? */
            (cookie1 & ATM2_GAP_COUNT_MASK)) {
            ether_print(p, length, caplen);
            return 12;
        }

        if (p[0] == 0x03) { /* Cisco style NLPID encaps ? */
            isoclns_print(p + 1, length - 1, caplen - 1);
            /* FIXME check if frame was recognized */
            return 12;
        }

        if(juniper_ppp_heuristic_guess(p, length) != 0) /* PPPoA vcmux encaps ? */
            return 12;

        if(ip_heuristic_guess(p, length) != 0) /* last try - vcmux encaps ? */
            return 12;

	return (12);
}


/* try to guess, based on all PPP protos that are supported in
 * a juniper router if the payload data is encapsulated using PPP */
int
juniper_ppp_heuristic_guess(register const u_char *p, u_int length) {

    switch(EXTRACT_16BITS(p)) {
    case PPP_IP :
    case PPP_OSI :
    case PPP_MPLS_UCAST :
    case PPP_MPLS_MCAST :
    case PPP_IPCP :
    case PPP_OSICP :
    case PPP_MPLSCP :
    case PPP_LCP :
    case PPP_PAP :
    case PPP_CHAP :
    case PPP_ML :
#ifdef INET6
    case PPP_IPV6 :
    case PPP_IPV6CP :
#endif
        ppp_print(p, length);
        break;

    default:
        return 0; /* did not find a ppp header */
        break;
    }
    return 1; /* we printed a ppp packet */
}

int
ip_heuristic_guess(register const u_char *p, u_int length) {

    switch(p[0]) {
    case 0x45:
    case 0x46:
    case 0x47:
    case 0x48:
    case 0x49:
    case 0x4a:
    case 0x4b:
    case 0x4c:
    case 0x4d:
    case 0x4e:
    case 0x4f:
	    ip_print(gndo, p, length);
	    break;
#ifdef INET6
    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67:
    case 0x68:
    case 0x69:
    case 0x6a:
    case 0x6b:
    case 0x6c:
    case 0x6d:
    case 0x6e:
    case 0x6f:
        ip6_print(p, length);
        break;
#endif
    default:
        return 0; /* did not find a ip header */
        break;
    }
    return 1; /* we printed an v4/v6 packet */
}

static int
juniper_parse_header (const u_char *p, u_int8_t *direction, u_int length) {

    *direction = p[3]&JUNIPER_BPF_PKT_IN;
    
    if (EXTRACT_24BITS(p) != 0x4d4743) /* magic number found ? */
        return -1;
    
    if (*direction == JUNIPER_BPF_PKT_IN) {
        if (eflag)
            printf("%3s ", "In");
    }
    else {
        if (eflag)
            printf("%3s ", "Out");
    }

    if ((p[3] & JUNIPER_BPF_NO_L2 ) == JUNIPER_BPF_NO_L2 ) {            
        if (eflag)
            printf("no-L2-hdr, ");

        /* there is no link-layer present -
         * perform the v4/v6 heuristics
         * to figure out what it is
         */
        if(ip_heuristic_guess(p+8,length-8) == 0)
            printf("no IP-hdr found!");

        return 0; /* stop parsing the output further */
        
    }
    return 1; /* everything went ok so far. continue parsing */
}


/*
 * Local Variables:
 * c-style: whitesmith
 * c-basic-offset: 8
 * End:
 */
