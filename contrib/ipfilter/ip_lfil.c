/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if !defined(lint)
static const char rcsid[] = "@(#)$Id: ip_lfil.c,v 2.6.2.4 2002/03/06 09:44:11 darrenr Exp $";
#endif

#if defined(KERNEL) && !defined(_KERNEL)
# define	_KERNEL
#endif
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/dir.h>
#include <sys/socket.h>
#ifndef	_KERNEL
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <ctype.h>
#else
# include <linux/module.h>
#endif

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#ifndef	_KERNEL
# include <syslog.h>
#endif
#include "netinet/ip_compat.h"
#include <netinet/tcpip.h>
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_auth.h"
#ifdef _KERNEL
#include <net/ip_forward.h>
#endif
#ifndef	MIN
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif


#ifndef	_KERNEL
# include "ipt.h"
static	struct	ifnet **ifneta = NULL;
static	int	nifs = 0;
#endif

int	fr_running = 0;
int	ipl_unreach = ICMP_UNREACH_FILTER;
u_long	ipl_frouteok[2] = {0, 0};

static	int	frzerostats __P((caddr_t));
static	void	frsync __P((void));
#if defined(__NetBSD__) || defined(__OpenBSD__)
static	int	frrequest __P((int, u_long, caddr_t, int));
#else
static	int	frrequest __P((int, u_long, caddr_t, int));
#endif
#ifdef	_KERNEL
static	int	(*fr_savep) __P((ip_t *, int, void *, int, mb_t **));
#else
int	ipllog __P((void));
void	init_ifp __P((void));
static int 	no_output __P((mb_t *, struct ifnet *));
static int	write_output __P((mb_t *, struct ifnet *));
#endif

#ifdef	_KERNEL

int fr_precheck(struct iphdr *ip, struct device *dev, int out, struct device **ifp)
{
	int hlen = ip->ihl << 2;

	return fr_check((ip_t *)ip, hlen, dev, out, (mb_t **)ifp);
}


int iplattach()
{
	char *defpass;
	int s;

	if (fr_running || (fr_checkp == fr_precheck)) {
		printk("IP Filter: already initialized\n");
		return EBUSY;
	}

	fr_running = 1;
	bzero((char *)frcache, sizeof(frcache));
	bzero((char *)nat_table, sizeof(nat_table));
	fr_savep = fr_checkp;
	fr_checkp = fr_precheck;

# ifdef	IPFILTER_LOG
	ipflog_init();
# endif
	if (fr_pass & FR_PASS)
		defpass = "pass";
	else if (fr_pass & FR_BLOCK)
		defpass = "block";
	else
		defpass = "no-match -> block";

	printk("IP Filter: initialized.  Default = %s all, Logging = %s\n",
		defpass,
# ifdef	IPFILTER_LOG
		"enabled");
# else
		"disabled");
# endif
	return 0;
}


/*
 * Disable the filter by removing the hooks from the IP input/output
 * stream.
 */
int ipldetach()
{
	int s, i = FR_INQUE|FR_OUTQUE;

	if (!fr_running)
	{
		printk("IP Filter: not initialized\n");
		return 0;
	}

	fr_checkp = fr_savep;
	i = frflush(IPL_LOGIPF, i);
	fr_running = 0;

	ipfr_unload();
	ip_natunload();
	fr_stateunload();
	fr_authunload();

	printk("IP Filter: unloaded\n");

	return 0;
}
#endif /* _KERNEL */


static	int	frzerostats(data)
caddr_t	data;
{
	struct	friostat	fio;
	int error;

	bcopy((char *)frstats, (char *)fio.f_st,
		sizeof(struct filterstats) * 2);
	fio.f_fin[0] = ipfilter[0][0];
	fio.f_fin[1] = ipfilter[0][1];
	fio.f_fout[0] = ipfilter[1][0];
	fio.f_fout[1] = ipfilter[1][1];
	fio.f_acctin[0] = ipacct[0][0];
	fio.f_acctin[1] = ipacct[0][1];
	fio.f_acctout[0] = ipacct[1][0];
	fio.f_acctout[1] = ipacct[1][1];
	fio.f_active = fr_active;
	fio.f_froute[0] = ipl_frouteok[0];
	fio.f_froute[1] = ipl_frouteok[1];
	error = IWCOPYPTR((caddr_t)&fio, data, sizeof(fio));
	if (!error)
		bzero((char *)frstats, sizeof(*frstats) * 2);
	return error;
}


/*
 * Filter ioctl interface.
 */
#if defined(_KERNEL)
int iplioctl(struct inode *inode, struct file *file, u_int cmd, u_long arg)
{
	int s;
	caddr_t data = (caddr_t)arg;

	int mode = file->f_mode;
#else
int iplioctl(dev_t dev, int cmd, caddr_t data, int mode)
{
#endif
	int error = 0, unit = 0, tmp;

#ifdef	_KERNEL
	unit = GET_MINOR(inode->i_rdev);
	if ((IPL_LOGMAX < unit) || (unit < 0))
		return ENXIO;
#endif

	if (unit == IPL_LOGNAT) {
		error = nat_ioctl(data, cmd, mode);
		return error;
	}
	if (unit == IPL_LOGSTATE) {
		error = fr_state_ioctl(data, cmd, mode);
		return error;
	}

	switch (cmd) {
	case FIONREAD :
#ifdef IPFILTER_LOG
		error = IWCOPY((caddr_t)&iplused[IPL_LOGIPF], data,
			       sizeof(iplused[IPL_LOGIPF]));
#endif
		break;
#if !defined(IPFILTER_LKM) && defined(_KERNEL)
	case SIOCFRENB :
	{
		u_int	enable;

		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = IRCOPY(data, (caddr_t)&enable, sizeof(enable));
			if (error)
				break;
			if (enable)
				error = iplattach();
			else
				error = ipldetach();
		}
		break;
	}
#endif
	case SIOCSETFF :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = IRCOPY(data, (caddr_t)&fr_flags,
				       sizeof(fr_flags));
		break;
	case SIOCGETFF :
		error = IWCOPY((caddr_t)&fr_flags, data, sizeof(fr_flags));
		break;
	case SIOCINAFR :
	case SIOCRMAFR :
	case SIOCADAFR :
	case SIOCZRLST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(unit, cmd, data, fr_active);
		break;
	case SIOCINIFR :
	case SIOCRMIFR :
	case SIOCADIFR :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(unit, cmd, data, 1 - fr_active);
		break;
	case SIOCSWAPA :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			bzero((char *)frcache, sizeof(frcache[0]) * 2);
			*(u_int *)data = fr_active;
			fr_active = 1 - fr_active;
		}
		break;
	case SIOCGETFS :
	{
		struct	friostat	fio;

		bcopy((char *)frstats, (char *)fio.f_st,
			sizeof(struct filterstats) * 2);
		fio.f_fin[0] = ipfilter[0][0];
		fio.f_fin[1] = ipfilter[0][1];
		fio.f_fout[0] = ipfilter[1][0];
		fio.f_fout[1] = ipfilter[1][1];
		fio.f_acctin[0] = ipacct[0][0];
		fio.f_acctin[1] = ipacct[0][1];
		fio.f_acctout[0] = ipacct[1][0];
		fio.f_acctout[1] = ipacct[1][1];
		fio.f_auth = ipauth;
		fio.f_active = fr_active;
		fio.f_froute[0] = ipl_frouteok[0];
		fio.f_froute[1] = ipl_frouteok[1];
		error = IWCOPYPTR((caddr_t)&fio, data, sizeof(fio));
		break;
	}
	case	SIOCFRZST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frzerostats(data);
		break;
	case	SIOCIPFFL :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = IRCOPY(data, (caddr_t)&tmp, sizeof(tmp));
			if (!error) {
				tmp = frflush(unit, tmp);
				error = IWCOPY((caddr_t)&tmp, data,
					       sizeof(tmp));
			}
		}
		break;
#ifdef	IPFILTER_LOG
	case	SIOCIPFFB :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			*(int *)data = ipflog_clear(unit);
		break;
#endif /* IPFILTER_LOG */
	case SIOCGFRST :
		error = IWCOPYPTR((caddr_t)ipfr_fragstats(), data,
			       sizeof(ipfrstat_t));
		break;
	case SIOCAUTHW :
	case SIOCAUTHR :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
	case SIOCATHST :
		error = fr_auth_ioctl(data, mode, cmd, NULL, NULL);
		break;
	case SIOCFRSYN :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
#if defined(_KERNEL) && defined(__sgi)
			ipfsync();
#endif
			frsync();
		}
		break;
	default :
		error = EINVAL;
		break;
	}
	return error;
}


static void frsync()
{
#ifdef _KERNEL
	struct device *dev;

	for (dev = dev_base; dev; dev = dev->next)
		ip_natsync(dev);
#endif
}


static int frrequest(unit, req, data, set)
int unit;
u_long req;
int set;
caddr_t data;
{
	register frentry_t *fp, *f, **fprev;
	register frentry_t **ftail;
	frentry_t frd;
	frdest_t *fdp;
	frgroup_t *fg = NULL;
	int error = 0, in;
	u_int group;

	fp = &frd;
	error = IRCOPYPTR(data, (caddr_t)fp, sizeof(*fp));
	if (error)
		return error;

	/*
	 * Check that the group number does exist and that if a head group
	 * has been specified, doesn't exist.
	 */
	if (fp->fr_grhead &&
	    fr_findgroup((u_int)fp->fr_grhead, fp->fr_flags, unit, set, NULL))
		return EEXIST;
	if (fp->fr_group &&
	    !fr_findgroup((u_int)fp->fr_group, fp->fr_flags, unit, set, NULL))
		return ESRCH;

	in = (fp->fr_flags & FR_INQUE) ? 0 : 1;

	if (unit == IPL_LOGAUTH)
		ftail = fprev = &ipauth;
	else if (fp->fr_flags & FR_ACCOUNT)
		ftail = fprev = &ipacct[in][set];
	else if (fp->fr_flags & (FR_OUTQUE|FR_INQUE))
		ftail = fprev = &ipfilter[in][set];
	else
		return ESRCH;

	if ((group = fp->fr_group)) {
		if (!(fg = fr_findgroup(group, fp->fr_flags, unit, set, NULL)))
			return ESRCH;
		ftail = fprev = fg->fg_start;
	}

	bzero((char *)frcache, sizeof(frcache[0]) * 2);

	if (*fp->fr_ifname) {
		fp->fr_ifa = GETUNIT(fp->fr_ifname, fp->fr_ip.fi_v);
		if (!fp->fr_ifa)
			fp->fr_ifa = (void *)-1;
	}

	fdp = &fp->fr_dif;
	fp->fr_flags &= ~FR_DUP;
	if (*fdp->fd_ifname) {
		fdp->fd_ifp = GETUNIT(fdp->fd_ifname, fp->fr_ip.fi_v);
		if (!fdp->fd_ifp)
			fdp->fd_ifp = (struct ifnet *)-1;
		else
			fp->fr_flags |= FR_DUP;
	}

	fdp = &fp->fr_tif;
	if (*fdp->fd_ifname) {
		fdp->fd_ifp = GETUNIT(fdp->fd_ifname, fp->fr_ip.fi_v);
		if (!fdp->fd_ifp)
			fdp->fd_ifp = (struct ifnet *)-1;
	}

	/*
	 * Look for a matching filter rule, but don't include the next or
	 * interface pointer in the comparison (fr_next, fr_ifa).
	 */
	for (; (f = *ftail); ftail = &f->fr_next)
		if (bcmp((char *)&f->fr_ip, (char *)&fp->fr_ip,
			 FR_CMPSIZ) == 0)
			break;

	/*
	 * If zero'ing statistics, copy current to caller and zero.
	 */
	if (req == SIOCZRLST) {
		if (!f)
			return ESRCH;
		error = IWCOPYPTR((caddr_t)f, data, sizeof(*f));
		if (error)
			return error;
		f->fr_hits = 0;
		f->fr_bytes = 0;
		return 0;
	}

	if (!f) {
		if (req == SIOCINAFR || req == SIOCINIFR) {
			ftail = fprev;
			if (fp->fr_hits) {
				while (--fp->fr_hits && (f = *ftail)) {
					ftail = &f->fr_next;
				}
			}
		}
		f = NULL;
	}

	if (req == SIOCRMAFR || req == SIOCRMIFR) {
		if (!f)
			error = ESRCH;
		else {
			if (f->fr_ref > 1)
				return EBUSY;
			if (fg && fg->fg_head)
				fg->fg_head->fr_ref--;
			if (unit == IPL_LOGAUTH)
				return fr_auth_ioctl(data, mode, req, f, ftail);
			if (f->fr_grhead)
				fr_delgroup((u_int)f->fr_grhead, fp->fr_flags,
					    unit, set);
			fixskip(fprev, f, -1);
			*ftail = f->fr_next;
			KFREE(f);
		}
	} else {
		if (f)
			error = EEXIST;
		else {
			if (unit == IPL_LOGAUTH)
				return fr_auth_ioctl(data, mode, req, f, ftail);
			KMALLOC(f, frentry_t *);
			if (f != NULL) {
				if (fg && fg->fg_head)
					fg->fg_head->fr_ref++;
				bcopy((char *)fp, (char *)f, sizeof(*f));
				f->fr_ref = 1;
				f->fr_hits = 0;
				f->fr_next = *ftail;
				*ftail = f;
				if (req == SIOCINIFR || req == SIOCINAFR)
					fixskip(fprev, f, 1);
				f->fr_grp = NULL;
				if ((group = f->fr_grhead))
					fg = fr_addgroup(group, f, unit, set);
			} else
				error = ENOMEM;
		}
	}
	return (error);
}


#ifdef	_KERNEL
/*
 * routines below for saving IP headers to buffer
 */
int iplopen(struct inode *inode, struct file *file)
{
	u_int min = GET_MINOR(inode->i_rdev);

	if (IPL_LOGMAX < min)
		min = ENXIO;
	else {
		MOD_INC_USE_COUNT;
		min = 0;
	}
	return min;
}


void iplclose(struct inode *inode, struct file *file)
{
	u_int	min = GET_MINOR(inode->i_rdev);

	if (IPL_LOGMAX >= min) {
		MOD_DEC_USE_COUNT;
	}
}

/*
 * iplread/ipllog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
int iplread(struct inode *inode, struct file *file, char *buf, int nbytes)
{
	struct uio uiob, *uio = &uiob;

	uio->uio_buf = buf;
	uio->uio_resid = nbytes;
#  ifdef IPFILTER_LOG
	return ipflog_read(GET_MINOR(inode->i_rdev), uio);
#  else
	return ENXIO;
#  endif
}


/*
 * send_reset - this could conceivably be a call to tcp_respond(), but that
 * requires a large amount of setting up and isn't any more efficient.
 */
int send_reset(ti, ifp)
struct tcpiphdr *ti;
struct ifnet *ifp;
{
	tcphdr_t *tcp;
	int tlen = 0;
	ip_t *ip;
	mb_t *m;

	if (ti->ti_flags & TH_RST)
		return -1;		/* feedback loop */

	m = alloc_skb(sizeof(tcpiphdr_t), GFP_ATOMIC);
	if (m == NULL)
		return -1;

	if (ti->ti_flags & TH_SYN)
		tlen = 1;

	m->dev = ifp;
	m->csum = 0;
	ip = mtod(m, ip_t *);
	m->h.iph = ip;
	m->ip_hdr = NULL;
	m->m_len = sizeof(tcpiphdr_t);
	tcp = (tcphdr_t *)((char *)ip + sizeof(ip_t));
	bzero((char *)ip, sizeof(tcpiphdr_t));
 
	ip->ip_v = IPVERSION;
	ip->ip_hl = sizeof(ip_t) >> 2;
	ip->ip_tos = ((ip_t *)ti)->ip_tos;
	ip->ip_p = ((ip_t *)ti)->ip_p;
	ip->ip_id = ((ip_t *)ti)->ip_id;
	ip->ip_len = htons(sizeof(tcpiphdr_t));
	ip->ip_ttl = 127;
	ip->ip_src.s_addr = ti->ti_dst.s_addr;
	ip->ip_dst.s_addr = ti->ti_src.s_addr;
	tcp->th_dport = ti->ti_sport;
	tcp->th_sport = ti->ti_dport;
	tcp->th_ack = htonl(ntohl(ti->ti_seq) + tlen);
	tcp->th_off = sizeof(tcphdr_t) >> 2;
	tcp->th_flags = TH_RST|TH_ACK;
 
	ip->ip_sum = 0;
	ip->ip_sum = ipf_cksum((u_short *)ip, sizeof(ip_t));
	tcp->th_sum = fr_tcpsum(m, ip, tcp);
	return ip_forward(m, NULL, IPFWD_NOTTLDEC, ip->ip_dst.s_addr);
}


size_t mbufchainlen(m0)
register mb_t *m0;
{
	register size_t len = 0;

	for (; m0; m0 = m0->m_next)
		len += m0->m_len;
	return len;
}


void ipfr_fastroute(m0, fin, fdp)
mb_t *m0;
fr_info_t *fin;
frdest_t *fdp;
{
#if notyet
	register ip_t *ip, *mhip;
	register mb_t *m = m0;
	register struct route *ro;
	struct ifnet *ifp = fdp->fd_ifp;
	int len, off, error = 0;
	int hlen = fin->fin_hlen;
	struct route iproute;
	struct sockaddr_in *dst;

	ip = mtod(m0, ip_t *);
	/*
	 * Route packet.
	 */
	ro = &iproute;
	bzero((caddr_t)ro, sizeof (*ro));
	dst = (struct sockaddr_in *)&ro->ro_dst;
	dst->sin_family = AF_INET;
	dst->sin_addr = fdp->fd_ip.s_addr ? fdp->fd_ip : ip->ip_dst;
	/*
	 * XXX -allocate route here
	 */
	if (!ifp) {
		if (!(fin->fin_fr->fr_flags & FR_FASTROUTE)) {
			error = -2;
			goto bad;
		}
		if (ro->ro_rt == 0 || (ifp = ro->ro_rt->rt_ifp) == 0) {
			if (in_localaddr(ip->ip_dst))
				error = EHOSTUNREACH;
			else
				error = ENETUNREACH;
			goto bad;
		}
		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst = (struct sockaddr_in *)&ro->ro_rt->rt_gateway;
	}
	ro->ro_rt->rt_use++;

	/*
	 * For input packets which are being "fastrouted", they won't
	 * go back through output filtering and miss their chance to get
	 * NAT'd.
	 */
	(void) ip_natout(ip, hlen, fin);
	if (fin->fin_out)
		ip->ip_sum = 0;
	/*
	 * If small enough for interface, can just send directly.
	 */
	if (ip->ip_len <= ifp->if_mtu) {
# ifndef sparc
		ip->ip_id = htons(ip->ip_id);
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
# endif
		if (!ip->ip_sum)
			ip->ip_sum = in_cksum(m, hlen);
		error = (*ifp->hard_start_xmit)(m, ifp, m);
		goto done;
	}
	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (ip->ip_off & IP_DF) {
		error = EMSGSIZE;
		goto bad;
	}
	len = (ifp->if_mtu - hlen) &~ 7;
	if (len < 8) {
		error = EMSGSIZE;
		goto bad;
	}

    {
	int mhlen, firstlen = len;
	mb_t **mnext = &m->m_act;

	/*
	 * Loop through length of segment after first fragment,
	 * make new header and copy data of each part and link onto chain.
	 */
	m0 = m;
	mhlen = sizeof (struct ip);
	for (off = hlen + len; off < ip->ip_len; off += len) {
		MGET(m, M_DONTWAIT, MT_HEADER);
		if (m == 0) {
			error = ENOBUFS;
			goto bad;
		}
		m->m_data += max_linkhdr;
		mhip = mtod(m, struct ip *);
		bcopy((char *)ip, (char *)mhip, sizeof(*ip));
		if (hlen > sizeof (struct ip)) {
			mhlen = ip_optcopy(ip, mhip) + sizeof (struct ip);
			mhip->ip_hl = mhlen >> 2;
		}
		m->m_len = mhlen;
		mhip->ip_off = ((off - hlen) >> 3) + (ip->ip_off & ~IP_MF);
		if (ip->ip_off & IP_MF)
			mhip->ip_off |= IP_MF;
		if (off + len >= ip->ip_len)
			len = ip->ip_len - off;
		else
			mhip->ip_off |= IP_MF;
		mhip->ip_len = htons((u_short)(len + mhlen));
		m->m_next = m_copy(m0, off, len);
		if (m->m_next == 0) {
			error = ENOBUFS;	/* ??? */
			goto sendorfree;
		}
# ifndef sparc
		mhip->ip_off = htons((u_short)mhip->ip_off);
# endif
		mhip->ip_sum = 0;
		mhip->ip_sum = in_cksum(m, mhlen);
		*mnext = m;
		mnext = &m->m_act;
	}
	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header, then send each fragment (in order).
	 */
	m_adj(m0, hlen + firstlen - ip->ip_len);
	ip->ip_len = htons((u_short)(hlen + firstlen));
	ip->ip_off = htons((u_short)(ip->ip_off | IP_MF));
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m0, hlen);
sendorfree:
	for (m = m0; m; m = m0) {
		m0 = m->m_act;
		m->m_act = 0;
		if (error == 0)
			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst);
		else
			m_freem(m);
	}
    }	
done:
	if (!error)
		ipl_frouteok[0]++;
	else
		ipl_frouteok[1]++;

	if (ro->ro_rt) {
		RTFREE(ro->ro_rt);
	}
	return;
bad:
	m_freem(m);
	goto done;
# endif
}


/*
 * Fake BSD uiomove() call.
 */
int uiomove(caddr_t src, size_t ssize, int rw, struct uio *uio)
{
	int error;
	size_t mv = MIN(ssize, uio->uio_resid);

	if (rw == UIO_READ) {
		error = IWCOPY(src, (caddr_t)uio->uio_buf, mv);
	} else if (rw == UIO_WRITE) {
		error = IRCOPY((caddr_t)uio->uio_buf, src, mv);
	} else
		error = EINVAL;
	if (!error) {
		uio->uio_resid -= mv;
		uio->uio_buf += mv;
	}
	return error;
}

# ifdef IPFILTER_LKM
#  ifndef	IPL_MAJOR
#   define	IPL_MAJOR	95
#  endif

#  ifndef	IPL_NAME
#   define	IPL_NAME	"/dev/ipl"
#  endif

static struct file_operations ipl_fops = {
	NULL,		/* lseek */
	iplread,	/* read */
	NULL,		/* write */
	NULL,		/* readdir */
	NULL,		/* select */
	iplioctl,	/* ioctl */
	NULL,		/* mmap */
	iplopen,	/* open */
	iplclose,	/* release */
	NULL,		/* fsync */
	NULL,		/* fasync */
	NULL,		/* check_media_change */
	NULL,		/* revalidate */
};


int init_module(void)
{
	int error = 0, major;

	if (register_chrdev(IPL_MAJOR, "ipf", &ipl_fops)) {
		printk("ipf: unable to get major number: %d\n", IPL_MAJOR);
		return -EIO;
	}

	error = iplattach();
	if (!error)
		register_symtab(0);
	return -error;
}

void cleanup_module(void)
{
	unregister_chrdev(IPL_MAJOR, "ipf");
	(void) ipldetach();
}
# endif /* IPFILTER_LKM */
#else /* #ifdef _KERNEL */


static int no_output __P((mb_t *m, struct ifnet *ifp))
{
	return 0;
}


static int write_output __P((mb_t *m, struct ifnet *ifp))
{
	FILE *fp;
	char fname[32];
	ip_t *ip;

	ip = mtod(m, ip_t *);
	sprintf(fname, "/tmp/%s", ifp->name);
	if ((fp = fopen(fname, "a"))) {
		fwrite((char *)ip, ntohs(ip->ip_len), 1, fp);
		fclose(fp);
	}
	return 0;
}


struct ifnet *get_unit(name, v)
char *name;
int v;
{
	struct ifnet *ifp, **ifa;
	char ifname[32], *s;

	for (ifa = ifneta; ifa && (ifp = *ifa); ifa++) {
		(void) sprintf(ifname, "%s", ifp->name);
		if (!strcmp(name, ifname))
			return ifp;
	}

	if (!ifneta) {
		ifneta = (struct ifnet **)malloc(sizeof(ifp) * 2);
		ifneta[1] = NULL;
		ifneta[0] = (struct ifnet *)calloc(1, sizeof(*ifp));
		nifs = 1;
	} else {
		nifs++;
		ifneta = (struct ifnet **)realloc(ifneta,
						  (nifs + 1) * sizeof(*ifa));
		ifneta[nifs] = NULL;
		ifneta[nifs - 1] = (struct ifnet *)malloc(sizeof(*ifp));
	}
	ifp = ifneta[nifs - 1];

	for (s = name; *s && !isdigit(*s); s++)
		;
	if (*s && isdigit(*s)) {
		ifp->name = (char *)malloc(s - name + 1);
		strncpy(ifp->name, name, s - name);
		ifp->name[s - name] = '\0';
	} else {
		ifp->name = strdup(name);
	}
	ifp->hard_start_xmit = no_output;
	return ifp;
}



void init_ifp()
{
	FILE *fp;
	struct ifnet *ifp, **ifa;
	char fname[32];

	for (ifa = ifneta; ifa && (ifp = *ifa); ifa++) {
		ifp->hard_start_xmit = write_output;
		sprintf(fname, "/tmp/%s", ifp->name);
		if ((fp = fopen(fname, "w")))
			fclose(fp);
	}
}


void ipfr_fastroute(ip, fin, fdp)
ip_t *ip;
fr_info_t *fin;
frdest_t *fdp;
{
	struct ifnet *ifp = fdp->fd_ifp;

	if (!ifp)
		return;	/* no routing table out here */

	ip->ip_len = htons((u_short)ip->ip_len);
	ip->ip_off = htons((u_short)(ip->ip_off | IP_MF));
	ip->ip_sum = 0;
	(*ifp->hard_start_xmit)((mb_t *)ip, ifp);
}


int ipllog __P((void))
{
	verbose("l");
	return 0;
}


int send_reset(ip, ifp)
ip_t *ip;
struct ifnet *ifp;
{
	verbose("- TCP RST sent\n");
	return 0;
}


int icmp_error(ip, ifp)
ip_t *ip;
struct ifnet *ifp;
{
	verbose("- TCP RST sent\n");
	return 0;
}
#endif /* _KERNEL */
