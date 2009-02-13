/*
 * arplib.h (C)opyright 1992 Darren Reed.
 */

#define MAXPACKET	1024
#define MAXALIASES	35
#define MAXADDRS	35

#define	RES_CHECKPTR	0x0400

struct	hent {
	char	*h_name;	/* official name of host */
	char	*h_aliases[MAXALIASES];	/* alias list */
	int	h_addrtype;	/* host address type */
	int	h_length;	/* length of address */
	/* list of addresses from name server */
	struct	in_addr	h_addr_list[MAXADDRS];
#define	h_addr	h_addr_list[0]	/* address, for backward compatiblity */
};

struct	resinfo {
	char	*ri_ptr;
	int	ri_size;
};

struct	reslist {
	int	re_id;
	char	re_type;
	char	re_retries;
	char	re_resend;	/* send flag. 0 == dont resend */
	char	re_sends;
	char	re_srch;
	int	re_sent;
	u_long	re_sentat;
	u_long	re_timeout;
	struct	in_addr	re_addr;
	struct	resinfo	re_rinfo;
	struct	hent re_he;
	struct	reslist	*re_next, *re_prev;
	char	re_name[65];
};

#ifndef	MIN
#define	MIN(a,b)	((a) > (b) ? (b) : (a))
#endif
