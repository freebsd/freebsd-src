/*
 * linux/net/sunrpc/xdr.c
 *
 * Generic XDR support.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/types.h>
#include <linux/socket.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/pagemap.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/sunrpc/xdr.h>
#include <linux/sunrpc/msg_prot.h>

/*
 * XDR functions for basic NFS types
 */
u32 *
xdr_encode_netobj(u32 *p, const struct xdr_netobj *obj)
{
	unsigned int	quadlen = XDR_QUADLEN(obj->len);

	p[quadlen] = 0;		/* zero trailing bytes */
	*p++ = htonl(obj->len);
	memcpy(p, obj->data, obj->len);
	return p + XDR_QUADLEN(obj->len);
}

u32 *
xdr_decode_netobj_fixed(u32 *p, void *obj, unsigned int len)
{
	if (ntohl(*p++) != len)
		return NULL;
	memcpy(obj, p, len);
	return p + XDR_QUADLEN(len);
}

u32 *
xdr_decode_netobj(u32 *p, struct xdr_netobj *obj)
{
	unsigned int	len;

	if ((len = ntohl(*p++)) > XDR_MAX_NETOBJ)
		return NULL;
	obj->len  = len;
	obj->data = (u8 *) p;
	return p + XDR_QUADLEN(len);
}

u32 *
xdr_encode_array(u32 *p, const char *array, unsigned int len)
{
	int quadlen = XDR_QUADLEN(len);

	p[quadlen] = 0;
	*p++ = htonl(len);
	memcpy(p, array, len);
	return p + quadlen;
}

u32 *
xdr_encode_string(u32 *p, const char *string)
{
	return xdr_encode_array(p, string, strlen(string));
}

u32 *
xdr_decode_string(u32 *p, char **sp, int *lenp, int maxlen)
{
	unsigned int	len;
	char		*string;

	if ((len = ntohl(*p++)) > maxlen)
		return NULL;
	if (lenp)
		*lenp = len;
	if ((len % 4) != 0) {
		string = (char *) p;
	} else {
		string = (char *) (p - 1);
		memmove(string, p, len);
	}
	string[len] = '\0';
	*sp = string;
	return p + XDR_QUADLEN(len);
}

u32 *
xdr_decode_string_inplace(u32 *p, char **sp, int *lenp, int maxlen)
{
	unsigned int	len;

	if ((len = ntohl(*p++)) > maxlen)
		return NULL;
	*lenp = len;
	*sp = (char *) p;
	return p + XDR_QUADLEN(len);
}


void
xdr_encode_pages(struct xdr_buf *xdr, struct page **pages, unsigned int base,
		 unsigned int len)
{
	xdr->pages = pages;
	xdr->page_base = base;
	xdr->page_len = len;

	if (len & 3) {
		struct iovec *iov = xdr->tail;
		unsigned int pad = 4 - (len & 3);

		iov->iov_base = (void *) "\0\0\0";
		iov->iov_len  = pad;
		len += pad;
	}
	xdr->len += len;
}

void
xdr_inline_pages(struct xdr_buf *xdr, unsigned int offset,
		 struct page **pages, unsigned int base, unsigned int len)
{
	struct iovec *head = xdr->head;
	struct iovec *tail = xdr->tail;
	char *buf = (char *)head->iov_base;
	unsigned int buflen = head->iov_len;

	head->iov_len  = offset;

	xdr->pages = pages;
	xdr->page_base = base;
	xdr->page_len = len;

	tail->iov_base = buf + offset;
	tail->iov_len = buflen - offset;

	xdr->len += len;
}

/*
 * Realign the iovec if the server missed out some reply elements
 * (such as post-op attributes,...)
 * Note: This is a simple implementation that assumes that
 *            len <= iov->iov_len !!!
 *       The RPC header (assumed to be the 1st element in the iov array)
 *            is not shifted.
 */
void xdr_shift_iovec(struct iovec *iov, int nr, size_t len)
{
	struct iovec *pvec;

	for (pvec = iov + nr - 1; nr > 1; nr--, pvec--) {
		struct iovec *svec = pvec - 1;

		if (len > pvec->iov_len) {
			printk(KERN_DEBUG "RPC: Urk! Large shift of short iovec.\n");
			return;
		}
		memmove((char *)pvec->iov_base + len, pvec->iov_base,
			pvec->iov_len - len);

		if (len > svec->iov_len) {
			printk(KERN_DEBUG "RPC: Urk! Large shift of short iovec.\n");
			return;
		}
		memcpy(pvec->iov_base,
		       (char *)svec->iov_base + svec->iov_len - len, len);
	}
}

/*
 * Map a struct xdr_buf into an iovec array.
 */
int xdr_kmap(struct iovec *iov_base, struct xdr_buf *xdr, unsigned int base)
{
	struct iovec	*iov = iov_base;
	struct page	**ppage = xdr->pages;
	struct page	**first_kmap = NULL;
	unsigned int	len, pglen = xdr->page_len;

	len = xdr->head[0].iov_len;
	if (base < len) {
		iov->iov_len = len - base;
		iov->iov_base = (char *)xdr->head[0].iov_base + base;
		iov++;
		base = 0;
	} else
		base -= len;

	if (pglen == 0)
		goto map_tail;
	if (base >= pglen) {
		base -= pglen;
		goto map_tail;
	}
	if (base || xdr->page_base) {
		pglen -= base;
		base  += xdr->page_base;
		ppage += base >> PAGE_CACHE_SHIFT;
		base &= ~PAGE_CACHE_MASK;
	}
	do {
		len = PAGE_CACHE_SIZE;
		if (!first_kmap) {
			first_kmap = ppage;
			iov->iov_base = kmap(*ppage);
		} else {
			iov->iov_base = kmap_nonblock(*ppage);
			if (!iov->iov_base)
				goto out_err;
		}
		if (base) {
			iov->iov_base += base;
			len -= base;
			base = 0;
		}
		if (pglen < len)
			len = pglen;
		iov->iov_len = len;
		iov++;
		ppage++;
	} while ((pglen -= len) != 0);
map_tail:
	if (xdr->tail[0].iov_len) {
		iov->iov_len = xdr->tail[0].iov_len - base;
		iov->iov_base = (char *)xdr->tail[0].iov_base + base;
		iov++;
	}
	return (iov - iov_base);
out_err:
	for (; first_kmap != ppage; first_kmap++)
		kunmap(*first_kmap);
	return 0;
}

void xdr_kunmap(struct xdr_buf *xdr, unsigned int base, int niov)
{
	struct page	**ppage = xdr->pages;
	unsigned int	pglen = xdr->page_len;

	if (!pglen)
		return;
	if (base >= xdr->head[0].iov_len)
		base -= xdr->head[0].iov_len;
	else {
		niov--;
		base = 0;
	}

	if (base >= pglen)
		return;
	if (base || xdr->page_base) {
		pglen -= base;
		base  += xdr->page_base;
		ppage += base >> PAGE_CACHE_SHIFT;
		/* Note: The offset means that the length of the first
		 * page is really (PAGE_CACHE_SIZE - (base & ~PAGE_CACHE_MASK)).
		 * In order to avoid an extra test inside the loop,
		 * we bump pglen here, and just subtract PAGE_CACHE_SIZE... */
		pglen += base & ~PAGE_CACHE_MASK;
	}
	/*
	 * In case we could only do a partial xdr_kmap, all remaining iovecs
	 * refer to pages. Otherwise we detect the end through pglen.
	 */
	for (; niov; niov--) {
		flush_dcache_page(*ppage);
		kunmap(*ppage);
		if (pglen <= PAGE_CACHE_SIZE)
			break;
		pglen -= PAGE_CACHE_SIZE;
		ppage++;
	}
}

void
xdr_partial_copy_from_skb(struct xdr_buf *xdr, unsigned int base,
			  skb_reader_t *desc,
			  skb_read_actor_t copy_actor)
{
	struct page	**ppage = xdr->pages;
	unsigned int	len, pglen = xdr->page_len;
	int		ret;

	len = xdr->head[0].iov_len;
	if (base < len) {
		len -= base;
		ret = copy_actor(desc, (char *)xdr->head[0].iov_base + base, len);
		if (ret != len || !desc->count)
			return;
		base = 0;
	} else
		base -= len;

	if (pglen == 0)
		goto copy_tail;
	if (base >= pglen) {
		base -= pglen;
		goto copy_tail;
	}
	if (base || xdr->page_base) {
		pglen -= base;
		base  += xdr->page_base;
		ppage += base >> PAGE_CACHE_SHIFT;
		base &= ~PAGE_CACHE_MASK;
	}
	do {
		char *kaddr;

		len = PAGE_CACHE_SIZE;
		kaddr = kmap_atomic(*ppage, KM_SKB_SUNRPC_DATA);
		if (base) {
			len -= base;
			if (pglen < len)
				len = pglen;
			ret = copy_actor(desc, kaddr + base, len);
			base = 0;
		} else {
			if (pglen < len)
				len = pglen;
			ret = copy_actor(desc, kaddr, len);
		}
		kunmap_atomic(kaddr, KM_SKB_SUNRPC_DATA);
		if (ret != len || !desc->count)
			return;
		ppage++;
	} while ((pglen -= len) != 0);
copy_tail:
	len = xdr->tail[0].iov_len;
	if (len)
		copy_actor(desc, (char *)xdr->tail[0].iov_base + base, len);
}

/*
 * Helper routines for doing 'memmove' like operations on a struct xdr_buf
 *
 * _shift_data_right_pages
 * @pages: vector of pages containing both the source and dest memory area.
 * @pgto_base: page vector address of destination
 * @pgfrom_base: page vector address of source
 * @len: number of bytes to copy
 *
 * Note: the addresses pgto_base and pgfrom_base are both calculated in
 *       the same way:
 *            if a memory area starts at byte 'base' in page 'pages[i]',
 *            then its address is given as (i << PAGE_CACHE_SHIFT) + base
 * Also note: pgfrom_base must be < pgto_base, but the memory areas
 * 	they point to may overlap.
 */
static void
_shift_data_right_pages(struct page **pages, size_t pgto_base,
		size_t pgfrom_base, size_t len)
{
	struct page **pgfrom, **pgto;
	char *vfrom, *vto;
	size_t copy;

	BUG_ON(pgto_base <= pgfrom_base);

	pgto_base += len;
	pgfrom_base += len;

	pgto = pages + (pgto_base >> PAGE_CACHE_SHIFT);
	pgfrom = pages + (pgfrom_base >> PAGE_CACHE_SHIFT);

	pgto_base &= ~PAGE_CACHE_MASK;
	pgfrom_base &= ~PAGE_CACHE_MASK;

	do {
		/* Are any pointers crossing a page boundary? */
		if (pgto_base == 0) {
			pgto_base = PAGE_CACHE_SIZE;
			pgto--;
		}
		if (pgfrom_base == 0) {
			pgfrom_base = PAGE_CACHE_SIZE;
			pgfrom--;
		}

		copy = len;
		if (copy > pgto_base)
			copy = pgto_base;
		if (copy > pgfrom_base)
			copy = pgfrom_base;
		pgto_base -= copy;
		pgfrom_base -= copy;

		vto = kmap_atomic(*pgto, KM_USER0);
		vfrom = kmap_atomic(*pgfrom, KM_USER1);
		memmove(vto + pgto_base, vfrom + pgfrom_base, copy);
		kunmap_atomic(vfrom, KM_USER1);
		kunmap_atomic(vto, KM_USER0);

	} while ((len -= copy) != 0);
}

/*
 * _copy_to_pages
 * @pages: array of pages
 * @pgbase: page vector address of destination
 * @p: pointer to source data
 * @len: length
 *
 * Copies data from an arbitrary memory location into an array of pages
 * The copy is assumed to be non-overlapping.
 */
static void
_copy_to_pages(struct page **pages, size_t pgbase, const char *p, size_t len)
{
	struct page **pgto;
	char *vto;
	size_t copy;

	pgto = pages + (pgbase >> PAGE_CACHE_SHIFT);
	pgbase &= ~PAGE_CACHE_MASK;

	do {
		copy = PAGE_CACHE_SIZE - pgbase;
		if (copy > len)
			copy = len;

		vto = kmap_atomic(*pgto, KM_USER0);
		memcpy(vto + pgbase, p, copy);
		kunmap_atomic(vto, KM_USER0);

		pgbase += copy;
		if (pgbase == PAGE_CACHE_SIZE) {
			pgbase = 0;
			pgto++;
		}
		p += copy;

	} while ((len -= copy) != 0);
}

/*
 * _copy_from_pages
 * @p: pointer to destination
 * @pages: array of pages
 * @pgbase: offset of source data
 * @len: length
 *
 * Copies data into an arbitrary memory location from an array of pages
 * The copy is assumed to be non-overlapping.
 */
static void
_copy_from_pages(char *p, struct page **pages, size_t pgbase, size_t len)
{
	struct page **pgfrom;
	char *vfrom;
	size_t copy;

	pgfrom = pages + (pgbase >> PAGE_CACHE_SHIFT);
	pgbase &= ~PAGE_CACHE_MASK;

	do {
		copy = PAGE_CACHE_SIZE - pgbase;
		if (copy > len)
			copy = len;

		vfrom = kmap_atomic(*pgfrom, KM_USER0);
		memcpy(p, vfrom + pgbase, copy);
		kunmap_atomic(vfrom, KM_USER0);

		pgbase += copy;
		if (pgbase == PAGE_CACHE_SIZE) {
			pgbase = 0;
			pgfrom++;
		}
		p += copy;

	} while ((len -= copy) != 0);
}

/*
 * xdr_shrink_bufhead
 * @buf: xdr_buf
 * @len: bytes to remove from buf->head[0]
 *
 * Shrinks XDR buffer's header iovec buf->head[0] by 
 * 'len' bytes. The extra data is not lost, but is instead
 * moved into the inlined pages and/or the tail.
 */
void
xdr_shrink_bufhead(struct xdr_buf *buf, size_t len)
{
	struct iovec *head, *tail;
	size_t copy, offs;
	unsigned int pglen = buf->page_len;

	tail = buf->tail;
	head = buf->head;
	BUG_ON (len > head->iov_len);

	/* Shift the tail first */
	if (tail->iov_len != 0) {
		if (tail->iov_len > len) {
			copy = tail->iov_len - len;
			memmove((char *)tail->iov_base + len,
					tail->iov_base, copy);
		}
		/* Copy from the inlined pages into the tail */
		copy = len;
		if (copy > pglen)
			copy = pglen;
		offs = len - copy;
		if (offs >= tail->iov_len)
			copy = 0;
		else if (copy > tail->iov_len - offs)
			copy = tail->iov_len - offs;
		if (copy != 0)
			_copy_from_pages((char *)tail->iov_base + offs,
					buf->pages,
					buf->page_base + pglen + offs - len,
					copy);
		/* Do we also need to copy data from the head into the tail ? */
		if (len > pglen) {
			offs = copy = len - pglen;
			if (copy > tail->iov_len)
				copy = tail->iov_len;
			memcpy(tail->iov_base,
					(char *)head->iov_base +
					head->iov_len - offs,
					copy);
		}
	}
	/* Now handle pages */
	if (pglen != 0) {
		if (pglen > len)
			_shift_data_right_pages(buf->pages,
					buf->page_base + len,
					buf->page_base,
					pglen - len);
		copy = len;
		if (len > pglen)
			copy = pglen;
		_copy_to_pages(buf->pages, buf->page_base,
				(char *)head->iov_base + head->iov_len - len,
				copy);
	}
	head->iov_len -= len;
	buf->len -= len;
}

void
xdr_shift_buf(struct xdr_buf *buf, size_t len)
{
	xdr_shrink_bufhead(buf, len);
}
