/*
 * Copyright (C) 1993-2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
/* #pragma ident   "@(#)solaris.c	1.12 6/5/96 (C) 1995 Darren Reed"*/
#pragma ident "@(#)$Id: solaris.c,v 2.15.2.29 2002/01/15 14:36:54 darrenr Exp $"

#include <sys/systm.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/cred.h>
#include <sys/dditypes.h>
#include <sys/stream.h>
#include <sys/poll.h>
#include <sys/autoconf.h>
#include <sys/byteorder.h>
#include <sys/socket.h>
#include <sys/dlpi.h>
#include <sys/stropts.h>
#include <sys/sockio.h>
#include <net/if.h>
#if SOLARIS2 >= 6
# include <net/if_types.h>
#endif
#include <net/af.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include "ip_compat.h"
#include "ipl.h"
#include "ip_fil.h"
#include "ip_nat.h"
#include "ip_state.h"


char	_depends_on[] = "drv/ip";


void	solipdrvattach __P((void));
int	solipdrvdetach __P((void));

void	solattach __P((void));
int	soldetach __P((void));

extern	struct	filterstats	frstats[];
extern	KRWLOCK_T	ipf_mutex, ipfs_mutex, ipf_nat, ipf_solaris;
extern	kmutex_t	ipf_rw;
extern	int	fr_running;
extern	int	fr_flags;

extern ipnat_t *nat_list;

static	qif_t	*qif_head = NULL;
static	int	ipf_getinfo __P((dev_info_t *, ddi_info_cmd_t,
				 void *, void **));
static	int	ipf_probe __P((dev_info_t *));
static	int	ipf_identify __P((dev_info_t *));
static	int	ipf_attach __P((dev_info_t *, ddi_attach_cmd_t));
static	int	ipf_detach __P((dev_info_t *, ddi_detach_cmd_t));
static	qif_t	*qif_from_queue __P((queue_t *));
static	void	fr_donotip __P((int, qif_t *, queue_t *, mblk_t *,
				mblk_t *, ip_t *, size_t));
static	char	*ipf_devfiles[] = { IPL_NAME, IPL_NAT, IPL_STATE, IPL_AUTH,
				    NULL };
static	int	(*ipf_ip_inp) __P((queue_t *, mblk_t *)) = NULL;


#if SOLARIS2 >= 7
extern	void	ipfr_slowtimer __P((void *));
timeout_id_t	ipfr_timer_id;
static	timeout_id_t	synctimeoutid = 0;
#else
extern	void	ipfr_slowtimer __P((void));
int	ipfr_timer_id;
static	int	synctimeoutid = 0;
#endif
int	ipf_debug = 0;
int	ipf_debug_verbose = 0;

/* #undef	IPFDEBUG	1 */
/* #undef	IPFDEBUG_VERBOSE	1 */
#ifdef	IPFDEBUG
void	printire __P((ire_t *));
#endif
#define	isdigit(x)	((x) >= '0' && (x) <= '9')

static int	fr_precheck __P((mblk_t **, queue_t *, qif_t *, int));


static struct cb_ops ipf_cb_ops = {
	iplopen,
	iplclose,
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	iplread,
	nodev,		/* write */
	iplioctl,	/* ioctl */
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,
	NULL,
	D_MTSAFE,
#if SOLARIS2 > 4
	CB_REV,
	nodev,		/* aread */
	nodev,		/* awrite */
#endif
};

static struct dev_ops ipf_ops = {
	DEVO_REV,
	0,
	ipf_getinfo,
	ipf_identify,
	ipf_probe,
	ipf_attach,
	ipf_detach,
	nodev,		/* reset */
	&ipf_cb_ops,
	(struct bus_ops *)0
};

extern struct mod_ops mod_driverops;
static struct modldrv iplmod = {
	&mod_driverops, IPL_VERSION, &ipf_ops };
static struct modlinkage modlink1 = { MODREV_1, &iplmod, NULL };

#if SOLARIS2 >= 6
static	size_t	hdrsizes[57][2] = {
	{ 0, 0 },
	{ IFT_OTHER, 0 },
	{ IFT_1822, 14 },	/* 14 for ire0 ?? */
	{ IFT_HDH1822, 0 },
	{ IFT_X25DDN, 0 },
	{ IFT_X25, 0 },
	{ IFT_ETHER, 14 },
	{ IFT_ISO88023, 14 },
	{ IFT_ISO88024, 0 },
	{ IFT_ISO88025, 0 },
	{ IFT_ISO88026, 0 },
	{ IFT_STARLAN, 0 },
	{ IFT_P10, 0 },
	{ IFT_P80, 0 },
	{ IFT_HY, 0 },
	{ IFT_FDDI, 24 },
	{ IFT_LAPB, 0 },
	{ IFT_SDLC, 0 },
	{ IFT_T1, 0 },
	{ IFT_CEPT, 0 },
	{ IFT_ISDNBASIC, 0 },
	{ IFT_ISDNPRIMARY, 0 },
	{ IFT_PTPSERIAL, 0 },
	{ IFT_PPP, 0 },
	{ IFT_LOOP, 0 },
	{ IFT_EON, 0 },
	{ IFT_XETHER, 0 },
	{ IFT_NSIP, 0 },
	{ IFT_SLIP, 0 },
	{ IFT_ULTRA, 0 },
	{ IFT_DS3, 0 },
	{ IFT_SIP, 0 },
	{ IFT_FRELAY, 0 },
	{ IFT_RS232, 0 },
	{ IFT_PARA, 0 },
	{ IFT_ARCNET, 0 },
	{ IFT_ARCNETPLUS, 0 },
	{ IFT_ATM, 0 },
	{ IFT_MIOX25, 0 },
	{ IFT_SONET, 0 },
	{ IFT_X25PLE, 0 },
	{ IFT_ISO88022LLC, 0 },
	{ IFT_LOCALTALK, 0 },
	{ IFT_SMDSDXI, 0 },
	{ IFT_FRELAYDCE, 0 },
	{ IFT_V35, 0 },
	{ IFT_HSSI, 0 },
	{ IFT_HIPPI, 0 },
	{ IFT_MODEM, 0 },
	{ IFT_AAL5, 0 },
	{ IFT_SONETPATH, 0 },
	{ IFT_SONETVT, 0 },
	{ IFT_SMDSICIP, 0 },
	{ IFT_PROPVIRTUAL, 0 },
	{ IFT_PROPMUX, 0 },
};
#endif /* SOLARIS2 >= 6 */

static dev_info_t *ipf_dev_info = NULL;


int _init()
{
	int ipfinst;

	ipfinst = mod_install(&modlink1);
#ifdef	IPFDEBUG
	if (ipf_debug)
		cmn_err(CE_NOTE, "IP Filter: _init() = %d", ipfinst);
#endif
	return ipfinst;
}


int _fini(void)
{
	int ipfinst;

	ipfinst = mod_remove(&modlink1);
#ifdef	IPFDEBUG
	if (ipf_debug)
		cmn_err(CE_NOTE, "IP Filter: _fini() = %d", ipfinst);
#endif
	return ipfinst;
}


int _info(modinfop)
struct modinfo *modinfop;
{
	int ipfinst;

	ipfinst = mod_info(&modlink1, modinfop);
#ifdef	IPFDEBUG
	if (ipf_debug)
		cmn_err(CE_NOTE, "IP Filter: _info(%x) = %x",
			modinfop, ipfinst);
#endif
	if (fr_running > 0)
		ipfsync();
	return ipfinst;
}


static int ipf_probe(dip)
dev_info_t *dip;
{
	if (fr_running < 0)
		return DDI_PROBE_FAILURE;
#ifdef	IPFDEBUG
	if (ipf_debug)
		cmn_err(CE_NOTE, "IP Filter: ipf_probe(%x)", dip);
#endif
	return DDI_PROBE_SUCCESS;
}


static int ipf_identify(dip)
dev_info_t *dip;
{
#ifdef	IPFDEBUG
	if (ipf_debug)
		cmn_err(CE_NOTE, "IP Filter: ipf_identify(%x)", dip);
#endif
	if (strcmp(ddi_get_name(dip), "ipf") == 0)
		return (DDI_IDENTIFIED);
	return (DDI_NOT_IDENTIFIED);
}


static void ipf_ire_walk(ire, arg)
ire_t *ire;
void *arg;
{
	qif_t *qif = arg;

	if ((ire->ire_type == IRE_CACHE) &&
#if SOLARIS2 >= 6
	    (ire->ire_ipif != NULL) &&
	    (ire->ire_ipif->ipif_ill == qif->qf_ill)
#else
	    (ire_to_ill(ire) == qif->qf_ill)
#endif
	    ) {
#if SOLARIS2 >= 8
		mblk_t *m = ire->ire_fp_mp;
#else
		mblk_t *m = ire->ire_ll_hdr_mp;
#endif
		if (m != NULL)
			qif->qf_hl = m->b_wptr - m->b_rptr;
	}
}


static int ipf_attach(dip, cmd)
dev_info_t *dip;
ddi_attach_cmd_t cmd;
{
#ifdef	IPFDEBUG
	int instance;

	if (ipf_debug)
		cmn_err(CE_NOTE, "IP Filter: ipf_attach(%x,%x)", dip, cmd);
#endif
	switch (cmd) {
	case DDI_ATTACH:
		if (fr_running < 0)
			break;
#ifdef	IPFDEBUG
		instance = ddi_get_instance(dip);

	if (ipf_debug)
		cmn_err(CE_NOTE, "IP Filter: attach ipf instance %d", instance);
#endif
		if (ddi_create_minor_node(dip, "ipf", S_IFCHR, IPL_LOGIPF,
					  DDI_PSEUDO, 0) == DDI_FAILURE) {
			ddi_remove_minor_node(dip, NULL);
			goto attach_failed;
		}
		if (ddi_create_minor_node(dip, "ipnat", S_IFCHR, IPL_LOGNAT,
					  DDI_PSEUDO, 0) == DDI_FAILURE) {
			ddi_remove_minor_node(dip, NULL);
			goto attach_failed;
		}
		if (ddi_create_minor_node(dip, "ipstate", S_IFCHR,IPL_LOGSTATE,
					  DDI_PSEUDO, 0) == DDI_FAILURE) {
			ddi_remove_minor_node(dip, NULL);
			goto attach_failed;
		}
		if (ddi_create_minor_node(dip, "ipauth", S_IFCHR, IPL_LOGAUTH,
					  DDI_PSEUDO, 0) == DDI_FAILURE) {
			ddi_remove_minor_node(dip, NULL);
			goto attach_failed;
		}
		ipf_dev_info = dip;
		sync();
		/*
		 * Initialize mutex's
		 */
		if (iplattach() == -1)
			goto attach_failed;
		/*
		 * Lock people out while we set things up.
		 */
		WRITE_ENTER(&ipf_solaris);
		solattach();
		solipdrvattach();
		RWLOCK_EXIT(&ipf_solaris);
		cmn_err(CE_CONT, "%s, attaching complete.\n",
			ipfilter_version);
		sync();
		if (fr_running == 0)
			fr_running = 1;
		if (ipfr_timer_id == 0)
			ipfr_timer_id = timeout(ipfr_slowtimer, NULL,
						drv_usectohz(500000));
		if (fr_running == 1)
			return DDI_SUCCESS;
#if SOLARIS2 >= 8
	case DDI_RESUME :
	case DDI_PM_RESUME :
		if (ipfr_timer_id == 0)
			ipfr_timer_id = timeout(ipfr_slowtimer, NULL,
						drv_usectohz(500000));
		return DDI_SUCCESS;
#endif
	default:
		return DDI_FAILURE;
	}

attach_failed:
	cmn_err(CE_NOTE, "IP Filter: failed to attach\n");
	/*
	 * Use our own detach routine to toss
	 * away any stuff we allocated above.
	 */
	(void) ipf_detach(dip, DDI_DETACH);
	return DDI_FAILURE;
}


static int ipf_detach(dip, cmd)
dev_info_t *dip;
ddi_detach_cmd_t cmd;
{
	int i;

#ifdef	IPFDEBUG
	if (ipf_debug)
		cmn_err(CE_NOTE, "IP Filter: ipf_detach(%x,%x)", dip, cmd);
#endif
	switch (cmd) {
	case DDI_DETACH:
		if (fr_running <= 0)
			break;
		/*
		 * Make sure we're the only one's modifying things.  With
		 * this lock others should just fall out of the loop.
		 */
		mutex_enter(&ipf_rw);
		if (ipfr_timer_id != 0) {
			untimeout(ipfr_timer_id);
			ipfr_timer_id = 0;
		}
		mutex_exit(&ipf_rw);
		WRITE_ENTER(&ipf_solaris);
		mutex_enter(&ipf_rw);
		if (fr_running <= 0) {
			mutex_exit(&ipf_rw);
			return DDI_FAILURE;
		}
		fr_running = -1;
		mutex_exit(&ipf_rw);
		/* NOTE: ipf_solaris rwlock is released in ipldetach */

		/*
		 * Undo what we did in ipf_attach, freeing resources
		 * and removing things we installed.  The system
		 * framework guarantees we are not active with this devinfo
		 * node in any other entry points at this time.
		 */
		ddi_prop_remove_all(dip);
		i = ddi_get_instance(dip);
		ddi_remove_minor_node(dip, NULL);
		sync();
		i = solipdrvdetach();
		if (i > 0) {
			cmn_err(CE_CONT, "IP Filter: still attached (%d)\n", i);
			return DDI_FAILURE;
		}
		if (!soldetach()) {
			cmn_err(CE_CONT, "%s detached\n", ipfilter_version);
			return (DDI_SUCCESS);
		}
#if SOLARIS2 >= 8
	case DDI_SUSPEND :
	case DDI_PM_SUSPEND :
		if (ipfr_timer_id != 0) {
			untimeout(ipfr_timer_id);
			ipfr_timer_id = 0;
		}
		if (synctimeoutid) {
			untimeout(synctimeoutid);
			synctimeoutid = 0;
		}
		return DDI_SUCCESS;
#endif
	default:
		return (DDI_FAILURE);
	}
	return DDI_FAILURE;
}


static int ipf_getinfo(dip, infocmd, arg, result)
dev_info_t *dip;
ddi_info_cmd_t infocmd;
void *arg, **result;
{
	int error;

	if (fr_running <= 0)
		return DDI_FAILURE;
	error = DDI_FAILURE;
#ifdef	IPFDEBUG
	if (ipf_debug)
		cmn_err(CE_NOTE, "IP Filter: ipf_getinfo(%x,%x,%x)",
			dip, infocmd, arg);
#endif
	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = ipf_dev_info;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)getminor((dev_t) arg);
		error = DDI_SUCCESS;
		break;
	default:
		break;
	}
	return (error);
}

/*
 * find the filter structure setup for this queue
 */
static qif_t *qif_from_queue(q)
queue_t *q;
{
	qif_t *qif;

	for (qif = qif_head; qif; qif = qif->qf_next)
		if ((qif->qf_iptr == q->q_ptr) || (qif->qf_optr == q->q_ptr))
			break;
	return qif;
}


/*
 * OK, this is pretty scrappy code, but then it's essentially just here for
 * debug purposes and that's it.  Packets should not normally come through
 * here, and if they do, well, we would like to see as much information as
 * possible about them and what they claim to hold.
 */
void fr_donotip(out, qif, q, m, mt, ip, off)
int out;
qif_t *qif;
queue_t *q;
mblk_t *m, *mt;
ip_t *ip;
size_t off;
{
	u_char *s, outb[256], *t;
	int i;

	outb[0] = '\0';
	outb[1] = '\0';
	outb[2] = '\0';
	outb[3] = '\0';
	s = ip ? (u_char *)ip : outb;
	if (!ip && (m == mt) && m->b_cont && (MTYPE(m) != M_DATA))
		m = m->b_cont;

	cmn_err(CE_CONT, " !IP %s:%d %d %p %p %p %d %p/%d %p/%d %p %d %d %p\n",
		qif ? qif->qf_name : "?", out, qif ? qif->qf_hl : -1, q,
		q ? q->q_ptr : NULL, q ? q->q_qinfo : NULL,
		mt->b_wptr - mt->b_rptr, m, MTYPE(m), mt, MTYPE(mt), m->b_rptr,
		m->b_wptr - m->b_rptr, off, ip);
	cmn_err(CE_CONT, "%02x%02x%02x%02x\n", *s, *(s+1), *(s+2), *(s+3));
	while (m != mt) {
		i = 0;
		t = outb;
		s = mt->b_rptr;
		sprintf((char *)t, "%d:", MTYPE(mt));
		t += strlen((char *)t);
		for (; (i < 100) && (s < mt->b_wptr); i++) {
			sprintf((char *)t, "%02x%s", *s++,
				((i & 3) == 3) ? " " : "");
			t += ((i & 3) == 3) ? 3 : 2;
		}
		*t++ = '\n';
		*t = '\0';
		cmn_err(CE_CONT, "%s", outb);
		mt = mt->b_cont;
	}
	i = 0;
	t = outb;
	s = m->b_rptr;
	sprintf((char *)t, "%d:", MTYPE(m));
	t += strlen((char *)t);
	for (; (i < 100) && (s < m->b_wptr); i++) {
		sprintf((char *)t, "%02x%s", *s++, ((i & 3) == 3) ? " " : "");
		t += ((i & 3) == 3) ? 3 : 2;
	}
	*t++ = '\n';
	*t = '\0';
	cmn_err(CE_CONT, "%s", outb);
}


/*
 * find the first data mblk, if present, in the chain we're processing.  Also
 * make a few sanity checks to try prevent the filter from causing a panic -
 * none of the nice IP sanity checks (including checksumming) should have been
 * done yet (for incoming packets) - dangerous!
 */
static int fr_precheck(mp, q, qif, out)
mblk_t **mp;
queue_t *q;
qif_t *qif;
int out;
{
	register mblk_t *m, *mt = *mp;
	register ip_t *ip;
	size_t hlen, len, off, off2, mlen, iphlen, plen, woff;
	int err, synced = 0, sap, p, realigned = 0, multi = 0;
	u_char *bp;
#if SOLARIS2 >= 8
	ip6_t *ip6;
#endif
#ifndef	sparc
	u_short __ipoff;
#endif
tryagain:
	ip = NULL;
	m = NULL;
	/*
	 * If there is only M_DATA for a packet going out, then any header
	 * information (which would otherwise appear in an M_PROTO mblk before
	 * the M_DATA) is prepended before the IP header.  We need to set the
	 * offset to account for this. - see MMM
	 */
	off = (out) ? qif->qf_hl : 0;

	/*
	 * If the message protocol block indicates that there isn't a data
	 * block following it, just return back.
	 */
	bp = (u_char *)ALIGN32(mt->b_rptr);
	if (MTYPE(mt) == M_PROTO || MTYPE(mt) == M_PCPROTO) {
		dl_unitdata_ind_t *dl = (dl_unitdata_ind_t *)bp;
		if (dl->dl_primitive == DL_UNITDATA_IND) {
			multi = dl->dl_group_address;
			m = mt->b_cont;
			/*
			 * This is a complete kludge to try and work around
			 * some bizarre packets which drop through into
			 * fr_donotip.
			 */
			if (m && multi && ((*((u_char *)m->b_rptr) == 0x0) &&
			    ((*((u_char *)m->b_rptr + 2) == 0x45)))) {
				ip = (ip_t *)(m->b_rptr + 2);
				off = 2;
			} else
				off = 0;
		} else if (dl->dl_primitive != DL_UNITDATA_REQ) {
			ip = (ip_t *)dl;
			if ((ip->ip_v == IPVERSION) &&
			    (ip->ip_hl == (sizeof(*ip) >> 2)) &&
			    (ntohs(ip->ip_len) == mt->b_wptr - mt->b_rptr)) {
				off = 0;
				m = mt;
			} else {
				frstats[out].fr_notdata++;
				return 0;
			}
		}
	}

	/*
	 * Find the first data block, count the data blocks in this chain and
	 * the total amount of data.
	 */
	if (ip == NULL)
		for (m = mt; m && (MTYPE(m) != M_DATA); m = m->b_cont)
			off = 0;	/* Any non-M_DATA cancels the offset */

	if (!m) {
		frstats[out].fr_nodata++;
		return 0;	/* No data blocks */
	}

	ip = (ip_t *)(m->b_rptr + off);		/* MMM */

	/*
	 * We might have a 1st data block which is really M_PROTO, i.e. it is
	 * only big enough for the link layer header
	 */
	while ((u_char *)ip >= m->b_wptr) {
		len = (u_char *)ip - m->b_wptr;
		m = m->b_cont;
		if (m == NULL)
			return 0;	/* not enough data for IP */
		ip = (ip_t *)(m->b_rptr + len);
	}
	off = (u_char *)ip - m->b_rptr;
	if (off != 0)
		m->b_rptr = (u_char *)ip;

	len = m->b_wptr - m->b_rptr;
	if (m->b_wptr < m->b_rptr) {
		cmn_err(CE_NOTE, "!IP Filter: Bad packet: wptr %p < rptr %p",
			m->b_wptr, m->b_rptr);
		frstats[out].fr_bad++;
		return -1;
	}

	mlen = msgdsize(m);
	sap = qif->qf_ill->ill_sap;

	if (sap == 0x800) {
		u_short tlen;

		hlen = sizeof(*ip);

		/* XXX - might not be aligned (from ppp?) */
		((char *)&tlen)[0] = ((char *)&ip->ip_len)[0];
		((char *)&tlen)[1] = ((char *)&ip->ip_len)[1];

		plen = ntohs(tlen);

		sap = 0;
	}
#if SOLARIS2 >= 8
	else if (sap == IP6_DL_SAP) {
		u_short tlen;

		hlen = sizeof(ip6_t);
		ip6 = (ip6_t *)ip;
		/* XXX - might not be aligned (from ppp?) */
		((char *)&tlen)[0] = ((char *)&ip6->ip6_plen)[0];
		((char *)&tlen)[1] = ((char *)&ip6->ip6_plen)[1];
		plen = ntohs(tlen);
		if (!plen)
			return -1;	/* Jumbo gram */
		plen += sizeof(*ip6);
	}
#endif
	else {
		plen = 0;
		hlen = 0;
		sap = -1;
	}

	/*
	 * Ok, the IP header isn't on a 32bit aligned address so junk it.
	 */
	if (((u_long)ip & 0x3) || (plen > mlen) || (len < hlen) ||
	    (sap == -1)) {
		mblk_t *m1, *m2;
		u_char *s, c;
		int v;

		/*
		 * Junk using pullupmsg - it's next to useless.
		 */
fixalign:
		if (off)
			m->b_rptr -= off;
		c = *(u_char *)ip;
		c >>= 4;
		if (c != 4
#if SOLARIS2 >= 8
		    && c != 6
#endif
		) {
			frstats[out].fr_notip++;
			return (fr_flags & FF_BLOCKNONIP) ? -1 : 0;
		}

		if (realigned)
			return -1;
		realigned = 1;
		off2 = (size_t)((u_long)ip & 0x3);
		if (off2)
			off2 = 4 - off2;
		len = msgdsize(m);
		m2 = allocb(len + off2, BPRI_HI);
		if (m2 == NULL) {
			frstats[out].fr_pull[1]++;
			return -1;
		}

		MTYPE(m2) = M_DATA;
		if (m->b_rptr != (u_char *)ip)
			m2->b_rptr += off2;
		m2->b_wptr = m2->b_rptr + len;
		m1 = m;
		s = (u_char *)m->b_rptr;
		for (bp = m2->b_rptr; m1 && (bp < m2->b_wptr); bp += len) {
			len = MIN(m1->b_wptr - s, m2->b_wptr - bp);
			bcopy(s, bp, len);
			m1 = m1->b_cont;
			if (m1)
				s = m1->b_rptr;
		}

		if (mt != m && mt->b_cont == m && !off) {
			/*
			 * check if the buffer we're changing is chained in-
			 * between other buffers and unlink/relink as required.
			 */
			(void) unlinkb(mt);	/* should return 'm' */
			m1 = unlinkb(m);
			if (m1)
				linkb(m2, m1);
			freemsg(m);
			linkb(mt, m2);
		} else {
			if (m == mt) {
				m1 = unlinkb(mt);
				if (m1)
					linkb(m2, m1);
			}
			freemsg(mt);
			*mp = m2;
			mt = m2;
		}

		frstats[out].fr_pull[0]++;
		synced = 1;
		off = 0;
		goto tryagain;
	}

	if (((sap == 0) && (ip->ip_v != IP_VERSION))
#if SOLARIS2 >= 8
	    || ((sap == IP6_DL_SAP) && ((ip6->ip6_vfc >> 4) != 6))
#endif
	) {
		m->b_rptr -= off;
		return -2;
	}

#ifndef	sparc
# if SOLARIS2 >= 8
	if (sap == IP6_DL_SAP) {
		ip6->ip6_plen = plen - sizeof(*ip6);
	} else {
# endif
		__ipoff = (u_short)ip->ip_off;

		ip->ip_len = plen;
		ip->ip_off = ntohs(__ipoff);
# if SOLARIS2 >= 8
	}
# endif
#endif
	if (sap == 0)
		iphlen = ip->ip_hl << 2;
#if SOLARIS2 >= 8
	else if (sap == IP6_DL_SAP)
		iphlen = sizeof(ip6_t);
#endif

	if ((
#if SOLARIS2 >= 8
	     (sap == IP6_DL_SAP) && (mlen < plen)) ||
	    ((sap == 0) &&
#endif
	     ((iphlen < hlen) || (iphlen > plen) || (mlen < plen)))) {
		/*
		 * Bad IP packet or not enough data/data length mismatches
		 */
#ifndef	sparc
# if SOLARIS2 >= 8
		if (sap == IP6_DL_SAP) {
			ip6->ip6_plen = htons(plen - sizeof(*ip6));
		} else {
# endif
			__ipoff = (u_short)ip->ip_off;

			ip->ip_len = htons(plen);
			ip->ip_off = htons(__ipoff);
# if SOLARIS2 >= 8
		}
# endif
#endif
		m->b_rptr -= off;
		frstats[out].fr_bad++;
		return -1;
	}

	/*
	 * Make hlen the total size of the IP header plus TCP/UDP/ICMP header
	 * (if it is one of these three).
	 */
	if (sap == 0)
		p = ip->ip_p;
#if SOLARIS2 >= 8
	else if (sap == IP6_DL_SAP)
		p = ip6->ip6_nxt;

	if ((sap == IP6_DL_SAP) || ((ip->ip_off & IP_OFFMASK) == 0))
#else
	if ((ip->ip_off & IP_OFFMASK) == 0)
#endif
		switch (p)
		{
		case IPPROTO_TCP :
			hlen += sizeof(tcphdr_t);
			break;
		case IPPROTO_UDP :
			hlen += sizeof(udphdr_t);
			break;
		case IPPROTO_ICMP :
			/* 76 bytes is enough for a complete ICMP error. */
			hlen += 76 + sizeof(icmphdr_t);
			break;
		default :
			break;
		}

	woff = 0;
	if (hlen > mlen) {
		hlen = mlen;
	} else if (m->b_wptr - m->b_rptr > plen) {
		woff = m->b_wptr - m->b_rptr - plen;
		m->b_wptr -= woff;
	}

	/*
	 * If we don't have enough data in the mblk or we haven't yet copied
	 * enough (above), then copy some more.
	 */
	if ((hlen > len)) {
		if (!pullupmsg(m, (int)hlen)) {
			cmn_err(CE_NOTE, "pullupmsg failed");
			frstats[out].fr_pull[1]++;
			return -1;
		}
		frstats[out].fr_pull[0]++;
		ip = (ip_t *)ALIGN32(m->b_rptr);
	}
	qif->qf_m = m;
	qif->qf_q = q;
	qif->qf_off = off;
	qif->qf_len = len;
	err = fr_check(ip, iphlen, qif->qf_ill, out, qif, mp);
	if (err == 2) {
		goto fixalign;
	}
	/*
	 * Copy back the ip header data if it was changed, we haven't yet
	 * freed the message and we aren't going to drop the packet.
	 * BUT only do this if there were no changes to the buffer, else
	 * we can't be sure that the ip pointer is still correct!
	 */
	if (*mp != NULL) {
		if (*mp == mt) {
			m->b_wptr += woff;
			m->b_rptr -= off;
#ifndef	sparc
# if SOLARIS2 >= 8
			if (sap == IP6_DL_SAP) {
				ip6->ip6_plen = htons(plen - sizeof(*ip6));
			} else {
# endif
				__ipoff = (u_short)ip->ip_off;
				/*
				 * plen is useless because of NAT.
				 */
				ip->ip_len = htons(ip->ip_len);
				ip->ip_off = htons(__ipoff);
# if SOLARIS2 >= 8
			}
# endif
#endif
		} else
			cmn_err(CE_NOTE,
				"!IP Filter: *mp %p mt %p %s", *mp, mt,
				"mblk changed, cannot revert ip_len, ip_off");
	}
	return err;
}


/*
 * Only called for M_IOCACK messages
 */
void fr_qif_update(qif, mp)
qif_t *qif;
mblk_t *mp;
{
	struct iocblk *iocp;

	if (!qif || !mp)
		return;
	iocp = (struct iocblk *)mp->b_rptr;
	if (mp->b_cont && (iocp->ioc_cmd == DL_IOC_HDR_INFO)) {
		mp = mp->b_cont;
		if (MTYPE(mp) == M_PROTO && mp->b_cont) {
			mp = mp->b_cont;
			if (MTYPE(mp) == M_DATA) {
				qif->qf_hl = mp->b_wptr - mp->b_rptr;
			}
		}
	}
}


int fr_qin(q, mb)
queue_t *q;
mblk_t *mb;
{
	int (*pnext) __P((queue_t *, mblk_t *)), type, synced = 0, err = 0;
	qif_t qf, *qif;

#ifdef	IPFDEBUG_VERBOSE
	if (ipf_debug_verbose)
		cmn_err(CE_CONT,
			"fr_qin(%lx,%lx) ptr %lx type 0x%x ref %d len %d\n",
			q, q->q_ptr, mb, MTYPE(mb), mb->b_datap->db_ref,
			msgdsize(mb));
#endif

	/*
	 * IPFilter is still in the packet path but not enabled.  Drop whatever
	 * it is that has come through.
	 */
	if (fr_running <= 0) {
		mb->b_prev = NULL;
		freemsg(mb);
		return 0;
	}

	type = MTYPE(mb);

	/*
	 * If a mblk has more than one reference, make a copy, filter that and
	 * free a reference to the original.
	 */
	if (mb->b_datap->db_ref > 1) {
		mblk_t *m1;

		m1 = copymsg(mb);
		if (!m1) {
			frstats[0].fr_drop++;
			mb->b_prev = NULL;
			freemsg(mb);
			return 0;
		}
		mb->b_prev = NULL;
		freemsg(mb);
		mb = m1;
		frstats[0].fr_copy++;
	}

	READ_ENTER(&ipf_solaris);
again:
	if (fr_running <= 0) {
		mb->b_prev = NULL;
		freemsg(mb);
		RWLOCK_EXIT(&ipf_solaris);
		return 0;
	}
	READ_ENTER(&ipfs_mutex);
	if (!(qif = qif_from_queue(q))) {
		for (qif = qif_head; qif; qif = qif->qf_next)
			if (&qif->qf_rqinit == q->q_qinfo && qif->qf_rqinfo &&
					qif->qf_rqinfo->qi_putp) {
				pnext = qif->qf_rqinfo->qi_putp;
				frstats[0].fr_notip++;
				RWLOCK_EXIT(&ipfs_mutex);
				if (!synced) {
					ipfsync();
					synced = 1;
					goto again;
				}
				RWLOCK_EXIT(&ipf_solaris);
				/* fr_donotip(0, NULL, q, mb, mb, NULL, 0); */
				return (*pnext)(q, mb);
			}
		RWLOCK_EXIT(&ipfs_mutex);
		if (!synced) {
			ipfsync();
			synced = 1;
			goto again;
		}
		cmn_err(CE_WARN,
			"!IP Filter: dropped: fr_qin(%x,%x): type %x qif %x",
			q, mb, type, qif);
		cmn_err(CE_CONT,
			"!IP Filter: info %x next %x ptr %x fsrv %x bsrv %x\n",
			q->q_qinfo, q->q_next, q->q_ptr, q->q_nfsrv,
			q->q_nbsrv);
		cmn_err(CE_CONT, "!IP Filter: info: putp %x srvp %x info %x\n",
			q->q_qinfo->qi_putp, q->q_qinfo->qi_srvp,
#if SOLARIS > 3
			q->q_qinfo->qi_infop
#else
			0
#endif
			);
		frstats[0].fr_drop++;
		mb->b_prev = NULL;
		freemsg(mb);
		RWLOCK_EXIT(&ipf_solaris);
		return 0;
	}

	qif->qf_incnt++;
	pnext = qif->qf_rqinfo->qi_putp;
	if (type == M_IOCACK)
		fr_qif_update(qif, mb);
	bcopy((char *)qif, (char *)&qf, sizeof(qf));
	if (datamsg(type) || (type == M_BREAK))
		err = fr_precheck(&mb, q, &qf, 0);

	RWLOCK_EXIT(&ipfs_mutex);

	if ((err == 0) && (mb != NULL)) {
		if (pnext) {
			RWLOCK_EXIT(&ipf_solaris);
			return (*pnext)(q, mb);
		}

		cmn_err(CE_WARN,
			"!IP Filter: inp NULL: qif %x %s q %x info %x",
			qif, qf.qf_name, q, q->q_qinfo);
	}

	if (err == -2) {
		if (synced == 0) {
			ipfsync();
			synced = 1;
			goto again;
		}
		frstats[0].fr_notip++;
		if (!(fr_flags & FF_BLOCKNONIP) && (pnext != NULL)) {
			RWLOCK_EXIT(&ipf_solaris);
			return (*pnext)(q, mb);
		}
	}
	

	if (mb) {
		mb->b_prev = NULL;
		freemsg(mb);
	}
	RWLOCK_EXIT(&ipf_solaris);
	return 0;
}


int fr_qout(q, mb)
queue_t *q;
mblk_t *mb;
{
	int (*pnext) __P((queue_t *, mblk_t *)), type, synced = 0, err = 0;
	qif_t qf, *qif;

#ifdef	IPFDEBUG_VERBOSE
	if (ipf_debug_verbose)
		cmn_err(CE_CONT,
			"fr_qout(%lx,%lx) ptr %lx type 0x%x ref %d len %d\n",
			q, q->q_ptr, mb, MTYPE(mb), mb->b_datap->db_ref,
			msgdsize(mb));
#endif

	if (fr_running <= 0) {
		mb->b_prev = NULL;
		freemsg(mb);
		return 0;
	}

	type = MTYPE(mb);

#if SOLARIS2 >= 6
	if ((!dohwcksum || mb->b_ick_flag != ICK_VALID) &&
	    (mb->b_datap->db_ref > 1))
#else
	if (mb->b_datap->db_ref > 1)
#endif
	{
		mblk_t *m1;

		m1 = copymsg(mb);
		if (!m1) {
			frstats[1].fr_drop++;
			mb->b_prev = NULL;
			freemsg(mb);
			return 0;
		}
		mb->b_prev = NULL;
		freemsg(mb);
		mb = m1;
		frstats[1].fr_copy++;
	}

	READ_ENTER(&ipf_solaris);
again:
	if (fr_running <= 0) {
		mb->b_prev = NULL;
		freemsg(mb);
		RWLOCK_EXIT(&ipf_solaris);
		return 0;
	}
	READ_ENTER(&ipfs_mutex);
	if (!(qif = qif_from_queue(q))) {
		for (qif = qif_head; qif; qif = qif->qf_next)
			if (&qif->qf_wqinit == q->q_qinfo && qif->qf_wqinfo &&
					qif->qf_wqinfo->qi_putp) {
				pnext = qif->qf_wqinfo->qi_putp;
				RWLOCK_EXIT(&ipfs_mutex);
				frstats[1].fr_notip++;
				if (!synced) {
					ipfsync();
					synced = 1;
					goto again;
				}
				/* fr_donotip(1, NULL, q, mb, mb, NULL, 0); */
				RWLOCK_EXIT(&ipf_solaris);
				return (*pnext)(q, mb);
			}
		RWLOCK_EXIT(&ipfs_mutex);
		if (!synced) {
			ipfsync();
			synced = 1;
			goto again;
		}
		cmn_err(CE_WARN,
			"!IP Filter: dropped: fr_qout(%x,%x): type %x: qif %x",
			q, mb, type, qif);
		cmn_err(CE_CONT,
			"!IP Filter: info %x next %x ptr %x fsrv %x bsrv %x\n",
			q->q_qinfo, q->q_next, q->q_ptr, q->q_nfsrv,
			q->q_nbsrv);
		cmn_err(CE_CONT, "!IP Filter: info: putp %x srvp %x info %x\n",
			q->q_qinfo->qi_putp, q->q_qinfo->qi_srvp,
#if SOLARIS > 3
			q->q_qinfo->qi_infop
#else
			0
#endif
			);
		if (q->q_nfsrv)
			cmn_err(CE_CONT,
				"!IP Filter: nfsrv: info %x next %x ptr %x\n",
				q->q_nfsrv->q_qinfo, q->q_nfsrv->q_next,
				q->q_nfsrv->q_ptr);
		if (q->q_nbsrv)
			cmn_err(CE_CONT,
				"!IP Filter: nbsrv: info %x next %x ptr %x\n",
				q->q_nbsrv->q_qinfo, q->q_nbsrv->q_next,
				q->q_nbsrv->q_ptr);
		frstats[1].fr_drop++;
		mb->b_prev = NULL;
		freemsg(mb);
		RWLOCK_EXIT(&ipf_solaris);
		return 0;
	}

	qif->qf_outcnt++;
	pnext = qif->qf_wqinfo->qi_putp;
	if (type == M_IOCACK)
		fr_qif_update(qif, mb);
	bcopy((char *)qif, (char *)&qf, sizeof(qf));
	if (datamsg(type) || (type == M_BREAK))
		err = fr_precheck(&mb, q, &qf, 1);

	RWLOCK_EXIT(&ipfs_mutex);

	if ((err == 0) && (mb != NULL)) {
		if (pnext) {
			RWLOCK_EXIT(&ipf_solaris);
			return (*pnext)(q, mb);
		}

		cmn_err(CE_WARN,
			"!IP Filter: outp NULL: qif %x %s q %x info %x",
			qif, qf.qf_name, q, q->q_qinfo);
	}

	if (err == -2) {
		if (synced == 0) {
			ipfsync();
			synced = 1;
			goto again;
		}
		frstats[1].fr_notip++;
		if (!(fr_flags & FF_BLOCKNONIP) && (pnext != NULL)) {
			RWLOCK_EXIT(&ipf_solaris);
			return (*pnext)(q, mb);
		}
	}

	if (mb) {
		mb->b_prev = NULL;
		freemsg(mb);
	}
	RWLOCK_EXIT(&ipf_solaris);
	return 0;
}


void ipf_synctimeout(arg)
void *arg;
{
	if (fr_running < 0)
		return;
	READ_ENTER(&ipf_solaris);
	ipfsync();
	WRITE_ENTER(&ipfs_mutex);
	synctimeoutid = 0;
	RWLOCK_EXIT(&ipfs_mutex);
	RWLOCK_EXIT(&ipf_solaris);
}


static int ipf_ip_qin(q, mb)
queue_t *q;
mblk_t *mb;
{
	struct iocblk *ioc;
	int ret;

	if (fr_running <= 0) {
		mb->b_prev = NULL;
		freemsg(mb);
		return 0;
	}

	if (MTYPE(mb) != M_IOCTL)
		return (*ipf_ip_inp)(q, mb);

	READ_ENTER(&ipf_solaris);
	if (fr_running <= 0) {
		RWLOCK_EXIT(&ipf_solaris);
		mb->b_prev = NULL;
		freemsg(mb);
		return 0;
	}
	ioc = (struct iocblk *)mb->b_rptr;

	switch (ioc->ioc_cmd)
	{
	case DL_IOC_HDR_INFO:
		fr_qif_update(qif_from_queue(q), mb);
		break;
	case I_LINK:
	case I_UNLINK:
	case SIOCSIFADDR:
	case SIOCSIFFLAGS:
#ifdef	IPFDEBUG
		if (ipf_debug)
			cmn_err(CE_NOTE,
				"IP Filter: ipf_ip_qin() M_IOCTL type=0x%x",
				ioc->ioc_cmd);
#endif
		WRITE_ENTER(&ipfs_mutex);
		if (synctimeoutid == 0) {
			synctimeoutid = timeout(ipf_synctimeout,
						NULL,
						drv_usectohz(1000000) /*1 sec*/
					);
		}
		RWLOCK_EXIT(&ipfs_mutex);
		break;
	default:
		break;
	}
	RWLOCK_EXIT(&ipf_solaris);
	return (*ipf_ip_inp)(q, mb);
}

static int ipdrvattcnt = 0;
extern struct streamtab ipinfo;

void solipdrvattach()
{
#ifdef	IPFDEBUG
	if (ipf_debug)
		cmn_err(CE_NOTE, "IP Filter: solipdrvattach() %d ipinfo=0x%lx",
			ipdrvattcnt, &ipinfo);
#endif

	if (++ipdrvattcnt == 1) {
		if (ipf_ip_inp == NULL) {
			ipf_ip_inp = ipinfo.st_wrinit->qi_putp;
			ipinfo.st_wrinit->qi_putp = ipf_ip_qin;
		}
	}
}

int solipdrvdetach()
{
#ifdef	IPFDEBUG
	if (ipf_debug)
		cmn_err(CE_NOTE, "IP Filter: solipdrvdetach() %d ipinfo=0x%lx",
			ipdrvattcnt, &ipinfo);
#endif

	WRITE_ENTER(&ipfs_mutex);
	if (--ipdrvattcnt <= 0) {
		if (ipf_ip_inp && (ipinfo.st_wrinit->qi_putp == ipf_ip_qin)) {
			ipinfo.st_wrinit->qi_putp = ipf_ip_inp;
			ipf_ip_inp = NULL;
		}
		if (synctimeoutid) {
			untimeout(synctimeoutid);
			synctimeoutid = 0;
		}
	}
	RWLOCK_EXIT(&ipfs_mutex);
	return ipdrvattcnt;
}

/*
 * attach the packet filter to each interface that is defined as having an
 * IP address associated with it and save some of the info. for that struct
 * so we're not out of date as soon as the ill disappears - but we must sync
 * to be correct!
 */
void solattach()
{
	queue_t *in, *out;
	struct frentry *f;
	qif_t *qif, *qf2;
	ipnat_t *np;
	size_t len;
	ill_t *il;

	for (il = ill_g_head; il; il = il->ill_next) {
		in = il->ill_rq;
		if (!in || !il->ill_wq)
			continue;

		out = il->ill_wq->q_next;

		WRITE_ENTER(&ipfs_mutex);
		/*
		 * Look for entry already setup for this device
		 */
		for (qif = qif_head; qif; qif = qif->qf_next)
			if (qif->qf_iptr == in->q_ptr &&
			    qif->qf_optr == out->q_ptr)
				break;
		if (qif) {
			RWLOCK_EXIT(&ipfs_mutex);
			continue;
		}
#ifdef	IPFDEBUGX
		if (ipf_debug)
		cmn_err(CE_NOTE,
			"IP Filter: il %x ipt %x opt %x ipu %x opu %x i %x/%x",
			il, in->q_ptr,  out->q_ptr, in->q_qinfo->qi_putp,
			out->q_qinfo->qi_putp, out->q_qinfo, in->q_qinfo);
#endif
		KMALLOC(qif, qif_t *);
		if (!qif) {
			cmn_err(CE_WARN,
				"IP Filter: malloc(%d) for qif_t failed",
				sizeof(qif_t));
			RWLOCK_EXIT(&ipfs_mutex);
			continue;
		}

		if (in->q_qinfo->qi_putp == fr_qin) {
			for (qf2 = qif_head; qf2; qf2 = qf2->qf_next)
				if (&qf2->qf_rqinit == in->q_qinfo) {
					qif->qf_rqinfo = qf2->qf_rqinfo;
					break;
				}
			if (!qf2) {
#ifdef	IPFDEBUGX
				if (ipf_debug)
				cmn_err(CE_WARN,
					"IP Filter: rq:%s put %x qi %x",
					il->ill_name, in->q_qinfo->qi_putp,
					in->q_qinfo);
#endif
				RWLOCK_EXIT(&ipfs_mutex);
				KFREE(qif);
				continue;
			}
		} else
			qif->qf_rqinfo = in->q_qinfo;

		if (out->q_qinfo->qi_putp == fr_qout) {
			for (qf2 = qif_head; qf2; qf2 = qf2->qf_next)
				if (&qf2->qf_wqinit == out->q_qinfo) {
					qif->qf_wqinfo = qf2->qf_wqinfo;
					break;
				}
			if (!qf2) {
#ifdef	IPFDEBUGX
				if (ipf_debug)
				cmn_err(CE_WARN,
					"IP Filter: wq:%s put %x qi %x",
					il->ill_name, out->q_qinfo->qi_putp,
					out->q_qinfo);
#endif
				RWLOCK_EXIT(&ipfs_mutex);
				KFREE(qif);
				continue;
			}
		} else
			qif->qf_wqinfo = out->q_qinfo;

		qif->qf_ill = il;
		qif->qf_in = in;
		qif->qf_out = out;
		qif->qf_iptr = in->q_ptr;
		qif->qf_optr = out->q_ptr;
#if SOLARIS2 < 8
		qif->qf_hl = il->ill_hdr_length;
#else
		{
		ire_t *ire;
		mblk_t *m;

		qif->qf_hl = 0;
		qif->qf_sap = il->ill_sap;
# if 0
		/*
		 * Can't seem to lookup a route for the IP address on the
		 * interface itself.
		 */
		ire = ire_route_lookup(il->ill_ipif->ipif_lcl_addr, 0xffffffff,
					0, 0, NULL, NULL, NULL,
					MATCH_IRE_DSTONLY|MATCH_IRE_RECURSIVE);
		if ((ire != NULL) && (m = ire->ire_fp_mp))
			qif->qf_hl = m->b_wptr - m->b_rptr;
# endif
		if ((qif->qf_hl == 0) && (il->ill_type > 0) &&
		    (il->ill_type < 0x37) &&
		    (hdrsizes[il->ill_type][0] == il->ill_type))
			qif->qf_hl = hdrsizes[il->ill_type][1];

		/* DREADFUL VLAN HACK - JUST HERE TO CHECK IT WORKS */
		if (il->ill_type == IFT_ETHER &&
		    il->ill_name[0] == 'c' && il->ill_name[1] == 'e' &&
		    isdigit(il->ill_name[2]) && il->ill_name_length >= 6) {
			cmn_err(CE_NOTE, "VLAN HACK ENABLED");
			qif->qf_hl += 4;
		}
		/* DREADFUL VLAN HACK - JUST HERE TO CHECK IT WORKS */

		if (qif->qf_hl == 0 && il->ill_type != IFT_OTHER)
			cmn_err(CE_WARN,
				"Unknown layer 2 header size for %s type %d",
				il->ill_name, il->ill_type);
		}

		/*
		 * XXX Awful hack for PPP; fix when PPP/snoop fixed.
		 */
		if (il->ill_type == IFT_ETHER && !il->ill_bcast_addr_length)
			qif->qf_hl = 0;
#endif
		strncpy(qif->qf_name, il->ill_name, sizeof(qif->qf_name));
		qif->qf_name[sizeof(qif->qf_name) - 1] = '\0';

		qif->qf_next = qif_head;
		qif_head = qif;

		/*
		 * Activate any rules directly associated with this interface
		 */
		WRITE_ENTER(&ipf_mutex);
		for (f = ipfilter[0][fr_active]; f; f = f->fr_next) {
			if ((f->fr_ifa == (struct ifnet *)-1)) {
				len = strlen(f->fr_ifname) + 1;
				if ((len != 0) &&
				    (len == (size_t)il->ill_name_length) &&
				    !strncmp(il->ill_name, f->fr_ifname, len))
					f->fr_ifa = il;
			}
		}
		for (f = ipfilter[1][fr_active]; f; f = f->fr_next) {
			if ((f->fr_ifa == (struct ifnet *)-1)) {
				len = strlen(f->fr_ifname) + 1;
				if ((len != 0) &&
				    (len == (size_t)il->ill_name_length) &&
				    !strncmp(il->ill_name, f->fr_ifname, len))
					f->fr_ifa = il;
			}
		}
#if SOLARIS2 >= 8
		for (f = ipfilter6[0][fr_active]; f; f = f->fr_next) {
			if ((f->fr_ifa == (struct ifnet *)-1)) {
				len = strlen(f->fr_ifname) + 1;
				if ((len != 0) &&
				    (len == (size_t)il->ill_name_length) &&
				    !strncmp(il->ill_name, f->fr_ifname, len))
					f->fr_ifa = il;
			}
		}
		for (f = ipfilter6[1][fr_active]; f; f = f->fr_next) {
			if ((f->fr_ifa == (struct ifnet *)-1)) {
				len = strlen(f->fr_ifname) + 1;
				if ((len != 0) &&
				    (len == (size_t)il->ill_name_length) &&
				    !strncmp(il->ill_name, f->fr_ifname, len))
					f->fr_ifa = il;
			}
		}
#endif
		RWLOCK_EXIT(&ipf_mutex);
		WRITE_ENTER(&ipf_nat);
		for (np = nat_list; np; np = np->in_next) {
			if ((np->in_ifp == (struct ifnet *)-1)) {
				len = strlen(np->in_ifname) + 1;
				if ((len != 0) &&
				    (len == (size_t)il->ill_name_length) &&
				    !strncmp(il->ill_name, np->in_ifname, len))
					np->in_ifp = il;
			}
		}
		RWLOCK_EXIT(&ipf_nat);

		bcopy((caddr_t)qif->qf_rqinfo, (caddr_t)&qif->qf_rqinit,
		      sizeof(struct qinit));
		qif->qf_rqinit.qi_putp = fr_qin;
#ifdef	IPFDEBUG
		if (ipf_debug)
			cmn_err(CE_NOTE,
				"IP Filter: solattach: in queue(%lx)->q_qinfo FROM %lx TO %lx",
				in, in->q_qinfo, &qif->qf_rqinit);
#endif
		in->q_qinfo = &qif->qf_rqinit;

		bcopy((caddr_t)qif->qf_wqinfo, (caddr_t)&qif->qf_wqinit,
		      sizeof(struct qinit));
		qif->qf_wqinit.qi_putp = fr_qout;
#ifdef	IPFDEBUG
		if (ipf_debug)
			cmn_err(CE_NOTE,
				"IP Filter: solattach: out queue(%lx)->q_qinfo FROM %lx TO %lx",
				out, out->q_qinfo, &qif->qf_wqinit);
#endif
		out->q_qinfo = &qif->qf_wqinit;

		ire_walk(ipf_ire_walk, (char *)qif);
		RWLOCK_EXIT(&ipfs_mutex);
		cmn_err(CE_CONT, "IP Filter: attach to [%s,%d] - %s\n",
			qif->qf_name, il->ill_ppa,
#if SOLARIS2 >= 8
			il->ill_isv6 ? "IPv6" : "IPv4"
#else
			"IPv4"
#endif
			);
	}
	if (!qif_head)
		cmn_err(CE_CONT, "IP Filter: not attached to any interfaces\n");
	return;
}


/*
 * look for bad consistancies between the list of interfaces the filter knows
 * about and those which are currently configured.
 */
int ipfsync()
{
	register struct frentry *f;
	register ipnat_t *np;
	register qif_t *qif, **qp;
	register ill_t *il;
	queue_t *in, *out;

	WRITE_ENTER(&ipfs_mutex);
	for (qp = &qif_head; (qif = *qp); ) {
		for (il = ill_g_head; il; il = il->ill_next)
			if ((qif->qf_ill == il) &&
			    !strcmp(qif->qf_name, il->ill_name)) {
#if SOLARIS2 < 8
				mblk_t	*m = il->ill_hdr_mp;

				qif->qf_hl = il->ill_hdr_length;
				if (m && qif->qf_hl != (m->b_wptr - m->b_rptr))
					cmn_err(CE_NOTE,
						"IP Filter: ILL Header Length Mismatch\n");
#endif
				break;
			}
		if (il) {
			qp = &qif->qf_next;
			continue;
		}
		cmn_err(CE_CONT, "IP Filter: detaching [%s] - %s\n",
			qif->qf_name,
#if SOLARIS2 >= 8
			(qif->qf_sap == IP6_DL_SAP) ? "IPv6" : "IPv4"
#else
			"IPv4"
#endif
			);
		*qp = qif->qf_next;

		/*
		 * Disable any rules directly associated with this interface
		 */
		WRITE_ENTER(&ipf_nat);
		for (np = nat_list; np; np = np->in_next)
			if (np->in_ifp == (void *)qif->qf_ill)
				np->in_ifp = (struct ifnet *)-1;
		RWLOCK_EXIT(&ipf_nat);
		WRITE_ENTER(&ipf_mutex);
		for (f = ipfilter[0][fr_active]; f; f = f->fr_next)
			if (f->fr_ifa == (void *)qif->qf_ill)
				f->fr_ifa = (struct ifnet *)-1;
		for (f = ipfilter[1][fr_active]; f; f = f->fr_next)
			if (f->fr_ifa == (void *)qif->qf_ill)
				f->fr_ifa = (struct ifnet *)-1;
#if SOLARIS2 >= 8
		for (f = ipfilter6[0][fr_active]; f; f = f->fr_next)
			if (f->fr_ifa == (void *)qif->qf_ill)
				f->fr_ifa = (struct ifnet *)-1;
		for (f = ipfilter6[1][fr_active]; f; f = f->fr_next)
			if (f->fr_ifa == (void *)qif->qf_ill)
				f->fr_ifa = (struct ifnet *)-1;
#endif

#if 0 /* XXX */
		/*
		 * As well as the ill disappearing when a device is unplumb'd,
		 * it also appears that the associated queue structures also
		 * disappear - at least in the case of ppp, which is the most
		 * volatile here.  Thanks to Greg for finding this problem.
		 */
		/*
		 * Restore q_qinfo pointers in interface queues
		 */
		out = qif->qf_out;
		in = qif->qf_in;
		if (in) {
# ifdef	IPFDEBUG
			if (ipf_debug)
				cmn_err(CE_NOTE,
					"IP Filter: ipfsync: in queue(%lx)->q_qinfo FROM %lx TO %lx",
					in, in->q_qinfo, qif->qf_rqinfo);
# endif
			in->q_qinfo = qif->qf_rqinfo;
		}
		if (out) {
# ifdef	IPFDEBUG
			if (ipf_debug)
				cmn_err(CE_NOTE,
					"IP Filter: ipfsync: out queue(%lx)->q_qinfo FROM %lx TO %lx",
					out, out->q_qinfo, qif->qf_wqinfo);
# endif
			out->q_qinfo = qif->qf_wqinfo;
		}
#endif /* XXX */
		RWLOCK_EXIT(&ipf_mutex);
		KFREE(qif);
		qif = *qp;
	}
	RWLOCK_EXIT(&ipfs_mutex);
	solattach();

	frsync();
	/*
	 * Resync. any NAT `connections' using this interface and its IP #.
	 */
	for (il = ill_g_head; il; il = il->ill_next) {
		ip_natsync((void *)il);
		ip_statesync((void *)il);
	}
	return 0;
}


/*
 * unhook the IP filter from all defined interfaces with IP addresses
 */
int soldetach()
{
	queue_t *in, *out;
	qif_t *qif, **qp;
	ill_t *il;

	WRITE_ENTER(&ipfs_mutex);
	/*
	 * Make two passes, first get rid of all the unknown devices, next
	 * unlink known devices.
	 */
	for (qp = &qif_head; (qif = *qp); ) {
		for (il = ill_g_head; il; il = il->ill_next)
			if (qif->qf_ill == il)
				break;
		if (il) {
			qp = &qif->qf_next;
			continue;
		}
		cmn_err(CE_CONT, "IP Filter: removing [%s]\n", qif->qf_name);
		*qp = qif->qf_next;
		KFREE(qif);
	}

	while ((qif = qif_head)) {
		qif_head = qif->qf_next;
		for (il = ill_g_head; il; il = il->ill_next)
			if (qif->qf_ill == il)
				break;
		if (il) {
			in = qif->qf_in;
			out = qif->qf_out;
			cmn_err(CE_CONT, "IP Filter: detaching [%s,%d] - %s\n",
				qif->qf_name, il->ill_ppa,
#if SOLARIS2 >= 8
			(qif->qf_sap == IP6_DL_SAP) ? "IPv6" : "IPv4"
#else
			"IPv4"
#endif
			);

#ifdef	IPFDEBUG
			if (ipf_debug)
				cmn_err(CE_NOTE,
					"IP Filter: soldetach: in queue(%lx)->q_qinfo FROM %lx TO %lx",
					in, in->q_qinfo, qif->qf_rqinfo);
#endif
			in->q_qinfo = qif->qf_rqinfo;

			/*
			 * and the write queue...
			 */
#ifdef	IPFDEBUG
			if (ipf_debug)
				cmn_err(CE_NOTE,
					"IP Filter: soldetach: out queue(%lx)->q_qinfo FROM %lx TO %lx",
					out, out->q_qinfo, qif->qf_wqinfo);
#endif
			out->q_qinfo = qif->qf_wqinfo;
		}
		KFREE(qif);
	}
	RWLOCK_EXIT(&ipfs_mutex);
	return ipldetach();
}


#ifdef	IPFDEBUG
void printire(ire)
ire_t *ire;
{
	if (!ipf_debug)
		return;
	printf("ire: ll_hdr_mp %p rfq %p stq %p src_addr %x max_frag %d\n",
# if SOLARIS2 >= 8
		NULL,
# else
		ire->ire_ll_hdr_mp,
# endif
		ire->ire_rfq, ire->ire_stq,
		ire->ire_src_addr, ire->ire_max_frag);
	printf("ire: mask %x addr %x gateway_addr %x type %d\n",
		ire->ire_mask, ire->ire_addr, ire->ire_gateway_addr,
		ire->ire_type);
	printf("ire: ll_hdr_length %d ll_hdr_saved_mp %p\n",
		ire->ire_ll_hdr_length,
# if SOLARIS2 >= 8
		NULL
# else
		ire->ire_ll_hdr_saved_mp
# endif
		);
}
#endif


int ipfr_fastroute(ip, mb, mpp, fin, fdp)
ip_t *ip;
mblk_t *mb, **mpp;
fr_info_t *fin;
frdest_t *fdp;
{
#ifdef	USE_INET6
	ip6_t *ip6 = (ip6_t *)ip;
#endif
	ire_t *ir, *dir, *gw;
	struct in_addr dst;
	queue_t *q = NULL;
	mblk_t *mp = NULL;
	size_t hlen = 0;
	frentry_t *fr;
	frdest_t fd;
	ill_t *ifp;
	u_char *s;
	qif_t *qf;
	int p;

#ifndef	sparc
	u_short __iplen, __ipoff;
#endif
	qf = fin->fin_qif;

	/*
	 * If this is a duplicate mblk then we want ip to point at that
	 * data, not the original, if and only if it is already pointing at
	 * the current mblk data.
	 */
	if ((ip == (ip_t *)qf->qf_m->b_rptr) && (qf->qf_m != mb))
		ip = (ip_t *)mb->b_rptr;

	/*
	 * If there is another M_PROTO, we don't want it
	 */
	if (*mpp != mb) {
		mp = *mpp;
		(void) unlinkb(mp);
		mp = (*mpp)->b_cont;
		(*mpp)->b_cont = NULL;
		(*mpp)->b_prev = NULL;
		freemsg(*mpp);
		*mpp = mp;
	}

	if (!fdp) {
		ipif_t *ipif;

		ifp = fin->fin_ifp;
		ipif = ifp->ill_ipif;
		if (!ipif)
			goto bad_fastroute;
#if SOLARIS2 > 5
		ir = ire_ctable_lookup(ipif->ipif_local_addr, 0, IRE_LOCAL,
				       NULL, NULL, MATCH_IRE_TYPE);
#else
		ir = ire_lookup_myaddr(ipif->ipif_local_addr);
#endif
		if (!ir)
			ir = (ire_t *)-1;

		fd.fd_ifp = (struct ifnet *)ir;
		fd.fd_ip = ip->ip_dst;
		fdp = &fd;
	}

	ir = (ire_t *)fdp->fd_ifp;

	if (fdp->fd_ip.s_addr)
		dst = fdp->fd_ip;
	else
		dst.s_addr = fin->fin_fi.fi_daddr;

#if SOLARIS2 >= 6
	gw = NULL;
	if (fin->fin_v == 4) {
		p = ip->ip_p;
		dir = ire_route_lookup(dst.s_addr, 0xffffffff, 0, 0, NULL,
					&gw, NULL, MATCH_IRE_DSTONLY|
					MATCH_IRE_DEFAULT|MATCH_IRE_RECURSIVE);
	}
# ifdef	USE_INET6
	else if (fin->fin_v == 6) {
		p = ip6->ip6_nxt;
		dir = ire_route_lookup_v6(&ip6->ip6_dst, NULL, 0, 0,
					NULL, &gw, NULL, MATCH_IRE_DSTONLY|
					MATCH_IRE_DEFAULT|MATCH_IRE_RECURSIVE);
	}
# endif
#else
	dir = ire_lookup(dst.s_addr);
#endif
#if SOLARIS2 < 8
	if (dir)
		if (!dir->ire_ll_hdr_mp || !dir->ire_ll_hdr_length)
			dir = NULL;
#else
	if (dir)
		if (!dir->ire_fp_mp || !dir->ire_dlureq_mp)
			dir = NULL;
#endif

	if (!ir)
		ir = dir;

	if (ir && dir) {
		ifp = ire_to_ill(ir);
		if (ifp == NULL)
			goto bad_fastroute;
		fr = fin->fin_fr;

		/*
		 * In case we're here due to "to <if>" being used with
		 * "keep state", check that we're going in the correct
		 * direction.
		 */
		if ((fr != NULL) && (fdp->fd_ifp != NULL) &&
		    (fin->fin_rev != 0) && (fdp == &fr->fr_tif))
			return 1;

		fin->fin_ifp = ifp;
		if (fin->fin_out == 0) {
			fin->fin_fr = ipacct[1][fr_active];
			if ((fin->fin_fr != NULL) &&
			    (fr_scanlist(FR_NOMATCH, ip, fin, mb)&FR_ACCOUNT)){
				ATOMIC_INCL(frstats[1].fr_acct);
			}
			fin->fin_fr = NULL;
			if (!fr || !(fr->fr_flags & FR_RETMASK))
				(void) fr_checkstate(ip, fin);
			(void) ip_natout(ip, fin);
		}
#ifndef	sparc
		if (fin->fin_v == 4) {
			__iplen = (u_short)ip->ip_len,
			__ipoff = (u_short)ip->ip_off;

			ip->ip_len = htons(__iplen);
			ip->ip_off = htons(__ipoff);
		}
#endif

#if SOLARIS2 < 8
		mp = dir->ire_ll_hdr_mp;
		hlen = dir->ire_ll_hdr_length;
#else
		mp = dir->ire_fp_mp;
		hlen = mp ? mp->b_wptr - mp->b_rptr : 0;
		mp = dir->ire_dlureq_mp;
#endif
		if (mp != NULL) {
			s = mb->b_rptr;
			if (
#if SOLARIS2 >= 6
			    (dohwcksum &&
			     ifp->ill_ick.ick_magic == ICK_M_CTL_MAGIC) ||
#endif
			    (hlen && (s - mb->b_datap->db_base) >= hlen)) {
				s -= hlen;
				mb->b_rptr = (u_char *)s;
				bcopy((char *)mp->b_rptr, (char *)s, hlen);
			} else {
				mblk_t	*mp2;

				mp2 = copyb(mp);
				if (!mp2)
					goto bad_fastroute;
				linkb(mp2, mb);
				mb = mp2;
			}
		}
		*mpp = mb;

		if (ir->ire_stq)
			q = ir->ire_stq;
		else if (ir->ire_rfq)
			q = WR(ir->ire_rfq);
		if (q) {
			mb->b_prev = NULL;
			mb->b_queue = q;
			RWLOCK_EXIT(&ipfs_mutex);
			RWLOCK_EXIT(&ipf_solaris);
#if SOLARIS2 >= 6
			if ((p == IPPROTO_TCP) && dohwcksum &&
			    (ifp->ill_ick.ick_magic == ICK_M_CTL_MAGIC)) {
				tcphdr_t *tcp;
				u_32_t t;

				tcp = (tcphdr_t *)((char *)ip + fin->fin_hlen);
				t = ip->ip_src.s_addr;
				t += ip->ip_dst.s_addr;
				t += 30;
				t = (t & 0xffff) + (t >> 16);
				tcp->th_sum = t & 0xffff;
			}
#endif
			putnext(q, mb);
			READ_ENTER(&ipf_solaris);
			READ_ENTER(&ipfs_mutex);
			ipl_frouteok[0]++;
			*mpp = NULL;
			return 0;
		}
	}
bad_fastroute:
	mb->b_prev = NULL;
	freemsg(mb);
	ipl_frouteok[1]++;
	*mpp = NULL;
	return -1;
}


void copyout_mblk(m, off, len, buf)
mblk_t *m;
size_t off, len;
char *buf;
{
	u_char *s, *bp = (u_char *)buf;
	size_t mlen, olen, clen;

	for (; m && len; m = m->b_cont) {
		if (MTYPE(m) != M_DATA)
			continue;
		s = m->b_rptr;
		mlen = m->b_wptr - s;
		olen = MIN(off, mlen);
		if ((olen == mlen) || (olen < off)) {
			off -= olen;
			continue;
		} else if (olen) {
			off -= olen;
			s += olen;
			mlen -= olen;
		}
		clen = MIN(mlen, len);
		bcopy(s, bp, clen);
		len -= clen;
		bp += clen;
	}
}


void copyin_mblk(m, off, len, buf)
mblk_t *m;
size_t off, len;
char *buf;
{
	u_char *s, *bp = (u_char *)buf;
	size_t mlen, olen, clen;

	for (; m && len; m = m->b_cont) {
		if (MTYPE(m) != M_DATA)
			continue;
		s = m->b_rptr;
		mlen = m->b_wptr - s;
		olen = MIN(off, mlen);
		if ((olen == mlen) || (olen < off)) {
			off -= olen;
			continue;
		} else if (olen) {
			off -= olen;
			s += olen;
			mlen -= olen;
		}
		clen = MIN(mlen, len);
		bcopy(bp, s, clen);
		len -= clen;
		bp += clen;
	}
}


int fr_verifysrc(ipa, ifp)
struct in_addr ipa;
void *ifp;
{
	ire_t *ir, *dir, *gw;

#if SOLARIS2 >= 6
	dir = ire_route_lookup(ipa.s_addr, 0xffffffff, 0, 0, NULL, &gw, NULL,
				MATCH_IRE_DSTONLY|MATCH_IRE_DEFAULT|
				MATCH_IRE_RECURSIVE);
#else
	dir = ire_lookup(ipa.s_addr);
#endif

	if (!dir)
		return 0;
	return (ire_to_ill(dir) == ifp);
}
