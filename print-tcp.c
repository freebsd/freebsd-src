/*	$NetBSD: print-tcp.c,v 1.9 2007/07/26 18:15:12 plunky Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1999-2004 The tcpdump.org project
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
"@(#) $Header: /tcpdump/master/tcpdump/print-tcp.c,v 1.135 2008-11-09 23:35:03 mcr Exp $ (LBL)";
#else
__RCSID("$NetBSD: print-tcp.c,v 1.8 2007/07/24 11:53:48 drochner Exp $");
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"

#include "tcp.h"

#include "ip.h"
#ifdef INET6
#include "ip6.h"
#endif
#include "ipproto.h"
#include "rpc_auth.h"
#include "rpc_msg.h"

#include "nameser.h"

#ifdef HAVE_LIBCRYPTO
#include <openssl/md5.h>
#include <signature.h>

static int tcp_verify_signature(const struct ip *ip, const struct tcphdr *tp,
                                const u_char *data, int length, const u_char *rcvsig);
#endif

static void print_tcp_rst_data(register const u_char *sp, u_int length);

#define MAX_RST_DATA_LEN	30


struct tha {
#ifndef INET6
        struct in_addr src;
        struct in_addr dst;
#else
        struct in6_addr src;
        struct in6_addr dst;
#endif /*INET6*/
        u_int port;
};

struct tcp_seq_hash {
        struct tcp_seq_hash *nxt;
        struct tha addr;
        tcp_seq seq;
        tcp_seq ack;
};

#define TSEQ_HASHSIZE 919

/* These tcp optinos do not have the size octet */
#define ZEROLENOPT(o) ((o) == TCPOPT_EOL || (o) == TCPOPT_NOP)

static struct tcp_seq_hash tcp_seq_hash[TSEQ_HASHSIZE];

struct tok tcp_flag_values[] = {
        { TH_FIN, "F" },
        { TH_SYN, "S" },
        { TH_RST, "R" },
        { TH_PUSH, "P" },
        { TH_ACK, "." },
        { TH_URG, "U" },
        { TH_ECNECHO, "E" },
        { TH_CWR, "W" },
        { 0, NULL }
};

struct tok tcp_option_values[] = {
        { TCPOPT_EOL, "eol" },
        { TCPOPT_NOP, "nop" },
        { TCPOPT_MAXSEG, "mss" },
        { TCPOPT_WSCALE, "wscale" },
        { TCPOPT_SACKOK, "sackOK" },
        { TCPOPT_SACK, "sack" },
        { TCPOPT_ECHO, "echo" },
        { TCPOPT_ECHOREPLY, "echoreply" },
        { TCPOPT_TIMESTAMP, "TS" },
        { TCPOPT_CC, "cc" },
        { TCPOPT_CCNEW, "ccnew" },
        { TCPOPT_CCECHO, "" },
        { TCPOPT_SIGNATURE, "md5" },
        { TCPOPT_AUTH, "enhanced auth" },
        { TCPOPT_UTO, "uto" },
        { 0, NULL }
};

static int tcp_cksum(register const struct ip *ip,
		     register const struct tcphdr *tp,
		     register u_int len)
{
        union phu {
                struct phdr {
                        u_int32_t src;
                        u_int32_t dst;
                        u_char mbz;
                        u_char proto;
                        u_int16_t len;
                } ph;
                u_int16_t pa[6];
        } phu;
        const u_int16_t *sp;

        /* pseudo-header.. */
        phu.ph.len = htons((u_int16_t)len);
        phu.ph.mbz = 0;
        phu.ph.proto = IPPROTO_TCP;
        memcpy(&phu.ph.src, &ip->ip_src.s_addr, sizeof(u_int32_t));
        if (IP_HL(ip) == 5)
                memcpy(&phu.ph.dst, &ip->ip_dst.s_addr, sizeof(u_int32_t));
        else
                phu.ph.dst = ip_finddst(ip);

        sp = &phu.pa[0];
        return in_cksum((u_short *)tp, len,
                        sp[0]+sp[1]+sp[2]+sp[3]+sp[4]+sp[5]);
}

void
tcp_print(register const u_char *bp, register u_int length,
	  register const u_char *bp2, int fragmented)
{
        register const struct tcphdr *tp;
        register const struct ip *ip;
        register u_char flags;
        register u_int hlen;
        register char ch;
        u_int16_t sport, dport, win, urp;
        u_int32_t seq, ack, thseq, thack;
        u_int utoval;
        int threv;
#ifdef INET6
        register const struct ip6_hdr *ip6;
#endif

        tp = (struct tcphdr *)bp;
        ip = (struct ip *)bp2;
#ifdef INET6
        if (IP_V(ip) == 6)
                ip6 = (struct ip6_hdr *)bp2;
        else
                ip6 = NULL;
#endif /*INET6*/
        ch = '\0';
        if (!TTEST(tp->th_dport)) {
                (void)printf("%s > %s: [|tcp]",
                             ipaddr_string(&ip->ip_src),
                             ipaddr_string(&ip->ip_dst));
                return;
        }

        sport = EXTRACT_16BITS(&tp->th_sport);
        dport = EXTRACT_16BITS(&tp->th_dport);

        hlen = TH_OFF(tp) * 4;

        /*
	 * If data present, header length valid, and NFS port used,
	 * assume NFS.
	 * Pass offset of data plus 4 bytes for RPC TCP msg length
	 * to NFS print routines.
	 */
	if (!qflag && hlen >= sizeof(*tp) && hlen <= length &&
	    (length - hlen) >= 4) {
		u_char *fraglenp;
		u_int32_t fraglen;
		register struct sunrpc_msg *rp;
		enum sunrpc_msg_type direction;

		fraglenp = (u_char *)tp + hlen;
		if (TTEST2(*fraglenp, 4)) {
			fraglen = EXTRACT_32BITS(fraglenp) & 0x7FFFFFFF;
			if (fraglen > (length - hlen) - 4)
				fraglen = (length - hlen) - 4;
			rp = (struct sunrpc_msg *)(fraglenp + 4);
			if (TTEST(rp->rm_direction)) {
				direction = (enum sunrpc_msg_type)EXTRACT_32BITS(&rp->rm_direction);
				if (dport == NFS_PORT &&
				    direction == SUNRPC_CALL) {
					nfsreq_print((u_char *)rp, fraglen,
					    (u_char *)ip);
					return;
				}
				if (sport == NFS_PORT &&
				    direction == SUNRPC_REPLY) {
					nfsreply_print((u_char *)rp, fraglen,
					    (u_char *)ip);
					return;
				}
			}
                }
        }
#ifdef INET6
        if (ip6) {
                if (ip6->ip6_nxt == IPPROTO_TCP) {
                        (void)printf("%s.%s > %s.%s: ",
                                     ip6addr_string(&ip6->ip6_src),
                                     tcpport_string(sport),
                                     ip6addr_string(&ip6->ip6_dst),
                                     tcpport_string(dport));
                } else {
                        (void)printf("%s > %s: ",
                                     tcpport_string(sport), tcpport_string(dport));
                }
        } else
#endif /*INET6*/
        {
                if (ip->ip_p == IPPROTO_TCP) {
                        (void)printf("%s.%s > %s.%s: ",
                                     ipaddr_string(&ip->ip_src),
                                     tcpport_string(sport),
                                     ipaddr_string(&ip->ip_dst),
                                     tcpport_string(dport));
                } else {
                        (void)printf("%s > %s: ",
                                     tcpport_string(sport), tcpport_string(dport));
                }
        }

        if (hlen < sizeof(*tp)) {
                (void)printf(" tcp %d [bad hdr length %u - too short, < %lu]",
                             length - hlen, hlen, (unsigned long)sizeof(*tp));
                return;
        }

        TCHECK(*tp);

        seq = EXTRACT_32BITS(&tp->th_seq);
        ack = EXTRACT_32BITS(&tp->th_ack);
        win = EXTRACT_16BITS(&tp->th_win);
        urp = EXTRACT_16BITS(&tp->th_urp);

        if (qflag) {
                (void)printf("tcp %d", length - hlen);
                if (hlen > length) {
                        (void)printf(" [bad hdr length %u - too long, > %u]",
                                     hlen, length);
                }
                return;
        }

        flags = tp->th_flags;
        printf("Flags [%s]", bittok2str_nosep(tcp_flag_values, "none", flags));

        if (!Sflag && (flags & TH_ACK)) {
                register struct tcp_seq_hash *th;
                const void *src, *dst;
                register int rev;
                struct tha tha;
                /*
                 * Find (or record) the initial sequence numbers for
                 * this conversation.  (we pick an arbitrary
                 * collating order so there's only one entry for
                 * both directions).
                 */
#ifdef INET6
                memset(&tha, 0, sizeof(tha));
                rev = 0;
                if (ip6) {
                        src = &ip6->ip6_src;
                        dst = &ip6->ip6_dst;
                        if (sport > dport)
                                rev = 1;
                        else if (sport == dport) {
                                if (memcmp(src, dst, sizeof ip6->ip6_dst) > 0)
                                        rev = 1;
                        }
                        if (rev) {
                                memcpy(&tha.src, dst, sizeof ip6->ip6_dst);
                                memcpy(&tha.dst, src, sizeof ip6->ip6_src);
                                tha.port = dport << 16 | sport;
                        } else {
                                memcpy(&tha.dst, dst, sizeof ip6->ip6_dst);
                                memcpy(&tha.src, src, sizeof ip6->ip6_src);
                                tha.port = sport << 16 | dport;
                        }
                } else {
                        src = &ip->ip_src;
                        dst = &ip->ip_dst;
                        if (sport > dport)
                                rev = 1;
                        else if (sport == dport) {
                                if (memcmp(src, dst, sizeof ip->ip_dst) > 0)
                                        rev = 1;
                        }
                        if (rev) {
                                memcpy(&tha.src, dst, sizeof ip->ip_dst);
                                memcpy(&tha.dst, src, sizeof ip->ip_src);
                                tha.port = dport << 16 | sport;
                        } else {
                                memcpy(&tha.dst, dst, sizeof ip->ip_dst);
                                memcpy(&tha.src, src, sizeof ip->ip_src);
                                tha.port = sport << 16 | dport;
                        }
                }
#else
                rev = 0;
                src = &ip->ip_src;
                dst = &ip->ip_dst;
                if (sport > dport)
                        rev = 1;
                else if (sport == dport) {
                        if (memcmp(src, dst, sizeof ip->ip_dst) > 0)
                                rev = 1;
                }
                if (rev) {
                        memcpy(&tha.src, dst, sizeof ip->ip_dst);
                        memcpy(&tha.dst, src, sizeof ip->ip_src);
                        tha.port = dport << 16 | sport;
                } else {
                        memcpy(&tha.dst, dst, sizeof ip->ip_dst);
                        memcpy(&tha.src, src, sizeof ip->ip_src);
                        tha.port = sport << 16 | dport;
                }
#endif

                threv = rev;
                for (th = &tcp_seq_hash[tha.port % TSEQ_HASHSIZE];
                     th->nxt; th = th->nxt)
                        if (memcmp((char *)&tha, (char *)&th->addr,
                                   sizeof(th->addr)) == 0)
                                break;

                if (!th->nxt || (flags & TH_SYN)) {
                        /* didn't find it or new conversation */
                        if (th->nxt == NULL) {
                                th->nxt = (struct tcp_seq_hash *)
                                        calloc(1, sizeof(*th));
                                if (th->nxt == NULL)
                                        error("tcp_print: calloc");
                        }
                        th->addr = tha;
                        if (rev)
                                th->ack = seq, th->seq = ack - 1;
                        else
                                th->seq = seq, th->ack = ack - 1;
                } else {
                        if (rev)
                                seq -= th->ack, ack -= th->seq;
                        else
                                seq -= th->seq, ack -= th->ack;
                }

                thseq = th->seq;
                thack = th->ack;
        } else {
                /*fool gcc*/
                thseq = thack = threv = 0;
        }
        if (hlen > length) {
                (void)printf(" [bad hdr length %u - too long, > %u]",
                             hlen, length);
                return;
        }

        if (IP_V(ip) == 4 && vflag && !Kflag && !fragmented) {
                u_int16_t sum, tcp_sum;
                if (TTEST2(tp->th_sport, length)) {
                        sum = tcp_cksum(ip, tp, length);

                        (void)printf(", cksum 0x%04x",EXTRACT_16BITS(&tp->th_sum));
                        if (sum != 0) {
                                tcp_sum = EXTRACT_16BITS(&tp->th_sum);
                                (void)printf(" (incorrect -> 0x%04x)",in_cksum_shouldbe(tcp_sum, sum));
                        } else
                                (void)printf(" (correct)");
                }
        }
#ifdef INET6
        if (IP_V(ip) == 6 && ip6->ip6_plen && vflag && !Kflag && !fragmented) {
                u_int16_t sum,tcp_sum;
                if (TTEST2(tp->th_sport, length)) {
                        sum = nextproto6_cksum(ip6, (u_short *)tp, length, IPPROTO_TCP);
                        (void)printf(", cksum 0x%04x",EXTRACT_16BITS(&tp->th_sum));
                        if (sum != 0) {
                                tcp_sum = EXTRACT_16BITS(&tp->th_sum);
                                (void)printf(" (incorrect -> 0x%04x)",in_cksum_shouldbe(tcp_sum, sum));
                        } else
                                (void)printf(" (correct)");

                }
        }
#endif

        length -= hlen;
        if (vflag > 1 || length > 0 || flags & (TH_SYN | TH_FIN | TH_RST)) {
                (void)printf(", seq %u", seq);

                if (length > 0) {
                        (void)printf(":%u", seq + length);
                }
        }

        if (flags & TH_ACK) {
                (void)printf(", ack %u", ack);
        }

        (void)printf(", win %d", win);

        if (flags & TH_URG)
                (void)printf(", urg %d", urp);
        /*
         * Handle any options.
         */
        if (hlen > sizeof(*tp)) {
                register const u_char *cp;
                register u_int i, opt, datalen;
                register u_int len;

                hlen -= sizeof(*tp);
                cp = (const u_char *)tp + sizeof(*tp);
                printf(", options [");
                while (hlen > 0) {
                        if (ch != '\0')
                                putchar(ch);
                        TCHECK(*cp);
                        opt = *cp++;
                        if (ZEROLENOPT(opt))
                                len = 1;
                        else {
                                TCHECK(*cp);
                                len = *cp++;	/* total including type, len */
                                if (len < 2 || len > hlen)
                                        goto bad;
                                --hlen;		/* account for length byte */
                        }
                        --hlen;			/* account for type byte */
                        datalen = 0;

/* Bail if "l" bytes of data are not left or were not captured  */
#define LENCHECK(l) { if ((l) > hlen) goto bad; TCHECK2(*cp, l); }


                        printf("%s", tok2str(tcp_option_values, "Unknown Option %u", opt));

                        switch (opt) {

                        case TCPOPT_MAXSEG:
                                datalen = 2;
                                LENCHECK(datalen);
                                (void)printf(" %u", EXTRACT_16BITS(cp));
                                break;

                        case TCPOPT_WSCALE:
                                datalen = 1;
                                LENCHECK(datalen);
                                (void)printf(" %u", *cp);
                                break;

                        case TCPOPT_SACK:
                                datalen = len - 2;
                                if (datalen % 8 != 0) {
                                        (void)printf("malformed sack");
                                } else {
                                        u_int32_t s, e;

                                        (void)printf(" %d ", datalen / 8);
                                        for (i = 0; i < datalen; i += 8) {
                                                LENCHECK(i + 4);
                                                s = EXTRACT_32BITS(cp + i);
                                                LENCHECK(i + 8);
                                                e = EXTRACT_32BITS(cp + i + 4);
                                                if (threv) {
                                                        s -= thseq;
                                                        e -= thseq;
                                                } else {
                                                        s -= thack;
                                                        e -= thack;
                                                }
                                                (void)printf("{%u:%u}", s, e);
                                        }
                                }
                                break;

                        case TCPOPT_CC:
                        case TCPOPT_CCNEW:
                        case TCPOPT_CCECHO:
                        case TCPOPT_ECHO:
                        case TCPOPT_ECHOREPLY:

                                /*
                                 * those options share their semantics.
                                 * fall through
                                 */
                                datalen = 4;
                                LENCHECK(datalen);
                                (void)printf(" %u", EXTRACT_32BITS(cp));
                                break;

                        case TCPOPT_TIMESTAMP:
                                datalen = 8;
                                LENCHECK(datalen);
                                (void)printf(" val %u ecr %u",
                                             EXTRACT_32BITS(cp),
                                             EXTRACT_32BITS(cp + 4));
                                break;

                        case TCPOPT_SIGNATURE:
                                datalen = TCP_SIGLEN;
                                LENCHECK(datalen);
#ifdef HAVE_LIBCRYPTO
                                switch (tcp_verify_signature(ip, tp,
                                                             bp + TH_OFF(tp) * 4, length, cp)) {

                                case SIGNATURE_VALID:
                                        (void)printf("valid");
                                        break;

                                case SIGNATURE_INVALID:
                                        (void)printf("invalid");
                                        break;

                                case CANT_CHECK_SIGNATURE:
                                        (void)printf("can't check - ");
                                        for (i = 0; i < TCP_SIGLEN; ++i)
                                                (void)printf("%02x", cp[i]);
                                        break;
                                }
#else
                                for (i = 0; i < TCP_SIGLEN; ++i)
                                        (void)printf("%02x", cp[i]);
#endif
                                break;

                        case TCPOPT_AUTH:
                                (void)printf("keyid %d", *cp++);
                                datalen = len - 3;
                                for (i = 0; i < datalen; ++i) {
                                        LENCHECK(i);
                                        (void)printf("%02x", cp[i]);
                                }
                                break;


                        case TCPOPT_EOL:
                        case TCPOPT_NOP:
                        case TCPOPT_SACKOK:
                                /*
                                 * Nothing interesting.
                                 * fall through
                                 */
                                break;

                        case TCPOPT_UTO:
                                datalen = 2;
                                LENCHECK(datalen);
                                utoval = EXTRACT_16BITS(cp);
                                (void)printf("0x%x", utoval);
                                if (utoval & 0x0001)
                                        utoval = (utoval >> 1) * 60;
                                else
                                        utoval >>= 1;
                                (void)printf(" %u", utoval);
                                break;

                        default:
                                datalen = len - 2;
                                for (i = 0; i < datalen; ++i) {
                                        LENCHECK(i);
                                        (void)printf("%02x", cp[i]);
                                }
                                break;
                        }

                        /* Account for data printed */
                        cp += datalen;
                        hlen -= datalen;

                        /* Check specification against observed length */
                        ++datalen;			/* option octet */
                        if (!ZEROLENOPT(opt))
                                ++datalen;		/* size octet */
                        if (datalen != len)
                                (void)printf("[len %d]", len);
                        ch = ',';
                        if (opt == TCPOPT_EOL)
                                break;
                }
                putchar(']');
        }

        /*
         * Print length field before crawling down the stack.
         */
        printf(", length %u", length);

        if (length <= 0)
                return;

        /*
         * Decode payload if necessary.
         */
        bp += TH_OFF(tp) * 4;
        if ((flags & TH_RST) && vflag) {
                print_tcp_rst_data(bp, length);
                return;
        } 

        if (sport == TELNET_PORT || dport == TELNET_PORT) {
                if (!qflag && vflag)
                        telnet_print(bp, length);
        } else if (sport == BGP_PORT || dport == BGP_PORT)
                bgp_print(bp, length);
        else if (sport == PPTP_PORT || dport == PPTP_PORT)
                pptp_print(bp);
#ifdef TCPDUMP_DO_SMB
        else if (sport == NETBIOS_SSN_PORT || dport == NETBIOS_SSN_PORT)
                nbt_tcp_print(bp, length);
	else if (sport == SMB_PORT || dport == SMB_PORT)
		smb_tcp_print(bp, length);
#endif
        else if (sport == BEEP_PORT || dport == BEEP_PORT)
                beep_print(bp, length);
        else if (length > 2 &&
                 (sport == NAMESERVER_PORT || dport == NAMESERVER_PORT ||
                  sport == MULTICASTDNS_PORT || dport == MULTICASTDNS_PORT)) {
                /*
                 * TCP DNS query has 2byte length at the head.
                 * XXX packet could be unaligned, it can go strange
                 */
                ns_print(bp + 2, length - 2, 0);
        } else if (sport == MSDP_PORT || dport == MSDP_PORT) {
                msdp_print(bp, length);
        }
        else if (length > 0 && (sport == LDP_PORT || dport == LDP_PORT)) {
                ldp_print(bp, length);
        }

        return;
 bad:
        fputs("[bad opt]", stdout);
        if (ch != '\0')
                putchar('>');
        return;
 trunc:
        fputs("[|tcp]", stdout);
        if (ch != '\0')
                putchar('>');
}

/*
 * RFC1122 says the following on data in RST segments:
 *
 *         4.2.2.12  RST Segment: RFC-793 Section 3.4
 *
 *            A TCP SHOULD allow a received RST segment to include data.
 *
 *            DISCUSSION
 *                 It has been suggested that a RST segment could contain
 *                 ASCII text that encoded and explained the cause of the
 *                 RST.  No standard has yet been established for such
 *                 data.
 *
 */

static void
print_tcp_rst_data(register const u_char *sp, u_int length)
{
        int c;

        if (TTEST2(*sp, length))
                printf(" [RST");
        else
                printf(" [!RST");
        if (length > MAX_RST_DATA_LEN) {
                length = MAX_RST_DATA_LEN;	/* can use -X for longer */
                putchar('+');			/* indicate we truncate */
        }
        putchar(' ');
        while (length-- && sp <= snapend) {
                c = *sp++;
                safeputchar(c);
        }
        putchar(']');
}

#ifdef HAVE_LIBCRYPTO
static int
tcp_verify_signature(const struct ip *ip, const struct tcphdr *tp,
                     const u_char *data, int length, const u_char *rcvsig)
{
        struct tcphdr tp1;
        u_char sig[TCP_SIGLEN];
        char zero_proto = 0;
        MD5_CTX ctx;
        u_int16_t savecsum, tlen;
#ifdef INET6
        struct ip6_hdr *ip6;
        u_int32_t len32;
        u_int8_t nxt;
#endif

	if (data + length > snapend) {
		printf("snaplen too short, ");
		return (CANT_CHECK_SIGNATURE);
	}

        tp1 = *tp;

        if (sigsecret == NULL) {
		printf("shared secret not supplied with -M, ");
                return (CANT_CHECK_SIGNATURE);
        }

        MD5_Init(&ctx);
        /*
         * Step 1: Update MD5 hash with IP pseudo-header.
         */
        if (IP_V(ip) == 4) {
                MD5_Update(&ctx, (char *)&ip->ip_src, sizeof(ip->ip_src));
                MD5_Update(&ctx, (char *)&ip->ip_dst, sizeof(ip->ip_dst));
                MD5_Update(&ctx, (char *)&zero_proto, sizeof(zero_proto));
                MD5_Update(&ctx, (char *)&ip->ip_p, sizeof(ip->ip_p));
                tlen = EXTRACT_16BITS(&ip->ip_len) - IP_HL(ip) * 4;
                tlen = htons(tlen);
                MD5_Update(&ctx, (char *)&tlen, sizeof(tlen));
#ifdef INET6
        } else if (IP_V(ip) == 6) {
                ip6 = (struct ip6_hdr *)ip;
                MD5_Update(&ctx, (char *)&ip6->ip6_src, sizeof(ip6->ip6_src));
                MD5_Update(&ctx, (char *)&ip6->ip6_dst, sizeof(ip6->ip6_dst));
                len32 = htonl(EXTRACT_16BITS(&ip6->ip6_plen));
                MD5_Update(&ctx, (char *)&len32, sizeof(len32));
                nxt = 0;
                MD5_Update(&ctx, (char *)&nxt, sizeof(nxt));
                MD5_Update(&ctx, (char *)&nxt, sizeof(nxt));
                MD5_Update(&ctx, (char *)&nxt, sizeof(nxt));
                nxt = IPPROTO_TCP;
                MD5_Update(&ctx, (char *)&nxt, sizeof(nxt));
#endif
        } else {
#ifdef INET6
		printf("IP version not 4 or 6, ");
#else
		printf("IP version not 4, ");
#endif
                return (CANT_CHECK_SIGNATURE);
        }

        /*
         * Step 2: Update MD5 hash with TCP header, excluding options.
         * The TCP checksum must be set to zero.
         */
        savecsum = tp1.th_sum;
        tp1.th_sum = 0;
        MD5_Update(&ctx, (char *)&tp1, sizeof(struct tcphdr));
        tp1.th_sum = savecsum;
        /*
         * Step 3: Update MD5 hash with TCP segment data, if present.
         */
        if (length > 0)
                MD5_Update(&ctx, data, length);
        /*
         * Step 4: Update MD5 hash with shared secret.
         */
        MD5_Update(&ctx, sigsecret, strlen(sigsecret));
        MD5_Final(sig, &ctx);

        if (memcmp(rcvsig, sig, TCP_SIGLEN) == 0)
                return (SIGNATURE_VALID);
        else
                return (SIGNATURE_INVALID);
}
#endif /* HAVE_LIBCRYPTO */

/*
 * Local Variables:
 * c-style: whitesmith
 * c-basic-offset: 8
 * End:
 */
