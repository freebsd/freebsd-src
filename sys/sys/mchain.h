/*
 * Copyright (c) 2000, 2001 Boris Popov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/mchain.h,v 1.7 2003/02/21 16:24:49 bmilekic Exp $
 */
#ifndef _SYS_MCHAIN_H_
#define _SYS_MCHAIN_H_

#include <machine/endian.h>

#ifndef _KERNEL
/*
 * This macros probably belongs to the endian.h
 */
#if (BYTE_ORDER == LITTLE_ENDIAN)

#define	htoles(x)	((u_int16_t)(x))
#define	letohs(x)	((u_int16_t)(x))
#define	htolel(x)	((u_int32_t)(x))
#define	letohl(x)	((u_int32_t)(x))
#define	htoleq(x)	((int64_t)(x))
#define	letohq(x)	((int64_t)(x))

#define	htobes(x)	(__htons(x))
#define	betohs(x)	(__ntohs(x))
#define	htobel(x)	(__htonl(x))
#define	betohl(x)	(__ntohl(x))

static __inline int64_t
htobeq(int64_t x)
{
	return (int64_t)__htonl((u_int32_t)(x >> 32)) |
	    (int64_t)__htonl((u_int32_t)(x & 0xffffffff)) << 32;
}

static __inline int64_t
betohq(int64_t x)
{
	return (int64_t)__ntohl((u_int32_t)(x >> 32)) |
	    (int64_t)__ntohl((u_int32_t)(x & 0xffffffff)) << 32;
}

#else	/* (BYTE_ORDER == LITTLE_ENDIAN) */

#error "Macros for Big-Endians are incomplete"

/*
#define	htoles(x)	((u_int16_t)(x))
#define	letohs(x)	((u_int16_t)(x))
#define	htolel(x)	((u_int32_t)(x))
#define	letohl(x)	((u_int32_t)(x))
*/
#endif	/* (BYTE_ORDER == LITTLE_ENDIAN) */
#endif	/* _KERNEL */


#ifdef _KERNEL

/*
 * Type of copy for mb_{put|get}_mem()
 */
#define	MB_MSYSTEM	0		/* use bcopy() */
#define	MB_MUSER	1		/* use copyin()/copyout() */
#define	MB_MINLINE	2		/* use an inline copy loop */
#define	MB_MZERO	3		/* bzero(), mb_put_mem only */
#define	MB_MCUSTOM	4		/* use an user defined function */

struct mbuf;
struct mbchain;

typedef int mb_copy_t(struct mbchain *mbp, c_caddr_t src, caddr_t dst, size_t len);

struct mbchain {
	struct mbuf *	mb_top;		/* head of mbufs chain */
	struct mbuf * 	mb_cur;		/* current mbuf */
	int		mb_mleft;	/* free space in the current mbuf */
	int		mb_count;	/* total number of bytes */
	mb_copy_t *	mb_copy;	/* user defined copy function */
	void *		mb_udata;	/* user data */
};

struct mdchain {
	struct mbuf *	md_top;		/* head of mbufs chain */
	struct mbuf * 	md_cur;		/* current mbuf */
	u_char *	md_pos;		/* offset in the current mbuf */
};

int  mb_init(struct mbchain *mbp);
void mb_initm(struct mbchain *mbp, struct mbuf *m);
void mb_done(struct mbchain *mbp);
struct mbuf *mb_detach(struct mbchain *mbp);
int  mb_fixhdr(struct mbchain *mbp);
caddr_t mb_reserve(struct mbchain *mbp, int size);

int  mb_put_uint8(struct mbchain *mbp, u_int8_t x);
int  mb_put_uint16be(struct mbchain *mbp, u_int16_t x);
int  mb_put_uint16le(struct mbchain *mbp, u_int16_t x);
int  mb_put_uint32be(struct mbchain *mbp, u_int32_t x);
int  mb_put_uint32le(struct mbchain *mbp, u_int32_t x);
int  mb_put_int64be(struct mbchain *mbp, int64_t x);
int  mb_put_int64le(struct mbchain *mbp, int64_t x);
int  mb_put_mem(struct mbchain *mbp, c_caddr_t source, int size, int type);
int  mb_put_mbuf(struct mbchain *mbp, struct mbuf *m);
int  mb_put_uio(struct mbchain *mbp, struct uio *uiop, int size);

int  md_init(struct mdchain *mdp);
void md_initm(struct mdchain *mbp, struct mbuf *m);
void md_done(struct mdchain *mdp);
void md_append_record(struct mdchain *mdp, struct mbuf *top);
int  md_next_record(struct mdchain *mdp);
int  md_get_uint8(struct mdchain *mdp, u_int8_t *x);
int  md_get_uint16(struct mdchain *mdp, u_int16_t *x);
int  md_get_uint16le(struct mdchain *mdp, u_int16_t *x);
int  md_get_uint16be(struct mdchain *mdp, u_int16_t *x);
int  md_get_uint32(struct mdchain *mdp, u_int32_t *x);
int  md_get_uint32be(struct mdchain *mdp, u_int32_t *x);
int  md_get_uint32le(struct mdchain *mdp, u_int32_t *x);
int  md_get_int64(struct mdchain *mdp, int64_t *x);
int  md_get_int64be(struct mdchain *mdp, int64_t *x);
int  md_get_int64le(struct mdchain *mdp, int64_t *x);
int  md_get_mem(struct mdchain *mdp, caddr_t target, int size, int type);
int  md_get_mbuf(struct mdchain *mdp, int size, struct mbuf **m);
int  md_get_uio(struct mdchain *mdp, struct uio *uiop, int size);

#endif	/* ifdef _KERNEL */

#endif	/* !_SYS_MCHAIN_H_ */
