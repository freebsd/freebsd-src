/*-
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: src/sys/netatm/port.h,v 1.16 2005/01/07 01:45:36 imp Exp $
 *
 */

/*
 * System Configuration
 * --------------------
 *
 * Porting aides
 *
 */

#ifndef _NETATM_PORT_H
#define	_NETATM_PORT_H


#ifdef _KERNEL
/*
 * Kernel buffers
 *
 * KBuffer 		Typedef for a kernel buffer.
 *
 * KB_NEXT(bfr)		Access next buffer in chain (r/w).
 * KB_LEN(bfr)		Access length of data in this buffer (r/w).
 * KB_QNEXT(bfr)	Access next buffer in queue (r/w).
 *
 * KB_ALLOC(bfr, size, flags, type)
 *			Allocates a new kernel buffer of at least size bytes.
 * KB_ALLOCPKT(bfr, size, flags, type)
 *			Allocates a new kernel packet header buffer of at
 *			least size bytes.
 * KB_ALLOCEXT(bfr, size, flags, type)
 *			Allocates a new kernel buffer with external storage
 *			of at least size bytes.
 * KB_FREEONE(bfr, nxt)	Free buffer bfr and set next buffer in chain in nxt.
 * KB_FREEALL(bfr)	Free bfr's entire buffer chain.
 * KB_COPY(bfr, off, len, new, flags)
 *			Copy len bytes of user data from buffer bfr starting at
 *			byte offset off and return new buffer chain in new.
 *			If len is KB_COPYALL, copy until end of chain.
 * KB_COPYDATA(bfr, off, len, datap)
 *			Copy data from buffer bfr starting at byte offset off
 *			for len bytes into the data area pointed to by datap.
 *			Returns the number of bytes not copied to datap.
 * KB_PULLUP(bfr, n, new)
 *			Get at least the first n bytes of data in the buffer 
 *			chain headed by bfr contiguous in the first buffer.
 *			Returns the (potentially new) head of the chain in new.
 *			On failure the chain is freed and NULL is returned.
 * KB_LINKHEAD(new, head)
 *			Link the kernel buffer new at the head of the buffer
 *			chain headed by head.  If both new and head are
 *			packet header buffers, new will become the packet
 *			header for the chain.
 * KB_LINK(new, prev)
 *			Link the kernel buffer new into the buffer chain
 *			after the buffer prev.
 * KB_UNLINKHEAD(head, next)
 *			Unlink the kernel buffer from the head of the buffer
 *			chain headed by head.  The buffer head will be freed
 *			and the new chain head will be placed in next.
 * KB_UNLINK(old, prev, next)
 *			Unlink the kernel buffer old with previous buffer prev
 *			from its buffer chain.  The following buffer in the
 *			chain will be placed in next and the buffer old will
 *			be freed.
 * KB_ISPKT(bfr)	Tests whether bfr is a packet header buffer.
 * KB_ISEXT(bfr)	Tests whether bfr has external storage.
 * KB_BFRSTART(bfr, x, t)
 *			Sets x (cast to type t) to point to the start of the
 *			buffer space in bfr.
 * KB_BFREND(bfr, x, t)
 *			Sets x (cast to type t) to point one byte past the end
 *			of the buffer space in bfr.
 * KB_BFRLEN(bfr)	Returns length of buffer space in bfr.
 * KB_DATASTART(bfr, x, t)
 *			Sets x (cast to type t) to point to the start of the
 *			buffer data contained in bfr.
 * KB_DATAEND(bfr, x, t)
 *			Sets x (cast to type t) to point one byte past the end
 *			of the buffer data contained in bfr.
 * KB_HEADSET(bfr, n)	Sets the start address for buffer data in buffer bfr to
 *			n bytes from the beginning of the buffer space.
 * KB_HEADMOVE(bfr, n) 	Adjust buffer data controls to move data down (n > 0) 
 *			or up (n < 0) n bytes in the buffer bfr.
 * KB_HEADADJ(bfr, n) 	Adjust buffer data controls to add (n > 0) or subtract
 *			(n < 0) n bytes of data to/from the beginning of bfr.
 * KB_TAILADJ(bfr, n)	Adjust buffer data controls to add (n > 0) or subtract
 *			(n < 0) n bytes of data to/from the end of bfr.
 * KB_TAILALIGN(bfr, n)	Set buffer data controls to place an object of size n
 *			at the end of bfr, longword aligned.
 * KB_HEADROOM(bfr, n)	Set n to the amount of buffer space available before
 *			the start of data in bfr.
 * KB_TAILROOM(bfr, n)	Set n to the amount of buffer space available after
 *			the end of data in bfr.
 * KB_PLENGET(bfr, n)	Set n to bfr's packet length.
 * KB_PLENSET(bfr, n)	Set bfr's packet length to n.
 * KB_PLENADJ(bfr, n)	Adjust total packet length by n bytes.
 *
 */
#include <sys/mbuf.h>
typedef struct mbuf	KBuffer;

#define	KB_F_WAIT	M_TRYWAIT
#define	KB_F_NOWAIT	M_DONTWAIT

#define	KB_T_HEADER	MT_HEADER
#define	KB_T_DATA	MT_DATA

#define	KB_COPYALL	M_COPYALL

#define	KB_NEXT(bfr)		(bfr)->m_next
#define	KB_LEN(bfr)		(bfr)->m_len
#define	KB_QNEXT(bfr)		(bfr)->m_nextpkt
#define KB_ALLOC(bfr, size, flags, type) {		\
	if ((size) <= MLEN) {				\
		MGET((bfr), (flags), (type));		\
	} else						\
		(bfr) = NULL;				\
}
#define KB_ALLOCPKT(bfr, size, flags, type) {		\
	if ((size) <= MHLEN) {				\
		MGETHDR((bfr), (flags), (type));	\
	} else						\
		(bfr) = NULL;				\
}
#define KB_ALLOCEXT(bfr, size, flags, type) {		\
	if ((size) <= MCLBYTES)	{			\
		MGET((bfr), (flags), (type));		\
		if ((bfr) != NULL) {			\
			MCLGET((bfr), (flags));		\
			if (((bfr)->m_flags & M_EXT) == 0) {	\
				m_freem((bfr));		\
				(bfr) = NULL;		\
			}				\
		}					\
	} else						\
		(bfr) = NULL;				\
}
#define KB_FREEONE(bfr, nxt) {				\
	(nxt) = m_free(bfr);				\
}
#define KB_FREEALL(bfr) {				\
	m_freem(bfr);					\
}
#define	KB_COPY(bfr, off, len, new, flags) {		\
	(new) = m_copym((bfr), (off), (len), (flags));	\
}
#define	KB_COPYDATA(bfr, off, len, datap) 		\
	(m_copydata((bfr), (off), (len), (datap)), 0)
#define	KB_PULLUP(bfr, n, new) {			\
	(new) = m_pullup((bfr), (n));			\
}
#define	KB_LINKHEAD(new, head) {			\
	if ((head) && KB_ISPKT(new) && KB_ISPKT(head)) {\
		M_MOVE_PKTHDR((new), (head));		\
	}						\
	(new)->m_next = (head);				\
}
#define	KB_LINK(new, prev) {				\
	(new)->m_next = (prev)->m_next;			\
	(prev)->m_next = (new);				\
}
#define	KB_UNLINKHEAD(head, next) {			\
	(next) = m_free((head));			\
	(head) = NULL;					\
}
#define	KB_UNLINK(old, prev, next) {			\
	(next) = m_free((old));				\
	(old) = NULL;					\
	(prev)->m_next = (next);			\
}
#define	KB_ISPKT(bfr)		(((bfr)->m_flags & M_PKTHDR) != 0)
#define	KB_ISEXT(bfr)		(((bfr)->m_flags & M_EXT) != 0)
#define	KB_BFRSTART(bfr, x, t) {			\
	if ((bfr)->m_flags & M_EXT)			\
		(x) = (t)((bfr)->m_ext.ext_buf);	\
	else if ((bfr)->m_flags & M_PKTHDR)		\
		(x) = (t)(&(bfr)->m_pktdat);		\
	else						\
		(x) = (t)((bfr)->m_dat);		\
}
#define	KB_BFREND(bfr, x, t) {				\
	if ((bfr)->m_flags & M_EXT)			\
		(x) = (t)((bfr)->m_ext.ext_buf + (bfr)->m_ext.ext_size);\
	else if ((bfr)->m_flags & M_PKTHDR)		\
		(x) = (t)(&(bfr)->m_pktdat + MHLEN);	\
	else						\
		(x) = (t)((bfr)->m_dat + MLEN);		\
}
#define	KB_BFRLEN(bfr)					\
	(((bfr)->m_flags & M_EXT) ? (bfr)->m_ext.ext_size :	\
		(((bfr)->m_flags & M_PKTHDR) ? MHLEN : MLEN))
#define	KB_DATASTART(bfr, x, t) {			\
	(x) = mtod((bfr), t);				\
}
#define	KB_DATAEND(bfr, x, t) {				\
	(x) = (t)(mtod((bfr), caddr_t) + (bfr)->m_len);	\
}
#define	KB_HEADSET(bfr, n) {				\
	if ((bfr)->m_flags & M_EXT)			\
		(bfr)->m_data = (bfr)->m_ext.ext_buf + (n);	\
	else if ((bfr)->m_flags & M_PKTHDR)		\
		(bfr)->m_data = (bfr)->m_pktdat + (n);	\
	else						\
		(bfr)->m_data = (bfr)->m_dat + (n);	\
}
#define	KB_HEADMOVE(bfr, n) {				\
	(bfr)->m_data += (n);				\
}
#define	KB_HEADADJ(bfr, n) {				\
	(bfr)->m_len += (n);				\
	(bfr)->m_data -= (n);				\
}
#define	KB_TAILADJ(bfr, n) {				\
	(bfr)->m_len += (n);				\
}
#define	KB_TAILALIGN(bfr, n) {				\
	(bfr)->m_len = (n);				\
	if ((bfr)->m_flags & M_EXT)			\
		(bfr)->m_data = (caddr_t)(((uintptr_t)(bfr)->m_ext.ext_buf \
			+ (bfr)->m_ext.ext_size - (n)) & ~(sizeof(long) - 1));\
	else						\
		(bfr)->m_data = (caddr_t)(((uintptr_t)(bfr)->m_dat + MLEN - (n)) \
			& ~(sizeof(long) - 1));		\
}
#define	KB_HEADROOM(bfr, n) {				\
	/* N = m_leadingspace(BFR) XXX */		\
	(n) = ((bfr)->m_flags & M_EXT ? (bfr)->m_data - (bfr)->m_ext.ext_buf : \
		(bfr)->m_flags & M_PKTHDR ? (bfr)->m_data - (bfr)->m_pktdat : \
			(bfr)->m_data - (bfr)->m_dat);	\
}
#define	KB_TAILROOM(bfr, n) {				\
	(n) = M_TRAILINGSPACE(bfr);			\
}
#define	KB_PLENGET(bfr, n) {				\
	(n) = (bfr)->m_pkthdr.len;			\
}
#define	KB_PLENSET(bfr, n) {				\
	(bfr)->m_pkthdr.len = (n);			\
}
#define	KB_PLENADJ(bfr, n) {				\
	(bfr)->m_pkthdr.len += (n);			\
}


/*
 * Kernel time
 *
 * KTimeout_ret		Typedef for timeout() function return
 *
 * KT_TIME(t)		Sets t to the current time.
 *
 */
typedef void	KTimeout_ret;
#define	KT_TIME(t)	microtime(&t)

#endif	/* _KERNEL */

#ifndef MAX
#define	MAX(a,b)	max((a),(b))
#endif
#ifndef MIN
#define	MIN(a,b)	min((a),(b))
#endif

#endif	/* _NETATM_PORT_H */
