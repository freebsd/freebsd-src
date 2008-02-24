/*-
 * Copyright (c) 1999 Luoqi Chen.
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
 * $FreeBSD: src/sys/dev/aic/aicvar.h,v 1.9 2007/06/17 05:55:46 scottl Exp $
 */

struct aic_transinfo {
	u_int8_t period;
	u_int8_t offset;
};

struct aic_tinfo {
	u_int16_t lubusy;
	u_int8_t flags;
	u_int8_t scsirate;
	struct aic_transinfo current;
	struct aic_transinfo goal;
	struct aic_transinfo user;
};

#define	TINFO_DISC_ENB		0x01
#define	TINFO_TAG_ENB		0x02
#define	TINFO_SDTR_NEGO		0x04
#define	TINFO_SDTR_SENT		0x08

struct aic_scb {
	union ccb	*ccb;
	u_int8_t	flags;
	u_int8_t	tag;
	u_int8_t	target;
	u_int8_t	lun;
	u_int8_t	status;
	u_int8_t	cmd_len;
	u_int8_t	*cmd_ptr;
	u_int32_t	data_len;
	u_int8_t	*data_ptr;
};

#define ccb_scb_ptr	spriv_ptr0
#define ccb_aic_ptr	spriv_ptr1

#define	SCB_ACTIVE		0x01
#define	SCB_DISCONNECTED	0x02
#define	SCB_DEVICE_RESET	0x04
#define	SCB_SENSE		0x08

enum { AIC6260, AIC6360, AIC6370, GM82C700 };

struct aic_softc {
	device_t		dev;
	int			unit;
	bus_space_tag_t		tag;
	bus_space_handle_t	bsh;
	bus_dma_tag_t		dmat;

	struct cam_sim		*sim;
	struct cam_path		*path;
	TAILQ_HEAD(,ccb_hdr)	pending_ccbs, nexus_ccbs;
	struct aic_scb		*nexus;

	u_int32_t		flags;
	u_int8_t		initiator;
	u_int8_t		state;
	u_int8_t		target;
	u_int8_t		lun;
	u_int8_t		prev_phase;

	u_int8_t		msg_outq;
	u_int8_t		msg_sent;
	int			msg_len;
	char			msg_buf[8];

	struct aic_tinfo	tinfo[8];
	struct aic_scb		scbs[256];

	int			min_period;
	int			max_period;
	int			chip_type;
};

#define	AIC_DISC_ENABLE		0x01
#define	AIC_DMA_ENABLE		0x02
#define	AIC_PARITY_ENABLE	0x04
#define	AIC_DWIO_ENABLE		0x08
#define	AIC_RESOURCE_SHORTAGE	0x10
#define	AIC_DROP_MSGIN		0x20
#define	AIC_BUSFREE_OK		0x40
#define	AIC_FAST_ENABLE		0x80

#define	AIC_IDLE		0x00
#define	AIC_SELECTING		0x01
#define	AIC_RESELECTED		0x02
#define	AIC_RECONNECTING	0x03
#define	AIC_HASNEXUS		0x04

#define	AIC_MSG_IDENTIFY	0x01
#define	AIC_MSG_TAG_Q		0x02
#define	AIC_MSG_SDTR		0x04
#define	AIC_MSG_WDTR		0x08
#define	AIC_MSG_MSGBUF		0x80

#define	AIC_SYNC_PERIOD		(200 / 4)
#define	AIC_FAST_SYNC_PERIOD	(100 / 4)
#define	AIC_MIN_SYNC_PERIOD	112
#define	AIC_SYNC_OFFSET		8

#define	aic_inb(aic, port) \
	bus_space_read_1((aic)->tag, (aic)->bsh, (port))

#define	aic_outb(aic, port, value) \
	bus_space_write_1((aic)->tag, (aic)->bsh, (port), (value))

#define	aic_insb(aic, port, addr, count) \
	bus_space_read_multi_1((aic)->tag, (aic)->bsh, (port), (addr), (count))

#define	aic_outsb(aic, port, addr, count) \
	bus_space_write_multi_1((aic)->tag, (aic)->bsh, (port), (addr), (count))

#define	aic_insw(aic, port, addr, count) \
	bus_space_read_multi_2((aic)->tag, (aic)->bsh, (port), \
		(u_int16_t *)(addr), (count))

#define	aic_outsw(aic, port, addr, count) \
	bus_space_write_multi_2((aic)->tag, (aic)->bsh, (port), \
		(u_int16_t *)(addr), (count))

#define	aic_insl(aic, port, addr, count) \
	bus_space_read_multi_4((aic)->tag, (aic)->bsh, (port), \
		(u_int32_t *)(addr), (count))

#define	aic_outsl(aic, port, addr, count) \
	bus_space_write_multi_4((aic)->tag, (aic)->bsh, (port), \
		(u_int32_t *)(addr), (count))

extern int aic_probe(struct aic_softc *);
extern int aic_attach(struct aic_softc *);
extern int aic_detach(struct aic_softc *);
extern void aic_intr(void *);
