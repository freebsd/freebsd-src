/*-
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/net/if_vlan_var.h,v 1.26 2007/02/28 22:05:30 bms Exp $
 */

#ifndef _NET_IF_VLAN_VAR_H_
#define	_NET_IF_VLAN_VAR_H_	1

struct	ether_vlan_header {
	u_char	evl_dhost[ETHER_ADDR_LEN];
	u_char	evl_shost[ETHER_ADDR_LEN];
	u_int16_t evl_encap_proto;
	u_int16_t evl_tag;
	u_int16_t evl_proto;
};

#define	EVL_VLID_MASK		0x0FFF
#define	EVL_PRI_MASK		0xE000
#define	EVL_VLANOFTAG(tag)	((tag) & EVL_VLID_MASK)
#define	EVL_PRIOFTAG(tag)	(((tag) >> 13) & 7)
#define	EVL_CFIOFTAG(tag)	(((tag) >> 12) & 1)
#define	EVL_MAKETAG(vlid, pri, cfi)					\
	((((((pri) & 7) << 1) | ((cfi) & 1)) << 12) | ((vlid) & EVL_VLID_MASK))

/* Set the VLAN ID in an mbuf packet header non-destructively. */
#define EVL_APPLY_VLID(m, vlid)						\
	do {								\
		if ((m)->m_flags & M_VLANTAG) {				\
			(m)->m_pkthdr.ether_vtag &= EVL_VLID_MASK;	\
			(m)->m_pkthdr.ether_vtag |= (vlid);		\
		} else {						\
			(m)->m_pkthdr.ether_vtag = (vlid);		\
			(m)->m_flags |= M_VLANTAG;			\
		}							\
	} while (0)

/* Set the priority ID in an mbuf packet header non-destructively. */
#define EVL_APPLY_PRI(m, pri)						\
	do {								\
		if ((m)->m_flags & M_VLANTAG) {				\
			uint16_t __vlantag = (m)->m_pkthdr.ether_vtag;	\
			(m)->m_pkthdr.ether_vtag |= EVL_MAKETAG(	\
			    EVL_VLANOFTAG(__vlantag), (pri),		\
			    EVL_CFIOFTAG(__vlantag));			\
		} else {						\
			(m)->m_pkthdr.ether_vtag =			\
			    EVL_MAKETAG(0, (pri), 0);			\
			(m)->m_flags |= M_VLANTAG;			\
		}							\
	} while (0)

/* sysctl(3) tags, for compatibility purposes */
#define	VLANCTL_PROTO	1
#define	VLANCTL_MAX	2

/*
 * Configuration structure for SIOCSETVLAN and SIOCGETVLAN ioctls.
 */
struct	vlanreq {
	char	vlr_parent[IFNAMSIZ];
	u_short	vlr_tag;
};
#define	SIOCSETVLAN	SIOCSIFGENERIC
#define	SIOCGETVLAN	SIOCGIFGENERIC

#ifdef _KERNEL
/*
 * Drivers that are capable of adding and removing the VLAN header
 * in hardware indicate they support this by marking IFCAP_VLAN_HWTAGGING
 * in if_capabilities.  Drivers for hardware that is capable
 * of handling larger MTU's that may include a software-appended
 * VLAN header w/o lowering the normal MTU should mark IFCAP_VLAN_MTU
 * in if_capabilities; this notifies the VLAN code it can leave the
 * MTU on the vlan interface at the normal setting.
 */

/*
 * VLAN tags are stored in host byte order.  Byte swapping may be
 * necessary.
 *
 * Drivers that support hardware VLAN tag stripping fill in the
 * received VLAN tag (containing both vlan and priority information)
 * into the ether_vtag mbuf packet header field:
 * 
 *	m->m_pkthdr.ether_vtag = vlan_id;	// ntohs()?
 *	m->m_flags |= M_VLANTAG;
 *
 * to mark the packet m with the specified VLAN tag.
 *
 * On output the driver should check the mbuf for the M_VLANTAG
 * flag to see if a VLAN tag is present and valid:
 *
 *	if (m->m_flags & M_VLANTAG) {
 *		... = m->m_pkthdr.ether_vtag;	// htons()?
 *		... pass tag to hardware ...
 *	}
 *
 * Note that a driver must indicate it supports hardware VLAN
 * stripping/insertion by marking IFCAP_VLAN_HWTAGGING in
 * if_capabilities.
 */

#define	VLAN_CAPABILITIES(_ifp) do {				\
	if ((_ifp)->if_vlantrunk != NULL) 			\
		(*vlan_trunk_cap_p)(_ifp);			\
} while (0)

extern	void (*vlan_trunk_cap_p)(struct ifnet *);
#endif /* _KERNEL */

#endif /* _NET_IF_VLAN_VAR_H_ */
