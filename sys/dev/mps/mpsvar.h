/*-
 * Copyright (c) 2009 Yahoo! Inc.
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
 * $FreeBSD$
 */

#ifndef _MPSVAR_H
#define _MPSVAR_H

#define MPS_DB_MAX_WAIT		2500

#define MPS_REQ_FRAMES		1024
#define MPS_EVT_REPLY_FRAMES	32
#define MPS_REPLY_FRAMES	MPS_REQ_FRAMES
#define MPS_CHAIN_FRAMES	1024
#define MPS_SENSE_LEN		SSD_FULL_SIZE
#define MPS_MSI_COUNT		1
#define MPS_SGE64_SIZE		12
#define MPS_SGE32_SIZE		8
#define MPS_SGC_SIZE		8

#define MPS_PERIODIC_DELAY	1	/* 1 second heartbeat/watchdog check */

struct mps_softc;
struct mps_command;
struct mpssas_softc;
struct mpssas_target;

MALLOC_DECLARE(M_MPT2);

typedef void mps_evt_callback_t(struct mps_softc *, uintptr_t,
    MPI2_EVENT_NOTIFICATION_REPLY *reply);
typedef void mps_command_callback_t(struct mps_softc *, struct mps_command *cm);

struct mps_chain {
	TAILQ_ENTRY(mps_chain)		chain_link;
	MPI2_SGE_IO_UNION		*chain;
	uint32_t			chain_busaddr;
};

/*
 * This needs to be at least 2 to support SMP passthrough.
 */
#define	MPS_IOVEC_COUNT	2

struct mps_command {
	TAILQ_ENTRY(mps_command)	cm_link;
	struct mps_softc		*cm_sc;
	void				*cm_data;
	u_int				cm_length;
	struct uio			cm_uio;
	struct iovec			cm_iovec[MPS_IOVEC_COUNT];
	u_int				cm_max_segs;
	u_int				cm_sglsize;
	MPI2_SGE_IO_UNION		*cm_sge;
	uint8_t				*cm_req;
	uint8_t				*cm_reply;
	uint32_t			cm_reply_data;
	mps_command_callback_t		*cm_complete;
	void				*cm_complete_data;
	struct mpssas_target		*cm_targ;
	MPI2_REQUEST_DESCRIPTOR_UNION	cm_desc;
	u_int				cm_flags;
#define MPS_CM_FLAGS_POLLED		(1 << 0)
#define MPS_CM_FLAGS_COMPLETE		(1 << 1)
#define MPS_CM_FLAGS_SGE_SIMPLE		(1 << 2)
#define MPS_CM_FLAGS_DATAOUT		(1 << 3)
#define MPS_CM_FLAGS_DATAIN		(1 << 4)
#define MPS_CM_FLAGS_WAKEUP		(1 << 5)
#define MPS_CM_FLAGS_ACTIVE		(1 << 6)
#define MPS_CM_FLAGS_USE_UIO		(1 << 7)
#define MPS_CM_FLAGS_SMP_PASS		(1 << 8)
	u_int				cm_state;
#define MPS_CM_STATE_FREE		0
#define MPS_CM_STATE_BUSY		1
#define MPS_CM_STATE_TIMEDOUT		2
	bus_dmamap_t			cm_dmamap;
	struct scsi_sense_data		*cm_sense;
	TAILQ_HEAD(, mps_chain)		cm_chain_list;
	uint32_t			cm_req_busaddr;
	uint32_t			cm_sense_busaddr;
	struct callout			cm_callout;
};

struct mps_event_handle {
	TAILQ_ENTRY(mps_event_handle)	eh_list;
	mps_evt_callback_t		*callback;
	void				*data;
	uint8_t				mask[16];
};

struct mps_softc {
	device_t			mps_dev;
	struct cdev			*mps_cdev;
	u_int				mps_flags;
#define MPS_FLAGS_INTX		(1 << 0)
#define MPS_FLAGS_MSI		(1 << 1)
#define MPS_FLAGS_BUSY		(1 << 2)
#define MPS_FLAGS_SHUTDOWN	(1 << 3)
	u_int				mps_debug;
	u_int				allow_multiple_tm_cmds;
	int				tm_cmds_active;
	struct sysctl_ctx_list		sysctl_ctx;
	struct sysctl_oid		*sysctl_tree;
	struct mps_command		*commands;
	struct mps_chain		*chains;
	struct callout			periodic;

	struct mpssas_softc		*sassc;

	TAILQ_HEAD(, mps_command)	req_list;
	TAILQ_HEAD(, mps_chain)		chain_list;
	TAILQ_HEAD(, mps_command)	tm_list;
	TAILQ_HEAD(, mps_command)	io_list;
	int				replypostindex;
	int				replyfreeindex;

	struct resource			*mps_regs_resource;
	bus_space_handle_t		mps_bhandle;
	bus_space_tag_t			mps_btag;
	int				mps_regs_rid;

	bus_dma_tag_t			mps_parent_dmat;
	bus_dma_tag_t			buffer_dmat;

	MPI2_IOC_FACTS_REPLY		*facts;
	MPI2_PORT_FACTS_REPLY		*pfacts;
	int				num_reqs;
	int				num_replies;
	int				fqdepth;	/* Free queue */
	int				pqdepth;	/* Post queue */

	uint8_t				event_mask[16];
	TAILQ_HEAD(, mps_event_handle)	event_list;
	struct mps_event_handle		*mps_log_eh;

	struct mtx			mps_mtx;
	struct intr_config_hook		mps_ich;
	struct resource			*mps_irq[MPS_MSI_COUNT];
	void				*mps_intrhand[MPS_MSI_COUNT];
	int				mps_irq_rid[MPS_MSI_COUNT];

	uint8_t				*req_frames;
	bus_addr_t			req_busaddr;
	bus_dma_tag_t			req_dmat;
	bus_dmamap_t			req_map;

	uint8_t				*reply_frames;
	bus_addr_t			reply_busaddr;
	bus_dma_tag_t			reply_dmat;
	bus_dmamap_t			reply_map;

	struct scsi_sense_data		*sense_frames;
	bus_addr_t			sense_busaddr;
	bus_dma_tag_t			sense_dmat;
	bus_dmamap_t			sense_map;

	uint8_t				*chain_frames;
	bus_addr_t			chain_busaddr;
	bus_dma_tag_t			chain_dmat;
	bus_dmamap_t			chain_map;

	MPI2_REPLY_DESCRIPTORS_UNION	*post_queue;
	bus_addr_t			post_busaddr;
	uint32_t			*free_queue;
	bus_addr_t			free_busaddr;
	bus_dma_tag_t			queues_dmat;
	bus_dmamap_t			queues_map;
};

struct mps_config_params {
	MPI2_CONFIG_EXT_PAGE_HEADER_UNION	hdr;
	u_int		action;
	u_int		page_address;	/* Attributes, not a phys address */
	u_int		status;
	void		*buffer;
	u_int		length;
	int		timeout;
	void		(*callback)(struct mps_softc *, struct mps_config_params *);
	void		*cbdata;
};

static __inline uint32_t
mps_regread(struct mps_softc *sc, uint32_t offset)
{
	return (bus_space_read_4(sc->mps_btag, sc->mps_bhandle, offset));
}

static __inline void
mps_regwrite(struct mps_softc *sc, uint32_t offset, uint32_t val)
{
	bus_space_write_4(sc->mps_btag, sc->mps_bhandle, offset, val);
}

static __inline void
mps_free_reply(struct mps_softc *sc, uint32_t busaddr)
{

	if (++sc->replyfreeindex >= sc->fqdepth)
		sc->replyfreeindex = 0;
	sc->free_queue[sc->replyfreeindex] = busaddr;
	mps_regwrite(sc, MPI2_REPLY_FREE_HOST_INDEX_OFFSET, sc->replyfreeindex);
}

static __inline struct mps_chain *
mps_alloc_chain(struct mps_softc *sc)
{
	struct mps_chain *chain;

	if ((chain = TAILQ_FIRST(&sc->chain_list)) != NULL)
		TAILQ_REMOVE(&sc->chain_list, chain, chain_link);
	return (chain);
}

static __inline void
mps_free_chain(struct mps_softc *sc, struct mps_chain *chain)
{
#if 0
	bzero(chain->chain, 128);
#endif
	TAILQ_INSERT_TAIL(&sc->chain_list, chain, chain_link);
}

static __inline void
mps_free_command(struct mps_softc *sc, struct mps_command *cm)
{
	struct mps_chain *chain, *chain_temp;

	if (cm->cm_reply != NULL) {
		mps_free_reply(sc, cm->cm_reply_data);
		cm->cm_reply = NULL;
	}
	cm->cm_flags = 0;
	cm->cm_complete = NULL;
	cm->cm_complete_data = NULL;
	cm->cm_targ = 0;
	cm->cm_max_segs = 0;
	cm->cm_state = MPS_CM_STATE_FREE;
	TAILQ_FOREACH_SAFE(chain, &cm->cm_chain_list, chain_link, chain_temp) {
		TAILQ_REMOVE(&cm->cm_chain_list, chain, chain_link);
		mps_free_chain(sc, chain);
	}
	TAILQ_INSERT_TAIL(&sc->req_list, cm, cm_link);
}

static __inline struct mps_command *
mps_alloc_command(struct mps_softc *sc)
{
	struct mps_command *cm;

	cm = TAILQ_FIRST(&sc->req_list);
	if (cm == NULL)
		return (NULL);

	TAILQ_REMOVE(&sc->req_list, cm, cm_link);
	KASSERT(cm->cm_state == MPS_CM_STATE_FREE, ("mps: Allocating busy command\n"));
	cm->cm_state = MPS_CM_STATE_BUSY;
	return (cm);
}

static __inline void
mps_lock(struct mps_softc *sc)
{
	mtx_lock(&sc->mps_mtx);
}

static __inline void
mps_unlock(struct mps_softc *sc)
{
	mtx_unlock(&sc->mps_mtx);
}

#define MPS_INFO	(1 << 0)
#define MPS_TRACE	(1 << 1)
#define MPS_FAULT	(1 << 2)
#define MPS_EVENT	(1 << 3)
#define MPS_LOG		(1 << 4)

#define mps_printf(sc, args...)				\
	device_printf((sc)->mps_dev, ##args)

#define mps_dprint(sc, level, msg, args...)		\
do {							\
	if (sc->mps_debug & level)			\
		device_printf(sc->mps_dev, msg, ##args);	\
} while (0)

#define mps_dprint_field(sc, level, msg, args...)		\
do {								\
	if (sc->mps_debug & level)				\
		printf("\t" msg, ##args);			\
} while (0)

#define MPS_PRINTFIELD_START(sc, tag...)	\
	mps_dprint((sc), MPS_INFO, ##tag);	\
	mps_dprint_field((sc), MPS_INFO, ":\n")
#define MPS_PRINTFIELD_END(sc, tag)		\
	mps_dprint((sc), MPS_INFO, tag "\n")
#define MPS_PRINTFIELD(sc, facts, attr, fmt)	\
	mps_dprint_field((sc), MPS_INFO, #attr ": " #fmt "\n", (facts)->attr)

#define MPS_EVENTFIELD_START(sc, tag...)	\
	mps_dprint((sc), MPS_EVENT, ##tag);	\
	mps_dprint_field((sc), MPS_EVENT, ":\n")
#define MPS_EVENTFIELD(sc, facts, attr, fmt)	\
	mps_dprint_field((sc), MPS_EVENT, #attr ": " #fmt "\n", (facts)->attr)

static __inline void
mps_from_u64(uint64_t data, U64 *mps)
{
	(mps)->High = (uint32_t)((data) >> 32);
	(mps)->Low = (uint32_t)((data) & 0xffffffff);
}

static __inline uint64_t
mps_to_u64(U64 *data)
{

	return (((uint64_t)data->High << 32) | data->Low);
}

static __inline void
mps_mask_intr(struct mps_softc *sc)
{
	uint32_t mask;

	mask = mps_regread(sc, MPI2_HOST_INTERRUPT_MASK_OFFSET);
	mask |= MPI2_HIM_REPLY_INT_MASK;
	mps_regwrite(sc, MPI2_HOST_INTERRUPT_MASK_OFFSET, mask);
}

static __inline void
mps_unmask_intr(struct mps_softc *sc)
{
	uint32_t mask;

	mask = mps_regread(sc, MPI2_HOST_INTERRUPT_MASK_OFFSET);
	mask &= ~MPI2_HIM_REPLY_INT_MASK;
	mps_regwrite(sc, MPI2_HOST_INTERRUPT_MASK_OFFSET, mask);
}

int mps_pci_setup_interrupts(struct mps_softc *);
int mps_attach(struct mps_softc *sc);
int mps_free(struct mps_softc *sc);
void mps_intr(void *);
void mps_intr_msi(void *);
void mps_intr_locked(void *);
int mps_register_events(struct mps_softc *, uint8_t *, mps_evt_callback_t *,
    void *, struct mps_event_handle **);
int mps_update_events(struct mps_softc *, struct mps_event_handle *, uint8_t *);
int mps_deregister_events(struct mps_softc *, struct mps_event_handle *);
int mps_request_polled(struct mps_softc *sc, struct mps_command *cm);
void mps_enqueue_request(struct mps_softc *, struct mps_command *);
int mps_push_sge(struct mps_command *, void *, size_t, int);
int mps_add_dmaseg(struct mps_command *, vm_paddr_t, size_t, u_int, int);
int mps_attach_sas(struct mps_softc *sc);
int mps_detach_sas(struct mps_softc *sc);
int mps_map_command(struct mps_softc *sc, struct mps_command *cm);
int mps_read_config_page(struct mps_softc *, struct mps_config_params *);
int mps_write_config_page(struct mps_softc *, struct mps_config_params *);
void mps_memaddr_cb(void *, bus_dma_segment_t *, int , int );
void mpi_init_sge(struct mps_command *cm, void *req, void *sge);
int mps_attach_user(struct mps_softc *);
void mps_detach_user(struct mps_softc *);

SYSCTL_DECL(_hw_mps);

#endif

