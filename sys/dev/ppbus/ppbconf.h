/*-
 * Copyright (c) 1997 Nicolas Souchu
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
 *	$Id$
 *
 */
#ifndef __PPBCONF_H
#define __PPBCONF_H

/*
 * Parallel Port Chipset modes.
 */
#define PPB_AUTODETECT	0x0	/* autodetect */
#define PPB_NIBBLE	0x1	/* standard 4 bit mode */
#define PPB_PS2		0x2	/* PS/2 byte mode */
#define PPB_EPP		0x3	/* EPP mode, 32 bit */
#define PPB_ECP_EPP	0x4	/* ECP in EPP mode */
#define PPB_ECP_PS2	0x5	/* ECP in PS/2 mode */
#define PPB_ECP		0x6	/* ECP mode */
#define PPB_UNKNOWN	0x7	/* the last one */

#define PPB_IS_EPP(mode) (mode == PPB_EPP || mode == PPB_ECP_EPP)

#define PPB_IN_EPP_MODE(dev) (PPB_IS_EPP (ppb_get_mode (dev)))

/*
 * Parallel Port Chipset control bits.
 */
#define STROBE		0x01
#define AUTOFEED	0x02
#define nINIT		0x04
#define SELECTIN	0x08
#define PCD		0x20

/*
 * Parallel Port Chipset status bits.
 */
#define TIMEOUT		0x01
#define nFAULT		0x08
#define SELECT		0x10
#define ERROR		0x20
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
 * How tsleep () is called in ppb_request_bus ().
 */
#define PPB_DONTWAIT	0
#define PPB_NOINTR	0
#define PPB_WAIT	0x1
#define PPB_INTR	0x2

struct ppb_data;	/* see below */

/*
 * Parallel Port Bus Device structure.
 */
struct ppb_device {

	int id_unit;			/* unit of the device */

	void (*intr)(int);		/* interrupt handler */

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

	void (*outsb_epp)(int, char *, int);
	void (*outsw_epp)(int, char *, int);
	void (*outsl_epp)(int, char *, int);
	void (*insb_epp)(int, char *, int);
	void (*insw_epp)(int, char *, int);
	void (*insl_epp)(int, char *, int);

	char (*r_dtr)(int);
	char (*r_str)(int);
	char (*r_ctr)(int);
	char (*r_epp)(int);
	char (*r_ecr)(int);
	char (*r_fifo)(int);

	void (*w_dtr)(int, char);
	void (*w_str)(int, char);
	void (*w_ctr)(int, char);
	void (*w_epp)(int, char);
	void (*w_ecr)(int, char);
	void (*w_fifo)(int, char);
};

/*
 * ppb_link structure.
 */
struct ppb_link {

	int adapter_unit;			/* unit of the adapter */

	int id_irq;				/* != 0 if irq enabled */
	int mode;				/* NIBBLE, PS2, EPP, ECP */

#define EPP_1_9		0x0			/* default */
#define EPP_1_7		0x1

	int epp_protocol;			/* EPP protocol: 0=1.9, 1=1.7 */

	struct ppb_adapter *adapter;		/* link to the ppc adapter */
	struct ppb_data *ppbus;			/* link to the ppbus */
};

/*
 * Parallel Port Bus structure.
 */
struct ppb_data {

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
extern int ppb_attachdevs(struct ppb_data *);
extern int ppb_request_bus(struct ppb_device *, int);
extern int ppb_release_bus(struct ppb_device *);
extern void ppb_intr(struct ppb_link *);

extern int ppb_reset_epp_timeout(struct ppb_device *);
extern int ppb_ecp_sync(struct ppb_device *);
extern int ppb_get_status(struct ppb_device *, struct ppb_status *);
extern int ppb_get_mode(struct ppb_device *);
extern int ppb_get_epp_protocol(struct ppb_device *);
extern int ppb_get_irq(struct ppb_device *);

/*
 * These are defined as macros for speedup.
 */
#define ppb_outsb_epp(dev,buf,cnt) \
			(*(dev)->ppb->ppb_link->adapter->outsb_epp) \
			((dev)->ppb->ppb_link->adapter_unit, buf, cnt)
#define ppb_outsw_epp(dev,buf,cnt) \
			(*(dev)->ppb->ppb_link->adapter->outsw_epp) \
			((dev)->ppb->ppb_link->adapter_unit, buf, cnt)
#define ppb_outsl_epp(dev,buf,cnt) \
			(*(dev)->ppb->ppb_link->adapter->outsl_epp) \
			((dev)->ppb->ppb_link->adapter_unit, buf, cnt)
#define ppb_insb_epp(dev,buf,cnt) \
			(*(dev)->ppb->ppb_link->adapter->insb_epp) \
			((dev)->ppb->ppb_link->adapter_unit, buf, cnt)
#define ppb_insw_epp(dev,buf,cnt) \
			(*(dev)->ppb->ppb_link->adapter->insw_epp) \
			((dev)->ppb->ppb_link->adapter_unit, buf, cnt)
#define ppb_insl_epp(dev,buf,cnt) \
			(*(dev)->ppb->ppb_link->adapter->insl_epp) \
			((dev)->ppb->ppb_link->adapter_unit, buf, cnt)

#define ppb_rdtr(dev) (*(dev)->ppb->ppb_link->adapter->r_dtr) \
				((dev)->ppb->ppb_link->adapter_unit)
#define ppb_rstr(dev) (*(dev)->ppb->ppb_link->adapter->r_str) \
				((dev)->ppb->ppb_link->adapter_unit)
#define ppb_rctr(dev) (*(dev)->ppb->ppb_link->adapter->r_ctr) \
				((dev)->ppb->ppb_link->adapter_unit)
#define ppb_repp(dev) (*(dev)->ppb->ppb_link->adapter->r_epp) \
				((dev)->ppb->ppb_link->adapter_unit)
#define ppb_recr(dev) (*(dev)->ppb->ppb_link->adapter->r_ecr) \
				((dev)->ppb->ppb_link->adapter_unit)
#define ppb_rfifo(dev) (*(dev)->ppb->ppb_link->adapter->r_fifo) \
				((dev)->ppb->ppb_link->adapter_unit)

#define ppb_wdtr(dev,byte) (*(dev)->ppb->ppb_link->adapter->w_dtr) \
				((dev)->ppb->ppb_link->adapter_unit, byte)
#define ppb_wstr(dev,byte) (*(dev)->ppb->ppb_link->adapter->w_str) \
				((dev)->ppb->ppb_link->adapter_unit, byte)
#define ppb_wctr(dev,byte) (*(dev)->ppb->ppb_link->adapter->w_ctr) \
				((dev)->ppb->ppb_link->adapter_unit, byte)
#define ppb_wepp(dev,byte) (*(dev)->ppb->ppb_link->adapter->w_epp) \
				((dev)->ppb->ppb_link->adapter_unit, byte)
#define ppb_wecr(dev,byte) (*(dev)->ppb->ppb_link->adapter->w_ecr) \
				((dev)->ppb->ppb_link->adapter_unit, byte)
#define ppb_wfifo(dev,byte) (*(dev)->ppb->ppb_link->adapter->w_fifo) \
				((dev)->ppb->ppb_link->adapter_unit, byte)

#endif
