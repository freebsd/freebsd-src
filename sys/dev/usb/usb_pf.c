/*-
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_core.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/usb_pf.h>
#include <dev/usb/usb_transfer.h>

/*
 * All usbpf implementations are extracted from bpf(9) APIs and it's
 * specialized for USB packet filtering between the driver and the host
 * controller.
 */

MALLOC_DEFINE(M_USBPF, "USBPktFilter", "USB Packet Filter");

/*
 * Rotate the packet buffers in descriptor ud.  Move the store buffer into the
 * hold slot, and the free buffer ino the store slot.  Zero the length of the
 * new store buffer.  Descriptor lock should be held.
 */
#define	USBPF_ROTATE_BUFFERS(ud)	do {				\
	(ud)->ud_hbuf = (ud)->ud_sbuf;					\
	(ud)->ud_hlen = (ud)->ud_slen;					\
	(ud)->ud_sbuf = (ud)->ud_fbuf;					\
	(ud)->ud_slen = 0;						\
	(ud)->ud_fbuf = NULL;						\
	usbpf_bufheld(ud);						\
} while (0)

#ifndef __i386__
#define	USBPF_ALIGN
#endif

#ifndef USBPF_ALIGN
#define	USBPF_EXTRACT_SHORT(p)	((u_int16_t)ntohs(*(u_int16_t *)p))
#define	USBPF_EXTRACT_LONG(p)	(ntohl(*(u_int32_t *)p))
#else
#define	USBPF_EXTRACT_SHORT(p)						\
	((u_int16_t)							\
	    ((u_int16_t)*((u_char *)p+0)<<8|				\
		(u_int16_t)*((u_char *)p+1)<<0))
#define	USBPF_EXTRACT_LONG(p)						\
	((u_int32_t)*((u_char *)p+0)<<24|				\
	    (u_int32_t)*((u_char *)p+1)<<16|				\
	    (u_int32_t)*((u_char *)p+2)<<8|				\
	    (u_int32_t)*((u_char *)p+3)<<0)
#endif

/*
 * Number of scratch memory words (for USBPF_LD|USBPF_MEM and USBPF_ST).
 */
#define	USBPF_MEMWORDS		 16

/* Values for ud_state */
#define	USBPF_IDLE		0	/* no select in progress */
#define	USBPF_WAITING		1	/* waiting for read timeout in select */
#define	USBPF_TIMED_OUT		2	/* read timeout has expired in select */

#define	PRIUSB			26	/* interruptible */

/* Frame directions */
enum usbpf_direction {
	USBPF_D_IN,	/* See incoming frames */
	USBPF_D_INOUT,	/* See incoming and outgoing frames */
	USBPF_D_OUT	/* See outgoing frames */
};

static void	usbpf_append_bytes(struct usbpf_d *, caddr_t, u_int, void *,
		    u_int);
static void	usbpf_attachd(struct usbpf_d *, struct usbpf_if *);
static void	usbpf_detachd(struct usbpf_d *);
static int	usbpf_canfreebuf(struct usbpf_d *);
static void	usbpf_buf_reclaimed(struct usbpf_d *);
static int	usbpf_canwritebuf(struct usbpf_d *);

static	d_open_t	usbpf_open;
static	d_read_t	usbpf_read;
static	d_write_t	usbpf_write;
static	d_ioctl_t	usbpf_ioctl;
static	d_poll_t	usbpf_poll;
static	d_kqfilter_t	usbpf_kqfilter;

static struct cdevsw usbpf_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	usbpf_open,
	.d_read =	usbpf_read,
	.d_write =	usbpf_write,
	.d_ioctl =	usbpf_ioctl,
	.d_poll =	usbpf_poll,
	.d_name =	"usbpf",
	.d_kqfilter =	usbpf_kqfilter,
};

static struct cdev *usbpf_cdev;
static LIST_HEAD(, usbpf_if)	usbpf_iflist;
static struct mtx	usbpf_mtx;		/* global lock */
static int usbpf_uifd_cnt;

static int usbpf_bufsize = 4096;
#define	USBPF_MINBUFSIZE 32
#define	USBPF_MAXBUFSIZE 0x80000
static int usbpf_maxbufsize = USBPF_MAXBUFSIZE;
#define	USBPF_MAXINSNS 512
static int usbpf_maxinsns = USBPF_MAXINSNS;

static void
usbpf_buffer_init(struct usbpf_d *ud)
{

	ud->ud_bufsize = usbpf_bufsize;
}

/*
 * Free USBPF kernel buffers on device close.
 */
static void
usbpf_buffer_free(struct usbpf_d *ud)
{

	if (ud->ud_sbuf != NULL)
		free(ud->ud_sbuf, M_USBPF);
	if (ud->ud_hbuf != NULL)
		free(ud->ud_hbuf, M_USBPF);
	if (ud->ud_fbuf != NULL)
		free(ud->ud_fbuf, M_USBPF);

#ifdef INVARIANTS
	ud->ud_sbuf = ud->ud_hbuf = ud->ud_fbuf = (caddr_t)~0;
#endif
}

static void
usbpf_buffer_alloc(struct usbpf_d *ud)
{

	KASSERT(ud->ud_fbuf == NULL, ("%s: ud_fbuf != NULL", __func__));
	KASSERT(ud->ud_sbuf == NULL, ("%s: ud_sbuf != NULL", __func__));
	KASSERT(ud->ud_hbuf == NULL, ("%s: ud_hbuf != NULL", __func__));

	ud->ud_fbuf = (caddr_t)malloc(ud->ud_bufsize, M_USBPF, M_WAITOK);
	ud->ud_sbuf = (caddr_t)malloc(ud->ud_bufsize, M_USBPF, M_WAITOK);
	ud->ud_hbuf = NULL;
	ud->ud_slen = 0;
	ud->ud_hlen = 0;
}

/*
 * Copy buffer storage to user space in read().
 */
static int
usbpf_buffer_uiomove(struct usbpf_d *ud, caddr_t buf, u_int len,
    struct uio *uio)
{

	return (uiomove(buf, len, uio));
}

/*
 * Simple data copy to the current kernel buffer.
 */
static void
usbpf_buffer_append_bytes(struct usbpf_d *ud, caddr_t buf, u_int offset,
    void *src, u_int len)
{
	u_char *src_bytes;

	src_bytes = (u_char *)src;
	bcopy(src_bytes, buf + offset, len);
}

/*
 * Allocate or resize buffers.
 */
static int
usbpf_buffer_ioctl_sblen(struct usbpf_d *ud, u_int *i)
{
	u_int size;

	USBPFD_LOCK(ud);
	if (ud->ud_bif != NULL) {
		USBPFD_UNLOCK(ud);
		return (EINVAL);
	}
	size = *i;
	if (size > usbpf_maxbufsize)
		*i = size = usbpf_maxbufsize;
	else if (size < USBPF_MINBUFSIZE)
		*i = size = USBPF_MINBUFSIZE;
	ud->ud_bufsize = size;
	USBPFD_UNLOCK(ud);
	return (0);
}

static const u_short	usbpf_code_map[] = {
	0x10ff,	/* 0x00-0x0f: 1111111100001000 */
	0x3070,	/* 0x10-0x1f: 0000111000001100 */
	0x3131,	/* 0x20-0x2f: 1000110010001100 */
	0x3031,	/* 0x30-0x3f: 1000110000001100 */
	0x3131,	/* 0x40-0x4f: 1000110010001100 */
	0x1011,	/* 0x50-0x5f: 1000100000001000 */
	0x1013,	/* 0x60-0x6f: 1100100000001000 */
	0x1010,	/* 0x70-0x7f: 0000100000001000 */
	0x0093,	/* 0x80-0x8f: 1100100100000000 */
	0x0000,	/* 0x90-0x9f: 0000000000000000 */
	0x0000,	/* 0xa0-0xaf: 0000000000000000 */
	0x0002,	/* 0xb0-0xbf: 0100000000000000 */
	0x0000,	/* 0xc0-0xcf: 0000000000000000 */
	0x0000,	/* 0xd0-0xdf: 0000000000000000 */
	0x0000,	/* 0xe0-0xef: 0000000000000000 */
	0x0000	/* 0xf0-0xff: 0000000000000000 */
};

#define	USBPF_VALIDATE_CODE(c)	\
    ((c) <= 0xff && (usbpf_code_map[(c) >> 4] & (1 << ((c) & 0xf))) != 0)

/*
 * Return true if the 'fcode' is a valid filter program.
 * The constraints are that each jump be forward and to a valid
 * code.  The code must terminate with either an accept or reject.
 *
 * The kernel needs to be able to verify an application's filter code.
 * Otherwise, a bogus program could easily crash the system.
 */
static int
usbpf_validate(const struct usbpf_insn *f, int len)
{
	register int i;
	register const struct usbpf_insn *p;

	/* Do not accept negative length filter. */
	if (len < 0)
		return (0);

	/* An empty filter means accept all. */
	if (len == 0)
		return (1);

	for (i = 0; i < len; ++i) {
		p = &f[i];
		/*
		 * Check that the code is valid.
		 */
		if (!USBPF_VALIDATE_CODE(p->code))
			return (0);
		/*
		 * Check that that jumps are forward, and within
		 * the code block.
		 */
		if (USBPF_CLASS(p->code) == USBPF_JMP) {
			register u_int offset;

			if (p->code == (USBPF_JMP|USBPF_JA))
				offset = p->k;
			else
				offset = p->jt > p->jf ? p->jt : p->jf;
			if (offset >= (u_int)(len - i) - 1)
				return (0);
			continue;
		}
		/*
		 * Check that memory operations use valid addresses.
		 */
		if (p->code == USBPF_ST || p->code == USBPF_STX ||
		    p->code == (USBPF_LD|USBPF_MEM) ||
		    p->code == (USBPF_LDX|USBPF_MEM)) {
			if (p->k >= USBPF_MEMWORDS)
				return (0);
			continue;
		}
		/*
		 * Check for constant division by 0.
		 */
		if (p->code == (USBPF_ALU|USBPF_DIV|USBPF_K) && p->k == 0)
			return (0);
	}
	return (USBPF_CLASS(f[len - 1].code) == USBPF_RET);
}

#ifdef _KERNEL
#define	MINDEX(m, k) \
{ \
	register int len = m->m_len; \
 \
	while (k >= len) { \
		k -= len; \
		m = m->m_next; \
		if (m == 0) \
			return (0); \
		len = m->m_len; \
	} \
}

static u_int16_t	m_xhalf(struct mbuf *m, usbpf_u_int32 k, int *err);
static u_int32_t	m_xword(struct mbuf *m, usbpf_u_int32 k, int *err);

static u_int32_t
m_xword(struct mbuf *m, usbpf_u_int32 k, int *err)
{
	size_t len;
	u_char *cp, *np;
	struct mbuf *m0;

	len = m->m_len;
	while (k >= len) {
		k -= len;
		m = m->m_next;
		if (m == 0)
			goto bad;
		len = m->m_len;
	}
	cp = mtod(m, u_char *) + k;
	if (len - k >= 4) {
		*err = 0;
		return (USBPF_EXTRACT_LONG(cp));
	}
	m0 = m->m_next;
	if (m0 == 0 || m0->m_len + len - k < 4)
		goto bad;
	*err = 0;
	np = mtod(m0, u_char *);
	switch (len - k) {
	case 1:
		return (((u_int32_t)cp[0] << 24) |
		    ((u_int32_t)np[0] << 16) |
		    ((u_int32_t)np[1] << 8)  |
		    (u_int32_t)np[2]);

	case 2:
		return (((u_int32_t)cp[0] << 24) |
		    ((u_int32_t)cp[1] << 16) |
		    ((u_int32_t)np[0] << 8) |
		    (u_int32_t)np[1]);

	default:
		return (((u_int32_t)cp[0] << 24) |
		    ((u_int32_t)cp[1] << 16) |
		    ((u_int32_t)cp[2] << 8) |
		    (u_int32_t)np[0]);
	}
    bad:
	*err = 1;
	return (0);
}

static u_int16_t
m_xhalf(struct mbuf *m, usbpf_u_int32 k, int *err)
{
	size_t len;
	u_char *cp;
	struct mbuf *m0;

	len = m->m_len;
	while (k >= len) {
		k -= len;
		m = m->m_next;
		if (m == 0)
			goto bad;
		len = m->m_len;
	}
	cp = mtod(m, u_char *) + k;
	if (len - k >= 2) {
		*err = 0;
		return (USBPF_EXTRACT_SHORT(cp));
	}
	m0 = m->m_next;
	if (m0 == 0)
		goto bad;
	*err = 0;
	return ((cp[0] << 8) | mtod(m0, u_char *)[0]);
 bad:
	*err = 1;
	return (0);
}
#endif

/*
 * Execute the filter program starting at pc on the packet p
 * wirelen is the length of the original packet
 * buflen is the amount of data present
 */
static u_int
usbpf_filter(const struct usbpf_insn *pc, u_char *p, u_int wirelen,
    u_int buflen)
{
	u_int32_t A = 0, X = 0;
	usbpf_u_int32 k;
	u_int32_t mem[USBPF_MEMWORDS];

	/*
	 * XXX temporarily the filter system is disabled because currently it
	 * could not handle the some machine code properly that leads to
	 * kernel crash by invalid usage.
	 */
	return ((u_int)-1);

	if (pc == NULL)
		/*
		 * No filter means accept all.
		 */
		return ((u_int)-1);

	--pc;
	while (1) {
		++pc;
		switch (pc->code) {
		default:
#ifdef _KERNEL
			return (0);
#else
			abort();
#endif

		case USBPF_RET|USBPF_K:
			return ((u_int)pc->k);

		case USBPF_RET|USBPF_A:
			return ((u_int)A);

		case USBPF_LD|USBPF_W|USBPF_ABS:
			k = pc->k;
			if (k > buflen || sizeof(int32_t) > buflen - k) {
#ifdef _KERNEL
				int merr;

				if (buflen != 0)
					return (0);
				A = m_xword((struct mbuf *)p, k, &merr);
				if (merr != 0)
					return (0);
				continue;
#else
				return (0);
#endif
			}
#ifdef USBPF_ALIGN
			if (((intptr_t)(p + k) & 3) != 0)
				A = USBPF_EXTRACT_LONG(&p[k]);
			else
#endif
				A = ntohl(*(int32_t *)(p + k));
			continue;

		case USBPF_LD|USBPF_H|USBPF_ABS:
			k = pc->k;
			if (k > buflen || sizeof(int16_t) > buflen - k) {
#ifdef _KERNEL
				int merr;

				if (buflen != 0)
					return (0);
				A = m_xhalf((struct mbuf *)p, k, &merr);
				continue;
#else
				return (0);
#endif
			}
			A = USBPF_EXTRACT_SHORT(&p[k]);
			continue;

		case USBPF_LD|USBPF_B|USBPF_ABS:
			k = pc->k;
			if (k >= buflen) {
#ifdef _KERNEL
				struct mbuf *m;

				if (buflen != 0)
					return (0);
				m = (struct mbuf *)p;
				MINDEX(m, k);
				A = mtod(m, u_char *)[k];
				continue;
#else
				return (0);
#endif
			}
			A = p[k];
			continue;

		case USBPF_LD|USBPF_W|USBPF_LEN:
			A = wirelen;
			continue;

		case USBPF_LDX|USBPF_W|USBPF_LEN:
			X = wirelen;
			continue;

		case USBPF_LD|USBPF_W|USBPF_IND:
			k = X + pc->k;
			if (pc->k > buflen || X > buflen - pc->k ||
			    sizeof(int32_t) > buflen - k) {
#ifdef _KERNEL
				int merr;

				if (buflen != 0)
					return (0);
				A = m_xword((struct mbuf *)p, k, &merr);
				if (merr != 0)
					return (0);
				continue;
#else
				return (0);
#endif
			}
#ifdef USBPF_ALIGN
			if (((intptr_t)(p + k) & 3) != 0)
				A = USBPF_EXTRACT_LONG(&p[k]);
			else
#endif
				A = ntohl(*(int32_t *)(p + k));
			continue;

		case USBPF_LD|USBPF_H|USBPF_IND:
			k = X + pc->k;
			if (X > buflen || pc->k > buflen - X ||
			    sizeof(int16_t) > buflen - k) {
#ifdef _KERNEL
				int merr;

				if (buflen != 0)
					return (0);
				A = m_xhalf((struct mbuf *)p, k, &merr);
				if (merr != 0)
					return (0);
				continue;
#else
				return (0);
#endif
			}
			A = USBPF_EXTRACT_SHORT(&p[k]);
			continue;

		case USBPF_LD|USBPF_B|USBPF_IND:
			k = X + pc->k;
			if (pc->k >= buflen || X >= buflen - pc->k) {
#ifdef _KERNEL
				struct mbuf *m;

				if (buflen != 0)
					return (0);
				m = (struct mbuf *)p;
				MINDEX(m, k);
				A = mtod(m, u_char *)[k];
				continue;
#else
				return (0);
#endif
			}
			A = p[k];
			continue;

		case USBPF_LDX|USBPF_MSH|USBPF_B:
			k = pc->k;
			if (k >= buflen) {
#ifdef _KERNEL
				register struct mbuf *m;

				if (buflen != 0)
					return (0);
				m = (struct mbuf *)p;
				MINDEX(m, k);
				X = (mtod(m, u_char *)[k] & 0xf) << 2;
				continue;
#else
				return (0);
#endif
			}
			X = (p[pc->k] & 0xf) << 2;
			continue;

		case USBPF_LD|USBPF_IMM:
			A = pc->k;
			continue;

		case USBPF_LDX|USBPF_IMM:
			X = pc->k;
			continue;

		case USBPF_LD|USBPF_MEM:
			A = mem[pc->k];
			continue;

		case USBPF_LDX|USBPF_MEM:
			X = mem[pc->k];
			continue;

		case USBPF_ST:
			mem[pc->k] = A;
			continue;

		case USBPF_STX:
			mem[pc->k] = X;
			continue;

		case USBPF_JMP|USBPF_JA:
			pc += pc->k;
			continue;

		case USBPF_JMP|USBPF_JGT|USBPF_K:
			pc += (A > pc->k) ? pc->jt : pc->jf;
			continue;

		case USBPF_JMP|USBPF_JGE|USBPF_K:
			pc += (A >= pc->k) ? pc->jt : pc->jf;
			continue;

		case USBPF_JMP|USBPF_JEQ|USBPF_K:
			pc += (A == pc->k) ? pc->jt : pc->jf;
			continue;

		case USBPF_JMP|USBPF_JSET|USBPF_K:
			pc += (A & pc->k) ? pc->jt : pc->jf;
			continue;

		case USBPF_JMP|USBPF_JGT|USBPF_X:
			pc += (A > X) ? pc->jt : pc->jf;
			continue;

		case USBPF_JMP|USBPF_JGE|USBPF_X:
			pc += (A >= X) ? pc->jt : pc->jf;
			continue;

		case USBPF_JMP|USBPF_JEQ|USBPF_X:
			pc += (A == X) ? pc->jt : pc->jf;
			continue;

		case USBPF_JMP|USBPF_JSET|USBPF_X:
			pc += (A & X) ? pc->jt : pc->jf;
			continue;

		case USBPF_ALU|USBPF_ADD|USBPF_X:
			A += X;
			continue;

		case USBPF_ALU|USBPF_SUB|USBPF_X:
			A -= X;
			continue;

		case USBPF_ALU|USBPF_MUL|USBPF_X:
			A *= X;
			continue;

		case USBPF_ALU|USBPF_DIV|USBPF_X:
			if (X == 0)
				return (0);
			A /= X;
			continue;

		case USBPF_ALU|USBPF_AND|USBPF_X:
			A &= X;
			continue;

		case USBPF_ALU|USBPF_OR|USBPF_X:
			A |= X;
			continue;

		case USBPF_ALU|USBPF_LSH|USBPF_X:
			A <<= X;
			continue;

		case USBPF_ALU|USBPF_RSH|USBPF_X:
			A >>= X;
			continue;

		case USBPF_ALU|USBPF_ADD|USBPF_K:
			A += pc->k;
			continue;

		case USBPF_ALU|USBPF_SUB|USBPF_K:
			A -= pc->k;
			continue;

		case USBPF_ALU|USBPF_MUL|USBPF_K:
			A *= pc->k;
			continue;

		case USBPF_ALU|USBPF_DIV|USBPF_K:
			A /= pc->k;
			continue;

		case USBPF_ALU|USBPF_AND|USBPF_K:
			A &= pc->k;
			continue;

		case USBPF_ALU|USBPF_OR|USBPF_K:
			A |= pc->k;
			continue;

		case USBPF_ALU|USBPF_LSH|USBPF_K:
			A <<= pc->k;
			continue;

		case USBPF_ALU|USBPF_RSH|USBPF_K:
			A >>= pc->k;
			continue;

		case USBPF_ALU|USBPF_NEG:
			A = -A;
			continue;

		case USBPF_MISC|USBPF_TAX:
			X = A;
			continue;

		case USBPF_MISC|USBPF_TXA:
			A = X;
			continue;
		}
	}
}

static void
usbpf_free(struct usbpf_d *ud)
{

	switch (ud->ud_bufmode) {
	case USBPF_BUFMODE_BUFFER:
		return (usbpf_buffer_free(ud));
	default:
		panic("usbpf_buf_free");
	}
}

/*
 * Notify the buffer model that a buffer has moved into the hold position.
 */
static void
usbpf_bufheld(struct usbpf_d *ud)
{

	USBPFD_LOCK_ASSERT(ud);
}

/*
 * Free buffers currently in use by a descriptor.
 * Called on close.
 */
static void
usbpf_freed(struct usbpf_d *ud)
{

	/*
	 * We don't need to lock out interrupts since this descriptor has
	 * been detached from its interface and it yet hasn't been marked
	 * free.
	 */
	usbpf_free(ud);
	if (ud->ud_rfilter != NULL)
		free((caddr_t)ud->ud_rfilter, M_USBPF);
	if (ud->ud_wfilter != NULL)
		free((caddr_t)ud->ud_wfilter, M_USBPF);
	mtx_destroy(&ud->ud_mtx);
}

/*
 * Close the descriptor by detaching it from its interface,
 * deallocating its buffers, and marking it free.
 */
static void
usbpf_dtor(void *data)
{
	struct usbpf_d *ud = data;

	USBPFD_LOCK(ud);
	if (ud->ud_state == USBPF_WAITING)
		callout_stop(&ud->ud_callout);
	ud->ud_state = USBPF_IDLE;
	USBPFD_UNLOCK(ud);
	funsetown(&ud->ud_sigio);
	mtx_lock(&usbpf_mtx);
	if (ud->ud_bif)
		usbpf_detachd(ud);
	mtx_unlock(&usbpf_mtx);
	selwakeuppri(&ud->ud_sel, PRIUSB);
	knlist_destroy(&ud->ud_sel.si_note);
	callout_drain(&ud->ud_callout);
	usbpf_freed(ud);
	free(ud, M_USBPF);
}

/*
 * Open device.  Returns ENXIO for illegal minor device number,
 * EBUSY if file is open by another process.
 */
/* ARGSUSED */
static	int
usbpf_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct usbpf_d *ud;
	int error;

	ud = malloc(sizeof(*ud), M_USBPF, M_WAITOK | M_ZERO);
	error = devfs_set_cdevpriv(ud, usbpf_dtor);
	if (error != 0) {
		free(ud, M_USBPF);
		return (error);
	}

	usbpf_buffer_init(ud);
	ud->ud_bufmode = USBPF_BUFMODE_BUFFER;
	ud->ud_sig = SIGIO;
	ud->ud_direction = USBPF_D_INOUT;
	ud->ud_pid = td->td_proc->p_pid;
	mtx_init(&ud->ud_mtx, devtoname(dev), "usbpf cdev lock", MTX_DEF);
	callout_init_mtx(&ud->ud_callout, &ud->ud_mtx, 0);
	knlist_init_mtx(&ud->ud_sel.si_note, &ud->ud_mtx);

	return (0);
}

static int
usbpf_uiomove(struct usbpf_d *ud, caddr_t buf, u_int len, struct uio *uio)
{

	if (ud->ud_bufmode != USBPF_BUFMODE_BUFFER)
		return (EOPNOTSUPP);
	return (usbpf_buffer_uiomove(ud, buf, len, uio));
}

/*
 *  usbpf_read - read next chunk of packets from buffers
 */
static	int
usbpf_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct usbpf_d *ud;
	int error;
	int non_block;
	int timed_out;

	error = devfs_get_cdevpriv((void **)&ud);
	if (error != 0)
		return (error);

	/*
	 * Restrict application to use a buffer the same size as
	 * as kernel buffers.
	 */
	if (uio->uio_resid != ud->ud_bufsize)
		return (EINVAL);

	non_block = ((ioflag & O_NONBLOCK) != 0);

	USBPFD_LOCK(ud);
	ud->ud_pid = curthread->td_proc->p_pid;
	if (ud->ud_bufmode != USBPF_BUFMODE_BUFFER) {
		USBPFD_UNLOCK(ud);
		return (EOPNOTSUPP);
	}
	if (ud->ud_state == USBPF_WAITING)
		callout_stop(&ud->ud_callout);
	timed_out = (ud->ud_state == USBPF_TIMED_OUT);
	ud->ud_state = USBPF_IDLE;
	/*
	 * If the hold buffer is empty, then do a timed sleep, which
	 * ends when the timeout expires or when enough packets
	 * have arrived to fill the store buffer.
	 */
	while (ud->ud_hbuf == NULL) {
		if (ud->ud_slen != 0) {
			/*
			 * A packet(s) either arrived since the previous
			 * read or arrived while we were asleep.
			 */
			if (ud->ud_immediate || non_block || timed_out) {
				/*
				 * Rotate the buffers and return what's here
				 * if we are in immediate mode, non-blocking
				 * flag is set, or this descriptor timed out.
				 */
				USBPF_ROTATE_BUFFERS(ud);
				break;
			}
		}

		/*
		 * No data is available, check to see if the usbpf device
		 * is still pointed at a real interface.  If not, return
		 * ENXIO so that the userland process knows to rebind
		 * it before using it again.
		 */
		if (ud->ud_bif == NULL) {
			USBPFD_UNLOCK(ud);
			return (ENXIO);
		}

		if (non_block) {
			USBPFD_UNLOCK(ud);
			return (EWOULDBLOCK);
		}
		error = msleep(ud, &ud->ud_mtx, PRIUSB|PCATCH,
		    "uff", ud->ud_rtout);
		if (error == EINTR || error == ERESTART) {
			USBPFD_UNLOCK(ud);
			return (error);
		}
		if (error == EWOULDBLOCK) {
			/*
			 * On a timeout, return what's in the buffer,
			 * which may be nothing.  If there is something
			 * in the store buffer, we can rotate the buffers.
			 */
			if (ud->ud_hbuf)
				/*
				 * We filled up the buffer in between
				 * getting the timeout and arriving
				 * here, so we don't need to rotate.
				 */
				break;

			if (ud->ud_slen == 0) {
				USBPFD_UNLOCK(ud);
				return (0);
			}
			USBPF_ROTATE_BUFFERS(ud);
			break;
		}
	}
	/*
	 * At this point, we know we have something in the hold slot.
	 */
	USBPFD_UNLOCK(ud);

	/*
	 * Move data from hold buffer into user space.
	 * We know the entire buffer is transferred since
	 * we checked above that the read buffer is usbpf_bufsize bytes.
	 *
	 * XXXRW: More synchronization needed here: what if a second thread
	 * issues a read on the same fd at the same time?  Don't want this
	 * getting invalidated.
	 */
	error = usbpf_uiomove(ud, ud->ud_hbuf, ud->ud_hlen, uio);

	USBPFD_LOCK(ud);
	ud->ud_fbuf = ud->ud_hbuf;
	ud->ud_hbuf = NULL;
	ud->ud_hlen = 0;
	usbpf_buf_reclaimed(ud);
	USBPFD_UNLOCK(ud);

	return (error);
}

static int
usbpf_write(struct cdev *dev, struct uio *uio, int ioflag)
{

	/* NOT IMPLEMENTED */
	return (ENOSYS);
}

static int
usbpf_ioctl_sblen(struct usbpf_d *ud, u_int *i)
{

	if (ud->ud_bufmode != USBPF_BUFMODE_BUFFER)
		return (EOPNOTSUPP);
	return (usbpf_buffer_ioctl_sblen(ud, i));
}

/*
 * Reset a descriptor by flushing its packet buffer and clearing the receive
 * and drop counts.  This is doable for kernel-only buffers, but with
 * zero-copy buffers, we can't write to (or rotate) buffers that are
 * currently owned by userspace.  It would be nice if we could encapsulate
 * this logic in the buffer code rather than here.
 */
static void
usbpf_reset_d(struct usbpf_d *ud)
{

	USBPFD_LOCK_ASSERT(ud);

	if ((ud->ud_hbuf != NULL) &&
	    (ud->ud_bufmode != USBPF_BUFMODE_ZBUF || usbpf_canfreebuf(ud))) {
		/* Free the hold buffer. */
		ud->ud_fbuf = ud->ud_hbuf;
		ud->ud_hbuf = NULL;
		ud->ud_hlen = 0;
		usbpf_buf_reclaimed(ud);
	}
	if (usbpf_canwritebuf(ud))
		ud->ud_slen = 0;
	ud->ud_rcount = 0;
	ud->ud_dcount = 0;
	ud->ud_fcount = 0;
	ud->ud_wcount = 0;
	ud->ud_wfcount = 0;
	ud->ud_wdcount = 0;
	ud->ud_zcopy = 0;
}

static int
usbpf_setif(struct usbpf_d *ud, struct usbpf_ifreq *ufr)
{
	struct usbpf_if *uif;
	struct usb_bus *theywant;

	theywant = usb_bus_find(ufr->ufr_name);
	if (theywant == NULL || theywant->uif == NULL)
		return (ENXIO);

	uif = theywant->uif;

	switch (ud->ud_bufmode) {
	case USBPF_BUFMODE_BUFFER:
		if (ud->ud_sbuf == NULL)
			usbpf_buffer_alloc(ud);
		KASSERT(ud->ud_sbuf != NULL, ("%s: ud_sbuf == NULL", __func__));
		break;

	default:
		panic("usbpf_setif: bufmode %d", ud->ud_bufmode);
	}
	if (uif != ud->ud_bif) {
		if (ud->ud_bif)
			/*
			 * Detach if attached to something else.
			 */
			usbpf_detachd(ud);

		usbpf_attachd(ud, uif);
	}
	USBPFD_LOCK(ud);
	usbpf_reset_d(ud);
	USBPFD_UNLOCK(ud);
	return (0);
}

/*
 * Set d's packet filter program to fp.  If this file already has a filter,
 * free it and replace it.  Returns EINVAL for bogus requests.
 */
static int
usbpf_setf(struct usbpf_d *ud, struct usbpf_program *fp, u_long cmd)
{
	struct usbpf_insn *fcode, *old;
	u_int wfilter, flen, size;

	if (cmd == UIOCSETWF) {
		old = ud->ud_wfilter;
		wfilter = 1;
	} else {
		wfilter = 0;
		old = ud->ud_rfilter;
	}
	if (fp->uf_insns == NULL) {
		if (fp->uf_len != 0)
			return (EINVAL);
		USBPFD_LOCK(ud);
		if (wfilter)
			ud->ud_wfilter = NULL;
		else {
			ud->ud_rfilter = NULL;
			if (cmd == UIOCSETF)
				usbpf_reset_d(ud);
		}
		USBPFD_UNLOCK(ud);
		if (old != NULL)
			free((caddr_t)old, M_USBPF);
		return (0);
	}
	flen = fp->uf_len;
	if (flen > usbpf_maxinsns)
		return (EINVAL);

	size = flen * sizeof(*fp->uf_insns);
	fcode = (struct usbpf_insn *)malloc(size, M_USBPF, M_WAITOK);
	if (copyin((caddr_t)fp->uf_insns, (caddr_t)fcode, size) == 0 &&
	    usbpf_validate(fcode, (int)flen)) {
		USBPFD_LOCK(ud);
		if (wfilter)
			ud->ud_wfilter = fcode;
		else {
			ud->ud_rfilter = fcode;
			if (cmd == UIOCSETF)
				usbpf_reset_d(ud);
		}
		USBPFD_UNLOCK(ud);
		if (old != NULL)
			free((caddr_t)old, M_USBPF);

		return (0);
	}
	free((caddr_t)fcode, M_USBPF);
	return (EINVAL);
}

static int
usbpf_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags,
    struct thread *td)
{
	struct usbpf_d *ud;
	int error;

	error = devfs_get_cdevpriv((void **)&ud);
	if (error != 0)
		return (error);

	/*
	 * Refresh PID associated with this descriptor.
	 */
	USBPFD_LOCK(ud);
	ud->ud_pid = td->td_proc->p_pid;
	if (ud->ud_state == USBPF_WAITING)
		callout_stop(&ud->ud_callout);
	ud->ud_state = USBPF_IDLE;
	USBPFD_UNLOCK(ud);

	if (ud->ud_locked == 1) {
		switch (cmd) {
		case UIOCGBLEN:
		case UIOCSBLEN:
		case UIOCVERSION:
			break;
		default:
			return (EPERM);
		}
	}

	switch (cmd) {

	default:
		error = EINVAL;
		break;

	/*
	 * Get buffer len [for read()].
	 */
	case UIOCGBLEN:
		*(u_int *)addr = ud->ud_bufsize;
		break;

	/*
	 * Set buffer length.
	 */
	case UIOCSBLEN:
		error = usbpf_ioctl_sblen(ud, (u_int *)addr);
		break;

	/*
	 * Set read filter.
	 */
	case UIOCSETF:
		error = usbpf_setf(ud, (struct usbpf_program *)addr, cmd);
		break;

	/*
	 * Set read timeout.
	 */
	case UIOCSRTIMEOUT:
		{
			struct timeval *tv = (struct timeval *)addr;

			/*
			 * Subtract 1 tick from tvtohz() since this isn't
			 * a one-shot timer.
			 */
			if ((error = itimerfix(tv)) == 0)
				ud->ud_rtout = tvtohz(tv) - 1;
			break;
		}

	/*
	 * Get read timeout.
	 */
	case UIOCGRTIMEOUT:
		{
			struct timeval *tv = (struct timeval *)addr;

			tv->tv_sec = ud->ud_rtout / hz;
			tv->tv_usec = (ud->ud_rtout % hz) * tick;
			break;
		}

	/*
	 * Get packet stats.
	 */
	case UIOCGSTATS:
		{
			struct usbpf_stat *us = (struct usbpf_stat *)addr;

			/* XXXCSJP overflow */
			us->us_recv = ud->ud_rcount;
			us->us_drop = ud->ud_dcount;
			break;
		}

	case UIOCVERSION:
		{
			struct usbpf_version *uv = (struct usbpf_version *)addr;

			uv->uv_major = USBPF_MAJOR_VERSION;
			uv->uv_minor = USBPF_MINOR_VERSION;
			break;
		}

	/*
	 * Set interface.
	 */
	case UIOCSETIF:
		error = usbpf_setif(ud, (struct usbpf_ifreq *)addr);
		break;

	}
	return (error);
}

/*
 * Support for select() and poll() system calls
 *
 * Return true iff the specific operation will not block indefinitely.
 * Otherwise, return false but make a note that a selwakeup() must be done.
 */
static int
usbpf_poll(struct cdev *dev, int events, struct thread *td)
{

	/* NOT IMPLEMENTED */
	return (ENOSYS);
}

/*
 * Support for kevent() system call.  Register EVFILT_READ filters and
 * reject all others.
 */
int
usbpf_kqfilter(struct cdev *dev, struct knote *kn)
{

	/* NOT IMPLEMENTED */
	return (ENOSYS);
}

/*
 * Attach file to the usbpf interface, i.e. make d listen on bp.
 */
static void
usbpf_attachd(struct usbpf_d *ud, struct usbpf_if *uif)
{

	USBPFIF_LOCK(uif);
	ud->ud_bif = uif;
	LIST_INSERT_HEAD(&uif->uif_dlist, ud, ud_next);

	usbpf_uifd_cnt++;
	USBPFIF_UNLOCK(uif);
}

/*
 * Detach a file from its interface.
 */
static void
usbpf_detachd(struct usbpf_d *ud)
{
	struct usbpf_if *uif;
	struct usb_bus *ubus;

	uif = ud->ud_bif;
	USBPFIF_LOCK(uif);
	USBPFD_LOCK(ud);
	ubus = ud->ud_bif->uif_ubus;

	/*
	 * Remove d from the interface's descriptor list.
	 */
	LIST_REMOVE(ud, ud_next);

	usbpf_uifd_cnt--;
	ud->ud_bif = NULL;
	USBPFD_UNLOCK(ud);
	USBPFIF_UNLOCK(uif);
}

void
usbpf_attach(struct usb_bus *ubus, struct usbpf_if **driverp)
{
	struct usbpf_if *uif;

	uif = malloc(sizeof(*uif), M_USBPF, M_WAITOK | M_ZERO);
	LIST_INIT(&uif->uif_dlist);
	uif->uif_ubus = ubus;
	mtx_init(&uif->uif_mtx, "usbpf interface lock", NULL, MTX_DEF);
	KASSERT(*driverp == NULL,
	    ("usbpf_attach: driverp already initialized"));
	*driverp = uif;

	mtx_lock(&usbpf_mtx);
	LIST_INSERT_HEAD(&usbpf_iflist, uif, uif_next);
	mtx_unlock(&usbpf_mtx);

	if (bootverbose)
		device_printf(ubus->parent, "usbpf attached\n");
}

/*
 * If there are processes sleeping on this descriptor, wake them up.
 */
static __inline void
usbpf_wakeup(struct usbpf_d *ud)
{

	USBPFD_LOCK_ASSERT(ud);
	if (ud->ud_state == USBPF_WAITING) {
		callout_stop(&ud->ud_callout);
		ud->ud_state = USBPF_IDLE;
	}
	wakeup(ud);
	if (ud->ud_async && ud->ud_sig && ud->ud_sigio)
		pgsigio(&ud->ud_sigio, ud->ud_sig, 0);

	selwakeuppri(&ud->ud_sel, PRIUSB);
	KNOTE_LOCKED(&ud->ud_sel.si_note, 0);
}

void
usbpf_detach(struct usb_bus *ubus)
{
	struct usbpf_if	*uif;
	struct usbpf_d	*ud;

	/* Locate USBPF interface information */
	mtx_lock(&usbpf_mtx);
	LIST_FOREACH(uif, &usbpf_iflist, uif_next) {
		if (ubus == uif->uif_ubus)
			break;
	}

	/* Interface wasn't attached */
	if ((uif == NULL) || (uif->uif_ubus == NULL)) {
		mtx_unlock(&usbpf_mtx);
		printf("usbpf_detach: not attached\n");	/* XXX */
		return;
	}

	LIST_REMOVE(uif, uif_next);
	mtx_unlock(&usbpf_mtx);

	while ((ud = LIST_FIRST(&uif->uif_dlist)) != NULL) {
		usbpf_detachd(ud);
		USBPFD_LOCK(ud);
		usbpf_wakeup(ud);
		USBPFD_UNLOCK(ud);
	}

	mtx_destroy(&uif->uif_mtx);
	free(uif, M_USBPF);
}

/* Time stamping functions */
#define	USBPF_T_MICROTIME	0x0000
#define	USBPF_T_NANOTIME	0x0001
#define	USBPF_T_BINTIME		0x0002
#define	USBPF_T_NONE		0x0003
#define	USBPF_T_FORMAT_MASK	0x0003
#define	USBPF_T_NORMAL		0x0000
#define	USBPF_T_FAST		0x0100
#define	USBPF_T_MONOTONIC	0x0200
#define	USBPF_T_FORMAT(t)	((t) & USBPF_T_FORMAT_MASK)

#define	USBPF_TSTAMP_NONE	0
#define	USBPF_TSTAMP_FAST	1
#define	USBPF_TSTAMP_NORMAL	2

static int
usbpf_ts_quality(int tstype)
{

	if (tstype == USBPF_T_NONE)
		return (USBPF_TSTAMP_NONE);
	if ((tstype & USBPF_T_FAST) != 0)
		return (USBPF_TSTAMP_FAST);

	return (USBPF_TSTAMP_NORMAL);
}

static int
usbpf_gettime(struct bintime *bt, int tstype)
{
	int quality;

	quality = usbpf_ts_quality(tstype);
	if (quality == USBPF_TSTAMP_NONE)
		return (quality);
	if (quality == USBPF_TSTAMP_NORMAL)
		binuptime(bt);
	else
		getbinuptime(bt);

	return (quality);
}

/*
 * If the buffer mechanism has a way to decide that a held buffer can be made
 * free, then it is exposed via the usbpf_canfreebuf() interface.  (1) is
 * returned if the buffer can be discarded, (0) is returned if it cannot.
 */
static int
usbpf_canfreebuf(struct usbpf_d *ud)
{

	USBPFD_LOCK_ASSERT(ud);

	return (0);
}

/*
 * Allow the buffer model to indicate that the current store buffer is
 * immutable, regardless of the appearance of space.  Return (1) if the
 * buffer is writable, and (0) if not.
 */
static int
usbpf_canwritebuf(struct usbpf_d *ud)
{

	USBPFD_LOCK_ASSERT(ud);
	return (1);
}

/*
 * Notify buffer model that an attempt to write to the store buffer has
 * resulted in a dropped packet, in which case the buffer may be considered
 * full.
 */
static void
usbpf_buffull(struct usbpf_d *ud)
{

	USBPFD_LOCK_ASSERT(ud);
}

/*
 * This function gets called when the free buffer is re-assigned.
 */
static void
usbpf_buf_reclaimed(struct usbpf_d *ud)
{

	USBPFD_LOCK_ASSERT(ud);

	switch (ud->ud_bufmode) {
	case USBPF_BUFMODE_BUFFER:
		return;

	default:
		panic("usbpf_buf_reclaimed");
	}
}

#define	SIZEOF_USBPF_HDR(type)	\
    (offsetof(type, uh_hdrlen) + sizeof(((type *)0)->uh_hdrlen))

static int
usbpf_hdrlen(struct usbpf_d *ud)
{
	int hdrlen;

	hdrlen = ud->ud_bif->uif_hdrlen;
	hdrlen += SIZEOF_USBPF_HDR(struct usbpf_xhdr);
	hdrlen = USBPF_WORDALIGN(hdrlen);

	return (hdrlen - ud->ud_bif->uif_hdrlen);
}

static void
usbpf_bintime2ts(struct bintime *bt, struct usbpf_ts *ts, int tstype)
{
	struct bintime bt2;
	struct timeval tsm;
	struct timespec tsn;

	if ((tstype & USBPF_T_MONOTONIC) == 0) {
		bt2 = *bt;
		bintime_add(&bt2, &boottimebin);
		bt = &bt2;
	}
	switch (USBPF_T_FORMAT(tstype)) {
	case USBPF_T_MICROTIME:
		bintime2timeval(bt, &tsm);
		ts->ut_sec = tsm.tv_sec;
		ts->ut_frac = tsm.tv_usec;
		break;
	case USBPF_T_NANOTIME:
		bintime2timespec(bt, &tsn);
		ts->ut_sec = tsn.tv_sec;
		ts->ut_frac = tsn.tv_nsec;
		break;
	case USBPF_T_BINTIME:
		ts->ut_sec = bt->sec;
		ts->ut_frac = bt->frac;
		break;
	}
}

/*
 * Move the packet data from interface memory (pkt) into the
 * store buffer.  "cpfn" is the routine called to do the actual data
 * transfer.  bcopy is passed in to copy contiguous chunks, while
 * usbpf_append_mbuf is passed in to copy mbuf chains.  In the latter case,
 * pkt is really an mbuf.
 */
static void
catchpacket(struct usbpf_d *ud, u_char *pkt, u_int pktlen, u_int snaplen,
    void (*cpfn)(struct usbpf_d *, caddr_t, u_int, void *, u_int),
    struct bintime *bt)
{
	struct usbpf_xhdr hdr;
	int caplen, curlen, hdrlen, totlen;
	int do_wakeup = 0;
	int do_timestamp;
	int tstype;

	USBPFD_LOCK_ASSERT(ud);

	/*
	 * Detect whether user space has released a buffer back to us, and if
	 * so, move it from being a hold buffer to a free buffer.  This may
	 * not be the best place to do it (for example, we might only want to
	 * run this check if we need the space), but for now it's a reliable
	 * spot to do it.
	 */
	if (ud->ud_fbuf == NULL && usbpf_canfreebuf(ud)) {
		ud->ud_fbuf = ud->ud_hbuf;
		ud->ud_hbuf = NULL;
		ud->ud_hlen = 0;
		usbpf_buf_reclaimed(ud);
	}

	/*
	 * Figure out how many bytes to move.  If the packet is
	 * greater or equal to the snapshot length, transfer that
	 * much.  Otherwise, transfer the whole packet (unless
	 * we hit the buffer size limit).
	 */
	hdrlen = usbpf_hdrlen(ud);
	totlen = hdrlen + min(snaplen, pktlen);
	if (totlen > ud->ud_bufsize)
		totlen = ud->ud_bufsize;

	/*
	 * Round up the end of the previous packet to the next longword.
	 *
	 * Drop the packet if there's no room and no hope of room
	 * If the packet would overflow the storage buffer or the storage
	 * buffer is considered immutable by the buffer model, try to rotate
	 * the buffer and wakeup pending processes.
	 */
	curlen = USBPF_WORDALIGN(ud->ud_slen);
	if (curlen + totlen > ud->ud_bufsize || !usbpf_canwritebuf(ud)) {
		if (ud->ud_fbuf == NULL) {
			/*
			 * There's no room in the store buffer, and no
			 * prospect of room, so drop the packet.  Notify the
			 * buffer model.
			 */
			usbpf_buffull(ud);
			++ud->ud_dcount;
			return;
		}
		USBPF_ROTATE_BUFFERS(ud);
		do_wakeup = 1;
		curlen = 0;
	} else if (ud->ud_immediate || ud->ud_state == USBPF_TIMED_OUT)
		/*
		 * Immediate mode is set, or the read timeout has already
		 * expired during a select call.  A packet arrived, so the
		 * reader should be woken up.
		 */
		do_wakeup = 1;
	caplen = totlen - hdrlen;
	tstype = ud->ud_tstamp;
	do_timestamp = tstype != USBPF_T_NONE;

	/*
	 * Append the usbpf header.  Note we append the actual header size, but
	 * move forward the length of the header plus padding.
	 */
	bzero(&hdr, sizeof(hdr));
	if (do_timestamp)
		usbpf_bintime2ts(bt, &hdr.uh_tstamp, tstype);
	hdr.uh_datalen = pktlen;
	hdr.uh_hdrlen = hdrlen;
	hdr.uh_caplen = caplen;
	usbpf_append_bytes(ud, ud->ud_sbuf, curlen, &hdr, sizeof(hdr));

	/*
	 * Copy the packet data into the store buffer and update its length.
	 */
	(*cpfn)(ud, ud->ud_sbuf, curlen + hdrlen, pkt, caplen);
	ud->ud_slen = curlen + totlen;

	if (do_wakeup)
		usbpf_wakeup(ud);
}

/*
 * Incoming linkage from device drivers.  Process the packet pkt, of length
 * pktlen, which is stored in a contiguous buffer.  The packet is parsed
 * by each process' filter, and if accepted, stashed into the corresponding
 * buffer.
 */
static void
usbpf_tap(struct usbpf_if *uif, u_char *pkt, u_int pktlen)
{
	struct bintime bt;
	struct usbpf_d *ud;
	u_int slen;
	int gottime;

	gottime = USBPF_TSTAMP_NONE;
	USBPFIF_LOCK(uif);
	LIST_FOREACH(ud, &uif->uif_dlist, ud_next) {
		USBPFD_LOCK(ud);
		++ud->ud_rcount;
		slen = usbpf_filter(ud->ud_rfilter, pkt, pktlen, pktlen);
		if (slen != 0) {
			ud->ud_fcount++;
			if (gottime < usbpf_ts_quality(ud->ud_tstamp))
				gottime = usbpf_gettime(&bt, ud->ud_tstamp);
			catchpacket(ud, pkt, pktlen, slen,
			    usbpf_append_bytes, &bt);
		}
		USBPFD_UNLOCK(ud);
	}
	USBPFIF_UNLOCK(uif);
}

static uint32_t
usbpf_aggregate_xferflags(struct usb_xfer_flags *flags)
{
	uint32_t val = 0;

	if (flags->force_short_xfer == 1)
		val |= USBPF_FLAG_FORCE_SHORT_XFER;
	if (flags->short_xfer_ok == 1)
		val |= USBPF_FLAG_SHORT_XFER_OK;
	if (flags->short_frames_ok == 1)
		val |= USBPF_FLAG_SHORT_FRAMES_OK;
	if (flags->pipe_bof == 1)
		val |= USBPF_FLAG_PIPE_BOF;
	if (flags->proxy_buffer == 1)
		val |= USBPF_FLAG_PROXY_BUFFER;
	if (flags->ext_buffer == 1)
		val |= USBPF_FLAG_EXT_BUFFER;
	if (flags->manual_status == 1)
		val |= USBPF_FLAG_MANUAL_STATUS;
	if (flags->no_pipe_ok == 1)
		val |= USBPF_FLAG_NO_PIPE_OK;
	if (flags->stall_pipe == 1)
		val |= USBPF_FLAG_STALL_PIPE;
	return (val);
}

static uint32_t
usbpf_aggregate_status(struct usb_xfer_flags_int *flags)
{
	uint32_t val = 0;

	if (flags->open == 1)
		val |= USBPF_STATUS_OPEN;
	if (flags->transferring == 1)
		val |= USBPF_STATUS_TRANSFERRING;
	if (flags->did_dma_delay == 1)
		val |= USBPF_STATUS_DID_DMA_DELAY;
	if (flags->did_close == 1)
		val |= USBPF_STATUS_DID_CLOSE;
	if (flags->draining == 1)
		val |= USBPF_STATUS_DRAINING;
	if (flags->started == 1)
		val |= USBPF_STATUS_STARTED;
	if (flags->bandwidth_reclaimed == 1)
		val |= USBPF_STATUS_BW_RECLAIMED;
	if (flags->control_xfr == 1)
		val |= USBPF_STATUS_CONTROL_XFR;
	if (flags->control_hdr == 1)
		val |= USBPF_STATUS_CONTROL_HDR;
	if (flags->control_act == 1)
		val |= USBPF_STATUS_CONTROL_ACT;
	if (flags->control_stall == 1)
		val |= USBPF_STATUS_CONTROL_STALL;
	if (flags->short_frames_ok == 1)
		val |= USBPF_STATUS_SHORT_FRAMES_OK;
	if (flags->short_xfer_ok == 1)
		val |= USBPF_STATUS_SHORT_XFER_OK;
#if USB_HAVE_BUSDMA
	if (flags->bdma_enable == 1)
		val |= USBPF_STATUS_BDMA_ENABLE;
	if (flags->bdma_no_post_sync == 1)
		val |= USBPF_STATUS_BDMA_NO_POST_SYNC;
	if (flags->bdma_setup == 1)
		val |= USBPF_STATUS_BDMA_SETUP;
#endif
	if (flags->isochronous_xfr == 1)
		val |= USBPF_STATUS_ISOCHRONOUS_XFR;
	if (flags->curr_dma_set == 1)
		val |= USBPF_STATUS_CURR_DMA_SET;
	if (flags->can_cancel_immed == 1)
		val |= USBPF_STATUS_CAN_CANCEL_IMMED;
	if (flags->doing_callback == 1)
		val |= USBPF_STATUS_DOING_CALLBACK;

	return (val);
}

void
usbpf_xfertap(struct usb_xfer *xfer, int type)
{
	struct usb_endpoint *ep = xfer->endpoint;
	struct usb_page_search res;
	struct usb_xfer_root *info = xfer->xroot;
	struct usb_bus *bus = info->bus;
	struct usbpf_pkthdr *up;
	usb_frlength_t isoc_offset = 0;
	int i;
	char *buf, *ptr, *end;

	/*
	 * NB: usbpf_uifd_cnt isn't protected by USBPFIF_LOCK() because it's
	 * not harmful.
	 */
	if (usbpf_uifd_cnt == 0)
		return;

	/*
	 * XXX TODO
	 * Allocating the buffer here causes copy operations twice what's
	 * really inefficient. Copying usbpf_pkthdr and data is for USB packet
	 * read filter to pass a virtually linear buffer.
	 */
	buf = ptr = malloc(sizeof(struct usbpf_pkthdr) + (USB_PAGE_SIZE * 5),
	    M_USBPF, M_NOWAIT);
	if (buf == NULL) {
		printf("usbpf_xfertap: out of memory\n");	/* XXX */
		return;
	}
	end = buf + sizeof(struct usbpf_pkthdr) + (USB_PAGE_SIZE * 5);

	bzero(ptr, sizeof(struct usbpf_pkthdr));
	up = (struct usbpf_pkthdr *)ptr;
	up->up_busunit = htole32(device_get_unit(bus->bdev));
	up->up_type = type;
	up->up_xfertype = ep->edesc->bmAttributes & UE_XFERTYPE;
	up->up_address = xfer->address;
	up->up_endpoint = xfer->endpointno;
	up->up_flags = htole32(usbpf_aggregate_xferflags(&xfer->flags));
	up->up_status = htole32(usbpf_aggregate_status(&xfer->flags_int));
	switch (type) {
	case USBPF_XFERTAP_SUBMIT:
		up->up_length = htole32(xfer->sumlen);
		up->up_frames = htole32(xfer->nframes);
		break;
	case USBPF_XFERTAP_DONE:
		up->up_length = htole32(xfer->actlen);
		up->up_frames = htole32(xfer->aframes);
		break;
	default:
		panic("wrong usbpf type (%d)", type);
	}

	up->up_error = htole32(xfer->error);
	up->up_interval = htole32(xfer->interval);
	ptr += sizeof(struct usbpf_pkthdr);

	for (i = 0; i < up->up_frames; i++) {
		if (ptr + sizeof(u_int32_t) >= end)
			goto done;
		*((u_int32_t *)ptr) = htole32(xfer->frlengths[i]);
		ptr += sizeof(u_int32_t);

		if (ptr + xfer->frlengths[i] >= end)
			goto done;
		if (xfer->flags_int.isochronous_xfr == 1) {
			usbd_get_page(&xfer->frbuffers[0], isoc_offset, &res);
			isoc_offset += xfer->frlengths[i];
		} else
			usbd_get_page(&xfer->frbuffers[i], 0, &res);
		bcopy(res.buffer, ptr, xfer->frlengths[i]);
		ptr += xfer->frlengths[i];
	}

	usbpf_tap(bus->uif, buf, ptr - buf);
done:
	free(buf, M_USBPF);
}

static void
usbpf_append_bytes(struct usbpf_d *ud, caddr_t buf, u_int offset, void *src,
    u_int len)
{

	USBPFD_LOCK_ASSERT(ud);

	switch (ud->ud_bufmode) {
	case USBPF_BUFMODE_BUFFER:
		return (usbpf_buffer_append_bytes(ud, buf, offset, src, len));
	default:
		panic("usbpf_buf_append_bytes");
	}
}

static void
usbpf_drvinit(void *unused)
{

	mtx_init(&usbpf_mtx, "USB packet filter global lock", NULL,
	    MTX_DEF);
	LIST_INIT(&usbpf_iflist);

	usbpf_cdev = make_dev(&usbpf_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "usbpf");
}

static void
usbpf_drvuninit(void)
{

	if (usbpf_cdev != NULL) {
		destroy_dev(usbpf_cdev);
		usbpf_cdev = NULL;
	}
	mtx_destroy(&usbpf_mtx);
}

SYSINIT(usbpf_dev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, usbpf_drvinit, NULL);
SYSUNINIT(usbpf_undev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, usbpf_drvuninit, NULL);

