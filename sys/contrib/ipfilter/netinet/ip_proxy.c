/*
 * Copyright (C) 1997-2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if defined(__FreeBSD__) && defined(KERNEL) && !defined(_KERNEL)
# define	_KERNEL
#endif

#ifdef __sgi
# include <sys/ptimers.h>
#endif
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#if !defined(__FreeBSD_version)  
# include <sys/ioctl.h>      
#endif
#include <sys/fcntl.h>
#if !defined(_KERNEL) && !defined(KERNEL)
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
#endif
#ifndef	linux
# include <sys/protosw.h>
#endif
#include <sys/socket.h>
#if defined(_KERNEL)
# if !defined(linux)
#  include <sys/systm.h>
# else
#  include <linux/string.h>
# endif
#endif
#if !defined(__SVR4) && !defined(__svr4__)
# ifndef linux
#  include <sys/mbuf.h>
# endif
#else
# include <sys/byteorder.h>
# ifdef _KERNEL
#  include <sys/dditypes.h>
# endif
# include <sys/stream.h>
# include <sys/kmem.h>
#endif
#if __FreeBSD__ > 2
# include <sys/queue.h>
#endif
#include <net/if.h>
#ifdef sun
# include <net/af.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifndef linux
# include <netinet/ip_var.h>
#endif
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include "netinet/ip_compat.h"
#include <netinet/tcpip.h>
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_state.h"
#include "netinet/ip_proxy.h"
#if (__FreeBSD_version >= 300000)
# include <sys/malloc.h>
#endif

#if !defined(lint)
/*static const char rcsid[] = "@(#)$Id: ip_proxy.c,v 2.9.2.24 2002/08/28 12:45:51 darrenr Exp $";*/
static const char rcsid[] = "@(#)$FreeBSD$";
#endif

#if defined(_KERNEL) && (SOLARIS || defined(__sgi))
extern  KRWLOCK_T       ipf_nat, ipf_state;
#endif

#ifndef MIN
#define MIN(a,b)        (((a)<(b))?(a):(b))
#endif

static int appr_fixseqack __P((fr_info_t *, ip_t *, ap_session_t *, int ));


#define	PROXY_DEBUG 0

#define	AP_SESS_SIZE	53

#include "netinet/ip_ftp_pxy.c"
#if defined(_KERNEL)
#include "netinet/ip_rcmd_pxy.c"
#include "netinet/ip_raudio_pxy.c"
#include "netinet/ip_netbios_pxy.c"
#include "netinet/ip_h323_pxy.c"
#endif
#include "netinet/ip_ipsec_pxy.c"

ap_session_t	*ap_sess_tab[AP_SESS_SIZE];
ap_session_t	*ap_sess_list = NULL;
aproxy_t	*ap_proxylist = NULL;
aproxy_t	ap_proxies[] = {
#ifdef	IPF_FTP_PROXY
	{ NULL, "ftp", (char)IPPROTO_TCP, 0, 0, ippr_ftp_init, NULL,
	  ippr_ftp_new, NULL, ippr_ftp_in, ippr_ftp_out, NULL },
#endif
#ifdef	IPF_RCMD_PROXY
	{ NULL, "rcmd", (char)IPPROTO_TCP, 0, 0, ippr_rcmd_init, NULL,
	  ippr_rcmd_new, NULL, NULL, ippr_rcmd_out, NULL },
#endif
#ifdef	IPF_RAUDIO_PROXY
	{ NULL, "raudio", (char)IPPROTO_TCP, 0, 0, ippr_raudio_init, NULL,
	  ippr_raudio_new, NULL, ippr_raudio_in, ippr_raudio_out, NULL },
#endif
#ifdef IPF_IPSEC_PROXY
	{ NULL, "ipsec", (char)IPPROTO_UDP, 0, 0, ippr_ipsec_init, NULL,
	  ippr_ipsec_new, ippr_ipsec_del, NULL, ippr_ipsec_out,
	  ippr_ipsec_match },
#endif
#ifdef	IPF_NETBIOS_PROXY
	{ NULL, "netbios", (char)IPPROTO_UDP, 0, 0, ippr_netbios_init, NULL,
	  NULL, NULL, NULL, ippr_netbios_out, NULL },
#endif
#ifdef  IPF_H323_PROXY
    { NULL, "h323", (char)IPPROTO_TCP, 0, 0, ippr_h323_init, NULL,
 	  ippr_h323_new, ippr_h323_del, ippr_h323_in, ippr_h323_out, NULL },
    { NULL, "h245", (char)IPPROTO_TCP, 0, 0, ippr_h245_init, NULL,
 	  ippr_h245_new, NULL, NULL, ippr_h245_out, NULL },
#endif   
	{ NULL, "", '\0', 0, 0, NULL, NULL, NULL }
};


/*
 * Dynamically add a new kernel proxy.  Ensure that it is unique in the
 * collection compiled in and dynamically added.
 */
int appr_add(ap)
aproxy_t *ap;
{
	aproxy_t *a;

	for (a = ap_proxies; a->apr_p; a++)
		if ((a->apr_p == ap->apr_p) &&
		    !strncmp(a->apr_label, ap->apr_label,
			     sizeof(ap->apr_label)))
			return -1;

	for (a = ap_proxylist; a && a->apr_p; a = a->apr_next)
		if ((a->apr_p == ap->apr_p) &&
		    !strncmp(a->apr_label, ap->apr_label,
			     sizeof(ap->apr_label)))
			return -1;
	ap->apr_next = ap_proxylist;
	ap_proxylist = ap;
	return (*ap->apr_init)();
}


/*
 * Delete a proxy that has been added dynamically from those available.
 * If it is in use, return 1 (do not destroy NOW), not in use 0 or -1
 * if it cannot be matched.
 */
int appr_del(ap)
aproxy_t *ap;
{
	aproxy_t *a, **app;

	for (app = &ap_proxylist; (a = *app); app = &a->apr_next)
		if (a == ap) {
			a->apr_flags |= APR_DELETE;
			*app = a->apr_next;
			if (ap->apr_ref != 0)
				return 1;
			return 0;
		}
	return -1;
}


/*
 * Return 1 if the packet is a good match against a proxy, else 0.
 */
int appr_ok(ip, tcp, nat)
ip_t *ip;
tcphdr_t *tcp;
ipnat_t *nat;
{
	aproxy_t *apr = nat->in_apr;
	u_short dport = nat->in_dport;

	if ((apr == NULL) || (apr->apr_flags & APR_DELETE) ||
	    (ip->ip_p != apr->apr_p))
		return 0;
	if (((tcp != NULL) && (tcp->th_dport != dport)) || (!tcp && dport))
		return 0;
	return 1;
}


/*
 * If a proxy has a match function, call that to do extended packet
 * matching.
 */
int appr_match(fin, nat)
fr_info_t *fin;
nat_t *nat;
{
	aproxy_t *apr;
	ipnat_t *ipn;

	ipn = nat->nat_ptr;
	if (ipn == NULL)
		return -1;
	apr = ipn->in_apr;
	if ((apr == NULL) || (apr->apr_flags & APR_DELETE) ||
	    (nat->nat_aps == NULL))
		return -1;
	if (apr->apr_match != NULL)
		if ((*apr->apr_match)(fin, nat->nat_aps, nat) != 0)
			return -1;
	return 0;
}


/*
 * Allocate a new application proxy structure and fill it in with the
 * relevant details.  call the init function once complete, prior to
 * returning.
 */
int appr_new(fin, ip, nat)
fr_info_t *fin;
ip_t *ip;
nat_t *nat;
{
	register ap_session_t *aps;
	aproxy_t *apr;

	if ((nat->nat_ptr == NULL) || (nat->nat_aps != NULL))
		return -1;

	apr = nat->nat_ptr->in_apr;

	if (!apr || (apr->apr_flags & APR_DELETE) || (ip->ip_p != apr->apr_p))
		return -1;

	KMALLOC(aps, ap_session_t *);
	if (!aps)
		return -1;
	bzero((char *)aps, sizeof(*aps));
	aps->aps_p = ip->ip_p;
	aps->aps_data = NULL;
	aps->aps_apr = apr;
	aps->aps_psiz = 0;
	if (apr->apr_new != NULL)
		if ((*apr->apr_new)(fin, ip, aps, nat) == -1) {
			if ((aps->aps_data != NULL) && (aps->aps_psiz != 0)) {
				KFREES(aps->aps_data, aps->aps_psiz);
			}
			KFREE(aps);
			return -1;
		}
	aps->aps_nat = nat;
	aps->aps_next = ap_sess_list;
	ap_sess_list = aps;
	nat->nat_aps = aps;

	return 0;
}


/*
 * check to see if a packet should be passed through an active proxy routine
 * if one has been setup for it.
 */
int appr_check(ip, fin, nat)
ip_t *ip;
fr_info_t *fin;
nat_t *nat;
{
#if SOLARIS && defined(_KERNEL) && (SOLARIS2 >= 6)
	mb_t *m = fin->fin_qfm;
	int dosum = 1;
#endif
	tcphdr_t *tcp = NULL;
	ap_session_t *aps;
	aproxy_t *apr;
	u_32_t sum;
	short rv;
	int err;

	aps = nat->nat_aps;
	if ((aps != NULL) && (aps->aps_p == ip->ip_p)) {
		if (ip->ip_p == IPPROTO_TCP) {
			tcp = (tcphdr_t *)fin->fin_dp;
			/*
			 * verify that the checksum is correct.  If not, then
			 * don't do anything with this packet.
			 */
#if SOLARIS && defined(_KERNEL) && (SOLARIS2 >= 6)
			if (dohwcksum && (m->b_ick_flag == ICK_VALID)) {
				sum = tcp->th_sum;
				dosum = 0;
			}
			if (dosum)
				sum = fr_tcpsum(fin->fin_qfm, ip, tcp);
#else
			sum = fr_tcpsum(*(mb_t **)fin->fin_mp, ip, tcp);
#endif
			if (sum != tcp->th_sum) {
#if PROXY_DEBUG
				printf("proxy tcp checksum failure\n");
#endif
				frstats[fin->fin_out].fr_tcpbad++;
				return -1;
			}

			/*
			 * Don't both the proxy with these...or in fact, should
			 * we free up proxy stuff when seen?
			 */
			if ((tcp->th_flags & TH_RST) != 0)
				return 0;
		}

		apr = aps->aps_apr;
		err = 0;
		if (fin->fin_out != 0) {
			if (apr->apr_outpkt != NULL)
				err = (*apr->apr_outpkt)(fin, ip, aps, nat);
		} else {
			if (apr->apr_inpkt != NULL)
				err = (*apr->apr_inpkt)(fin, ip, aps, nat);
		}

		rv = APR_EXIT(err);
		if (rv == 1) {
#if PROXY_DEBUG
			printf("proxy says bad packet received\n");
#endif
			return -1;
		}
		if (rv == 2) {
#if PROXY_DEBUG
			printf("proxy says free app proxy data\n");
#endif
			appr_free(apr);
			nat->nat_aps = NULL;
			return -1;
		}

		if (tcp != NULL) {
			err = appr_fixseqack(fin, ip, aps, APR_INC(err));
#if SOLARIS && defined(_KERNEL) && (SOLARIS2 >= 6)
			if (dosum)
				tcp->th_sum = fr_tcpsum(fin->fin_qfm, ip, tcp);
#else
			tcp->th_sum = fr_tcpsum(*(mb_t **)fin->fin_mp, ip, tcp);
#endif
		}
		aps->aps_bytes += ip->ip_len;
		aps->aps_pkts++;
		return 1;
	}
	return 0;
}


/*
 * Search for an proxy by the protocol it is being used with and its name.
 */
aproxy_t *appr_lookup(pr, name)
u_int pr;
char *name;
{
	aproxy_t *ap;

	for (ap = ap_proxies; ap->apr_p; ap++)
		if ((ap->apr_p == pr) &&
		    !strncmp(name, ap->apr_label, sizeof(ap->apr_label))) {
			ap->apr_ref++;
			return ap;
		}

	for (ap = ap_proxylist; ap; ap = ap->apr_next)
		if ((ap->apr_p == pr) &&
		    !strncmp(name, ap->apr_label, sizeof(ap->apr_label))) {
			ap->apr_ref++;
			return ap;
		}
	return NULL;
}


void appr_free(ap)
aproxy_t *ap;
{
	ap->apr_ref--;
}


void aps_free(aps)
ap_session_t *aps;
{
	ap_session_t *a, **ap;
	aproxy_t *apr;

	if (!aps)
		return;

	for (ap = &ap_sess_list; (a = *ap); ap = &a->aps_next)
		if (a == aps) {
			*ap = a->aps_next;
			break;
		}

	apr = aps->aps_apr;
	if ((apr != NULL) && (apr->apr_del != NULL))
		(*apr->apr_del)(aps);
 
	if ((aps->aps_data != NULL) && (aps->aps_psiz != 0))
		KFREES(aps->aps_data, aps->aps_psiz);
	KFREE(aps);
}


/*
 * returns 2 if ack or seq number in TCP header is changed, returns 0 otherwise
 */
static int appr_fixseqack(fin, ip, aps, inc)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
int inc;
{
	int sel, ch = 0, out, nlen;
	u_32_t seq1, seq2;
	tcphdr_t *tcp;
	short inc2;

	tcp = (tcphdr_t *)fin->fin_dp;
	out = fin->fin_out;
	/*
	 * ip_len has already been adjusted by 'inc'.
	 */
	nlen = ip->ip_len;
	nlen -= (ip->ip_hl << 2) + (tcp->th_off << 2);

	inc2 = inc;
	inc = (int)inc2;

	if (out != 0) {
		seq1 = (u_32_t)ntohl(tcp->th_seq);
		sel = aps->aps_sel[out];

		/* switch to other set ? */
		if ((aps->aps_seqmin[!sel] > aps->aps_seqmin[sel]) &&
		    (seq1 > aps->aps_seqmin[!sel])) {
#if PROXY_DEBUG
			printf("proxy out switch set seq %d -> %d %x > %x\n",
				sel, !sel, seq1, aps->aps_seqmin[!sel]);
#endif
			sel = aps->aps_sel[out] = !sel;
}

		if (aps->aps_seqoff[sel]) {
			seq2 = aps->aps_seqmin[sel] - aps->aps_seqoff[sel];
			if (seq1 > seq2) {
				seq2 = aps->aps_seqoff[sel];
				seq1 += seq2;
				tcp->th_seq = htonl(seq1);
				ch = 1;
			}
		}

		if (inc && (seq1 > aps->aps_seqmin[!sel])) {
			aps->aps_seqmin[sel] = seq1 + nlen - 1;
			aps->aps_seqoff[sel] = aps->aps_seqoff[sel] + inc;
#if PROXY_DEBUG
			printf("proxy seq set %d at %x to %d + %d\n", sel,
				aps->aps_seqmin[sel], aps->aps_seqoff[sel],
				inc);
#endif
		}

		/***/

		seq1 = ntohl(tcp->th_ack);
		sel = aps->aps_sel[1 - out];

		/* switch to other set ? */
		if ((aps->aps_ackmin[!sel] > aps->aps_ackmin[sel]) &&
		    (seq1 > aps->aps_ackmin[!sel])) {
#if PROXY_DEBUG
			printf("proxy out switch set ack %d -> %d %x > %x\n",
				sel, !sel, seq1, aps->aps_ackmin[!sel]);
#endif
			sel = aps->aps_sel[1 - out] = !sel;
}

		if (aps->aps_ackoff[sel] && (seq1 > aps->aps_ackmin[sel])) {
			seq2 = aps->aps_ackoff[sel];
			tcp->th_ack = htonl(seq1 - seq2);
			ch = 1;
		}
	} else {
		seq1 = ntohl(tcp->th_seq);
		sel = aps->aps_sel[out];

		/* switch to other set ? */
		if ((aps->aps_ackmin[!sel] > aps->aps_ackmin[sel]) &&
		    (seq1 > aps->aps_ackmin[!sel])) {
#if PROXY_DEBUG
			printf("proxy in switch set ack %d -> %d %x > %x\n",
				sel, !sel, seq1, aps->aps_ackmin[!sel]);
#endif
			sel = aps->aps_sel[out] = !sel;
}

		if (aps->aps_ackoff[sel]) {
			seq2 = aps->aps_ackmin[sel] - aps->aps_ackoff[sel];
			if (seq1 > seq2) {
				seq2 = aps->aps_ackoff[sel];
				seq1 += seq2;
				tcp->th_seq = htonl(seq1);
				ch = 1;
			}
		}

		if (inc && (seq1 > aps->aps_ackmin[!sel])) {
			aps->aps_ackmin[!sel] = seq1 + nlen - 1;
			aps->aps_ackoff[!sel] = aps->aps_ackoff[sel] + inc;
#if PROXY_DEBUG
			printf("proxy ack set %d at %x to %d + %d\n", !sel,
				aps->aps_seqmin[!sel], aps->aps_seqoff[sel],
				inc);
#endif
		}

		/***/

		seq1 = ntohl(tcp->th_ack);
		sel = aps->aps_sel[1 - out];

		/* switch to other set ? */
		if ((aps->aps_seqmin[!sel] > aps->aps_seqmin[sel]) &&
		    (seq1 > aps->aps_seqmin[!sel])) {
#if PROXY_DEBUG
			printf("proxy in switch set seq %d -> %d %x > %x\n",
				sel, !sel, seq1, aps->aps_seqmin[!sel]);
#endif
			sel = aps->aps_sel[1 - out] = !sel;
}

		if (aps->aps_seqoff[sel] != 0) {
#if PROXY_DEBUG
			printf("sel %d seqoff %d seq1 %x seqmin %x\n", sel,
				aps->aps_seqoff[sel], seq1,
				aps->aps_seqmin[sel]);
#endif
			if (seq1 > aps->aps_seqmin[sel]) {
				seq2 = aps->aps_seqoff[sel];
				tcp->th_ack = htonl(seq1 - seq2);
				ch = 1;
			}
		}
	}
#if PROXY_DEBUG
	printf("appr_fixseqack: seq %x ack %x\n", ntohl(tcp->th_seq),
		ntohl(tcp->th_ack));
#endif
	return ch ? 2 : 0;
}


/*
 * Initialise hook for kernel application proxies.
 * Call the initialise routine for all the compiled in kernel proxies.
 */
int appr_init()
{
	aproxy_t *ap;
	int err = 0;

	for (ap = ap_proxies; ap->apr_p; ap++) {
		err = (*ap->apr_init)();
		if (err != 0)
			break;
	}
	return err;
}


/*
 * Unload hook for kernel application proxies.
 * Call the finialise routine for all the compiled in kernel proxies.
 */
void appr_unload()
{
	aproxy_t *ap;

	for (ap = ap_proxies; ap->apr_p; ap++)
		if (ap->apr_fini)
			(*ap->apr_fini)();
	for (ap = ap_proxylist; ap; ap = ap->apr_next)
		if (ap->apr_fini)
			(*ap->apr_fini)();
}
