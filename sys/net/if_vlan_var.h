/*
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
 * $FreeBSD: src/sys/net/if_vlan_var.h,v 1.19 2004/01/18 19:29:04 yar Exp $
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

#define EVL_VLID_MASK	0x0FFF
#define	EVL_VLANOFTAG(tag) ((tag) & EVL_VLID_MASK)
#define	EVL_PRIOFTAG(tag) (((tag) >> 13) & 7)

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
 * Drivers that support hardware VLAN tagging pass a packet's tag
 * up through the stack by appending a packet tag with this value.
 * Output is handled likewise, the driver must locate the packet
 * tag to extract the VLAN tag.  The following macros are used to
 * do this work.  On input, do:
 *
 *	VLAN_INPUT_TAG(ifp, m, tag,);
 *
 * to mark the packet m with the specified VLAN tag.  The last
 * parameter provides code to execute in case of an error.  On
 * output the driver should check ifnet to see if any VLANs are
 * in use and only then check for a packet tag; this is done with:
 *
 *	struct m_tag *mtag;
 *	mtag = VLAN_OUTPUT_TAG(ifp, m);
 *	if (mtag != NULL) {
 *		... = VLAN_TAG_VALUE(mtag);
 *		... pass tag to hardware ...
 *	}
 *
 * Note that a driver must indicate it supports hardware VLAN
 * tagging by marking IFCAP_VLAN_HWTAGGING in if_capabilities.
 */
#define	MTAG_VLAN	1035328035
#define	MTAG_VLAN_TAG	0		/* tag of VLAN interface */

#define	VLAN_INPUT_TAG(_ifp, _m, _t, _errcase) do {		\
	struct m_tag *mtag;					\
	mtag = m_tag_alloc(MTAG_VLAN, MTAG_VLAN_TAG,		\
			   sizeof (u_int), M_NOWAIT);		\
	if (mtag == NULL) {					\
		(_ifp)->if_ierrors++;				\
		m_freem(_m);					\
		_errcase;					\
	}							\
	*(u_int *)(mtag+1) = (_t);				\
	m_tag_prepend((_m), mtag);				\
} while (0)

#define	VLAN_OUTPUT_TAG(_ifp, _m)				\
	((_ifp)->if_nvlans != 0 ?				\
		m_tag_locate((_m), MTAG_VLAN, MTAG_VLAN_TAG, NULL) : NULL)
#define	VLAN_TAG_VALUE(_mt)	(*(u_int *)((_mt)+1))
#endif /* _KERNEL */

#endif /* _NET_IF_VLAN_VAR_H_ */
