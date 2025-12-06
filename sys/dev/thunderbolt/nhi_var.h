/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Scott Long
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
 * Thunderbolt 3 / Native Host Interface driver variables
 *
 * $FreeBSD$
 */

#ifndef _NHI_VAR
#define _NHI_VAR

MALLOC_DECLARE(M_NHI);

#define NHI_MSIX_MAX		32
#define NHI_RING0_TX_DEPTH	16
#define NHI_RING0_RX_DEPTH	16
#define NHI_DEFAULT_NUM_RINGS	1
#define NHI_MAX_NUM_RINGS	32	/* XXX 2? */
#define NHI_RING0_FRAME_SIZE	256
#define NHI_MAILBOX_TIMEOUT	15

#define NHI_CMD_TIMEOUT		3	/* 3 seconds */

struct nhi_softc;
struct nhi_ring_pair;
struct nhi_intr_tracker;
struct nhi_cmd_frame;
struct hcm_softc;
struct router_softc;

struct nhi_cmd_frame {
	TAILQ_ENTRY(nhi_cmd_frame)	cm_link;
	uint32_t		*data;
	bus_addr_t		data_busaddr;
	u_int			req_len;
	uint16_t		flags;
#define CMD_MAPPED		(1 << 0)
#define CMD_POLLED		(1 << 1)
#define CMD_REQ_COMPLETE	(1 << 2)
#define CMD_RESP_COMPLETE	(1 << 3)
#define CMD_RESP_OVERRUN	(1 << 4)
	uint16_t		retries;
	uint16_t		pdf;
	uint16_t		idx;

	void			*context;
	u_int			timeout;

	uint32_t		*resp_buffer;
	u_int			resp_len;
};

#define NHI_RING_NAMELEN	16
struct nhi_ring_pair {
	struct nhi_softc	*sc;

	union nhi_ring_desc	*tx_ring;
	union nhi_ring_desc	*rx_ring;

	uint16_t		tx_pi;
	uint16_t		tx_ci;
	uint16_t		rx_pi;
	uint16_t		rx_ci;

	uint16_t		rx_pici_reg;
	uint16_t		tx_pici_reg;

	struct nhi_cmd_frame	**rx_cmd_ring;
	struct nhi_cmd_frame	**tx_cmd_ring;

	struct mtx		mtx;
	char			name[NHI_RING_NAMELEN];
	struct nhi_intr_tracker	*tracker;
	SLIST_ENTRY(nhi_ring_pair)	ring_link;

	TAILQ_HEAD(, nhi_cmd_frame)	tx_head;
	TAILQ_HEAD(, nhi_cmd_frame)	rx_head;

	uint16_t		tx_ring_depth;
	uint16_t		tx_ring_mask;
	uint16_t		rx_ring_depth;
	uint16_t		rx_ring_mask;
	uint16_t		rx_buffer_size;
	u_char			ring_num;

	bus_dma_tag_t		ring_dmat;
	bus_dmamap_t		ring_map;
	void			*ring;
	bus_addr_t		tx_ring_busaddr;
	bus_addr_t		rx_ring_busaddr;

	bus_dma_tag_t		frames_dmat;
	bus_dmamap_t		frames_map;
	void			*frames;
	bus_addr_t		tx_frames_busaddr;
	bus_addr_t		rx_frames_busaddr;
};

/* PDF-indexed array of dispatch routines for interrupts */
typedef void (nhi_ring_cb_t)(void *, union nhi_ring_desc *,
    struct nhi_cmd_frame *);
struct nhi_pdf_dispatch {
	nhi_ring_cb_t		*cb;
	void			*context;
};

struct nhi_intr_tracker {
	struct nhi_softc	*sc;
	struct nhi_ring_pair	*ring;
	struct nhi_pdf_dispatch	txpdf[16];
	struct nhi_pdf_dispatch	rxpdf[16];
	u_int			vector;
};

struct nhi_softc {
	device_t		dev;
	device_t		ufp;
	u_int			debug;
	u_int			hwflags;
#define NHI_TYPE_UNKNOWN	0x00
#define NHI_TYPE_AR		0x01		/* Alpine Ridge */
#define NHI_TYPE_TR		0x02		/* Titan Ridge */
#define NHI_TYPE_ICL		0x03		/* IceLake */
#define NHI_TYPE_MR		0x04		/* Maple Ridge */
#define NHI_TYPE_ADL		0x05		/* AlderLake */
#define NHI_TYPE_USB4		0x0f
#define NHI_TYPE_MASK		0x0f
#define NHI_MBOX_BUSY		0x10
	u_int			caps;
#define NHI_CAP_ICM		0x01
#define NHI_CAP_HCM		0x02
#define NHI_USE_ICM(sc)		((sc)->caps & NHI_CAP_ICM)
#define NHI_USE_HCM(sc)		((sc)->caps & NHI_CAP_HCM)
	struct hcm_softc	*hcm;
	struct router_softc	*root_rsc;

	struct nhi_ring_pair	*ring0;
	struct nhi_intr_tracker	*intr_trackers;

	uint16_t		path_count;
	uint16_t		max_ring_count;

	struct mtx		nhi_mtx;
	SLIST_HEAD(, nhi_ring_pair)	ring_list;

	int			msix_count;
	struct resource		*irqs[NHI_MSIX_MAX];
	void			*intrhand[NHI_MSIX_MAX];
	int			irq_rid[NHI_MSIX_MAX];
	struct resource		*irq_pba;
	int			irq_pba_rid;
	struct resource		*irq_table;
	int			irq_table_rid;

	struct resource		*regs_resource;
	bus_space_handle_t	regs_bhandle;
	bus_space_tag_t		regs_btag;
	int			regs_rid;

	bus_dma_tag_t		parent_dmat;

	bus_dma_tag_t		ring0_dmat;
	bus_dmamap_t		ring0_map;
	void			*ring0_frames;
	bus_addr_t		ring0_frames_busaddr;
	struct nhi_cmd_frame	*ring0_cmds;

	struct sysctl_ctx_list	*sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;

	struct intr_config_hook	ich;

	uint8_t			force_hcm;
#define NHI_FORCE_HCM_DEFAULT	0x00
#define NHI_FORCE_HCM_ON	0x01
#define NHI_FORCE_HCM_OFF	0x02

	uint8_t			uuid[16];
	uint8_t			lc_uuid[16];
};

struct nhi_dispatch {
	uint8_t			pdf;
	nhi_ring_cb_t		*cb;
	void			*context;
};

#define NHI_IS_AR(sc)	(((sc)->hwflags & NHI_TYPE_MASK) == NHI_TYPE_AR)
#define NHI_IS_TR(sc)	(((sc)->hwflags & NHI_TYPE_MASK) == NHI_TYPE_TR)
#define NHI_IS_ICL(sc)	(((sc)->hwflags & NHI_TYPE_MASK) == NHI_TYPE_ICL)
#define NHI_IS_USB4(sc)	(((sc)->hwflags & NHI_TYPE_MASK) == NHI_TYPE_USB4)

int nhi_pci_configure_interrupts(struct nhi_softc *sc);
void nhi_pci_enable_interrupt(struct nhi_ring_pair *r);
void nhi_pci_disable_interrupts(struct nhi_softc *sc);
void nhi_pci_free_interrupts(struct nhi_softc *sc);
int nhi_pci_get_uuid(struct nhi_softc *sc);
int nhi_read_lc_mailbox(struct nhi_softc *, u_int reg, uint32_t *val);
int nhi_write_lc_mailbox(struct nhi_softc *, u_int reg, uint32_t val);

void nhi_get_tunables(struct nhi_softc *);
int nhi_attach(struct nhi_softc *);
int nhi_detach(struct nhi_softc *);

struct nhi_cmd_frame * nhi_alloc_tx_frame(struct nhi_ring_pair *);
void nhi_free_tx_frame(struct nhi_ring_pair *, struct nhi_cmd_frame *);

int nhi_inmail_cmd(struct nhi_softc *, uint32_t, uint32_t);
int nhi_outmail_cmd(struct nhi_softc *, uint32_t *);

int nhi_tx_schedule(struct nhi_ring_pair *, struct nhi_cmd_frame *);
int nhi_tx_synchronous(struct nhi_ring_pair *, struct nhi_cmd_frame *);
void nhi_intr(void *);

int nhi_register_pdf(struct nhi_ring_pair *, struct nhi_dispatch *,
    struct nhi_dispatch *);
int nhi_deregister_pdf(struct nhi_ring_pair *, struct nhi_dispatch *,
    struct nhi_dispatch *);

/* Low level read/write MMIO registers */
static __inline uint32_t
nhi_read_reg(struct nhi_softc *sc, u_int offset)
{
	return (le32toh(bus_space_read_4(sc->regs_btag, sc->regs_bhandle,
	    offset)));
}

static __inline void
nhi_write_reg(struct nhi_softc *sc, u_int offset, uint32_t val)
{
	bus_space_write_4(sc->regs_btag, sc->regs_bhandle, offset,
	    htole32(val));
}

static __inline struct nhi_cmd_frame *
nhi_alloc_tx_frame_locked(struct nhi_ring_pair *r)
{
	struct nhi_cmd_frame *cmd;

	if ((cmd = TAILQ_FIRST(&r->tx_head)) != NULL)
		TAILQ_REMOVE(&r->tx_head, cmd, cm_link);
	return (cmd);
}

static __inline void
nhi_free_tx_frame_locked(struct nhi_ring_pair *r, struct nhi_cmd_frame *cmd)
{
	/* Clear all flags except for MAPPED */
	cmd->flags &= CMD_MAPPED;
	cmd->resp_buffer = NULL;
	TAILQ_INSERT_TAIL(&r->tx_head, cmd, cm_link);
}

#endif /* _NHI_VAR */
