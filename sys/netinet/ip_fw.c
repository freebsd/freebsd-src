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
 *	$Id: ip_fw.c,v 1.14.4.10 1996/11/12 17:31:31 jkh Exp $
 */

/*
 * Implement IP packet firewall
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#if 0 /* XXX -current, but not -stable */
#include <sys/sysctl.h>
#endif
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_fw.h>

static int fw_debug = 1;
#ifdef IPFIREWALL_VERBOSE
static int fw_verbose = 1;
#else
static int fw_verbose = 0;
#endif
#ifdef IPFIREWALL_VERBOSE_LIMIT
static int fw_verbose_limit = IPFIREWALL_VERBOSE_LIMIT;
#else
static int fw_verbose_limit = 0;
#endif

LIST_HEAD (ip_fw_head, ip_fw_chain) ip_fw_chain;

#ifdef SYSCTL_NODE
SYSCTL_NODE(net_inet_ip, OID_AUTO, fw, CTLFLAG_RW, 0, "Firewall");
SYSCTL_INT(net_inet_ip_fw, OID_AUTO, debug, CTLFLAG_RW, &fw_debug, 0, "");
SYSCTL_INT(net_inet_ip_fw, OID_AUTO, verbose, CTLFLAG_RW, &fw_verbose, 0, "");
SYSCTL_INT(net_inet_ip_fw, OID_AUTO, verbose_limit, CTLFLAG_RW, &fw_verbose_limit, 0, "");
#endif

#define dprintf(a)	if (!fw_debug); else printf a

#define print_ip(a)	 printf("%ld.%ld.%ld.%ld",(ntohl(a.s_addr)>>24)&0xFF,\
				 		  (ntohl(a.s_addr)>>16)&0xFF,\
						  (ntohl(a.s_addr)>>8)&0xFF,\
						  (ntohl(a.s_addr))&0xFF);

#define dprint_ip(a)	if (!fw_debug); else print_ip(a)

static int	add_entry __P((struct ip_fw_head *chainptr, struct ip_fw *frwl));
static int	del_entry __P((struct ip_fw_head *chainptr, struct ip_fw *frwl));
static int	zero_entry __P((struct mbuf *m));
static struct ip_fw *
		check_ipfw_struct __P(( struct mbuf *m));
static int	ipopts_match __P((struct ip *ip, struct ip_fw *f));
static int	port_match __P((u_short *portptr, int nports, u_short port,
				int range_flag));
static int	tcpflg_match __P((struct tcphdr *tcp, struct ip_fw *f));
static void	ipfw_report __P((char *txt, int rule, struct ip *ip, int counter));

/*
 * Returns 1 if the port is matched by the vector, 0 otherwise
 */
static inline int 
port_match(portptr, nports, port, range_flag)
	u_short *portptr;
	int nports;
	u_short port;
	int range_flag;
{
	if (!nports)
		return 1;
	if (range_flag) {
		if (portptr[0] <= port && port <= portptr[1]) {
			return 1;
		}
		nports -= 2;
		portptr += 2;
	}
	while (nports-- > 0) {
		if (*portptr++ == port) {
			return 1;
		}
	}
	return 0;
}

static int
tcpflg_match(tcp, f)
	struct tcphdr		*tcp;
	struct ip_fw		*f;
{
	u_char		flg_set, flg_clr;
	
	if ((f->fw_tcpf & IP_FW_TCPF_ESTAB) &&
	    (tcp->th_flags & (IP_FW_TCPF_RST | IP_FW_TCPF_ACK)))
		return 1;

	flg_set = tcp->th_flags & f->fw_tcpf;
	flg_clr = tcp->th_flags & f->fw_tcpnf;

	if (flg_set != f->fw_tcpf)
		return 0;
	if (flg_clr)
		return 0;

	return 1;
}

static int
icmptype_match(struct icmp *icmp, struct ip_fw *f)
{
	int type;

	if (!(f->fw_flg & IP_FW_F_ICMPBIT))
		return(1);

	type = icmp->icmp_type;

	/* check for matching type in the bitmap */
	if (f->fw_icmptypes[type / (sizeof(unsigned) * 8)] & 
		(1U << (type % (8 * sizeof(unsigned)))))
		return(1);

	return(0); /* no match */
}

static int
ipopts_match(ip, f)
	struct ip 	*ip;
	struct ip_fw	*f;
{
	register u_char *cp;
	int opt, optlen, cnt;
	u_char	opts, nopts, nopts_sve;

	cp = (u_char *)(ip + 1);
	cnt = (ip->ip_hl << 2) - sizeof (struct ip);
	opts = f->fw_ipopt;
	nopts = nopts_sve = f->fw_ipnopt;

	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];
			if (optlen <= 0 || optlen > cnt) {
				return 0; /*XXX*/
			}
		}
		switch (opt) {

		default:
			break;

		case IPOPT_LSRR:
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
	if (opts == 0 && nopts == nopts_sve)
		return 1;
	else
		return 0;
}

static void
ipfw_report(char *txt, int rule, struct ip *ip, int counter)
{
	struct tcphdr *tcp = (struct tcphdr *) ((u_long *) ip + ip->ip_hl);
	struct udphdr *udp = (struct udphdr *) ((u_long *) ip + ip->ip_hl);
	struct icmp *icmp = (struct icmp *) ((u_long *) ip + ip->ip_hl);
	if (!fw_verbose)
		return;
	if (fw_verbose_limit != 0 && counter > fw_verbose_limit)
		return;
	printf("ipfw: %d %s ",rule, txt);
	switch (ip->ip_p) {
	case IPPROTO_TCP:
		printf("TCP ");
		print_ip(ip->ip_src);
		printf(":%d ", ntohs(tcp->th_sport));
		print_ip(ip->ip_dst);
		printf(":%d", ntohs(tcp->th_dport));
		break;
	case IPPROTO_UDP:
		printf("UDP ");
		print_ip(ip->ip_src);
		printf(":%d ", ntohs(udp->uh_sport));
		print_ip(ip->ip_dst);
		printf(":%d", ntohs(udp->uh_dport));
		break;
	case IPPROTO_ICMP:
		printf("ICMP:%u.%u ", icmp->icmp_type, icmp->icmp_code);
		print_ip(ip->ip_src);
		printf(" ");
		print_ip(ip->ip_dst);
		break;
	default:
		printf("P:%d ", ip->ip_p);
		print_ip(ip->ip_src);
		printf(" ");
		print_ip(ip->ip_dst);
		break;
	}
	if ((ip->ip_off & IP_OFFMASK)) 
		printf(" Fragment = %d",ip->ip_off & IP_OFFMASK);
	printf("\n");
}

/*
 * Returns 1 if it should be accepted, 0 otherwise.
 */

int 
ip_fw_chk(m, ip, rif, dir)
	struct mbuf *m;
	struct ip *ip;
	struct ifnet *rif;
	int dir;
{
	struct ip_fw_chain *chain;
	register struct ip_fw *f = NULL;
	struct tcphdr *tcp = (struct tcphdr *) ((u_long *) ip + ip->ip_hl);
	struct udphdr *udp = (struct udphdr *) ((u_long *) ip + ip->ip_hl);
	struct icmp *icmp = (struct icmp *) ((u_long *) ip + ip->ip_hl);
	struct ifaddr *ia = NULL, *ia_p;
	struct in_addr src, dst, ia_i;
	u_short src_port = 0, dst_port = 0;
	u_short f_prt = 0, prt, len = 0;

	/*
	 * ... else if non-zero, highly unusual and interesting, but 
	 * we're not going to pass it...
	 */
	if ((ip->ip_off & IP_OFFMASK) == 1) {
		static int frag_counter = 0;

		++frag_counter;
		ipfw_report("Refuse", -1, ip, frag_counter);
		m_freem(m);
		return 0;
	}

	src = ip->ip_src;
	dst = ip->ip_dst;

	/*
	 * If we got interface from which packet came-store pointer to it's
	 * first adress
	 */
	if (rif != NULL)
		ia = rif->if_addrlist;

	/*
	 * Determine the protocol and extract some useful stuff
	 */
	switch (ip->ip_p) {
	case IPPROTO_TCP:
		src_port = ntohs(tcp->th_sport);
		dst_port = ntohs(tcp->th_dport);
		prt = IP_FW_F_TCP;
		len = sizeof (*tcp);
		break;
	case IPPROTO_UDP:
		src_port = ntohs(udp->uh_sport);
		dst_port = ntohs(udp->uh_dport);
		prt = IP_FW_F_UDP;
		len = sizeof (*udp);
		break;
	case IPPROTO_ICMP:
		prt = IP_FW_F_ICMP;
		len = sizeof (*icmp);
		break;
	default:
		prt = IP_FW_F_ALL;
		break;
	}

	/* XXX Check that we have sufficient header for TCP analysis */

	/*
	 * Go down the chain, looking for enlightment
	 */
	for (chain=ip_fw_chain.lh_first; chain; chain = chain->chain.le_next) {
		f = chain->rule;

		/* Check direction inbound */
		if (!dir && !(f->fw_flg & IP_FW_F_IN))
			continue;

		/* Check direction outbound */
		if (dir && !(f->fw_flg & IP_FW_F_OUT))
			continue;

		/* Fragments */
		if ((f->fw_flg & IP_FW_F_FRAG) && !(ip->ip_off & IP_OFFMASK))
			continue;

		/* If src-addr doesn't match, not this rule. */
		if ((src.s_addr & f->fw_smsk.s_addr) != f->fw_src.s_addr)
			continue;

		/* If dest-addr doesn't match, not this rule. */
		if ((dst.s_addr & f->fw_dmsk.s_addr) != f->fw_dst.s_addr)
			continue;

		/* If a i/f name was specified, and we don't know */
		if ((f->fw_flg & IP_FW_F_IFNAME) && !rif)
			continue;

		/* If a i/f name was specified, check it */
		if ((f->fw_flg & IP_FW_F_IFNAME) && f->fw_via_name[0]) {

			/* Not same unit, don't match */
			if (!(f->fw_flg & IP_FW_F_IFUWILD) && rif->if_unit != f->fw_via_unit)
				continue;

			/* Not same name */
			if (strncmp(rif->if_name, f->fw_via_name, FW_IFNLEN))
				continue;
		}

		/* If a i/f addr was specified, check it */
		if (!(f->fw_flg & IP_FW_F_IFNAME) && f->fw_via_ip.s_addr) {
			int match = 0;

			for (ia_p = ia; ia_p != NULL; ia_p = ia_p->ifa_next) {
				if ((ia_p->ifa_addr == NULL))
					continue;
				if (ia_p->ifa_addr->sa_family != AF_INET)
					continue;
				ia_i.s_addr =
				    ((struct sockaddr_in *)
				    (ia_p->ifa_addr))->sin_addr.s_addr;
				if (ia_i.s_addr != f->fw_via_ip.s_addr)
					continue;
				match = 1;
				break;
			}
			if (!match)
				continue;
		}

		/* 
		 * Check IP options
		 */
		if (f->fw_ipopt != f->fw_ipnopt)
			if (!ipopts_match(ip, f))
				continue;
			
		/*
		 * Check protocol
		 */
		f_prt = f->fw_flg & IP_FW_F_KIND;
		
		/* If wildcard, match */
		if (f_prt == IP_FW_F_ALL)
			goto got_match;

		/* If different, dont match */
		if (prt != f_prt) 
			continue;

		/* ICMP, done */
		if (prt == IP_FW_F_ICMP) {
			if (!icmptype_match(icmp, f))
				continue;

			goto got_match;
		}

		/* Check TCP flags and TCP/UDP ports only if packet is not fragment */
		if (!(ip->ip_off & IP_OFFMASK)) {
			/* TCP, a little more checking */
			if (prt == IP_FW_F_TCP &&
				(f->fw_tcpf != f->fw_tcpnf) &&
				(!tcpflg_match(tcp, f)))
				continue;

			if (!port_match(&f->fw_pts[0], f->fw_nsp,
							src_port, f->fw_flg & IP_FW_F_SRNG))
				continue;

			if (!port_match(&f->fw_pts[f->fw_nsp], f->fw_ndp,
							dst_port, f->fw_flg & IP_FW_F_DRNG)) 
				continue;
		}

got_match:
		f->fw_pcnt++;
		f->fw_bcnt+=ip->ip_len;
		f->timestamp = time.tv_sec;
		if (f->fw_flg & IP_FW_F_PRN) {
			if (f->fw_flg & IP_FW_F_ACCEPT)
				ipfw_report("Allow", f->fw_number, ip, f->fw_pcnt);
			else if (f->fw_flg & IP_FW_F_COUNT)
				ipfw_report("Count", f->fw_number, ip, f->fw_pcnt);
			else
				ipfw_report("Deny", f->fw_number, ip, f->fw_pcnt);
		}
		if (f->fw_flg & IP_FW_F_ACCEPT)
			return 1;
		if (f->fw_flg & IP_FW_F_COUNT)
			continue;
		break;

	}

	/*
	 * Don't icmp outgoing packets at all
	 */
	if (f != NULL && !dir) {
		/*
		 * Do not ICMP reply to icmp packets....:) or to packets
		 * rejected by entry without the special ICMP reply flag.
		 */
		if ((f_prt != IP_FW_F_ICMP) && (f->fw_flg & IP_FW_F_ICMPRPL)) {
			if (f_prt == IP_FW_F_ALL)
				icmp_error(m, ICMP_UNREACH, 
					ICMP_UNREACH_HOST, 0L, 0);
			else
				icmp_error(m, ICMP_UNREACH, 
					ICMP_UNREACH_PORT, 0L, 0);
			return 0;
		}
	}
	m_freem(m);
	return 0;
}

static int
add_entry(chainptr, frwl)
	struct ip_fw_head *chainptr;
	struct ip_fw *frwl;
{
	struct ip_fw *ftmp = 0;
	struct ip_fw_chain *fwc = 0, *fcp, *fcpl = 0;
	u_short nbr = 0;
	int s;

	fwc = malloc(sizeof *fwc, M_IPFW, M_DONTWAIT);
	ftmp = malloc(sizeof *ftmp, M_IPFW, M_DONTWAIT);
	if (!fwc || !ftmp) {
		dprintf(("ip_fw_ctl:  malloc said no\n"));
		if (fwc)  free(fwc, M_IPFW);
		if (ftmp) free(ftmp, M_IPFW);
		return (ENOSPC);
	}

	bcopy(frwl, ftmp, sizeof(struct ip_fw));
	ftmp->fw_pcnt = 0L;
	ftmp->fw_bcnt = 0L;
	fwc->rule = ftmp;
	
	s = splnet();

	if (!chainptr->lh_first) {
		LIST_INSERT_HEAD(chainptr, fwc, chain);
		splx(s);
		return(0);
	} else if (ftmp->fw_number == (u_short)-1) {
		if (fwc)  free(fwc, M_IPFW);
		if (ftmp) free(ftmp, M_IPFW);
		splx(s);
		return (EINVAL);
	}

	/* If entry number is 0, find highest numbered rule and add 100 */
	if (ftmp->fw_number == 0) {
		for (fcp = chainptr->lh_first; fcp; fcp = fcp->chain.le_next) {
			if (fcp->rule->fw_number != (u_short)-1)
				nbr = fcp->rule->fw_number;
			else
				break;
		}
		if (nbr < (u_short)-1 - 100)
			nbr += 100;
		ftmp->fw_number = nbr;
	}

	/* Got a valid number; now insert it, keeping the list ordered */
	for (fcp = chainptr->lh_first; fcp; fcp = fcp->chain.le_next) {
		if (fcp->rule->fw_number > ftmp->fw_number) {
			if (fcpl) {
				LIST_INSERT_AFTER(fcpl, fwc, chain);
			} else {
				LIST_INSERT_HEAD(chainptr, fwc, chain);
			}
			break;
		} else {
			fcpl = fcp;
		}
	}

	splx(s);
	return (0);
}

static int
del_entry(chainptr, frwl)
	struct ip_fw_head *chainptr;
	struct ip_fw *frwl;
{
	struct ip_fw_chain *fcp;
	int s;

	s = splnet();

	fcp = chainptr->lh_first; 
	if (frwl->fw_number != (u_short)-1) {
		for (; fcp; fcp = fcp->chain.le_next) {
			if (fcp->rule->fw_number == frwl->fw_number) {
				LIST_REMOVE(fcp, chain);
				splx(s);
				free(fcp->rule, M_IPFW);
				free(fcp, M_IPFW);
				return 0;
			}
		}
	}

	splx(s);
	return (EINVAL);
}

static int
zero_entry(struct mbuf *m)
{
	struct ip_fw *frwl;
	struct ip_fw_chain *fcp;
	int s;

	if (m) {
		frwl = check_ipfw_struct(m);

		if (!frwl)
			return(EINVAL);
	}
	else
		frwl = NULL;

	/*
	 *	It's possible to insert multiple chain entries with the
	 *	same number, so we don't stop after finding the first
	 *	match if zeroing a specific entry.
	 */
	s = splnet();
	for (fcp = ip_fw_chain.lh_first; fcp; fcp = fcp->chain.le_next)
		if (!frwl || frwl->fw_number == fcp->rule->fw_number) {
			fcp->rule->fw_bcnt = fcp->rule->fw_pcnt = 0;
			fcp->rule->timestamp = 0;
		}
	splx(s);

	return(0);
}

static struct ip_fw *
check_ipfw_struct(m)
	struct mbuf *m;
{
	struct ip_fw *frwl;

	if (m->m_len != sizeof(struct ip_fw)) {
		dprintf(("ip_fw_ctl: len=%d, want %d\n", m->m_len,
		    sizeof(struct ip_fw)));
		return (NULL);
	}
	frwl = mtod(m, struct ip_fw *);

	if ((frwl->fw_flg & ~IP_FW_F_MASK) != 0) {
		dprintf(("ip_fw_ctl: undefined flag bits set (flags=%x)\n",
		    frwl->fw_flg));
		return (NULL);
	}

	/* If neither In nor Out, then both */
	if (!(frwl->fw_flg & (IP_FW_F_IN | IP_FW_F_OUT)))
		frwl->fw_flg |= IP_FW_F_IN | IP_FW_F_OUT;

	if ((frwl->fw_flg & IP_FW_F_SRNG) && frwl->fw_nsp < 2) {
		dprintf(("ip_fw_ctl: src range set but n_src_p=%d\n",
		    frwl->fw_nsp));
		return (NULL);
	}
	if ((frwl->fw_flg & IP_FW_F_DRNG) && frwl->fw_ndp < 2) {
		dprintf(("ip_fw_ctl: dst range set but n_dst_p=%d\n",
		    frwl->fw_ndp));
		return (NULL);
	}
	if (frwl->fw_nsp + frwl->fw_ndp > IP_FW_MAX_PORTS) {
		dprintf(("ip_fw_ctl: too many ports (%d+%d)\n",
		    frwl->fw_nsp, frwl->fw_ndp));
		return (NULL);
	}
	return frwl;
}

int
ip_fw_ctl(stage, mm)
	int stage;
	struct mbuf **mm;
{
	int error;
	struct mbuf *m;

	if (stage == IP_FW_GET) {
		struct ip_fw_chain *fcp = ip_fw_chain.lh_first;
		*mm = m = m_get(M_WAIT, MT_SOOPTS);
		for (; fcp; fcp = fcp->chain.le_next) {
			memcpy(m->m_data, fcp->rule, sizeof *(fcp->rule));
			m->m_len = sizeof *(fcp->rule);
			m->m_next = m_get(M_WAIT, MT_SOOPTS);
			m = m->m_next;
			m->m_len = 0;
		}
		return (0);
	}
	m = *mm;
	/* only allow get calls if secure mode > 2 */
	if (securelevel > 2) {
		if (m) (void)m_free(m);
		return(EPERM);
	}
	if (stage == IP_FW_FLUSH) {
		while (ip_fw_chain.lh_first != NULL && 
		    ip_fw_chain.lh_first->rule->fw_number != (u_short)-1) {
			struct ip_fw_chain *fcp = ip_fw_chain.lh_first;
			int s = splnet();
			LIST_REMOVE(ip_fw_chain.lh_first, chain);
			splx(s);
			free(fcp->rule, M_IPFW);
			free(fcp, M_IPFW);
		}
		if (m) (void)m_free(m);
		return (0);
	}
	if (stage == IP_FW_ZERO) {
		error = zero_entry(m);
		if (m) (void)m_free(m);
		return (error);
	}
	if (m == NULL) {
		printf("ip_fw_ctl:  NULL mbuf ptr\n");
		return (EINVAL);
	}

	if (stage == IP_FW_ADD || stage == IP_FW_DEL) {
		struct ip_fw *frwl = check_ipfw_struct(m);

		if (!frwl) {
			if (m) (void)m_free(m);
			return (EINVAL);
		}

		if (stage == IP_FW_ADD)
			error = add_entry(&ip_fw_chain, frwl);
		else
			error = del_entry(&ip_fw_chain, frwl);
		if (m) (void)m_free(m);
		return error;
	}
	dprintf(("ip_fw_ctl:  unknown request %d\n", stage));
	if (m) (void)m_free(m);
	return (EINVAL);
}

void
ip_fw_init(void)
{
	struct ip_fw deny;

	ip_fw_chk_ptr = ip_fw_chk;
	ip_fw_ctl_ptr = ip_fw_ctl;
	LIST_INIT(&ip_fw_chain);

	bzero(&deny, sizeof deny);
	deny.fw_flg = IP_FW_F_ALL;
	deny.fw_number = (u_short)-1;
	deny.fw_flg = IP_FW_F_IN | IP_FW_F_OUT;
	add_entry(&ip_fw_chain, &deny);
	
	printf("IP firewall initialized, ");
#ifndef IPFIREWALL_VERBOSE
	printf("logging disabled\n");
#else
	if (fw_verbose_limit == 0)
		printf("unlimited logging\n");
	else
		printf("logging limited to %d packets/entry\n", fw_verbose_limit);
#endif
}

#ifdef ACTUALLY_LKM_NOT_KERNEL

#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

static int (*old_chk_ptr)(struct mbuf *, struct ip *, struct ifnet *, int dir);
static int (*old_ctl_ptr)(int, struct mbuf **);

static int
ipfw_load(struct lkm_table *lkmtp, int cmd)
{
	int s=splnet();

	old_chk_ptr = ip_fw_chk_ptr;
	old_ctl_ptr = ip_fw_ctl_ptr;

	ip_fw_init();
	splx(s);
	return 0;
}

static int
ipfw_unload(struct lkm_table *lkmtp, int cmd)
{
	int s=splnet();

	ip_fw_chk_ptr =  old_chk_ptr;
	ip_fw_ctl_ptr =  old_ctl_ptr;

	while (ip_fw_chain.lh_first != NULL) {
		struct ip_fw_chain *fcp = ip_fw_chain.lh_first;
		LIST_REMOVE(ip_fw_chain.lh_first, chain);
		free(fcp->rule, M_IPFW);
		free(fcp, M_IPFW);
	}
	
	splx(s);
	printf("IP firewall unloaded\n");
	return 0;
}

MOD_MISC("ipfw_mod")

int
ipfw_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, ipfw_load, ipfw_unload, nosys);
}
#endif
