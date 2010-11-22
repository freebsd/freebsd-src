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
 *
 * $FreeBSD$
 */

#ifndef _DEV_USB_PF_H
#define	_DEV_USB_PF_H

#ifdef _KERNEL
#include <sys/callout.h>
#include <sys/selinfo.h>
#include <sys/queue.h>
#include <sys/conf.h>
#endif

typedef	int32_t	  usbpf_int32;
typedef	u_int32_t usbpf_u_int32;
typedef	int64_t	  usbpf_int64;
typedef	u_int64_t usbpf_u_int64;

struct usbpf_if;

/*
 * Alignment macros.  USBPF_WORDALIGN rounds up to the next
 * even multiple of USBPF_ALIGNMENT.
 */
#define	USBPF_ALIGNMENT sizeof(long)
#define	USBPF_WORDALIGN(x) (((x)+(USBPF_ALIGNMENT-1))&~(USBPF_ALIGNMENT-1))

/*
 * The instruction encodings.
 */

/* instruction classes */
#define	USBPF_CLASS(code) ((code) & 0x07)
#define		USBPF_LD	0x00
#define		USBPF_LDX	0x01
#define		USBPF_ST	0x02
#define		USBPF_STX	0x03
#define		USBPF_ALU	0x04
#define		USBPF_JMP	0x05
#define		USBPF_RET	0x06
#define		USBPF_MISC	0x07

/* ld/ldx fields */
#define	USBPF_SIZE(code)	((code) & 0x18)
#define		USBPF_W		0x00
#define		USBPF_H		0x08
#define		USBPF_B		0x10
#define	USBPF_MODE(code)	((code) & 0xe0)
#define		USBPF_IMM 	0x00
#define		USBPF_ABS	0x20
#define		USBPF_IND	0x40
#define		USBPF_MEM	0x60
#define		USBPF_LEN	0x80
#define		USBPF_MSH	0xa0

/* alu/jmp fields */
#define	USBPF_OP(code)	((code) & 0xf0)
#define		USBPF_ADD	0x00
#define		USBPF_SUB	0x10
#define		USBPF_MUL	0x20
#define		USBPF_DIV	0x30
#define		USBPF_OR	0x40
#define		USBPF_AND	0x50
#define		USBPF_LSH	0x60
#define		USBPF_RSH	0x70
#define		USBPF_NEG	0x80
#define		USBPF_JA	0x00
#define		USBPF_JEQ	0x10
#define		USBPF_JGT	0x20
#define		USBPF_JGE	0x30
#define		USBPF_JSET	0x40
#define	USBPF_SRC(code)	((code) & 0x08)
#define		USBPF_K		0x00
#define		USBPF_X		0x08

/* ret - USBPF_K and USBPF_X also apply */
#define	USBPF_RVAL(code)	((code) & 0x18)
#define		USBPF_A		0x10

/* misc */
#define	USBPF_MISCOP(code) ((code) & 0xf8)
#define		USBPF_TAX	0x00
#define		USBPF_TXA	0x80

/*
 * The instruction data structure.
 */
struct usbpf_insn {
	u_short		code;
	u_char		jt;
	u_char		jf;
	usbpf_u_int32	k;
};

#ifdef _KERNEL

/*
 * Descriptor associated with each open uff file.
 */

struct usbpf_d {
	LIST_ENTRY(usbpf_d) ud_next;	/* Linked list of descriptors */
	/*
	 * Buffer slots: two memory buffers store the incoming packets.
	 *   The model has three slots.  Sbuf is always occupied.
	 *   sbuf (store) - Receive interrupt puts packets here.
	 *   hbuf (hold) - When sbuf is full, put buffer here and
	 *                 wakeup read (replace sbuf with fbuf).
	 *   fbuf (free) - When read is done, put buffer here.
	 * On receiving, if sbuf is full and fbuf is 0, packet is dropped.
	 */
	caddr_t		ud_sbuf;	/* store slot */
	caddr_t		ud_hbuf;	/* hold slot */
	caddr_t		ud_fbuf;	/* free slot */
	int		ud_slen;	/* current length of store buffer */
	int		ud_hlen;	/* current length of hold buffer */

	int		ud_bufsize;	/* absolute length of buffers */

	struct usbpf_if *ud_bif;	/* interface descriptor */
	u_long		ud_rtout;	/* Read timeout in 'ticks' */
	struct usbpf_insn *ud_rfilter;	/* read filter code */
	struct usbpf_insn *ud_wfilter;	/* write filter code */
	void		*ud_bfilter;	/* binary filter code */
	u_int64_t	ud_rcount;	/* number of packets received */
	u_int64_t	ud_dcount;	/* number of packets dropped */

	u_char		ud_promisc;	/* true if listening promiscuously */
	u_char		ud_state;	/* idle, waiting, or timed out */
	u_char		ud_immediate;	/* true to return on packet arrival */
	int		ud_hdrcmplt;	/* false to fill in src lladdr automatically */
	int		ud_direction;	/* select packet direction */
	int		ud_tstamp;	/* select time stamping function */
	int		ud_feedback;	/* true to feed back sent packets */
	int		ud_async;	/* non-zero if packet reception should generate signal */
	int		ud_sig;		/* signal to send upon packet reception */
	struct sigio *	ud_sigio;	/* information for async I/O */
	struct selinfo	ud_sel;		/* bsd select info */
	struct mtx	ud_mtx;		/* mutex for this descriptor */
	struct callout	ud_callout;	/* for USBPF timeouts with select */
	struct label	*ud_label;	/* MAC label for descriptor */
	u_int64_t	ud_fcount;	/* number of packets which matched filter */
	pid_t		ud_pid;		/* PID which created descriptor */
	int		ud_locked;	/* true if descriptor is locked */
	u_int		ud_bufmode;	/* Current buffer mode. */
	u_int64_t	ud_wcount;	/* number of packets written */
	u_int64_t	ud_wfcount;	/* number of packets that matched write filter */
	u_int64_t	ud_wdcount;	/* number of packets dropped during a write */
	u_int64_t	ud_zcopy;	/* number of zero copy operations */
	u_char		ud_compat32;	/* 32-bit stream on LP64 system */
};

#define	USBPFD_LOCK(ud)		mtx_lock(&(ud)->ud_mtx)
#define	USBPFD_UNLOCK(ud)	mtx_unlock(&(ud)->ud_mtx)
#define	USBPFD_LOCK_ASSERT(ud)	mtx_assert(&(ud)->ud_mtx, MA_OWNED)

/*
 * Descriptor associated with each attached hardware interface.
 */
struct usbpf_if {
	LIST_ENTRY(usbpf_if) uif_next; /* list of all interfaces */
	LIST_HEAD(, usbpf_d) uif_dlist;	/* descriptor list */
	u_int uif_hdrlen;		/* length of link header */
	struct usb_bus *uif_ubus;	/* corresponding interface */
	struct mtx	uif_mtx;	/* mutex for interface */
};

#define	USBPFIF_LOCK(uif)	mtx_lock(&(uif)->uif_mtx)
#define	USBPFIF_UNLOCK(uif)	mtx_unlock(&(uif)->uif_mtx)

#endif

/*
 * Structure prepended to each packet.
 */
struct usbpf_ts {
	usbpf_int64	ut_sec;		/* seconds */
	usbpf_u_int64	ut_frac;	/* fraction */
};
struct usbpf_xhdr {
	struct usbpf_ts	uh_tstamp;	/* time stamp */
	usbpf_u_int32	uh_caplen;	/* length of captured portion */
	usbpf_u_int32	uh_datalen;	/* original length of packet */
	u_short		uh_hdrlen;	/* length of uff header (this struct
					   plus alignment padding) */
};

#define	USBPF_BUFMODE_BUFFER	1	/* Kernel buffers with read(). */
#define	USBPF_BUFMODE_ZBUF	2	/* Zero-copy buffers. */

struct usbpf_pkthdr {
	int		up_busunit;	/* Host controller unit number */
	u_char		up_address;	/* USB device address */
	u_char		up_endpoint;	/* USB endpoint */
	u_char		up_type;	/* points SUBMIT / DONE */
	u_char		up_xfertype;	/* Transfer type */
	u_int32_t	up_flags;	/* Transfer flags */
#define	USBPF_FLAG_FORCE_SHORT_XFER	(1 << 0)
#define	USBPF_FLAG_SHORT_XFER_OK	(1 << 1)
#define	USBPF_FLAG_SHORT_FRAMES_OK	(1 << 2)
#define	USBPF_FLAG_PIPE_BOF		(1 << 3)
#define	USBPF_FLAG_PROXY_BUFFER		(1 << 4)
#define	USBPF_FLAG_EXT_BUFFER		(1 << 5)
#define	USBPF_FLAG_MANUAL_STATUS	(1 << 6)
#define	USBPF_FLAG_NO_PIPE_OK		(1 << 7)
#define	USBPF_FLAG_STALL_PIPE		(1 << 8)
	u_int32_t	up_status;	/* Transfer status */
#define	USBPF_STATUS_OPEN		(1 << 0)
#define	USBPF_STATUS_TRANSFERRING	(1 << 1)
#define	USBPF_STATUS_DID_DMA_DELAY	(1 << 2)
#define	USBPF_STATUS_DID_CLOSE		(1 << 3)
#define	USBPF_STATUS_DRAINING		(1 << 4)
#define	USBPF_STATUS_STARTED		(1 << 5)
#define	USBPF_STATUS_BW_RECLAIMED	(1 << 6)
#define	USBPF_STATUS_CONTROL_XFR	(1 << 7)
#define	USBPF_STATUS_CONTROL_HDR	(1 << 8)
#define	USBPF_STATUS_CONTROL_ACT	(1 << 9)
#define	USBPF_STATUS_CONTROL_STALL	(1 << 10)
#define	USBPF_STATUS_SHORT_FRAMES_OK	(1 << 11)
#define	USBPF_STATUS_SHORT_XFER_OK	(1 << 12)
#if USB_HAVE_BUSDMA
#define	USBPF_STATUS_BDMA_ENABLE	(1 << 13)
#define	USBPF_STATUS_BDMA_NO_POST_SYNC	(1 << 14)
#define	USBPF_STATUS_BDMA_SETUP		(1 << 15)
#endif
#define	USBPF_STATUS_ISOCHRONOUS_XFR	(1 << 16)
#define	USBPF_STATUS_CURR_DMA_SET	(1 << 17)
#define	USBPF_STATUS_CAN_CANCEL_IMMED	(1 << 18)
#define	USBPF_STATUS_DOING_CALLBACK	(1 << 19)
	u_int32_t	up_length;	/* Total data length (submit/actual) */
	u_int32_t	up_frames;	/* USB frame number (submit/actual) */
	u_int32_t	up_error;	/* usb_error_t */
	u_int32_t	up_interval;	/* for interrupt and isoc */
	/* sizeof(struct usbpf_pkthdr) == 128 bytes */
	u_char		up_reserved[96];
};

struct usbpf_version {
	u_short		uv_major;
	u_short		uv_minor;
};
#define	USBPF_MAJOR_VERSION	1
#define	USBPF_MINOR_VERSION	1

#define	USBPF_IFNAMSIZ	32
struct usbpf_ifreq {
	/* bus name, e.g. "usbus0" */
	char		ufr_name[USBPF_IFNAMSIZ];
};

/*
 *  Structure for UIOCSETF.
 */
struct usbpf_program {
	u_int			uf_len;
	struct usbpf_insn	*uf_insns;
};

/*
 * Struct returned by UIOCGSTATS.
 */
struct usbpf_stat {
	u_int us_recv;		/* number of packets received */
	u_int us_drop;		/* number of packets dropped */
};

#define	UIOCGBLEN	_IOR('U', 102, u_int)
#define	UIOCSBLEN	_IOWR('U', 102, u_int)
#define	UIOCSETF	_IOW('U', 103, struct usbpf_program)
#define	UIOCSETIF	_IOW('U', 108, struct usbpf_ifreq)
#define	UIOCSRTIMEOUT	_IOW('U', 109, struct timeval)
#define	UIOCGRTIMEOUT	_IOR('U', 110, struct timeval)
#define	UIOCGSTATS	_IOR('U', 111, struct usbpf_stat)
#define	UIOCVERSION	_IOR('U', 113, struct usbpf_version)
#define	UIOCSETWF	_IOW('U', 123, struct usbpf_program)

#define	USBPF_XFERTAP_SUBMIT	0
#define	USBPF_XFERTAP_DONE	1

#ifdef _KERNEL
void	usbpf_attach(struct usb_bus *, struct usbpf_if **);
void	usbpf_detach(struct usb_bus *);
void	usbpf_xfertap(struct usb_xfer *, int);
#endif

#endif
