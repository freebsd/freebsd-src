/*
 * Fundamental constants relating to ethernet.
 *
 * $FreeBSD$
 *
 */

#ifndef _NET_ETHERNET_H_
#define _NET_ETHERNET_H_

/*
 * The number of bytes in an ethernet (MAC) address.
 */
#define	ETHER_ADDR_LEN		6

/*
 * The number of bytes in the type field.
 */
#define	ETHER_TYPE_LEN		2

/*
 * The number of bytes in the trailing CRC field.
 */
#define	ETHER_CRC_LEN		4

/*
 * The length of the combined header.
 */
#define	ETHER_HDR_LEN		(ETHER_ADDR_LEN*2+ETHER_TYPE_LEN)

/*
 * The minimum packet length.
 */
#define	ETHER_MIN_LEN		64

/*
 * The maximum packet length.
 */
#define	ETHER_MAX_LEN		1518

/*
 * A macro to validate a length with
 */
#define	ETHER_IS_VALID_LEN(foo)	\
	((foo) >= ETHER_MIN_LEN && (foo) <= ETHER_MAX_LEN)

/*
 * Structure of a 10Mb/s Ethernet header.
 */
struct	ether_header {
	u_char	ether_dhost[ETHER_ADDR_LEN];
	u_char	ether_shost[ETHER_ADDR_LEN];
	u_short	ether_type;
};

/*
 * Structure of a 48-bit Ethernet address.
 */
struct	ether_addr {
	u_char octet[ETHER_ADDR_LEN];
};

#define	ETHERTYPE_PUP		0x0200	/* PUP protocol */
#define	ETHERTYPE_IP		0x0800	/* IP protocol */
#define	ETHERTYPE_ARP		0x0806	/* Addr. resolution protocol */
#define	ETHERTYPE_REVARP	0x8035	/* reverse Addr. resolution protocol */
#define	ETHERTYPE_VLAN		0x8100	/* IEEE 802.1Q VLAN tagging */
#define	ETHERTYPE_IPV6		0x86dd	/* IPv6 */
#define	ETHERTYPE_LOOPBACK	0x9000	/* used to test interfaces */
/* XXX - add more useful types here */

/*
 * The ETHERTYPE_NTRAILER packet types starting at ETHERTYPE_TRAIL have
 * (type-ETHERTYPE_TRAIL)*512 bytes of data followed
 * by an ETHER type (as given above) and then the (variable-length) header.
 */
#define	ETHERTYPE_TRAIL		0x1000		/* Trailer packet */
#define	ETHERTYPE_NTRAILER	16

#define	ETHERMTU	(ETHER_MAX_LEN-ETHER_HDR_LEN-ETHER_CRC_LEN)
#define	ETHERMIN	(ETHER_MIN_LEN-ETHER_HDR_LEN-ETHER_CRC_LEN)

#ifdef _KERNEL

/*
 * For device drivers to specify whether they support BPF or not
 */
#define ETHER_BPF_UNSUPPORTED	0
#define ETHER_BPF_SUPPORTED	1

struct ifnet;
struct mbuf;

extern	void (*ng_ether_input_p)(struct ifnet *ifp,
		struct mbuf **mp, struct ether_header *eh);
extern	void (*ng_ether_input_orphan_p)(struct ifnet *ifp,
		struct mbuf *m, struct ether_header *eh);
extern	int  (*ng_ether_output_p)(struct ifnet *ifp, struct mbuf **mp);
extern	void (*ng_ether_attach_p)(struct ifnet *ifp);
extern	void (*ng_ether_detach_p)(struct ifnet *ifp);

extern	int (*vlan_input_p)(struct ether_header *eh, struct mbuf *m);
extern	int (*vlan_input_tag_p)(struct ether_header *eh, struct mbuf *m,
		u_int16_t t);

#define	VLAN_INPUT_TAG(eh, m, t) do {			\
	/* XXX: lock */					\
	if (vlan_input_tag_p != NULL) 			\
		(*vlan_input_tag_p)(eh, m, t);		\
	else {						\
		(m)->m_pkthdr.rcvif->if_noproto++;	\
		m_freem(m);				\
	}						\
	/* XXX: unlock */				\
} while (0)

#else /* _KERNEL */

#include <sys/cdefs.h>

/*
 * Ethernet address conversion/parsing routines.
 */
__BEGIN_DECLS
struct	ether_addr *ether_aton(const char *);
int	ether_hostton(const char *, struct ether_addr *);
int	ether_line(const char *, struct ether_addr *, char *);
char 	*ether_ntoa(const struct ether_addr *);
int	ether_ntohost(char *, const struct ether_addr *);
__END_DECLS

#endif /* !_KERNEL */

#endif /* !_NET_ETHERNET_H_ */
