/*
 * Fundamental constants relating to ethernet.
 *
 * $Id: ethernet.h,v 1.4 1996/12/19 15:42:52 wollman Exp $
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

#ifndef KERNEL
#include <sys/cdefs.h>

/*
 * Ethernet address conversion/parsing routines.
 */
__BEGIN_DECLS
struct	ether_addr *ether_aton __P((char *));
int	ether_hostton __P((char *, struct ether_addr *));
int	ether_line __P((char *, struct ether_addr *, char *));
char 	*ether_ntoa __P((struct ether_addr *));
int	ether_ntohost __P((char *, struct ether_addr *));
__END_DECLS
#endif /* !KERNEL */

#endif /* !_NET_ETHERNET_H_ */
