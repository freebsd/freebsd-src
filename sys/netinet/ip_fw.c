/*
 * Copyright (c) 1993 Daniel Boulet
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 *	$Id: ip_fw.c,v 1.22 1995/07/31 13:58:35 gpalmer Exp $
 */

/*
 * Implement IP packet firewall
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_fw.h>

#ifdef IPFIREWALL_DEBUG
#define dprintf1(a)		printf(a)
#define dprintf2(a1,a2)		printf(a1,a2)
#define dprintf3(a1,a2,a3)	printf(a1,a2,a3)
#define dprintf4(a1,a2,a3,a4)	printf(a1,a2,a3,a4)
#else
#define dprintf1(a)
#define dprintf2(a1,a2)
#define dprintf3(a1,a2,a3)
#define dprintf4(a1,a2,a3,a4)
#endif

#ifndef TRUE
#define TRUE	1
#endif

#ifndef FALSE
#define FALSE	0
#endif

#define print_ip(a)	 printf("%ld.%ld.%ld.%ld",(ntohl(a.s_addr)>>24)&0xFF,\
				 		  (ntohl(a.s_addr)>>16)&0xFF,\
						  (ntohl(a.s_addr)>>8)&0xFF,\
						  (ntohl(a.s_addr))&0xFF);

#ifdef IPFIREWALL_DEBUG
#define dprint_ip(a)	print_ip(a)
#else
#define dprint_ip(a)
#endif


/*
 * Returns TRUE if the port is matched by the vector, FALSE otherwise
 */
inline int 
port_match(portptr, nports, port, range_flag)
	u_short *portptr;
	int nports;
	u_short port;
	int range_flag;
{
	if (!nports)
		return TRUE;
	if (range_flag) {
		if (portptr[0] <= port && port <= portptr[1]) {
			return TRUE;
		}
		nports -= 2;
		portptr += 2;
	}
	while (nports-- > 0) {
		if (*portptr++ == port) {
			return TRUE;
		}
	}
	return FALSE;
}


int
ipopts_match(ip, f)
	struct ip 	*ip;
	struct ip_fw	*f;
{
	register u_char *cp;
	int opt, optlen, cnt;
	u_char	opts, nopts, nopts_sve;

printf("Here\n");

	cp = (u_char *)(ip + 1);
	cnt = (ip->ip_hl << 2) - sizeof (struct ip);
	opts = f->fw_ipopt;
	nopts = nopts_sve = f->fw_ipnopt;
printf("opts = %x, nopts = %x\n", opts, nopts);
printf("Cnt = %d\n", cnt);
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];
			if (optlen <= 0 || optlen > cnt) {
				goto bad;
			}
		}
		switch (opt) {

		default:
			break;

		case IPOPT_LSRR:
printf("Has LSRR\n");
			opts &= ~IP_FW_IPOPT_LSRR;
			nopts &= ~IP_FW_IPOPT_LSRR;
			break;

		case IPOPT_SSRR:
			opts &= ~IP_FW_IPOPT_SSRR;
			nopts &= ~IP_FW_IPOPT_SSRR;
			break;

		case IPOPT_RR:
			opts &= ~IP_FW_IPOPT_RR;
			nopts &= ~IP_FW_IPOPT_RR;
			break;
		case IPOPT_TS:
			opts &= ~IP_FW_IPOPT_TS;
			nopts &= ~IP_FW_IPOPT_TS;
			break;
		}
		if (opts == nopts)
			break;
	}
printf("opts = %x, nopts = %x\n", opts, nopts);
	if (opts == 0 && nopts == nopts_sve)
		return 1;
	else
		return 0;
bad:
	if (ip_fw_policy & IP_FW_P_MBIPO)
		return 1;
	else
		return 0;

}


/*
 * Returns TRUE if it should be accepted, FALSE otherwise.
 */

#ifdef IPFIREWALL
int 
ip_fw_chk(m, ip, rif, chain)
	struct mbuf *m;
	struct ip *ip;
	struct ifnet *rif;
	struct ip_fw *chain;
{
	register struct ip_fw *f;
	struct tcphdr *tcp = (struct tcphdr *) ((u_long *) ip + ip->ip_hl);
	struct udphdr *udp = (struct udphdr *) ((u_long *) ip + ip->ip_hl);
	struct icmp *icmp = (struct icmp *) ((u_long *) ip + ip->ip_hl);
	struct ifaddr *ia = NULL, *ia_p;
	struct in_addr src, dst, ia_i;
	u_short src_port = 0, dst_port = 0;
	u_short f_prt = 0, prt;

	/*
	 * If the chain is empty allow any packet-this is equal to disabling
	 * firewall.
	 */
	if (chain == NULL)
		return TRUE;

	/*
	 * This way we handle fragmented packets. we ignore all fragments but
	 * the first one so the whole packet can't be reassembled. This way we
	 * relay on the full info which stored only in first packet.
	 */
	if (ip->ip_off & IP_OFFMASK)
		return TRUE;

	src = ip->ip_src;
	dst = ip->ip_dst;

	/*
	 * If we got interface from which packet came-store pointer to it's
	 * first adress
	 */
	if (rif != NULL)
		ia = rif->if_addrlist;

	dprintf1("Packet ");
	switch (ip->ip_p) {
	case IPPROTO_TCP:
		dprintf1("TCP ");
		src_port = ntohs(tcp->th_sport);
		dst_port = ntohs(tcp->th_dport);
		prt = IP_FW_F_TCP;
		break;
	case IPPROTO_UDP:
		dprintf1("UDP ");
		src_port = ntohs(udp->uh_sport);
		dst_port = ntohs(udp->uh_dport);
		prt = IP_FW_F_UDP;
		break;
	case IPPROTO_ICMP:
		dprintf2("ICMP:%u ", icmp->icmp_type);
		prt = IP_FW_F_ICMP;
		break;
	default:
		dprintf2("p=%d ", ip->ip_p);
		prt = IP_FW_F_ALL;
		break;
	}
	dprint_ip(ip->ip_src);
	if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP) {
		dprintf2(":%d", src_port);
	} 
	dprintf1(" ");
	dprint_ip(ip->ip_dst);
	if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP) {
		dprintf2(":%d", dst_port);
	}
	dprintf1("\n");


	for (f = chain; f != NULL; f = f->fw_next)
		if ((src.s_addr & f->fw_smsk.s_addr) == f->fw_src.s_addr &&
		    (dst.s_addr & f->fw_dmsk.s_addr) == f->fw_dst.s_addr) {

			if (rif == NULL)
				goto via_match;

			if (f->fw_flg & IP_FW_F_IFNAME) {
				/*
				 * No name/unit set so
				 * match any.
				 */
				if (!f->fw_via_name[0])
					goto via_match;

				if (rif->if_unit == f->fw_via_unit &&
				    !strncmp(rif->if_name, f->fw_via_name, FW_IFNLEN))
					goto via_match;
			} else {
				/*
				 * No via ip set so match
				 * any.
				 */
				if (!f->fw_via_ip.s_addr)
					goto via_match;

				for (ia_p = ia; ia_p != NULL; ia_p = ia_p->ifa_next) {
					if ((ia_p->ifa_addr == NULL) ||
					    (ia_p->ifa_addr->sa_family != AF_INET))
						continue;
					ia_i.s_addr =
					    ((struct sockaddr_in *)(ia_p->ifa_addr))->sin_addr.s_addr;
					if (ia_i.s_addr == f->fw_via_ip.s_addr)
						goto via_match;
				}
			}
			/*
			 * If we got here,no "via"'s matched,so we should
			 * continue to the next firewall entry.
			 */
			continue;
	via_match:
			/* 
			 * If we get matching IP options, we may continue
			 * checking ,else we have nothing to do here.
			 * We DO check options only if some are set in
			 * the entry definition. If both set and unset
 			 * options equal - nothing to check.
			 */
			if (f->fw_ipopt != f->fw_ipnopt)
				if (!ipopts_match(ip, f))
					continue;
				
			f_prt = f->fw_flg & IP_FW_F_KIND;
			if (f_prt == IP_FW_F_ALL) {
				/* Universal frwl - we've got a match! */
				goto got_match;
			} else {
				/*
				 * Specific firewall - packet's protocol must
				 * match firewall's
				 */
				if (prt == f_prt) {
					if (prt == IP_FW_F_ICMP ||
					    (port_match(&f->fw_pts[0], f->fw_nsp, src_port,
					    f->fw_flg & IP_FW_F_SRNG) &&
					    port_match(&f->fw_pts[f->fw_nsp], f->fw_ndp, dst_port,
					    f->fw_flg & IP_FW_F_DRNG))) {
						goto got_match;
					}	/* Ports match */
				}	/* Proto matches */
			}	/* ALL/Specific */
		}	/* IP addr/mask matches */
	/*
	 * If we get here then none of the firewalls matched. So now we relay
	 * on policy defined by user-unmatched packet can be ever accepted or
	 * rejected...
	 */
	if (ip_fw_policy & IP_FW_P_DENY) {
		f = (struct ip_fw *) NULL;
		goto bad_packet;
	}
	return TRUE;

got_match:
#ifdef IPFIREWALL_VERBOSE
	/*
	 * VERY ugly piece of code which actually makes kernel printf for
	 * denied packets...
	 */
	if (f->fw_flg & IP_FW_F_PRN) {
		if (f->fw_flg & IP_FW_F_ACCEPT)
			printf("Accept ");
		else
			printf("Deny ");
		switch (ip->ip_p) {
		case IPPROTO_TCP:
			printf("TCP ");
			break;
		case IPPROTO_UDP:
			printf("UDP ");
			break;
		case IPPROTO_ICMP:
			printf("ICMP:%u ", icmp->icmp_type);
			break;
		default:
			printf("p=%d ", ip->ip_p);
			break;
		}
		print_ip(ip->ip_src);
		if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
			printf(":%d", src_port);
		printf(" ");
		print_ip(ip->ip_dst);
		if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
			printf(":%d", dst_port);
		printf("\n");
	}
#endif
	if (f->fw_flg & IP_FW_F_ACCEPT)
		return TRUE;

bad_packet:
	if (f != NULL) {
		/*
		 * Do not ICMP reply to icmp packets....:) or to packets
		 * rejected by entry without the special ICMP reply flag.
		 */
		if ((f_prt == IP_FW_F_ICMP) ||
		    !(f->fw_flg & IP_FW_F_ICMPRPL)) {
			m_freem(m);
			return FALSE;
		}
		if (f_prt == IP_FW_F_ALL)
			icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_HOST, 0L, 0);
		else
			icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PORT, 0L, 0);
		return FALSE;
	}
	m_freem(m);
	return FALSE;
}
#endif				/* IPFIREWALL */

#ifdef IPACCT
void 
ip_acct_cnt(ip, rif, chain, nh_conv)
	struct ip *ip;
	struct ifnet *rif;
	struct ip_fw *chain;
	int nh_conv;
{
	register struct ip_fw *f;
	struct tcphdr *tcp = (struct tcphdr *) ((u_long *) ip + ip->ip_hl);
	struct udphdr *udp = (struct udphdr *) ((u_long *) ip + ip->ip_hl);
	struct ifaddr *ia = NULL, *ia_p;
	struct in_addr src, dst, ia_i;
	u_short src_port = 0, dst_port = 0;
	u_short f_prt, prt = 0;
	char rev = 0;

	if (chain == NULL)
		return;

	if (ip->ip_off & IP_OFFMASK)
		return;

	src = ip->ip_src;
	dst = ip->ip_dst;

	if (rif != NULL)
		ia = rif->if_addrlist;

	switch (ip->ip_p) {
	case IPPROTO_TCP:
		src_port = ntohs(tcp->th_sport);
		dst_port = ntohs(tcp->th_dport);
		prt = IP_FW_F_TCP;
		break;
	case IPPROTO_UDP:
		src_port = ntohs(udp->uh_sport);
		dst_port = ntohs(udp->uh_dport);
		prt = IP_FW_F_UDP;
		break;
	case IPPROTO_ICMP:
		prt = IP_FW_F_ICMP;
		break;
	default:
		prt = IP_FW_F_ALL;
		break;
	}

	for (f = chain; f != NULL; f = f->fw_next) {
		if ((src.s_addr & f->fw_smsk.s_addr) == f->fw_src.s_addr
		    && (dst.s_addr & f->fw_dmsk.s_addr) == f->fw_dst.s_addr) {
			rev = 0;
			goto addr_match;
		}
		if ((f->fw_flg & IP_FW_F_BIDIR) &&
		    ((src.s_addr & f->fw_smsk.s_addr) == f->fw_dst.s_addr
			&& (dst.s_addr & f->fw_dmsk.s_addr) == f->fw_src.s_addr)) {
			rev = 1;
			goto addr_match;
		}
		continue;
addr_match:
		/*
		 * We use here same code for "via" matching as in
		 * firewall.This is wrong and does not do much use,because in
		 * most cases instead of interface passed NULL pointer.Need to
		 * be completely rewritten.
		 */
		if (rif == NULL)
			goto via_match;

		if (f->fw_flg & IP_FW_F_IFNAME) {

			if (!f->fw_via_name[0])
				goto via_match;	/* No name/unit set,match any */

			if (rif->if_unit == f->fw_via_unit &&
			    !strncmp(rif->if_name, f->fw_via_name, FW_IFNLEN))
				goto via_match;
		} else {

			/*
			 * No via ip set so
			 * match any?
			 */
			if (!f->fw_via_ip.s_addr)
				goto via_match;

			for (ia_p = ia; ia_p != NULL; ia_p = ia_p->ifa_next) {
				if (!ia_p->ifa_addr || ia_p->ifa_addr->sa_family != AF_INET)
					continue;

				ia_i.s_addr =
				    ((struct sockaddr_in *) (ia_p->ifa_addr))->sin_addr.s_addr;
				if (ia_i.s_addr == f->fw_via_ip.s_addr)
					goto via_match;
			}

		}
		/*
		 * If we got here,no "via"'s matched,so we should continue to
		 * the next firewall entry.
		 */
		continue;
via_match:
		f_prt = f->fw_flg & IP_FW_F_KIND;
		if (f_prt == IP_FW_F_ALL) {
			/* Universal frwl - we've got a match! */

			f->fw_pcnt++;	/* Rise packet count */

			/*
			 * Rise byte count, if need to convert from host to
			 * network byte order,do it.
			 */
			if (nh_conv)
				f->fw_bcnt += ntohs(ip->ip_len);
			else
				f->fw_bcnt += ip->ip_len;
		} else {
			/*
			 * Specific firewall - packet's protocol must match
			 * firewall's
			 */
			if (prt == f_prt) {

				if ((prt == IP_FW_F_ICMP ||
				    (port_match(&f->fw_pts[0], f->fw_nsp, src_port,
				    f->fw_flg & IP_FW_F_SRNG) &&
				    port_match(&f->fw_pts[f->fw_nsp], f->fw_ndp, dst_port,
				    f->fw_flg & IP_FW_F_DRNG))) ||
				    ((rev) && (port_match(&f->fw_pts[0], f->fw_nsp, dst_port,
				    f->fw_flg & IP_FW_F_SRNG) &&
				    port_match(&f->fw_pts[f->fw_nsp], f->fw_ndp, src_port,
				    f->fw_flg & IP_FW_F_DRNG)))) {
					f->fw_pcnt++;	/* Rise packet count */
					/*
					 * Rise byte count, if need to convert
					 * from host to network byte order,do
					 * it.
					 */
					if (nh_conv)
						f->fw_bcnt += ntohs(ip->ip_len);
					else
						f->fw_bcnt += ip->ip_len;
				}	/* Ports match */
			}	/* Proto matches */
		}	/* ALL/Specific */
	}	/* IP addr/mask matches */
}
#endif				/* IPACCT */

static void
zero_fw_chain(chainptr)
	struct ip_fw *chainptr;
{
	struct ip_fw *ctmp = chainptr;

	while (ctmp != NULL) {
		ctmp->fw_pcnt = 0l;
		ctmp->fw_bcnt = 0l;
		ctmp = ctmp->fw_next;
	}
}

static void
free_fw_chain(chainptr)
	struct ip_fw **chainptr;
{
	int s;

	s = splnet();
	while (*chainptr != NULL) {
		struct ip_fw *ftmp;

		ftmp = *chainptr;
		*chainptr = ftmp->fw_next;
		free(ftmp, M_IPFW);
	}
	splx(s);
}

static int
add_entry(chainptr, frwl)
	struct ip_fw **chainptr;
	struct ip_fw *frwl;
{
	struct ip_fw *ftmp;
	struct ip_fw *chtmp = NULL;
	struct ip_fw *chtmp_prev = NULL;
	u_long m_src_mask, m_dst_mask;
	u_long n_sa, n_da, o_sa, o_da, o_sm, o_dm, n_sm, n_dm;
	u_short n_sr, n_dr, o_sr, o_dr;
	u_short oldkind, newkind;
	int addb4 = 0;
	int n_o, n_n;
	int s;

	s = splnet();

	ftmp = malloc(sizeof(struct ip_fw), M_IPFW, M_DONTWAIT);
	if (ftmp == NULL) {
		dprintf1("ip_fw_ctl:  malloc said no\n");
		splx(s);
		return (ENOSPC);
	}
	bcopy(frwl, ftmp, sizeof(struct ip_fw));
	ftmp->fw_pcnt = 0L;
	ftmp->fw_bcnt = 0L;

	ftmp->fw_next = NULL;

	if (*chainptr == NULL) {
		*chainptr = ftmp;
	} else {
		chtmp_prev = NULL;
		for (chtmp = *chainptr; chtmp != NULL; chtmp = chtmp->fw_next) {

			addb4 = 0;

			newkind = ftmp->fw_flg & IP_FW_F_KIND;
			oldkind = chtmp->fw_flg & IP_FW_F_KIND;

			if (newkind != IP_FW_F_ALL &&
			    oldkind != IP_FW_F_ALL &&
			    oldkind != newkind) {
				chtmp_prev = chtmp;
				continue;
			}
			/*
			 * Very very *UGLY* code... Sorry,but i had to do
			 * this....
			 */
			n_sa = ntohl(ftmp->fw_src.s_addr);
			n_da = ntohl(ftmp->fw_dst.s_addr);
			n_sm = ntohl(ftmp->fw_smsk.s_addr);
			n_dm = ntohl(ftmp->fw_dmsk.s_addr);

			o_sa = ntohl(chtmp->fw_src.s_addr);
			o_da = ntohl(chtmp->fw_dst.s_addr);
			o_sm = ntohl(chtmp->fw_smsk.s_addr);
			o_dm = ntohl(chtmp->fw_dmsk.s_addr);

			m_src_mask = o_sm & n_sm;
			m_dst_mask = o_dm & n_dm;

			if ((o_sa & m_src_mask) == (n_sa & m_src_mask)) {
				if (n_sm > o_sm)
					addb4++;
				if (n_sm < o_sm)
					addb4--;
			}
			if ((o_da & m_dst_mask) == (n_da & m_dst_mask)) {
				if (n_dm > o_dm)
					addb4++;
				if (n_dm < o_dm)
					addb4--;
			}
			if (((o_da & o_dm) == (n_da & n_dm))
			    && ((o_sa & o_sm) == (n_sa & n_sm))) {
				if (newkind != IP_FW_F_ALL &&
				    oldkind == IP_FW_F_ALL)
					addb4++;
				if (newkind == oldkind && (oldkind == IP_FW_F_TCP ||
				    oldkind == IP_FW_F_UDP)) {
					/*
					 * Here the main idea is to check the
					 * size of port range which the frwl
					 * covers We actually don't check
					 * their values but just the wideness
					 * of range they have so that less
					 * wide ranges or single ports go
					 * first and wide ranges go later. No
					 * ports at all treated as a range of
					 * maximum number of ports.
					 */

					if (ftmp->fw_flg & IP_FW_F_SRNG)
						n_sr = ftmp->fw_pts[1] - ftmp->fw_pts[0];
					else
						n_sr = (ftmp->fw_nsp) ?
						    ftmp->fw_nsp : USHRT_MAX;

					if (chtmp->fw_flg & IP_FW_F_SRNG)
						o_sr = chtmp->fw_pts[1] - chtmp->fw_pts[0];
					else
						o_sr = (chtmp->fw_nsp) ?
						    chtmp->fw_nsp : USHRT_MAX;

					if (n_sr < o_sr)
						addb4++;
					if (n_sr > o_sr)
						addb4--;

					n_n = ftmp->fw_nsp;
					n_o = chtmp->fw_nsp;
					/*
					 * Actually this cannot happen as the
					 * frwl control procedure checks for
					 * number of ports in source and
					 * destination range but we will try
					 * to be more safe.
					 */
					if ((n_n > (IP_FW_MAX_PORTS - 2)) ||
					    (n_o > (IP_FW_MAX_PORTS - 2)))
						goto skip_check;

					if (ftmp->fw_flg & IP_FW_F_DRNG)
						n_dr = ftmp->fw_pts[n_n + 1] - ftmp->fw_pts[n_n];
					else
						n_dr = (ftmp->fw_ndp) ?
						    ftmp->fw_ndp : USHRT_MAX;

					if (chtmp->fw_flg & IP_FW_F_DRNG)
						o_dr = chtmp->fw_pts[n_o + 1] - chtmp->fw_pts[n_o];
					else
						o_dr = (chtmp->fw_ndp) ?
						    chtmp->fw_ndp : USHRT_MAX;
					if (n_dr < o_dr)
						addb4++;
					if (n_dr > o_dr)
						addb4--;

			skip_check:
				}
				if (ftmp->fw_ipopt != 0 && chtmp->fw_ipopt == 0)
					addb4++;
				if (ftmp->fw_ipnopt != 0 && chtmp->fw_ipnopt == 0)
					addb4++;
			}
			if (addb4 > 0) {
				if (chtmp_prev != NULL) {
					chtmp_prev->fw_next = ftmp;
					ftmp->fw_next = chtmp;
				} else {
					*chainptr = ftmp;
					ftmp->fw_next = chtmp;
				}
				splx(s);
				return 0;
			}
			chtmp_prev = chtmp;
		}
		if (chtmp_prev != NULL)
			chtmp_prev->fw_next = ftmp;
		else
#ifdef DIAGNOSTIC
			panic("Can't happen");
#else
			*chainptr = ftmp;
#endif
	}
	splx(s);
	return (0);
}

static int
del_entry(chainptr, frwl)
	struct ip_fw **chainptr;
	struct ip_fw *frwl;
{
	struct ip_fw *ftmp, *ltmp;
	u_short tport1, tport2, tmpnum;
	char matches, was_found;
	int s;

	s = splnet();

	ftmp = *chainptr;

	if (ftmp == NULL) {
		dprintf1("ip_fw_ctl:  chain is empty\n");
		splx(s);
		return (EINVAL);
	}
	ltmp = NULL;
	was_found = 0;

	while (ftmp != NULL) {
		matches = 1;
		if (ftmp->fw_src.s_addr != frwl->fw_src.s_addr ||
		    ftmp->fw_dst.s_addr != frwl->fw_dst.s_addr ||
		    ftmp->fw_smsk.s_addr != frwl->fw_smsk.s_addr ||
		    ftmp->fw_dmsk.s_addr != frwl->fw_dmsk.s_addr ||
		    ftmp->fw_via_ip.s_addr != frwl->fw_via_ip.s_addr ||
		    ftmp->fw_flg != frwl->fw_flg)
			matches = 0;
		tport1 = ftmp->fw_nsp + ftmp->fw_ndp;
		tport2 = frwl->fw_nsp + frwl->fw_ndp;
		if (tport1 != tport2)
			matches = 0;
		else if (tport1 != 0) {
			for (tmpnum = 0; tmpnum < tport1 && tmpnum < IP_FW_MAX_PORTS; tmpnum++)
				if (ftmp->fw_pts[tmpnum] != frwl->fw_pts[tmpnum])
					matches = 0;
		}
		if (matches) {
			was_found = 1;
			if (ltmp != NULL) {
				ltmp->fw_next = ftmp->fw_next;
				free(ftmp, M_IPFW);
				ftmp = ltmp->fw_next;
			} else {
				*chainptr = ftmp->fw_next;
				free(ftmp, M_IPFW);
				ftmp = *chainptr;
			}

		} else {
			ltmp = ftmp;
			ftmp = ftmp->fw_next;
		}
	}
	splx(s);

	if (was_found)
		return 0;
	else
		return (EINVAL);
}

static int
clr_entry(chainptr, frwl)
	struct ip_fw **chainptr;
	struct ip_fw *frwl;
{
	struct ip_fw *ftmp, *ltmp;
	u_short tport1, tport2, tmpnum;
	char matches, was_found;

	ftmp = *chainptr;

	if (ftmp == NULL) {
		dprintf1("ip_fw_ctl:  chain is empty\n");
		return (EINVAL);
	}
	was_found = 0;

	while (ftmp != NULL) {
		matches = 1;
		if (ftmp->fw_src.s_addr != frwl->fw_src.s_addr ||
		    ftmp->fw_dst.s_addr != frwl->fw_dst.s_addr ||
		    ftmp->fw_smsk.s_addr != frwl->fw_smsk.s_addr ||
		    ftmp->fw_dmsk.s_addr != frwl->fw_dmsk.s_addr ||
		    ftmp->fw_via_ip.s_addr != frwl->fw_via_ip.s_addr ||
		    ftmp->fw_flg != frwl->fw_flg)
			matches = 0;
		tport1 = ftmp->fw_nsp + ftmp->fw_ndp;
		tport2 = frwl->fw_nsp + frwl->fw_ndp;
		if (tport1 != tport2)
			matches = 0;
		else if (tport1 != 0) {
			for (tmpnum = 0; tmpnum < tport1 && tmpnum < IP_FW_MAX_PORTS; tmpnum++)
				if (ftmp->fw_pts[tmpnum] != frwl->fw_pts[tmpnum])
					matches = 0;
		}
		if (matches) {
			was_found = 1;
			ftmp->fw_pcnt = 0L;
			ftmp->fw_bcnt = 0L;
		}
		ftmp = ftmp->fw_next;
	}
	if (was_found)
		return 0;
	else
		return (EINVAL);
}

struct ip_fw *
check_ipfw_struct(m)
	struct mbuf *m;
{
	struct ip_fw *frwl;

	if (m->m_len != sizeof(struct ip_fw)) {
		dprintf3("ip_fw_ctl: len=%d, want %d\n", m->m_len,
		    sizeof(struct ip_fw));
		return (NULL);
	}
	frwl = mtod(m, struct ip_fw *);

	if ((frwl->fw_flg & ~IP_FW_F_MASK) != 0) {
		dprintf2("ip_fw_ctl: undefined flag bits set (flags=%x)\n",
		    frwl->fw_flg);
		return (NULL);
	}
	if ((frwl->fw_flg & IP_FW_F_SRNG) && frwl->fw_nsp < 2) {
		dprintf2("ip_fw_ctl: src range set but n_src_p=%d\n",
		    frwl->fw_nsp);
		return (NULL);
	}
	if ((frwl->fw_flg & IP_FW_F_DRNG) && frwl->fw_ndp < 2) {
		dprintf2("ip_fw_ctl: dst range set but n_dst_p=%d\n",
		    frwl->fw_ndp);
		return (NULL);
	}
	if (frwl->fw_nsp + frwl->fw_ndp > IP_FW_MAX_PORTS) {
		dprintf3("ip_fw_ctl: too many ports (%d+%d)\n",
		    frwl->fw_nsp, frwl->fw_ndp);
		return (NULL);
	}
#if 0
	if ((frwl->fw_flg & IP_FW_F_KIND) == IP_FW_F_ICMP) {
		dprintf1("ip_fw_ctl:  request for unsupported ICMP frwling\n");
		return (NULL);
	}
#endif
	return frwl;
}

#ifdef IPACCT
int
ip_acct_ctl(stage, m)
	int stage;
	struct mbuf *m;
{
	if (stage == IP_ACCT_FLUSH) {
		free_fw_chain(&ip_acct_chain);
		return (0);
	}
	if (stage == IP_ACCT_ZERO) {
		zero_fw_chain(ip_acct_chain);
		return (0);
	}
	if (stage == IP_ACCT_ADD ||
	    stage == IP_ACCT_DEL ||
	    stage == IP_ACCT_CLR) {

		struct ip_fw *frwl;

		if ((frwl = check_ipfw_struct(m)) == NULL)
			return (EINVAL);

		switch (stage) {
		case IP_ACCT_ADD:
			return (add_entry(&ip_acct_chain, frwl));
		case IP_ACCT_DEL:
			return (del_entry(&ip_acct_chain, frwl));
		case IP_ACCT_CLR:
			return (clr_entry(&ip_acct_chain, frwl));
		default:
#ifdef DIAGNOSTIC
			panic("Can't happen");
#else
			dprintf2("ip_acct_ctl:  unknown request %d\n", stage);
			return (EINVAL);
#endif
		}
	}
	dprintf2("ip_acct_ctl:  unknown request %d\n", stage);
	return (EINVAL);
}
#endif

#ifdef IPFIREWALL
int
ip_fw_ctl(stage, m)
	int stage;
	struct mbuf *m;
{
	if (stage == IP_FW_FLUSH) {
		free_fw_chain(&ip_fw_chain);
		return (0);
	}
	if (m == NULL) {
		printf("ip_fw_ctl:  NULL mbuf ptr\n");
		return (EINVAL);
	}
	if (stage == IP_FW_POLICY) {
		u_short *tmp_policy_ptr;

		tmp_policy_ptr = mtod(m, u_short *);
		if ((*tmp_policy_ptr) & ~IP_FW_P_MASK)
			return (EINVAL);
		ip_fw_policy = *tmp_policy_ptr;
		return 0;
	}
	/*
	 * Here we really working hard-adding new elements
	 * to firewall chain or deleting'em
	 */

	if (stage == IP_FW_ADD ||
	    stage == IP_FW_DEL) {

		struct ip_fw *frwl;

		if ((frwl = check_ipfw_struct(m)) == NULL)
			return (EINVAL);

		switch (stage) {
		case IP_FW_ADD:
			return (add_entry(&ip_fw_chain, frwl));
		case IP_FW_DEL:
			return (del_entry(&ip_fw_chain, frwl));
		default:
#ifdef DIAGNOSTIC
			panic("Can't happen");
#else
			dprintf2("ip_fw_ctl:  unknown request %d\n", stage);
			return (EINVAL);
#endif
		}
	}
	dprintf2("ip_fw_ctl:  unknown request %d\n", stage);
	return (EINVAL);
}
#endif				/* IPFIREWALL */
