/*-
 * Copyright (c) 1997, 1998 Nicolas Souchu
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
 *	$Id: ppbconf.h,v 1.14 1999/04/05 15:43:11 peter Exp $
 *
 */
#ifndef __PPBCONF_H
#define __PPBCONF_H

#include <sys/queue.h>

/*
 * Parallel Port Bus sleep/wakeup queue.
 */
#define PPBPRI	(PZERO+8)

/*
 * Parallel Port Chipset mode masks.
 * NIBBLE mode is supposed to be available under each other modes.
 */
#define PPB_COMPATIBLE	0x0	/* Centronics compatible mode */

#define PPB_NIBBLE	0x1	/* reverse 4 bit mode */
#define PPB_PS2		0x2	/* PS/2 byte mode */
#define PPB_EPP		0x4	/* EPP mode, 32 bit */
#define PPB_ECP		0x8	/* ECP mode */

/* mode aliases */
#define PPB_SPP		PPB_NIBBLE|PPB_PS2
#define PPB_BYTE	PPB_PS2

#define PPB_MASK		0x0f
#define PPB_OPTIONS_MASK	0xf0

#define PPB_IS_EPP(mode) (mode & PPB_EPP)
#define PPB_IN_EPP_MODE(dev) (PPB_IS_EPP (ppb_get_mode (dev)))
#define PPB_IN_NIBBLE_MODE(dev) (ppb_get_mode (dev) & PPB_NIBBLE)
#define PPB_IN_PS2_MODE(dev) (ppb_get_mode (dev) & PPB_PS2)

#define n(flags) (~(flags) & (flags))

/*
 * Parallel Port Chipset control bits.
 */
#define STROBE		0x01
#define AUTOFEED	0x02
#define nINIT		0x04
#define SELECTIN	0x08
#define IRQENABLE	0x10
#define PCD		0x20

#define nSTROBE		n(STROBE)
#define nAUTOFEED	n(AUTOFEED)
#define INIT		n(nINIT)
#define nSELECTIN	n(SELECTIN)
#define nPCD		n(PCD)

/*
 * Parallel Port Chipset status bits.
 */
#define TIMEOUT		0x01
#define nFAULT		0x08
#define SELECT		0x10
#define PERROR		0x20
#define nACK		0x40
#define nBUSY		0x80

/*
 * Structure to store status information.
 */
struct ppb_status {
	unsigned char status;

	unsigned int timeout:1;
	unsigned int error:1;
	unsigned int select:1;
	unsigned int paper_end:1;
	unsigned int ack:1;
	unsigned int busy:1;
};

/*
 * How tsleep() is called in ppb_request_bus().
 */
#define PPB_DONTWAIT	0
#define PPB_NOINTR	0
#define PPB_WAIT	0x1
#define PPB_INTR	0x2
#define PPB_POLL	0x4
#define PPB_FOREVER	-1

/*
 * Microsequence stuff.
 */
#define PPB_MS_MAXLEN	64		/* XXX according to MS_INS_MASK */
#define PPB_MS_MAXARGS	3		/* according to MS_ARG_MASK */

/* maximum number of mode dependent
 * submicrosequences for in/out operations
 */
#define PPB_MAX_XFER	6

union ppb_insarg {
	int	i;
	void	*p;
	char	*c;
	int	(* f)(void *, char *);
};

struct ppb_microseq {
	int			opcode;			/* microins. opcode */
	union ppb_insarg	arg[PPB_MS_MAXARGS];	/* arguments */
};

/* microseqences used for GET/PUT operations */
struct ppb_xfer {
	struct ppb_microseq *loop;		/* the loop microsequence */
};

/*
 * Parallel Port Bus Device structure.
 */
struct ppb_data;			/* see below */

struct ppb_context {
	int valid;			/* 1 if the struct is valid */
	int mode;			/* XXX chipset operating mode */

	struct microseq *curpc;		/* pc in curmsq */
	struct microseq *curmsq;	/* currently executed microseqence */
};

struct ppb_device {

	int id_unit;			/* unit of the device */
	char *name;			/* name of the device */

	ushort mode;			/* current mode of the device */
	ushort avm;			/* available IEEE1284 modes of 
					 * the device */

	struct ppb_context ctx;		/* context of the device */

					/* mode dependent get msq. If NULL,
					 * IEEE1284 code is used */
	struct ppb_xfer
		get_xfer[PPB_MAX_XFER];

					/* mode dependent put msq. If NULL,
					 * IEEE1284 code is used */
	struct ppb_xfer
		put_xfer[PPB_MAX_XFER];

	void (*intr)(int);		/* interrupt handler */
	void (*bintr)(struct ppb_device *);	/* interrupt handler */

	void *drv1, *drv2;		/* drivers private data */

	struct ppb_data *ppb;		/* link to the ppbus */

	LIST_ENTRY(ppb_device)	chain;	/* list of devices on the bus */
};

/*
 * Parallel Port Bus Adapter structure.
 */
struct ppb_adapter {

	void (*intr_handler)(int);
	void (*reset_epp_timeout)(int);
	void (*ecp_sync)(int);

	int (*exec_microseq)(int, struct ppb_microseq **);

	int (*setmode)(int, int);
	int (*read)(int, char *, int, int);
	int (*write)(int, char *, int, int);

	void (*outsb_epp)(int, char *, int);
	void (*outsw_epp)(int, char *, int);
	void (*outsl_epp)(int, char *, int);
	void (*insb_epp)(int, char *, int);
	void (*insw_epp)(int, char *, int);
	void (*insl_epp)(int, char *, int);

	u_char (*r_dtr)(int);
	u_char (*r_str)(int);
	u_char (*r_ctr)(int);
	u_char (*r_epp_A)(int);
	u_char (*r_epp_D)(int);
	u_char (*r_ecr)(int);
	u_char (*r_fifo)(int);

	void (*w_dtr)(int, char);
	void (*w_str)(int, char);
	void (*w_ctr)(int, char);
	void (*w_epp_A)(int, char);
	void (*w_epp_D)(int, char);
	void (*w_ecr)(int, char);
	void (*w_fifo)(int, char);
};

/*
 * ppb_link structure.
 */
struct ppb_link {

	int adapter_unit;			/* unit of the adapter */
	int base;				/* base address of the port */
	int id_irq;				/* != 0 if irq enabled */
	int accum;				/* microseq accum */
	char *ptr;				/* current buffer pointer */

#define EPP_1_9		0x0			/* default */
#define EPP_1_7		0x1

	int epp_protocol;			/* EPP protocol: 0=1.9, 1=1.7 */

	struct ppb_adapter *adapter;		/* link to the ppc adapter */
	struct ppb_data *ppbus;			/* link to the ppbus */
};

/*
 * Maximum size of the PnP info string
 */
#define PPB_PnP_STRING_SIZE	256			/* XXX */

/*
 * Parallel Port Bus structure.
 */
struct ppb_data {

#define PPB_PnP_PRINTER	0
#define PPB_PnP_MODEM	1
#define PPB_PnP_NET	2
#define PPB_PnP_HDC	3
#define PPB_PnP_PCMCIA	4
#define PPB_PnP_MEDIA	5
#define PPB_PnP_FDC	6
#define PPB_PnP_PORTS	7
#define PPB_PnP_SCANNER	8
#define PPB_PnP_DIGICAM	9
#define PPB_PnP_UNKNOWN	10
	int	class_id;	/* not a PnP device if class_id < 0 */

	int state;				/* current IEEE1284 state */
	int error;				/* last IEEE1284 error */

	ushort mode;				/* IEEE 1284-1994 mode
						 * NIBBLE, PS2, EPP or ECP */

	struct ppb_link *ppb_link;		/* link to the adapter */
	struct ppb_device *ppb_owner;		/* device which owns the bus */
	LIST_HEAD(, ppb_device)	ppb_devs;	/* list of devices on the bus */
	LIST_ENTRY(ppb_data)	ppb_chain;	/* list of busses */
};

/*
 * Parallel Port Bus driver structure.
 */
struct ppb_driver 
{
    struct ppb_device	*(*probe)(struct ppb_data *ppb);
    int			(*attach)(struct ppb_device *pdp);
    char		*name;
};

extern struct linker_set ppbdriver_set;

extern struct ppb_data *ppb_alloc_bus(void);
extern struct ppb_data *ppb_next_bus(struct ppb_data *);
extern struct ppb_data *ppb_lookup_bus(int);
extern struct ppb_data *ppb_lookup_link(int);

extern int ppb_attach_device(struct ppb_device *);
extern void ppb_remove_device(struct ppb_device *);
extern int ppb_attachdevs(struct ppb_data *);

extern int ppb_request_bus(struct ppb_device *, int);
extern int ppb_release_bus(struct ppb_device *);

extern void ppb_intr(struct ppb_link *);

extern int ppb_poll_device(struct ppb_device *, int, char, char, int);

extern int ppb_reset_epp_timeout(struct ppb_device *);
extern int ppb_ecp_sync(struct ppb_device *);
extern int ppb_get_status(struct ppb_device *, struct ppb_status *);

extern int ppb_set_mode(struct ppb_device *, int);
extern int ppb_write(struct ppb_device *, char *, int, int);

/*
 * These are defined as macros for speedup.
 */
#define ppb_get_base_addr(dev) ((dev)->ppb->ppb_link->base)
#define ppb_get_epp_protocol(dev) ((dev)->ppb->ppb_link->epp_protocol)
#define ppb_get_irq(dev) ((dev)->ppb->ppb_link->id_irq)

#define ppb_get_mode(dev) ((dev)->mode)

/* This set of function access only to the EPP _data_ registers
 * in 8, 16 and 32 bit modes */
#define ppb_outsb_epp(dev,buf,cnt)					    \
			(*(dev)->ppb->ppb_link->adapter->outsb_epp)	    \
			((dev)->ppb->ppb_link->adapter_unit, buf, cnt)
#define ppb_outsw_epp(dev,buf,cnt)					    \
			(*(dev)->ppb->ppb_link->adapter->outsw_epp)	    \
			((dev)->ppb->ppb_link->adapter_unit, buf, cnt)
#define ppb_outsl_epp(dev,buf,cnt)					    \
			(*(dev)->ppb->ppb_link->adapter->outsl_epp)	    \
			((dev)->ppb->ppb_link->adapter_unit, buf, cnt)
#define ppb_insb_epp(dev,buf,cnt)					    \
			(*(dev)->ppb->ppb_link->adapter->insb_epp)	    \
			((dev)->ppb->ppb_link->adapter_unit, buf, cnt)
#define ppb_insw_epp(dev,buf,cnt)					    \
			(*(dev)->ppb->ppb_link->adapter->insw_epp)	    \
			((dev)->ppb->ppb_link->adapter_unit, buf, cnt)
#define ppb_insl_epp(dev,buf,cnt)					    \
			(*(dev)->ppb->ppb_link->adapter->insl_epp)	    \
			((dev)->ppb->ppb_link->adapter_unit, buf, cnt)

#define ppb_repp_A(dev) (*(dev)->ppb->ppb_link->adapter->r_epp_A)	    \
				((dev)->ppb->ppb_link->adapter_unit)
#define ppb_repp_D(dev) (*(dev)->ppb->ppb_link->adapter->r_epp_D)	    \
				((dev)->ppb->ppb_link->adapter_unit)
#define ppb_recr(dev) (*(dev)->ppb->ppb_link->adapter->r_ecr)		    \
				((dev)->ppb->ppb_link->adapter_unit)
#define ppb_rfifo(dev) (*(dev)->ppb->ppb_link->adapter->r_fifo)		    \
				((dev)->ppb->ppb_link->adapter_unit)
#define ppb_wepp_A(dev,byte) (*(dev)->ppb->ppb_link->adapter->w_epp_A)	    \
				((dev)->ppb->ppb_link->adapter_unit, byte)
#define ppb_wepp_D(dev,byte) (*(dev)->ppb->ppb_link->adapter->w_epp_D)	    \
				((dev)->ppb->ppb_link->adapter_unit, byte)
#define ppb_wecr(dev,byte) (*(dev)->ppb->ppb_link->adapter->w_ecr)	    \
				((dev)->ppb->ppb_link->adapter_unit, byte)
#define ppb_wfifo(dev,byte) (*(dev)->ppb->ppb_link->adapter->w_fifo)	    \
				((dev)->ppb->ppb_link->adapter_unit, byte)

#define ppb_rdtr(dev) (*(dev)->ppb->ppb_link->adapter->r_dtr)		    \
				((dev)->ppb->ppb_link->adapter_unit)
#define ppb_rstr(dev) (*(dev)->ppb->ppb_link->adapter->r_str)		    \
				((dev)->ppb->ppb_link->adapter_unit)
#define ppb_rctr(dev) (*(dev)->ppb->ppb_link->adapter->r_ctr)		    \
				((dev)->ppb->ppb_link->adapter_unit)
#define ppb_wdtr(dev,byte) (*(dev)->ppb->ppb_link->adapter->w_dtr)	    \
				((dev)->ppb->ppb_link->adapter_unit, byte)
#define ppb_wstr(dev,byte) (*(dev)->ppb->ppb_link->adapter->w_str)	    \
				((dev)->ppb->ppb_link->adapter_unit, byte)
#define ppb_wctr(dev,byte) (*(dev)->ppb->ppb_link->adapter->w_ctr)	    \
				((dev)->ppb->ppb_link->adapter_unit, byte)

#endif
