#ifndef _IP_CONNTRACK_AMANDA_H
#define _IP_CONNTRACK_AMANDA_H
/* AMANDA tracking. */

struct ip_ct_amanda_expect
{
	u_int16_t port;		/* port number of this expectation */
	u_int16_t offset;	/* offset of port in ctrl packet */
	u_int16_t len;		/* length of the port number string */
};

#endif /* _IP_CONNTRACK_AMANDA_H */
