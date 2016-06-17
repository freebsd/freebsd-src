/*
 * include/linux/sunrpc/xdr.h
 *
 * Copyright (C) 1995-1997 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef _SUNRPC_XDR_H_
#define _SUNRPC_XDR_H_

#ifdef __KERNEL__

#include <linux/uio.h>

/*
 * Buffer adjustment
 */
#define XDR_QUADLEN(l)		(((l) + 3) >> 2)

/*
 * Generic opaque `network object.' At the kernel level, this type
 * is used only by lockd.
 */
#define XDR_MAX_NETOBJ		1024
struct xdr_netobj {
	unsigned int		len;
	u8 *			data;
};

/*
 * This is the generic XDR function. rqstp is either a rpc_rqst (client
 * side) or svc_rqst pointer (server side).
 * Encode functions always assume there's enough room in the buffer.
 */
typedef int	(*kxdrproc_t)(void *rqstp, u32 *data, void *obj);

/*
 * Basic structure for transmission/reception of a client XDR message.
 * Features a header (for a linear buffer containing RPC headers
 * and the data payload for short messages), and then an array of
 * pages.
 * The tail iovec allows you to append data after the page array. Its
 * main interest is for appending padding to the pages in order to
 * satisfy the int_32-alignment requirements in RFC1832.
 *
 * For the future, we might want to string several of these together
 * in a list if anybody wants to make use of NFSv4 COMPOUND
 * operations and/or has a need for scatter/gather involving pages.
 */
struct xdr_buf {
	struct iovec	head[1],	/* RPC header + non-page data */
			tail[1];	/* Appended after page data */

	struct page **	pages;		/* Array of contiguous pages */
	unsigned int	page_base,	/* Start of page data */
			page_len;	/* Length of page data */

	unsigned int	len;		/* Total length of data */

};

/*
 * pre-xdr'ed macros.
 */

#define	xdr_zero	__constant_htonl(0)
#define	xdr_one		__constant_htonl(1)
#define	xdr_two		__constant_htonl(2)

#define	rpc_success		__constant_htonl(RPC_SUCCESS)
#define	rpc_prog_unavail	__constant_htonl(RPC_PROG_UNAVAIL)
#define	rpc_prog_mismatch	__constant_htonl(RPC_PROG_MISMATCH)
#define	rpc_proc_unavail	__constant_htonl(RPC_PROC_UNAVAIL)
#define	rpc_garbage_args	__constant_htonl(RPC_GARBAGE_ARGS)
#define	rpc_system_err		__constant_htonl(RPC_SYSTEM_ERR)

#define	rpc_auth_ok		__constant_htonl(RPC_AUTH_OK)
#define	rpc_autherr_badcred	__constant_htonl(RPC_AUTH_BADCRED)
#define	rpc_autherr_rejectedcred __constant_htonl(RPC_AUTH_REJECTEDCRED)
#define	rpc_autherr_badverf	__constant_htonl(RPC_AUTH_BADVERF)
#define	rpc_autherr_rejectedverf __constant_htonl(RPC_AUTH_REJECTEDVERF)
#define	rpc_autherr_tooweak	__constant_htonl(RPC_AUTH_TOOWEAK)


/*
 * Miscellaneous XDR helper functions
 */
u32 *	xdr_encode_array(u32 *p, const char *s, unsigned int len);
u32 *	xdr_encode_string(u32 *p, const char *s);
u32 *	xdr_decode_string(u32 *p, char **sp, int *lenp, int maxlen);
u32 *	xdr_decode_string_inplace(u32 *p, char **sp, int *lenp, int maxlen);
u32 *	xdr_encode_netobj(u32 *p, const struct xdr_netobj *);
u32 *	xdr_decode_netobj(u32 *p, struct xdr_netobj *);
u32 *	xdr_decode_netobj_fixed(u32 *p, void *obj, unsigned int len);

void	xdr_encode_pages(struct xdr_buf *, struct page **, unsigned int,
			 unsigned int);
void	xdr_inline_pages(struct xdr_buf *, unsigned int,
			 struct page **, unsigned int, unsigned int);

/*
 * Decode 64bit quantities (NFSv3 support)
 */
static inline u32 *
xdr_encode_hyper(u32 *p, __u64 val)
{
	*p++ = htonl(val >> 32);
	*p++ = htonl(val & 0xFFFFFFFF);
	return p;
}

static inline u32 *
xdr_decode_hyper(u32 *p, __u64 *valp)
{
	*valp  = ((__u64) ntohl(*p++)) << 32;
	*valp |= ntohl(*p++);
	return p;
}

/*
 * Adjust iovec to reflect end of xdr'ed data (RPC client XDR)
 */
static inline int
xdr_adjust_iovec(struct iovec *iov, u32 *p)
{
	return iov->iov_len = ((u8 *) p - (u8 *) iov->iov_base);
}

void xdr_shift_iovec(struct iovec *, int, size_t);
void xdr_zero_iovec(struct iovec *, int, size_t);

/*
 * Maximum number of iov's we use.
 */
#define MAX_IOVEC	(12)

/*
 * XDR buffer helper functions
 */
extern int xdr_kmap(struct iovec *, struct xdr_buf *, unsigned int);
extern void xdr_kunmap(struct xdr_buf *, unsigned int, int);
extern void xdr_shift_buf(struct xdr_buf *, size_t);

/*
 * Helper structure for copying from an sk_buff.
 */
typedef struct {
	struct sk_buff	*skb;
	unsigned int	offset;
	size_t		count;
	unsigned int	csum;
} skb_reader_t;

typedef size_t (*skb_read_actor_t)(skb_reader_t *desc, void *to, size_t len);

extern void xdr_partial_copy_from_skb(struct xdr_buf *, unsigned int,
		skb_reader_t *, skb_read_actor_t);

extern int xdr_copy_skb(struct xdr_buf *xdr, unsigned int base,
		struct sk_buff *skb, unsigned int offset);

extern int xdr_copy_and_csum_skb(struct xdr_buf *xdr, unsigned int base,
		struct sk_buff *skb, unsigned int offset, unsigned int csum);

#endif /* __KERNEL__ */

#endif /* _SUNRPC_XDR_H_ */
