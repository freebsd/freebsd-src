/*
 * (C)opyright 1993-1996 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#if !defined(lint) && defined(LIBC_SCCS)
static	char	sccsid[] = "@(#)fil.c	1.36 6/5/96 (C) 1993-1996 Darren Reed";
static	char	rcsid[] = "$Id: fil.c,v 1.1.1.3 1997/04/03 10:10:10 darrenr Exp $";
#endif

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#if defined(_KERNEL) || defined(KERNEL)
# include <sys/systm.h>
#else
# include <stdio.h>
# include <string.h>
#endif
#include <sys/uio.h>
#if !defined(__SVR4) && !defined(__svr4__)
# include <sys/mbuf.h>
#else
# include <sys/byteorder.h>
# include <sys/dditypes.h>
# include <sys/stream.h>
#endif
#include <sys/protosw.h>
#include <sys/socket.h>
#include <net/if.h>
#ifdef sun
# include <net/af.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include "ip_compat.h"
#include "ip_fil.h"
#include "ip_nat.h"
#include "ip_frag.h"
#include "ip_state.h"
#ifndef	MIN
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif

#ifndef	_KERNEL
# include "ipf.h"
# include "ipt.h"
extern	int	opts;

# define	FR_IFVERBOSE(ex,second,verb_pr)	if (ex) { verbose verb_pr; \
							  second; }
# define	FR_IFDEBUG(ex,second,verb_pr)	if (ex) { debug verb_pr; \
							  second; }
# define	FR_VERBOSE(verb_pr)			verbose verb_pr
# define	FR_DEBUG(verb_pr)			debug verb_pr
# define	FR_SCANLIST(p, ip, fi, m)	fr_scanlist(p, ip, fi, m)
# define	SEND_RESET(ip, qif, q, if)		send_reset(ip, if)
# define	IPLLOG(a, c, d, e)		ipllog()
# if SOLARIS
#  define	ICMP_ERROR(b, ip, t, c, if, src) 	icmp_error(ip)
#  define	bcmp	memcmp
# else
#  define	ICMP_ERROR(b, ip, t, c, if, src) 	icmp_error(b, ip, if)
# endif

#else /* #ifndef _KERNEL */
# define	FR_IFVERBOSE(ex,second,verb_pr)	;
# define	FR_IFDEBUG(ex,second,verb_pr)	;
# define	FR_VERBOSE(verb_pr)
# define	FR_DEBUG(verb_pr)
# define	FR_SCANLIST(p, ip, fi, m)	fr_scanlist(p, ip, fi, m)
# define	IPLLOG(a, c, d, e)		ipllog(a, IPL_LOGIPF, c, d, e)
# if SOLARIS
extern	kmutex_t	ipf_mutex;
#  define	SEND_RESET(ip, qif, q, if)	send_reset(ip, qif, q)
#  define	ICMP_ERROR(b, ip, t, c, if, src) \
			icmp_error(b, ip, t, c, if, src)
# else
#  define	FR_SCANLIST(p, ip, fi, m)	fr_scanlist(p, ip, fi, m)
#  define	SEND_RESET(ip, qif, q, if)	send_reset((struct tcpiphdr *)ip)
#  if BSD < 199103
#   define	ICMP_ERROR(b, ip, t, c, if, src) \
			icmp_error(mtod(b, ip_t *), t, c, if, src)
#  else
#   define	ICMP_ERROR(b, ip, t, c, if, src) \
			icmp_error(b, t, c, (src).s_addr, if)
#  endif
# endif
#endif

#ifndef	IPF_LOGGING
#define	IPF_LOGGING	0
#endif
#ifdef	IPF_DEFAULT_PASS
#define	IPF_NOMATCH	(IPF_DEFAULT_PASS|FR_NOMATCH)
#else
#define	IPF_NOMATCH	(FR_PASS|FR_NOMATCH)
#endif

struct	filterstats frstats[2] = {{0,0,0,0,0},{0,0,0,0,0}};
struct	frentry	*ipfilter[2][2] = { { NULL, NULL }, { NULL, NULL } },
		*ipacct[2][2] = { { NULL, NULL }, { NULL, NULL } };
int	fr_flags = IPF_LOGGING, fr_active = 0;

fr_info_t	frcache[2];

static	void	fr_makefrip __P((int, ip_t *, fr_info_t *));
static	int	fr_tcpudpchk __P((frentry_t *, fr_info_t *));
static	int	fr_scanlist __P((int, ip_t *, fr_info_t *, void *));


/*
 * bit values for identifying presence of individual IP options
 */
struct	optlist	ipopts[20] = {
	{ IPOPT_NOP,	0x000001 },
	{ IPOPT_RR,	0x000002 },
	{ IPOPT_ZSU,	0x000004 },
	{ IPOPT_MTUP,	0x000008 },
	{ IPOPT_MTUR,	0x000010 },
	{ IPOPT_ENCODE,	0x000020 },
	{ IPOPT_TS,	0x000040 },
	{ IPOPT_TR,	0x000080 },
	{ IPOPT_SECURITY, 0x000100 },
	{ IPOPT_LSRR,	0x000200 },
	{ IPOPT_E_SEC,	0x000400 },
	{ IPOPT_CIPSO,	0x000800 },
	{ IPOPT_SATID,	0x001000 },
	{ IPOPT_SSRR,	0x002000 },
	{ IPOPT_ADDEXT,	0x004000 },
	{ IPOPT_VISA,	0x008000 },
	{ IPOPT_IMITD,	0x010000 },
	{ IPOPT_EIP,	0x020000 },
	{ IPOPT_FINN,	0x040000 },
	{ 0,		0x000000 }
};

/*
 * bit values for identifying presence of individual IP security options
 */
struct	optlist	secopt[8] = {
	{ IPSO_CLASS_RES4,	0x01 },
	{ IPSO_CLASS_TOPS,	0x02 },
	{ IPSO_CLASS_SECR,	0x04 },
	{ IPSO_CLASS_RES3,	0x08 },
	{ IPSO_CLASS_CONF,	0x10 },
	{ IPSO_CLASS_UNCL,	0x20 },
	{ IPSO_CLASS_RES2,	0x40 },
	{ IPSO_CLASS_RES1,	0x80 }
};


/*
 * compact the IP header into a structure which contains just the info.
 * which is useful for comparing IP headers with.
 */
static	void	fr_makefrip(hlen, ip, fin)
int hlen;
ip_t *ip;
fr_info_t *fin;
{
	struct optlist *op;
	tcphdr_t *tcp;
	fr_ip_t *fi = &fin->fin_fi;
	u_short optmsk = 0, secmsk = 0, auth = 0;
	int i, mv, ol, off;
	u_char *s, opt;

	fin->fin_fr = NULL;
	fin->fin_tcpf = 0;
	fin->fin_data[0] = 0;
	fin->fin_data[1] = 0;
	fin->fin_rule = -1;
#ifdef	_KERNEL
	fin->fin_icode = ipl_unreach;
#endif
	fi->fi_v = ip->ip_v;
	fi->fi_tos = ip->ip_tos;
	fin->fin_hlen = hlen;
	fin->fin_dlen = ip->ip_len - hlen;
	tcp = (tcphdr_t *)((char *)ip + hlen);
	fin->fin_dp = (void *)tcp;
	(*(((u_short *)fi) + 1)) = (*(((u_short *)ip) + 4));
	(*(((u_long *)fi) + 1)) = (*(((u_long *)ip) + 3));
	(*(((u_long *)fi) + 2)) = (*(((u_long *)ip) + 4));

	fi->fi_fl = (hlen > sizeof(struct ip)) ? FI_OPTIONS : 0;
	off = (ip->ip_off & 0x1fff) << 3;
	if (ip->ip_off & 0x3fff)
		fi->fi_fl |= FI_FRAG;
	switch (ip->ip_p)
	{
	case IPPROTO_ICMP :
		if ((!IPMINLEN(ip, icmp) && !off) ||
		    (off && off < sizeof(struct icmp)))
			fi->fi_fl |= FI_SHORT;
		if (fin->fin_dlen > 1)
			fin->fin_data[0] = *(u_short *)tcp;
		break;
	case IPPROTO_TCP :
		fi->fi_fl |= FI_TCPUDP;
		if ((!IPMINLEN(ip, tcphdr) && !off) ||
		    (off && off < sizeof(struct tcphdr)))
			fi->fi_fl |= FI_SHORT;
		if (!(fi->fi_fl & FI_SHORT) && !off)
			fin->fin_tcpf = tcp->th_flags;
		goto getports;
	case IPPROTO_UDP :
		fi->fi_fl |= FI_TCPUDP;
		if ((!IPMINLEN(ip, udphdr) && !off) ||
		    (off && off < sizeof(struct udphdr)))
			fi->fi_fl |= FI_SHORT;
getports:
		if (!off && (fin->fin_dlen > 3)) {
			fin->fin_data[0] = ntohs(tcp->th_sport);
			fin->fin_data[1] = ntohs(tcp->th_dport);
		}
		break;
	default :
		break;
	}


	for (s = (u_char *)(ip + 1), hlen -= sizeof(*ip); hlen; ) {
		if (!(opt = *s))
			break;
		ol = (opt == IPOPT_NOP) ? 1 : (int)*(s+1);
		if (opt > 1 && (ol < 2 || ol > hlen))
			break;
		for (i = 9, mv = 4; mv >= 0; ) {
			op = ipopts + i;
			if (opt == (u_char)op->ol_val) {
				optmsk |= op->ol_bit;
				if (opt == IPOPT_SECURITY) {
					struct optlist *sp;
					u_char	sec;
					int j, m;

					sec = *(s + 2);	/* classification */
					for (j = 3, m = 2; m >= 0; ) {
						sp = secopt + j;
						if (sec == sp->ol_val) {
							secmsk |= sp->ol_bit;
							auth = *(s + 3);
							auth *= 256;
							auth += *(s + 4);
							break;
						}
						if (sec < sp->ol_val)
							j -= m--;
						else
							j += m--;
					}
				}
				break;
			}
			if (opt < op->ol_val)
				i -= mv--;
			else
				i += mv--;
		}
		hlen -= ol;
		s += ol;
	}
	if (auth && !(auth & 0x0100))
		auth &= 0xff00;
	fi->fi_optmsk = optmsk;
	fi->fi_secmsk = secmsk;
	fi->fi_auth = auth;
}


/*
 * check an IP packet for TCP/UDP characteristics such as ports and flags.
 */
static int fr_tcpudpchk(fr, fin)
frentry_t *fr;
fr_info_t *fin;
{
	register u_short po, tup;
	register char i;
	register int err = 1;

	/*
	 * Both ports should *always* be in the first fragment.
	 * So far, I cannot find any cases where they can not be.
	 *
	 * compare destination ports
	 */
	if ((i = (int)fr->fr_dcmp)) {
		po = fr->fr_dport;
		tup = fin->fin_data[1];
		/*
		 * Do opposite test to that required and
		 * continue if that succeeds.
		 */
		if (!--i && tup != po) /* EQUAL */
			err = 0;
		else if (!--i && tup == po) /* NOTEQUAL */
			err = 0;
		else if (!--i && tup >= po) /* LESSTHAN */
			err = 0;
		else if (!--i && tup <= po) /* GREATERTHAN */
			err = 0;
		else if (!--i && tup > po) /* LT or EQ */
			err = 0;
		else if (!--i && tup < po) /* GT or EQ */
			err = 0;
		else if (!--i &&	   /* Out of range */
			 (tup >= po && tup <= fr->fr_dtop))
			err = 0;
		else if (!--i &&	   /* In range */
			 (tup <= po || tup >= fr->fr_dtop))
			err = 0;
	}
	/*
	 * compare source ports
	 */
	if (err && (i = (int)fr->fr_scmp)) {
		po = fr->fr_sport;
		tup = fin->fin_data[0];
		if (!--i && tup != po)
			err = 0;
		else if (!--i && tup == po)
			err = 0;
		else if (!--i && tup >= po)
			err = 0;
		else if (!--i && tup <= po)
			err = 0;
		else if (!--i && tup > po)
			err = 0;
		else if (!--i && tup < po)
			err = 0;
		else if (!--i &&	   /* Out of range */
			 (tup >= po && tup <= fr->fr_stop))
			err = 0;
		else if (!--i &&	   /* In range */
			 (tup <= po || tup >= fr->fr_stop))
			err = 0;
	}

	/*
	 * If we don't have all the TCP/UDP header, then how can we
	 * expect to do any sort of match on it ?  If we were looking for
	 * TCP flags, then NO match.  If not, then match (which should
	 * satisfy the "short" class too).
	 */
	if (err && (fin->fin_fi.fi_p == IPPROTO_TCP)) {
		if (fin->fin_fi.fi_fl & FI_SHORT)
			return !(fr->fr_tcpf | fr->fr_tcpfm);
		/*
		 * Match the flags ?  If not, abort this match.
		 */
		if (fr->fr_tcpf &&
		    fr->fr_tcpf != (fin->fin_tcpf & fr->fr_tcpfm)) {
			FR_DEBUG(("f. %#x & %#x != %#x\n", fin->fin_tcpf,
				 fr->fr_tcpfm, fr->fr_tcpf));
			err = 0;
		}
	}
	return err;
}

/*
 * Check the input/output list of rules for a match and result.
 * Could be per interface, but this gets real nasty when you don't have
 * kernel sauce.
 */
static int fr_scanlist(pass, ip, fin, m)
int pass;
ip_t *ip;
register fr_info_t *fin;
void *m;
{
	register struct frentry *fr;
	register fr_ip_t *fi = &fin->fin_fi;
	int rulen, portcmp = 0, off;

	fr = fin->fin_fr;
	fin->fin_fr = NULL;
	fin->fin_rule = 0;
	off = ip->ip_off & 0x1fff;
	pass |= (fi->fi_fl << 20);

	if ((fi->fi_fl & FI_TCPUDP) && (fin->fin_dlen > 3) && !off)
		portcmp = 1;

	for (rulen = 0; fr; fr = fr->fr_next, rulen++) {
		/*
		 * In all checks below, a null (zero) value in the
		 * filter struture is taken to mean a wildcard.
		 *
		 * check that we are working for the right interface
		 */
#ifdef	_KERNEL
		if (fr->fr_ifa && fr->fr_ifa != fin->fin_ifp)
			continue;
#else
		if (opts & (OPT_VERBOSE|OPT_DEBUG))
			printf("\n");
		FR_VERBOSE(("%c", (pass & FR_PASS) ? 'p' : 'b'));
		if (fr->fr_ifa && fr->fr_ifa != fin->fin_ifp)
			continue;
		FR_VERBOSE((":i"));
#endif
		{
			register u_long	*ld, *lm, *lip;
			register int i;

			lip = (u_long *)fi;
			lm = (u_long *)&fr->fr_mip;
			ld = (u_long *)&fr->fr_ip;
			i = ((lip[0] & lm[0]) != ld[0]);
			FR_IFDEBUG(i,continue,("0. %#08x & %#08x != %#08x\n",
				   lip[0], lm[0], ld[0]));
			i |= ((lip[1] & lm[1]) != ld[1]);
			FR_IFDEBUG(i,continue,("1. %#08x & %#08x != %#08x\n",
				   lip[1], lm[1], ld[1]));
			i |= ((lip[2] & lm[2]) != ld[2]);
			FR_IFDEBUG(i,continue,("2. %#08x & %#08x != %#08x\n",
				   lip[2], lm[2], ld[2]));
			i |= ((lip[3] & lm[3]) != ld[3]);
			FR_IFDEBUG(i,continue,("3. %#08x & %#08x != %#08x\n",
				   lip[3], lm[3], ld[3]));
			i |= ((lip[4] & lm[4]) != ld[4]);
			FR_IFDEBUG(i,continue,("4. %#08x & %#08x != %#08x\n",
				   lip[4], lm[4], ld[4]));
			if (i)
				continue;
		}

		/*
		 * If a fragment, then only the first has what we're looking
		 * for here...
		 */
		if (fi->fi_fl & FI_TCPUDP) {
			if (portcmp) {
				if (!fr_tcpudpchk(fr, fin))
					continue;
			} else if (fr->fr_dcmp || fr->fr_scmp || fr->fr_tcpf ||
				   fr->fr_tcpfm)
				continue;
		} else if (fi->fi_p == IPPROTO_ICMP) {
			if (!off && (fin->fin_dlen > 1)) {
				if ((fin->fin_data[0] & fr->fr_icmpm) !=
				    fr->fr_icmp) {
					FR_DEBUG(("i. %#x & %#x != %#x\n",
						 fin->fin_data[0],
						 fr->fr_icmpm, fr->fr_icmp));
					continue;
				}
			} else if (fr->fr_icmpm || fr->fr_icmp)
				continue;
		}
		FR_VERBOSE(("*"));
		/*
		 * Just log this packet...
		 */
		pass = fr->fr_flags;
		if ((pass & FR_CALLNOW) && fr->fr_func)
			pass = (*fr->fr_func)(pass, ip, fin);
#ifdef  IPFILTER_LOG
		if ((pass & FR_LOGMASK) == FR_LOG) {
			if (!IPLLOG(fr->fr_flags, ip, fin, m))
				frstats[fin->fin_out].fr_skip++;
			frstats[fin->fin_out].fr_pkl++;
		}
#endif /* IPFILTER_LOG */
		FR_DEBUG(("pass %#x\n", pass));
		fr->fr_hits++;
		if (pass & FR_ACCOUNT)
			fr->fr_bytes += (U_QUAD_T)ip->ip_len;
		else
			fin->fin_icode = fr->fr_icode;
		fin->fin_rule = rulen;
		fin->fin_fr = fr;
		if (pass & FR_QUICK)
			break;
	}
	return pass;
}


/*
 * frcheck - filter check
 * check using source and destination addresses/pors in a packet whether
 * or not to pass it on or not.
 */
int fr_check(ip, hlen, ifp, out
#ifdef _KERNEL
# if SOLARIS
, qif, q, mp)
qif_t *qif;
queue_t *q;
mblk_t **mp;
# else
, mp)
struct mbuf **mp;
# endif
#else
, mp)
char *mp;
#endif
ip_t *ip;
int hlen;
struct ifnet *ifp;
int out;
{
	/*
	 * The above really sucks, but short of writing a diff
	 */
	fr_info_t frinfo, *fc;
	register fr_info_t *fin = &frinfo;
	frentry_t *fr = NULL;
	int pass, changed;
#ifndef	_KERNEL
	char	*mc = mp, *m = mp;
#endif

#ifdef	_KERNEL
# if !defined(__SVR4) && !defined(__svr4__)
	register struct mbuf *m = *mp;
	struct mbuf *mc = NULL;

	if ((ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP ||
	     ip->ip_p == IPPROTO_ICMP)) {
		register int up = MIN(hlen + 8, ip->ip_len);

		if (up > m->m_len) {
			if ((*mp = m_pullup(m, up)) == 0) {
				frstats[out].fr_pull[1]++;
				return -1;
			} else {
				frstats[out].fr_pull[0]++;
				m = *mp;
				ip = mtod(m, struct ip *);
			}
		}
	}
# endif
# if SOLARIS
	mblk_t *mc = NULL, *m = qif->qf_m;
# endif
#endif
	fr_makefrip(hlen, ip, fin);
	fin->fin_ifp = ifp;
	fin->fin_out = out;

	MUTEX_ENTER(&ipf_mutex);
	if (!out) {
		changed = ip_natin(ip, hlen, fin);
		if ((fin->fin_fr = ipacct[0][fr_active]) &&
		    (FR_SCANLIST(FR_NOMATCH, ip, fin, m) & FR_ACCOUNT))
			frstats[0].fr_acct++;
	}

	if ((pass = ipfr_knownfrag(ip, fin))) {
		if ((pass & FR_KEEPSTATE)) {
			if (fr_addstate(ip, fin, pass) == -1)
				frstats[out].fr_bads++;
			else
				frstats[out].fr_ads++;
		}
	} else if ((pass = fr_checkstate(ip, fin))) {
		if ((pass & FR_KEEPFRAG)) {
			if (fin->fin_fi.fi_fl & FI_FRAG) {
				if (ipfr_newfrag(ip, fin, pass) == -1)
					frstats[out].fr_bnfr++;
				else
					frstats[out].fr_nfr++;
			} else
				frstats[out].fr_cfr++;
		}
	} else {
		fc = frcache + out;
		if (fc->fin_fr && !bcmp((char *)fin, (char *)fc, FI_CSIZE)) {
			/*
			 * copy cached data so we can unlock the mutex
			 * earlier.
			 */
			bcopy((char *)fc, (char *)fin, sizeof(*fin));
			frstats[out].fr_chit++;
			pass = fin->fin_fr->fr_flags;
		} else {
			pass = IPF_NOMATCH;
			if ((fin->fin_fr = ipfilter[out][fr_active]))
				pass = FR_SCANLIST(IPF_NOMATCH, ip, fin, m);
			bcopy((char *)fin, (char *)fc, FI_CSIZE);
			if (pass & FR_NOMATCH)
				frstats[out].fr_nom++;
		}
		fr = fin->fin_fr;

		if ((pass & FR_KEEPFRAG)) {
			if (fin->fin_fi.fi_fl & FI_FRAG) {
				if (ipfr_newfrag(ip, fin, pass) == -1)
					frstats[out].fr_bnfr++;
				else
					frstats[out].fr_nfr++;
			} else
				frstats[out].fr_cfr++;
		}
		if (pass & FR_KEEPSTATE) {
			if (fr_addstate(ip, fin, pass) == -1)
				frstats[out].fr_bads++;
			else
				frstats[out].fr_ads++;
		}
	}

	if (fr && fr->fr_func && !(pass & FR_CALLNOW))
		pass = (*fr->fr_func)(pass, ip, fin);

	if (out) {
		if ((fin->fin_fr = ipacct[1][fr_active]) &&
		    (FR_SCANLIST(FR_NOMATCH, ip, fin, m) & FR_ACCOUNT))
			frstats[1].fr_acct++;
		fin->fin_fr = NULL;
		changed = ip_natout(ip, hlen, fin);
	}
	fin->fin_fr = fr;
	MUTEX_EXIT(&ipf_mutex);

#ifdef	IPFILTER_LOG
	if ((fr_flags & FF_LOGGING) || (pass & FR_LOGMASK)) {
		if ((fr_flags & FF_LOGNOMATCH) && (pass & FR_NOMATCH)) {
			pass |= FF_LOGNOMATCH;
			frstats[out].fr_npkl++;
			goto logit;
		} else if (((pass & FR_LOGMASK) == FR_LOGP) ||
		    ((pass & FR_PASS) && (fr_flags & FF_LOGPASS))) {
			if ((pass & FR_LOGMASK) != FR_LOGP)
				pass |= FF_LOGPASS;
			frstats[out].fr_ppkl++;
			goto logit;
		} else if (((pass & FR_LOGMASK) == FR_LOGB) ||
			   ((pass & FR_BLOCK) && (fr_flags & FF_LOGBLOCK))) {
			if ((pass & FR_LOGMASK) != FR_LOGB)
				pass |= FF_LOGBLOCK;
			frstats[out].fr_bpkl++;
logit:
			if (!IPLLOG(pass, ip, fin, m)) {
				frstats[out].fr_skip++;
				if ((pass & (FR_PASS|FR_LOGORBLOCK)) ==
				    (FR_PASS|FR_LOGORBLOCK))
					pass ^= FR_PASS|FR_BLOCK;
			}
		}
	}
#endif /* IPFILTER_LOG */

	if (pass & FR_PASS)
		frstats[out].fr_pass++;
	else if (pass & FR_BLOCK) {
		frstats[out].fr_block++;
		/*
		 * Should we return an ICMP packet to indicate error
		 * status passing through the packet filter ?
		 * WARNING: ICMP error packets AND TCP RST packets should
		 * ONLY be sent in repsonse to incoming packets.  Sending them
		 * in response to outbound packets can result in a panic on
		 * some operating systems.
		 */
		if (!out) {
#ifdef	_KERNEL
			if (pass & FR_RETICMP) {
# if SOLARIS
				ICMP_ERROR(q, ip, ICMP_UNREACH, fin->fin_icode,
					   qif, ip->ip_src);
# else
				ICMP_ERROR(m, ip, ICMP_UNREACH, fin->fin_icode,
					   ifp, ip->ip_src);
				m = *mp = NULL;	/* freed by icmp_error() */
# endif

				frstats[0].fr_ret++;
			} else if ((pass & FR_RETRST) &&
				   !(fin->fin_fi.fi_fl & FI_SHORT)) {
				if (SEND_RESET(ip, qif, q, ifp) == 0)
					frstats[1].fr_ret++;
			}
#else
			if (pass & FR_RETICMP) {
				verbose("- ICMP unreachable sent\n");
				frstats[0].fr_ret++;
			} else if ((pass & FR_RETRST) &&
				   !(fin->fin_fi.fi_fl & FI_SHORT)) {
				verbose("- TCP RST sent\n");
				frstats[1].fr_ret++;
			}
#endif
		}
	}
#ifdef	_KERNEL
# if	!SOLARIS
	if (pass & FR_DUP)
		mc = m_copy(m, 0, M_COPYALL);
	if (fr) {
		frdest_t *fdp = &fr->fr_tif;

		if ((pass & FR_FASTROUTE) ||
		    (fdp->fd_ifp && fdp->fd_ifp != (struct ifnet *)-1)) {
			ipfr_fastroute(m, fin, fdp);
			m = *mp = NULL;
		}
		if (mc)
			ipfr_fastroute(mc, fin, &fr->fr_dif);
	}
	if (!(pass & FR_PASS) && m)
		m_freem(m);
	return (pass & FR_PASS) ? 0 : -1;
# else
	if (pass & FR_DUP)
		mc = dupmsg(m);
	if (fr) {
		frdest_t *fdp = &fr->fr_tif;

		if ((pass & FR_FASTROUTE) ||
		    (fdp->fd_ifp && fdp->fd_ifp != (struct ifnet *)-1)) {
			ipfr_fastroute(qif, ip, m, mp, fin, fdp);
			m = *mp = NULL;
		}
		if (mc)
			ipfr_fastroute(qif, ip, mc, mp, fin, &fr->fr_dif);
	}
	return (pass & FR_PASS) ? changed : -1;
# endif
#else
	if (pass & FR_NOMATCH)
		return 1;
	if (pass & FR_PASS)
		return 0;
	return -1;
#endif
}


#ifdef	IPFILTER_LOG
int fr_copytolog(dev, buf, len)
int dev;
char *buf;
int len;
{
	register char	*bufp = iplbuf[dev], *tp = iplt[dev], *hp = iplh[dev];
	register int	clen, tail;

	tail = (hp >= tp) ? (bufp + IPLLOGSIZE - hp) : (tp - hp);
	clen = MIN(tail, len);
	bcopy(buf, hp, clen);
	len -= clen;
	tail -= clen;
	hp += clen;
	buf += clen;
	if (hp == bufp + IPLLOGSIZE) {
		hp = bufp;
		tail = tp - hp;
	}
	if (len && tail) {
		clen = MIN(tail, len);
		bcopy(buf, hp, clen);
		len -= clen;
		hp += clen;
	}
	iplh[dev] = hp;
	return len;
}
#endif
