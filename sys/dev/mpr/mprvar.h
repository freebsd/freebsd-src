/*-
 * Copyright (c) 2009 Yahoo! Inc.
 * Copyright (c) 2011-2015 LSI Corp.
 * Copyright (c) 2013-2016 Avago Technologies
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
 * Avago Technologies (LSI) MPT-Fusion Host Adapter FreeBSD
 *
 * $FreeBSD$
 */

#ifndef _MPRVAR_H
#define _MPRVAR_H

#define MPR_DRIVER_VERSION	"13.01.00.00-fbsd"

#define MPR_DB_MAX_WAIT		2500

#define MPR_REQ_FRAMES		1024
#define MPR_EVT_REPLY_FRAMES	32
#define MPR_REPLY_FRAMES	MPR_REQ_FRAMES
#define MPR_CHAIN_FRAMES	2048
#define MPR_MAXIO_PAGES		(-1)
#define MPR_SENSE_LEN		SSD_FULL_SIZE
#define MPR_MSI_COUNT		1
#define MPR_SGE64_SIZE		12
#define MPR_SGE32_SIZE		8
#define MPR_SGC_SIZE		8
#define MPR_DEFAULT_CHAIN_SEG_SIZE	8
#define MPR_MAX_CHAIN_ELEMENT_SIZE	16

#define MPR_FUNCTRACE(sc)			\
	mpr_dprint((sc), MPR_TRACE, "%s\n", __func__)

#define	CAN_SLEEP			1
#define	NO_SLEEP			0

#define MPR_PERIODIC_DELAY	1	/* 1 second heartbeat/watchdog check */
#define MPR_ATA_ID_TIMEOUT	5	/* 5 second timeout for SATA ID cmd */

#define	IFAULT_IOP_OVER_TEMP_THRESHOLD_EXCEEDED	0x2810

#define MPR_SCSI_RI_INVALID_FRAME	(0x00000002)
#define MPR_STRING_LENGTH               64

#define DEFAULT_SPINUP_WAIT	3	/* seconds to wait for spinup */

#include <sys/endian.h>

/*
 * host mapping related macro definitions
 */
#define MPR_MAPTABLE_BAD_IDX	0xFFFFFFFF
#define MPR_DPM_BAD_IDX		0xFFFF
#define MPR_ENCTABLE_BAD_IDX	0xFF
#define MPR_MAX_MISSING_COUNT	0x0F
#define MPR_DEV_RESERVED	0x20000000
#define MPR_MAP_IN_USE		0x10000000
#define MPR_RAID_CHANNEL	1
#define MPR_MAP_BAD_ID		0xFFFFFFFF

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

/**
 * struct dev_mapping_table - device mapping information
 * @physical_id: SAS address for drives or WWID for RAID volumes
 * @device_info: bitfield provides detailed info about the device
 * @phy_bits: bitfields indicating controller phys
 * @dpm_entry_num: index of this device in device persistent map table
 * @dev_handle: device handle for the device pointed by this entry
 * @channel: target channel
 * @id: target id
 * @missing_count: number of times the device not detected by driver
 * @hide_flag: Hide this physical disk/not (foreign configuration)
 * @init_complete: Whether the start of the day checks completed or not
 * @TLR_bits: Turn TLR support on or off
 */
struct dev_mapping_table {
	u64	physical_id;
	u32	device_info;
	u32	phy_bits;
	u16	dpm_entry_num;
	u16	dev_handle;
	u8	reserved1;
	u8	channel;
	u16	id;
	u8	missing_count;
	u8	init_complete;
	u8	TLR_bits;
	u8	reserved2;
};

/**
 * struct enc_mapping_table -  mapping information about an enclosure
 * @enclosure_id: Logical ID of this enclosure
 * @start_index: index to the entry in dev_mapping_table
 * @phy_bits: bitfields indicating controller phys
 * @dpm_entry_num: index of this enclosure in device persistent map table
 * @enc_handle: device handle for the enclosure pointed by this entry
 * @num_slots: number of slots in the enclosure
 * @start_slot: Starting slot id
 * @missing_count: number of times the device not detected by driver
 * @removal_flag: used to mark the device for removal
 * @skip_search: used as a flag to include/exclude enclosure for search
 * @init_complete: Whether the start of the day checks completed or not
 */
struct enc_mapping_table {
	u64	enclosure_id;
	u32	start_index;
	u32	phy_bits;
	u16	dpm_entry_num;
	u16	enc_handle;
	u16	num_slots;
	u16	start_slot;
	u8	missing_count;
	u8	removal_flag;
	u8	skip_search;
	u8	init_complete;
};

/**
 * struct map_removal_table - entries to be removed from mapping table
 * @dpm_entry_num: index of this device in device persistent map table
 * @dev_handle: device handle for the device pointed by this entry
 */
struct map_removal_table{
	u16	dpm_entry_num;
	u16	dev_handle;
};

typedef struct mpr_fw_diagnostic_buffer {
	size_t		size;
	uint8_t		extended_type;
	uint8_t		buffer_type;
	uint8_t		force_release;
	uint32_t	product_specific[23];
	uint8_t		immediate;
	uint8_t		enabled;
	uint8_t		valid_data;
	uint8_t		owned_by_firmware;
	uint32_t	unique_id;
} mpr_fw_diagnostic_buffer_t;

struct mpr_softc;
struct mpr_command;
struct mprsas_softc;
union ccb;
struct mprsas_target;
struct mpr_column_map;

MALLOC_DECLARE(M_MPR);

typedef void mpr_evt_callback_t(struct mpr_softc *, uintptr_t,
    MPI2_EVENT_NOTIFICATION_REPLY *reply);
typedef void mpr_command_callback_t(struct mpr_softc *, struct mpr_command *cm);

struct mpr_chain {
	TAILQ_ENTRY(mpr_chain)		chain_link;
	void				*chain;
	uint64_t			chain_busaddr;
};

/*
 * This needs to be at least 2 to support SMP passthrough.
 */
#define       MPR_IOVEC_COUNT 2

struct mpr_command {
	TAILQ_ENTRY(mpr_command)	cm_link;
	TAILQ_ENTRY(mpr_command)	cm_recovery;
	struct mpr_softc		*cm_sc;
	union ccb			*cm_ccb;
	void				*cm_data;
	u_int				cm_length;
	u_int				cm_out_len;
	struct uio			cm_uio;
	struct iovec			cm_iovec[MPR_IOVEC_COUNT];
	u_int				cm_max_segs;
	u_int				cm_sglsize;
	void				*cm_sge;
	uint8_t				*cm_req;
	uint8_t				*cm_reply;
	uint32_t			cm_reply_data;
	mpr_command_callback_t		*cm_complete;
	void				*cm_complete_data;
	struct mprsas_target		*cm_targ;
	MPI2_REQUEST_DESCRIPTOR_UNION	cm_desc;
	u_int	                	cm_lun;
	u_int				cm_flags;
#define MPR_CM_FLAGS_POLLED		(1 << 0)
#define MPR_CM_FLAGS_COMPLETE		(1 << 1)
#define MPR_CM_FLAGS_SGE_SIMPLE		(1 << 2)
#define MPR_CM_FLAGS_DATAOUT		(1 << 3)
#define MPR_CM_FLAGS_DATAIN		(1 << 4)
#define MPR_CM_FLAGS_WAKEUP		(1 << 5)
#define MPR_CM_FLAGS_USE_UIO		(1 << 6)
#define MPR_CM_FLAGS_SMP_PASS		(1 << 7)
#define	MPR_CM_FLAGS_CHAIN_FAILED	(1 << 8)
#define	MPR_CM_FLAGS_ERROR_MASK		MPR_CM_FLAGS_CHAIN_FAILED
#define	MPR_CM_FLAGS_USE_CCB		(1 << 9)
#define	MPR_CM_FLAGS_SATA_ID_TIMEOUT	(1 << 10)
	u_int				cm_state;
#define MPR_CM_STATE_FREE		0
#define MPR_CM_STATE_BUSY		1
#define MPR_CM_STATE_TIMEDOUT		2
	bus_dmamap_t			cm_dmamap;
	struct scsi_sense_data		*cm_sense;
	TAILQ_HEAD(, mpr_chain)		cm_chain_list;
	uint32_t			cm_req_busaddr;
	uint32_t			cm_sense_busaddr;
	struct callout			cm_callout;
};

struct mpr_column_map {
	uint16_t			dev_handle;
	uint8_t				phys_disk_num;
};

struct mpr_event_handle {
	TAILQ_ENTRY(mpr_event_handle)	eh_list;
	mpr_evt_callback_t		*callback;
	void				*data;
	uint8_t				mask[16];
};

struct mpr_softc {
	device_t			mpr_dev;
	struct cdev			*mpr_cdev;
	u_int				mpr_flags;
#define MPR_FLAGS_INTX		(1 << 0)
#define MPR_FLAGS_MSI		(1 << 1)
#define MPR_FLAGS_BUSY		(1 << 2)
#define MPR_FLAGS_SHUTDOWN	(1 << 3)
#define MPR_FLAGS_DIAGRESET	(1 << 4)
#define	MPR_FLAGS_ATTACH_DONE	(1 << 5)
	u_int				mpr_debug;
	u_int				disable_msix;
	u_int				disable_msi;
	int				tm_cmds_active;
	int				io_cmds_active;
	int				io_cmds_highwater;
	int				chain_free;
	int				max_chains;
	int				max_io_pages;
	int				chain_free_lowwater;
	uint32_t			chain_frame_size;
	uint16_t			chain_seg_size;
	u_int				enable_ssu;
	int				spinup_wait_time;
#if __FreeBSD_version >= 900030
	uint64_t			chain_alloc_fail;
#endif
	struct sysctl_ctx_list		sysctl_ctx;
	struct sysctl_oid		*sysctl_tree;
	char                            fw_version[16];
	struct mpr_command		*commands;
	struct mpr_chain		*chains;
	struct callout			periodic;

	struct mprsas_softc		*sassc;
	char            tmp_string[MPR_STRING_LENGTH];
	TAILQ_HEAD(, mpr_command)	req_list;
	TAILQ_HEAD(, mpr_command)	high_priority_req_list;
	TAILQ_HEAD(, mpr_chain)		chain_list;
	TAILQ_HEAD(, mpr_command)	tm_list;
	int				replypostindex;
	int				replyfreeindex;

	struct resource			*mpr_regs_resource;
	bus_space_handle_t		mpr_bhandle;
	bus_space_tag_t			mpr_btag;
	int				mpr_regs_rid;

	bus_dma_tag_t			mpr_parent_dmat;
	bus_dma_tag_t			buffer_dmat;

	MPI2_IOC_FACTS_REPLY		*facts;
	int				num_reqs;
	int				num_replies;
	int				fqdepth;	/* Free queue */
	int				pqdepth;	/* Post queue */

	uint8_t				event_mask[16];
	TAILQ_HEAD(, mpr_event_handle)	event_list;
	struct mpr_event_handle		*mpr_log_eh;

	struct mtx			mpr_mtx;
	struct intr_config_hook		mpr_ich;
	struct resource			*mpr_irq[MPR_MSI_COUNT];
	void				*mpr_intrhand[MPR_MSI_COUNT];
	int				mpr_irq_rid[MPR_MSI_COUNT];

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

	uint8_t				*fw_diag_buffer;
	bus_addr_t			fw_diag_busaddr;
	bus_dma_tag_t			fw_diag_dmat;
	bus_dmamap_t			fw_diag_map;

	uint8_t				ir_firmware;

	/* static config pages */
	Mpi2IOCPage8_t			ioc_pg8;
	Mpi2IOUnitPage8_t		iounit_pg8;

	/* host mapping support */
	struct dev_mapping_table	*mapping_table;
	struct enc_mapping_table	*enclosure_table;
	struct map_removal_table	*removal_table;
	uint8_t				*dpm_entry_used;
	uint8_t				*dpm_flush_entry;
	Mpi2DriverMappingPage0_t	*dpm_pg0;
	uint16_t			max_devices;
	uint16_t			max_enclosures;
	uint16_t			max_expanders;
	uint8_t				max_volumes;
	uint8_t				num_enc_table_entries;
	uint8_t				num_rsvd_entries;
	uint8_t				num_channels;
	uint16_t			max_dpm_entries;
	uint8_t				is_dpm_enable;
	uint8_t				track_mapping_events;
	uint32_t			pending_map_events;
	uint8_t				mt_full_retry;
	uint8_t				mt_add_device_failed;

	/* FW diag Buffer List */
	mpr_fw_diagnostic_buffer_t
				fw_diag_buffer_list[MPI2_DIAG_BUF_TYPE_COUNT];

	/* Event Recording IOCTL support */
	uint32_t			events_to_record[4];
	mpr_event_entry_t		recorded_events[MPR_EVENT_QUEUE_SIZE];
	uint8_t				event_index;
	uint32_t			event_number;

	/* EEDP and TLR support */
	uint8_t				eedp_enabled;
	uint8_t				control_TLR;

	/* Shutdown Event Handler */
	eventhandler_tag		shutdown_eh;

	/* To track topo events during reset */
#define	MPR_DIAG_RESET_TIMEOUT	300000
	uint8_t				wait_for_port_enable;
	uint8_t				port_enable_complete;
	uint8_t				msleep_fake_chan;

	/* StartStopUnit command handling at shutdown */
	uint32_t			SSU_refcount;
	uint8_t				SSU_started;

	char				exclude_ids[80];
	struct timeval			lastfail;
};

struct mpr_config_params {
	MPI2_CONFIG_EXT_PAGE_HEADER_UNION	hdr;
	u_int		action;
	u_int		page_address;	/* Attributes, not a phys address */
	u_int		status;
	void		*buffer;
	u_int		length;
	int		timeout;
	void		(*callback)(struct mpr_softc *, struct mpr_config_params *);
	void		*cbdata;
};

struct scsi_read_capacity_eedp
{
	uint8_t addr[8];
	uint8_t length[4];
	uint8_t protect;
};

static __inline uint32_t
mpr_regread(struct mpr_softc *sc, uint32_t offset)
{
	return (bus_space_read_4(sc->mpr_btag, sc->mpr_bhandle, offset));
}

static __inline void
mpr_regwrite(struct mpr_softc *sc, uint32_t offset, uint32_t val)
{
	bus_space_write_4(sc->mpr_btag, sc->mpr_bhandle, offset, val);
}

/* free_queue must have Little Endian address 
 * TODO- cm_reply_data is unwanted. We can remove it.
 * */
static __inline void
mpr_free_reply(struct mpr_softc *sc, uint32_t busaddr)
{
	if (++sc->replyfreeindex >= sc->fqdepth)
		sc->replyfreeindex = 0;
	sc->free_queue[sc->replyfreeindex] = htole32(busaddr);
	mpr_regwrite(sc, MPI2_REPLY_FREE_HOST_INDEX_OFFSET, sc->replyfreeindex);
}

static __inline struct mpr_chain *
mpr_alloc_chain(struct mpr_softc *sc)
{
	struct mpr_chain *chain;

	if ((chain = TAILQ_FIRST(&sc->chain_list)) != NULL) {
		TAILQ_REMOVE(&sc->chain_list, chain, chain_link);
		sc->chain_free--;
		if (sc->chain_free < sc->chain_free_lowwater)
			sc->chain_free_lowwater = sc->chain_free;
	}
#if __FreeBSD_version >= 900030
	else
		sc->chain_alloc_fail++;
#endif
	return (chain);
}

static __inline void
mpr_free_chain(struct mpr_softc *sc, struct mpr_chain *chain)
{
#if 0
	bzero(chain->chain, 128);
#endif
	sc->chain_free++;
	TAILQ_INSERT_TAIL(&sc->chain_list, chain, chain_link);
}

static __inline void
mpr_free_command(struct mpr_softc *sc, struct mpr_command *cm)
{
	struct mpr_chain *chain, *chain_temp;

	if (cm->cm_reply != NULL)
		mpr_free_reply(sc, cm->cm_reply_data);
	cm->cm_reply = NULL;
	cm->cm_flags = 0;
	cm->cm_complete = NULL;
	cm->cm_complete_data = NULL;
	cm->cm_ccb = NULL;
	cm->cm_targ = NULL;
	cm->cm_max_segs = 0;
	cm->cm_lun = 0;
	cm->cm_state = MPR_CM_STATE_FREE;
	cm->cm_data = NULL;
	cm->cm_length = 0;
	cm->cm_out_len = 0;
	cm->cm_sglsize = 0;
	cm->cm_sge = NULL;

	TAILQ_FOREACH_SAFE(chain, &cm->cm_chain_list, chain_link, chain_temp) {
		TAILQ_REMOVE(&cm->cm_chain_list, chain, chain_link);
		mpr_free_chain(sc, chain);
	}
	TAILQ_INSERT_TAIL(&sc->req_list, cm, cm_link);
}

static __inline struct mpr_command *
mpr_alloc_command(struct mpr_softc *sc)
{
	struct mpr_command *cm;

	cm = TAILQ_FIRST(&sc->req_list);
	if (cm == NULL)
		return (NULL);

	TAILQ_REMOVE(&sc->req_list, cm, cm_link);
	KASSERT(cm->cm_state == MPR_CM_STATE_FREE, ("mpr: Allocating busy command\n"));
	cm->cm_state = MPR_CM_STATE_BUSY;
	return (cm);
}

static __inline void
mpr_free_high_priority_command(struct mpr_softc *sc, struct mpr_command *cm)
{
	struct mpr_chain *chain, *chain_temp;

	if (cm->cm_reply != NULL)
		mpr_free_reply(sc, cm->cm_reply_data);
	cm->cm_reply = NULL;
	cm->cm_flags = 0;
	cm->cm_complete = NULL;
	cm->cm_complete_data = NULL;
	cm->cm_ccb = NULL;
	cm->cm_targ = NULL;
	cm->cm_lun = 0;
	cm->cm_state = MPR_CM_STATE_FREE;
	TAILQ_FOREACH_SAFE(chain, &cm->cm_chain_list, chain_link, chain_temp) {
		TAILQ_REMOVE(&cm->cm_chain_list, chain, chain_link);
		mpr_free_chain(sc, chain);
	}
	TAILQ_INSERT_TAIL(&sc->high_priority_req_list, cm, cm_link);
}

static __inline struct mpr_command *
mpr_alloc_high_priority_command(struct mpr_softc *sc)
{
	struct mpr_command *cm;

	cm = TAILQ_FIRST(&sc->high_priority_req_list);
	if (cm == NULL)
		return (NULL);

	TAILQ_REMOVE(&sc->high_priority_req_list, cm, cm_link);
	KASSERT(cm->cm_state == MPR_CM_STATE_FREE, ("mpr: Allocating busy command\n"));
	cm->cm_state = MPR_CM_STATE_BUSY;
	return (cm);
}

static __inline void
mpr_lock(struct mpr_softc *sc)
{
	mtx_lock(&sc->mpr_mtx);
}

static __inline void
mpr_unlock(struct mpr_softc *sc)
{
	mtx_unlock(&sc->mpr_mtx);
}

#define MPR_INFO	(1 << 0)	/* Basic info */
#define MPR_FAULT	(1 << 1)	/* Hardware faults */
#define MPR_EVENT	(1 << 2)	/* Event data from the controller */
#define MPR_LOG		(1 << 3)	/* Log data from the controller */
#define MPR_RECOVERY	(1 << 4)	/* Command error recovery tracing */
#define MPR_ERROR	(1 << 5)	/* Parameter errors, programming bugs */
#define MPR_INIT	(1 << 6)	/* Things related to system init */
#define MPR_XINFO	(1 << 7)	/* More detailed/noisy info */
#define MPR_USER	(1 << 8)	/* Trace user-generated commands */
#define MPR_MAPPING	(1 << 9)	/* Trace device mappings */
#define MPR_TRACE	(1 << 10)	/* Function-by-function trace */

#define	MPR_SSU_DISABLE_SSD_DISABLE_HDD	0
#define	MPR_SSU_ENABLE_SSD_DISABLE_HDD	1
#define	MPR_SSU_DISABLE_SSD_ENABLE_HDD	2
#define	MPR_SSU_ENABLE_SSD_ENABLE_HDD	3

#define mpr_printf(sc, args...)				\
	device_printf((sc)->mpr_dev, ##args)

#define mpr_vprintf(sc, args...)			\
do {							\
	if (bootverbose)				\
		mpr_printf(sc, ##args);			\
} while (0)

#define mpr_dprint(sc, level, msg, args...)		\
do {							\
	if ((sc)->mpr_debug & (level))			\
		device_printf((sc)->mpr_dev, msg, ##args);	\
} while (0)

#define mpr_dprint_field(sc, level, msg, args...)		\
do {								\
	if ((sc)->mpr_debug & (level))				\
		printf("\t" msg, ##args);			\
} while (0)

#define MPR_PRINTFIELD_START(sc, tag...)	\
	mpr_dprint((sc), MPR_INFO, ##tag);	\
	mpr_dprint_field((sc), MPR_INFO, ":\n")
#define MPR_PRINTFIELD_END(sc, tag)		\
	mpr_dprint((sc), MPR_INFO, tag "\n")
#define MPR_PRINTFIELD(sc, facts, attr, fmt)	\
	mpr_dprint_field((sc), MPR_INFO, #attr ": " #fmt "\n", (facts)->attr)

#define MPR_EVENTFIELD_START(sc, tag...)	\
	mpr_dprint((sc), MPR_EVENT, ##tag);	\
	mpr_dprint_field((sc), MPR_EVENT, ":\n")
#define MPR_EVENTFIELD(sc, facts, attr, fmt)	\
	mpr_dprint_field((sc), MPR_EVENT, #attr ": " #fmt "\n", (facts)->attr)

static __inline void
mpr_from_u64(uint64_t data, U64 *mpr)
{
	(mpr)->High = htole32((uint32_t)((data) >> 32));
	(mpr)->Low = htole32((uint32_t)((data) & 0xffffffff));
}

static __inline uint64_t
mpr_to_u64(U64 *data)
{
	return (((uint64_t)le32toh(data->High) << 32) | le32toh(data->Low));
}

static __inline void
mpr_mask_intr(struct mpr_softc *sc)
{
	uint32_t mask;

	mask = mpr_regread(sc, MPI2_HOST_INTERRUPT_MASK_OFFSET);
	mask |= MPI2_HIM_REPLY_INT_MASK;
	mpr_regwrite(sc, MPI2_HOST_INTERRUPT_MASK_OFFSET, mask);
}

static __inline void
mpr_unmask_intr(struct mpr_softc *sc)
{
	uint32_t mask;

	mask = mpr_regread(sc, MPI2_HOST_INTERRUPT_MASK_OFFSET);
	mask &= ~MPI2_HIM_REPLY_INT_MASK;
	mpr_regwrite(sc, MPI2_HOST_INTERRUPT_MASK_OFFSET, mask);
}

int mpr_pci_setup_interrupts(struct mpr_softc *sc);
int mpr_pci_restore(struct mpr_softc *sc);

int mpr_attach(struct mpr_softc *sc);
int mpr_free(struct mpr_softc *sc);
void mpr_intr(void *);
void mpr_intr_msi(void *);
void mpr_intr_locked(void *);
int mpr_register_events(struct mpr_softc *, uint8_t *, mpr_evt_callback_t *,
    void *, struct mpr_event_handle **);
int mpr_restart(struct mpr_softc *);
int mpr_update_events(struct mpr_softc *, struct mpr_event_handle *, uint8_t *);
int mpr_deregister_events(struct mpr_softc *, struct mpr_event_handle *);
int mpr_push_sge(struct mpr_command *, MPI2_SGE_SIMPLE64 *, size_t, int);
int mpr_push_ieee_sge(struct mpr_command *, void *, int);
int mpr_add_dmaseg(struct mpr_command *, vm_paddr_t, size_t, u_int, int);
int mpr_attach_sas(struct mpr_softc *sc);
int mpr_detach_sas(struct mpr_softc *sc);
int mpr_read_config_page(struct mpr_softc *, struct mpr_config_params *);
int mpr_write_config_page(struct mpr_softc *, struct mpr_config_params *);
void mpr_memaddr_cb(void *, bus_dma_segment_t *, int , int );
void mpr_init_sge(struct mpr_command *cm, void *req, void *sge);
int mpr_attach_user(struct mpr_softc *);
void mpr_detach_user(struct mpr_softc *);
void mprsas_record_event(struct mpr_softc *sc,
    MPI2_EVENT_NOTIFICATION_REPLY *event_reply);

int mpr_map_command(struct mpr_softc *sc, struct mpr_command *cm);
int mpr_wait_command(struct mpr_softc *sc, struct mpr_command *cm, int timeout,
    int sleep_flag);
int mpr_request_polled(struct mpr_softc *sc, struct mpr_command *cm);

int mpr_config_get_bios_pg3(struct mpr_softc *sc, Mpi2ConfigReply_t
    *mpi_reply, Mpi2BiosPage3_t *config_page);
int mpr_config_get_raid_volume_pg0(struct mpr_softc *sc, Mpi2ConfigReply_t
    *mpi_reply, Mpi2RaidVolPage0_t *config_page, u32 page_address);
int mpr_config_get_ioc_pg8(struct mpr_softc *sc, Mpi2ConfigReply_t *,
    Mpi2IOCPage8_t *);
int mpr_config_get_iounit_pg8(struct mpr_softc *sc,
    Mpi2ConfigReply_t *mpi_reply, Mpi2IOUnitPage8_t *config_page);
int mpr_config_get_sas_device_pg0(struct mpr_softc *, Mpi2ConfigReply_t *,
    Mpi2SasDevicePage0_t *, u32 , u16 );
int mpr_config_get_dpm_pg0(struct mpr_softc *, Mpi2ConfigReply_t *,
    Mpi2DriverMappingPage0_t *, u16 );
int mpr_config_get_raid_volume_pg1(struct mpr_softc *sc,
    Mpi2ConfigReply_t *mpi_reply, Mpi2RaidVolPage1_t *config_page, u32 form,
    u16 handle);
int mpr_config_get_volume_wwid(struct mpr_softc *sc, u16 volume_handle,
    u64 *wwid);
int mpr_config_get_raid_pd_pg0(struct mpr_softc *sc,
    Mpi2ConfigReply_t *mpi_reply, Mpi2RaidPhysDiskPage0_t *config_page,
    u32 page_address);
void mprsas_ir_shutdown(struct mpr_softc *sc);

int mpr_reinit(struct mpr_softc *sc);
void mprsas_handle_reinit(struct mpr_softc *sc);

void mpr_base_static_config_pages(struct mpr_softc *sc);

int mpr_mapping_initialize(struct mpr_softc *);
void mpr_mapping_topology_change_event(struct mpr_softc *,
    Mpi2EventDataSasTopologyChangeList_t *);
int mpr_mapping_is_reinit_required(struct mpr_softc *);
void mpr_mapping_free_memory(struct mpr_softc *sc);
int mpr_config_set_dpm_pg0(struct mpr_softc *, Mpi2ConfigReply_t *,
    Mpi2DriverMappingPage0_t *, u16 );
void mpr_mapping_exit(struct mpr_softc *);
void mpr_mapping_check_devices(struct mpr_softc *, int);
int mpr_mapping_allocate_memory(struct mpr_softc *sc);
unsigned int mpr_mapping_get_sas_id(struct mpr_softc *, uint64_t , u16);
unsigned int mpr_mapping_get_sas_id_from_handle(struct mpr_softc *sc,
    u16 handle);
unsigned int mpr_mapping_get_raid_id(struct mpr_softc *sc, u64 wwid,
    u16 handle);
unsigned int mpr_mapping_get_raid_id_from_handle(struct mpr_softc *sc,
    u16 volHandle);
void mpr_mapping_enclosure_dev_status_change_event(struct mpr_softc *,
    Mpi2EventDataSasEnclDevStatusChange_t *event_data);
void mpr_mapping_ir_config_change_event(struct mpr_softc *sc,
    Mpi2EventDataIrConfigChangeList_t *event_data);

void mprsas_evt_handler(struct mpr_softc *sc, uintptr_t data,
    MPI2_EVENT_NOTIFICATION_REPLY *event);
void mprsas_prepare_remove(struct mprsas_softc *sassc, uint16_t handle);
void mprsas_prepare_volume_remove(struct mprsas_softc *sassc, uint16_t handle);
int mprsas_startup(struct mpr_softc *sc);
struct mprsas_target * mprsas_find_target_by_handle(struct mprsas_softc *, int,
    uint16_t);
void mprsas_realloc_targets(struct mpr_softc *sc, int maxtargets);
struct mpr_command * mprsas_alloc_tm(struct mpr_softc *sc);
void mprsas_free_tm(struct mpr_softc *sc, struct mpr_command *tm);
void mprsas_release_simq_reinit(struct mprsas_softc *sassc);
int mprsas_send_reset(struct mpr_softc *sc, struct mpr_command *tm,
    uint8_t type);

SYSCTL_DECL(_hw_mpr);

/* Compatibility shims for different OS versions */
#if __FreeBSD_version >= 800001
#define mpr_kproc_create(func, farg, proc_ptr, flags, stackpgs, fmtstr, arg) \
    kproc_create(func, farg, proc_ptr, flags, stackpgs, fmtstr, arg)
#define mpr_kproc_exit(arg)	kproc_exit(arg)
#else
#define mpr_kproc_create(func, farg, proc_ptr, flags, stackpgs, fmtstr, arg) \
    kthread_create(func, farg, proc_ptr, flags, stackpgs, fmtstr, arg)
#define mpr_kproc_exit(arg)	kthread_exit(arg)
#endif

#if defined(CAM_PRIORITY_XPT)
#define MPR_PRIORITY_XPT	CAM_PRIORITY_XPT
#else
#define MPR_PRIORITY_XPT	5
#endif

#if __FreeBSD_version < 800107
// Prior to FreeBSD-8.0 scp3_flags was not defined.
#define spc3_flags reserved

#define SPC3_SID_PROTECT    0x01
#define SPC3_SID_3PC        0x08
#define SPC3_SID_TPGS_MASK  0x30
#define SPC3_SID_TPGS_IMPLICIT  0x10
#define SPC3_SID_TPGS_EXPLICIT  0x20
#define SPC3_SID_ACC        0x40
#define SPC3_SID_SCCS       0x80

#define CAM_PRIORITY_NORMAL CAM_PRIORITY_NONE
#endif

#endif

