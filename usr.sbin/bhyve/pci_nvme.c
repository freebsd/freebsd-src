/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2017 Shunsuke Mie
 * Copyright (c) 2018 Leon Dang
 * Copyright (c) 2020 Chuck Tuffli
 *
 * Function crc16 Copyright (c) 2017, Fedor Uporov
 *     Obtained from function ext2_crc16() in sys/fs/ext2fs/ext2_csum.c
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
 */

/*
 * bhyve PCIe-NVMe device emulation.
 *
 * options:
 *  -s <n>,nvme,devpath,maxq=#,qsz=#,ioslots=#,sectsz=#,ser=A-Z,eui64=#,dsm=<opt>
 *
 *  accepted devpath:
 *    /dev/blockdev
 *    /path/to/image
 *    ram=size_in_MiB
 *
 *  maxq    = max number of queues
 *  qsz     = max elements in each queue
 *  ioslots = max number of concurrent io requests
 *  sectsz  = sector size (defaults to blockif sector size)
 *  ser     = serial number (20-chars max)
 *  eui64   = IEEE Extended Unique Identifier (8 byte value)
 *  dsm     = DataSet Management support. Option is one of auto, enable,disable
 *
 */

/* TODO:
    - create async event for smart and log
    - intr coalesce
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/errno.h>
#include <sys/types.h>
#include <net/ieee_oui.h>

#include <assert.h>
#include <pthread.h>
#include <pthread_np.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <machine/atomic.h>
#include <machine/vmm.h>
#include <vmmapi.h>

#include <dev/nvme/nvme.h>

#include "bhyverun.h"
#include "block_if.h"
#include "config.h"
#include "debug.h"
#include "pci_emul.h"


static int nvme_debug = 0;
#define	DPRINTF(fmt, args...) if (nvme_debug) PRINTLN(fmt, ##args)
#define	WPRINTF(fmt, args...) PRINTLN(fmt, ##args)

/* defaults; can be overridden */
#define	NVME_MSIX_BAR		4

#define	NVME_IOSLOTS		8

/* The NVMe spec defines bits 13:4 in BAR0 as reserved */
#define NVME_MMIO_SPACE_MIN	(1 << 14)

#define	NVME_QUEUES		16
#define	NVME_MAX_QENTRIES	2048
/* Memory Page size Minimum reported in CAP register */
#define	NVME_MPSMIN		0
/* MPSMIN converted to bytes */
#define	NVME_MPSMIN_BYTES	(1 << (12 + NVME_MPSMIN))

#define	NVME_PRP2_ITEMS		(PAGE_SIZE/sizeof(uint64_t))
#define	NVME_MDTS		9
/* Note the + 1 allows for the initial descriptor to not be page aligned */
#define	NVME_MAX_IOVEC		((1 << NVME_MDTS) + 1)
#define	NVME_MAX_DATA_SIZE	((1 << NVME_MDTS) * NVME_MPSMIN_BYTES)

/* This is a synthetic status code to indicate there is no status */
#define NVME_NO_STATUS		0xffff
#define NVME_COMPLETION_VALID(c)	((c).status != NVME_NO_STATUS)

/* Reported temperature in Kelvin (i.e. room temperature) */
#define NVME_TEMPERATURE 296

/* helpers */

/* Convert a zero-based value into a one-based value */
#define ONE_BASED(zero)		((zero) + 1)
/* Convert a one-based value into a zero-based value */
#define ZERO_BASED(one)		((one)  - 1)

/* Encode number of SQ's and CQ's for Set/Get Features */
#define NVME_FEATURE_NUM_QUEUES(sc) \
	(ZERO_BASED((sc)->num_squeues) & 0xffff) | \
	(ZERO_BASED((sc)->num_cqueues) & 0xffff) << 16;

#define	NVME_DOORBELL_OFFSET	offsetof(struct nvme_registers, doorbell)

enum nvme_controller_register_offsets {
	NVME_CR_CAP_LOW = 0x00,
	NVME_CR_CAP_HI  = 0x04,
	NVME_CR_VS      = 0x08,
	NVME_CR_INTMS   = 0x0c,
	NVME_CR_INTMC   = 0x10,
	NVME_CR_CC      = 0x14,
	NVME_CR_CSTS    = 0x1c,
	NVME_CR_NSSR    = 0x20,
	NVME_CR_AQA     = 0x24,
	NVME_CR_ASQ_LOW = 0x28,
	NVME_CR_ASQ_HI  = 0x2c,
	NVME_CR_ACQ_LOW = 0x30,
	NVME_CR_ACQ_HI  = 0x34,
};

enum nvme_cmd_cdw11 {
	NVME_CMD_CDW11_PC  = 0x0001,
	NVME_CMD_CDW11_IEN = 0x0002,
	NVME_CMD_CDW11_IV  = 0xFFFF0000,
};

enum nvme_copy_dir {
	NVME_COPY_TO_PRP,
	NVME_COPY_FROM_PRP,
};

#define	NVME_CQ_INTEN	0x01
#define	NVME_CQ_INTCOAL	0x02

struct nvme_completion_queue {
	struct nvme_completion *qbase;
	pthread_mutex_t	mtx;
	uint32_t	size;
	uint16_t	tail; /* nvme progress */
	uint16_t	head; /* guest progress */
	uint16_t	intr_vec;
	uint32_t	intr_en;
};

struct nvme_submission_queue {
	struct nvme_command *qbase;
	pthread_mutex_t	mtx;
	uint32_t	size;
	uint16_t	head; /* nvme progress */
	uint16_t	tail; /* guest progress */
	uint16_t	cqid; /* completion queue id */
	int		qpriority;
};

enum nvme_storage_type {
	NVME_STOR_BLOCKIF = 0,
	NVME_STOR_RAM = 1,
};

struct pci_nvme_blockstore {
	enum nvme_storage_type type;
	void		*ctx;
	uint64_t	size;
	uint32_t	sectsz;
	uint32_t	sectsz_bits;
	uint64_t	eui64;
	uint32_t	deallocate:1;
};

/*
 * Calculate the number of additional page descriptors for guest IO requests
 * based on the advertised Max Data Transfer (MDTS) and given the number of
 * default iovec's in a struct blockif_req.
 */
#define MDTS_PAD_SIZE \
	( NVME_MAX_IOVEC > BLOCKIF_IOV_MAX ? \
	  NVME_MAX_IOVEC - BLOCKIF_IOV_MAX : \
	  0 )

struct pci_nvme_ioreq {
	struct pci_nvme_softc *sc;
	STAILQ_ENTRY(pci_nvme_ioreq) link;
	struct nvme_submission_queue *nvme_sq;
	uint16_t	sqid;

	/* command information */
	uint16_t	opc;
	uint16_t	cid;
	uint32_t	nsid;

	uint64_t	prev_gpaddr;
	size_t		prev_size;
	size_t		bytes;

	struct blockif_req io_req;

	struct iovec	iovpadding[MDTS_PAD_SIZE];
};

enum nvme_dsm_type {
	/* Dataset Management bit in ONCS reflects backing storage capability */
	NVME_DATASET_MANAGEMENT_AUTO,
	/* Unconditionally set Dataset Management bit in ONCS */
	NVME_DATASET_MANAGEMENT_ENABLE,
	/* Unconditionally clear Dataset Management bit in ONCS */
	NVME_DATASET_MANAGEMENT_DISABLE,
};

struct pci_nvme_softc;
struct nvme_feature_obj;

typedef void (*nvme_feature_cb)(struct pci_nvme_softc *,
    struct nvme_feature_obj *,
    struct nvme_command *,
    struct nvme_completion *);

struct nvme_feature_obj {
	uint32_t	cdw11;
	nvme_feature_cb	set;
	nvme_feature_cb	get;
	bool namespace_specific;
};

#define NVME_FID_MAX		(NVME_FEAT_ENDURANCE_GROUP_EVENT_CONFIGURATION + 1)

typedef enum {
	PCI_NVME_AE_TYPE_ERROR = 0,
	PCI_NVME_AE_TYPE_SMART,
	PCI_NVME_AE_TYPE_NOTICE,
	PCI_NVME_AE_TYPE_IO_CMD = 6,
	PCI_NVME_AE_TYPE_VENDOR = 7,
	PCI_NVME_AE_TYPE_MAX		/* Must be last */
} pci_nvme_async_type;

/* Asynchronous Event Requests */
struct pci_nvme_aer {
	STAILQ_ENTRY(pci_nvme_aer) link;
	uint16_t	cid;	/* Command ID of the submitted AER */
};

typedef enum {
	PCI_NVME_AE_INFO_NS_ATTR_CHANGED = 0,
	PCI_NVME_AE_INFO_FW_ACTIVATION,
	PCI_NVME_AE_INFO_TELEMETRY_CHANGE,
	PCI_NVME_AE_INFO_ANA_CHANGE,
	PCI_NVME_AE_INFO_PREDICT_LATENCY_CHANGE,
	PCI_NVME_AE_INFO_LBA_STATUS_ALERT,
	PCI_NVME_AE_INFO_ENDURANCE_GROUP_CHANGE,
	PCI_NVME_AE_INFO_MAX,
} pci_nvme_async_info;

/* Asynchronous Event Notifications */
struct pci_nvme_aen {
	pci_nvme_async_type atype;
	uint32_t	event_data;
	bool		posted;
};

typedef enum {
	NVME_CNTRLTYPE_IO = 1,
	NVME_CNTRLTYPE_DISCOVERY = 2,
	NVME_CNTRLTYPE_ADMIN = 3,
} pci_nvme_cntrl_type;

struct pci_nvme_softc {
	struct pci_devinst *nsc_pi;

	pthread_mutex_t	mtx;

	struct nvme_registers regs;

	struct nvme_namespace_data  nsdata;
	struct nvme_controller_data ctrldata;
	struct nvme_error_information_entry err_log;
	struct nvme_health_information_page health_log;
	struct nvme_firmware_page fw_log;
	struct nvme_ns_list ns_log;

	struct pci_nvme_blockstore nvstore;

	uint16_t	max_qentries;	/* max entries per queue */
	uint32_t	max_queues;	/* max number of IO SQ's or CQ's */
	uint32_t	num_cqueues;
	uint32_t	num_squeues;
	bool		num_q_is_set; /* Has host set Number of Queues */

	struct pci_nvme_ioreq *ioreqs;
	STAILQ_HEAD(, pci_nvme_ioreq) ioreqs_free; /* free list of ioreqs */
	uint32_t	pending_ios;
	uint32_t	ioslots;
	sem_t		iosemlock;

	/*
	 * Memory mapped Submission and Completion queues
	 * Each array includes both Admin and IO queues
	 */
	struct nvme_completion_queue *compl_queues;
	struct nvme_submission_queue *submit_queues;

	struct nvme_feature_obj feat[NVME_FID_MAX];

	enum nvme_dsm_type dataset_management;

	/* Accounting for SMART data */
	__uint128_t	read_data_units;
	__uint128_t	write_data_units;
	__uint128_t	read_commands;
	__uint128_t	write_commands;
	uint32_t	read_dunits_remainder;
	uint32_t	write_dunits_remainder;

	STAILQ_HEAD(, pci_nvme_aer) aer_list;
	pthread_mutex_t	aer_mtx;
	uint32_t	aer_count;
	struct pci_nvme_aen aen[PCI_NVME_AE_TYPE_MAX];
	pthread_t	aen_tid;
	pthread_mutex_t	aen_mtx;
	pthread_cond_t	aen_cond;
};


static void pci_nvme_cq_update(struct pci_nvme_softc *sc,
    struct nvme_completion_queue *cq,
    uint32_t cdw0,
    uint16_t cid,
    uint16_t sqid,
    uint16_t status);
static struct pci_nvme_ioreq *pci_nvme_get_ioreq(struct pci_nvme_softc *);
static void pci_nvme_release_ioreq(struct pci_nvme_softc *, struct pci_nvme_ioreq *);
static void pci_nvme_io_done(struct blockif_req *, int);

/* Controller Configuration utils */
#define	NVME_CC_GET_EN(cc) \
	((cc) >> NVME_CC_REG_EN_SHIFT & NVME_CC_REG_EN_MASK)
#define	NVME_CC_GET_CSS(cc) \
	((cc) >> NVME_CC_REG_CSS_SHIFT & NVME_CC_REG_CSS_MASK)
#define	NVME_CC_GET_SHN(cc) \
	((cc) >> NVME_CC_REG_SHN_SHIFT & NVME_CC_REG_SHN_MASK)
#define	NVME_CC_GET_IOSQES(cc) \
	((cc) >> NVME_CC_REG_IOSQES_SHIFT & NVME_CC_REG_IOSQES_MASK)
#define	NVME_CC_GET_IOCQES(cc) \
	((cc) >> NVME_CC_REG_IOCQES_SHIFT & NVME_CC_REG_IOCQES_MASK)

#define	NVME_CC_WRITE_MASK \
	((NVME_CC_REG_EN_MASK << NVME_CC_REG_EN_SHIFT) | \
	 (NVME_CC_REG_IOSQES_MASK << NVME_CC_REG_IOSQES_SHIFT) | \
	 (NVME_CC_REG_IOCQES_MASK << NVME_CC_REG_IOCQES_SHIFT))

#define	NVME_CC_NEN_WRITE_MASK \
	((NVME_CC_REG_CSS_MASK << NVME_CC_REG_CSS_SHIFT) | \
	 (NVME_CC_REG_MPS_MASK << NVME_CC_REG_MPS_SHIFT) | \
	 (NVME_CC_REG_AMS_MASK << NVME_CC_REG_AMS_SHIFT))

/* Controller Status utils */
#define	NVME_CSTS_GET_RDY(sts) \
	((sts) >> NVME_CSTS_REG_RDY_SHIFT & NVME_CSTS_REG_RDY_MASK)

#define	NVME_CSTS_RDY	(1 << NVME_CSTS_REG_RDY_SHIFT)

/* Completion Queue status word utils */
#define	NVME_STATUS_P	(1 << NVME_STATUS_P_SHIFT)
#define	NVME_STATUS_MASK \
	((NVME_STATUS_SCT_MASK << NVME_STATUS_SCT_SHIFT) |\
	 (NVME_STATUS_SC_MASK << NVME_STATUS_SC_SHIFT))

#define NVME_ONCS_DSM	(NVME_CTRLR_DATA_ONCS_DSM_MASK << \
	NVME_CTRLR_DATA_ONCS_DSM_SHIFT)

static void nvme_feature_invalid_cb(struct pci_nvme_softc *,
    struct nvme_feature_obj *,
    struct nvme_command *,
    struct nvme_completion *);
static void nvme_feature_temperature(struct pci_nvme_softc *,
    struct nvme_feature_obj *,
    struct nvme_command *,
    struct nvme_completion *);
static void nvme_feature_num_queues(struct pci_nvme_softc *,
    struct nvme_feature_obj *,
    struct nvme_command *,
    struct nvme_completion *);
static void nvme_feature_iv_config(struct pci_nvme_softc *,
    struct nvme_feature_obj *,
    struct nvme_command *,
    struct nvme_completion *);

static void *aen_thr(void *arg);

static __inline void
cpywithpad(char *dst, size_t dst_size, const char *src, char pad)
{
	size_t len;

	len = strnlen(src, dst_size);
	memset(dst, pad, dst_size);
	memcpy(dst, src, len);
}

static __inline void
pci_nvme_status_tc(uint16_t *status, uint16_t type, uint16_t code)
{

	*status &= ~NVME_STATUS_MASK;
	*status |= (type & NVME_STATUS_SCT_MASK) << NVME_STATUS_SCT_SHIFT |
		(code & NVME_STATUS_SC_MASK) << NVME_STATUS_SC_SHIFT;
}

static __inline void
pci_nvme_status_genc(uint16_t *status, uint16_t code)
{

	pci_nvme_status_tc(status, NVME_SCT_GENERIC, code);
}

/*
 * Initialize the requested number or IO Submission and Completion Queues.
 * Admin queues are allocated implicitly.
 */
static void
pci_nvme_init_queues(struct pci_nvme_softc *sc, uint32_t nsq, uint32_t ncq)
{
	uint32_t i;

	/*
	 * Allocate and initialize the Submission Queues
	 */
	if (nsq > NVME_QUEUES) {
		WPRINTF("%s: clamping number of SQ from %u to %u",
					__func__, nsq, NVME_QUEUES);
		nsq = NVME_QUEUES;
	}

	sc->num_squeues = nsq;

	sc->submit_queues = calloc(sc->num_squeues + 1,
				sizeof(struct nvme_submission_queue));
	if (sc->submit_queues == NULL) {
		WPRINTF("%s: SQ allocation failed", __func__);
		sc->num_squeues = 0;
	} else {
		struct nvme_submission_queue *sq = sc->submit_queues;

		for (i = 0; i < sc->num_squeues; i++)
			pthread_mutex_init(&sq[i].mtx, NULL);
	}

	/*
	 * Allocate and initialize the Completion Queues
	 */
	if (ncq > NVME_QUEUES) {
		WPRINTF("%s: clamping number of CQ from %u to %u",
					__func__, ncq, NVME_QUEUES);
		ncq = NVME_QUEUES;
	}

	sc->num_cqueues = ncq;

	sc->compl_queues = calloc(sc->num_cqueues + 1,
				sizeof(struct nvme_completion_queue));
	if (sc->compl_queues == NULL) {
		WPRINTF("%s: CQ allocation failed", __func__);
		sc->num_cqueues = 0;
	} else {
		struct nvme_completion_queue *cq = sc->compl_queues;

		for (i = 0; i < sc->num_cqueues; i++)
			pthread_mutex_init(&cq[i].mtx, NULL);
	}
}

static void
pci_nvme_init_ctrldata(struct pci_nvme_softc *sc)
{
	struct nvme_controller_data *cd = &sc->ctrldata;

	cd->vid = 0xFB5D;
	cd->ssvid = 0x0000;

	cpywithpad((char *)cd->mn, sizeof(cd->mn), "bhyve-NVMe", ' ');
	cpywithpad((char *)cd->fr, sizeof(cd->fr), "1.0", ' ');

	/* Num of submission commands that we can handle at a time (2^rab) */
	cd->rab   = 4;

	/* FreeBSD OUI */
	cd->ieee[0] = 0x58;
	cd->ieee[1] = 0x9c;
	cd->ieee[2] = 0xfc;

	cd->mic = 0;

	cd->mdts = NVME_MDTS;	/* max data transfer size (2^mdts * CAP.MPSMIN) */

	cd->ver = NVME_REV(1,4);

	cd->cntrltype = NVME_CNTRLTYPE_IO;
	cd->oacs = 1 << NVME_CTRLR_DATA_OACS_FORMAT_SHIFT;
	cd->acl = 2;
	cd->aerl = 4;

	/* Advertise 1, Read-only firmware slot */
	cd->frmw = NVME_CTRLR_DATA_FRMW_SLOT1_RO_MASK |
	    (1 << NVME_CTRLR_DATA_FRMW_NUM_SLOTS_SHIFT);
	cd->lpa = 0;	/* TODO: support some simple things like SMART */
	cd->elpe = 0;	/* max error log page entries */
	cd->npss = 1;	/* number of power states support */

	/* Warning Composite Temperature Threshold */
	cd->wctemp = 0x0157;
	cd->cctemp = 0x0157;

	cd->sqes = (6 << NVME_CTRLR_DATA_SQES_MAX_SHIFT) |
	    (6 << NVME_CTRLR_DATA_SQES_MIN_SHIFT);
	cd->cqes = (4 << NVME_CTRLR_DATA_CQES_MAX_SHIFT) |
	    (4 << NVME_CTRLR_DATA_CQES_MIN_SHIFT);
	cd->nn = 1;	/* number of namespaces */

	cd->oncs = 0;
	switch (sc->dataset_management) {
	case NVME_DATASET_MANAGEMENT_AUTO:
		if (sc->nvstore.deallocate)
			cd->oncs |= NVME_ONCS_DSM;
		break;
	case NVME_DATASET_MANAGEMENT_ENABLE:
		cd->oncs |= NVME_ONCS_DSM;
		break;
	default:
		break;
	}

	cd->fna = NVME_CTRLR_DATA_FNA_FORMAT_ALL_MASK <<
	    NVME_CTRLR_DATA_FNA_FORMAT_ALL_SHIFT;

	cd->power_state[0].mp = 10;
}

/*
 * Calculate the CRC-16 of the given buffer
 * See copyright attribution at top of file
 */
static uint16_t
crc16(uint16_t crc, const void *buffer, unsigned int len)
{
	const unsigned char *cp = buffer;
	/* CRC table for the CRC-16. The poly is 0x8005 (x16 + x15 + x2 + 1). */
	static uint16_t const crc16_table[256] = {
		0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
		0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
		0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
		0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
		0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
		0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
		0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
		0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
		0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
		0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
		0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
		0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
		0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
		0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
		0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
		0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
		0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
		0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
		0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
		0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
		0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
		0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
		0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
		0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
		0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
		0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
		0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
		0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
		0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
		0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
		0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
		0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
	};

	while (len--)
		crc = (((crc >> 8) & 0xffU) ^
		    crc16_table[(crc ^ *cp++) & 0xffU]) & 0x0000ffffU;
	return crc;
}

static void
pci_nvme_init_nsdata_size(struct pci_nvme_blockstore *nvstore,
    struct nvme_namespace_data *nd)
{

	/* Get capacity and block size information from backing store */
	nd->nsze = nvstore->size / nvstore->sectsz;
	nd->ncap = nd->nsze;
	nd->nuse = nd->nsze;
}

static void
pci_nvme_init_nsdata(struct pci_nvme_softc *sc,
    struct nvme_namespace_data *nd, uint32_t nsid,
    struct pci_nvme_blockstore *nvstore)
{

	pci_nvme_init_nsdata_size(nvstore, nd);

	if (nvstore->type == NVME_STOR_BLOCKIF)
		nvstore->deallocate = blockif_candelete(nvstore->ctx);

	nd->nlbaf = 0; /* NLBAF is a 0's based value (i.e. 1 LBA Format) */
	nd->flbas = 0;

	/* Create an EUI-64 if user did not provide one */
	if (nvstore->eui64 == 0) {
		char *data = NULL;
		uint64_t eui64 = nvstore->eui64;

		asprintf(&data, "%s%u%u%u", get_config_value("name"),
		    sc->nsc_pi->pi_bus, sc->nsc_pi->pi_slot,
		    sc->nsc_pi->pi_func);

		if (data != NULL) {
			eui64 = OUI_FREEBSD_NVME_LOW | crc16(0, data, strlen(data));
			free(data);
		}
		nvstore->eui64 = (eui64 << 16) | (nsid & 0xffff);
	}
	be64enc(nd->eui64, nvstore->eui64);

	/* LBA data-sz = 2^lbads */
	nd->lbaf[0] = nvstore->sectsz_bits << NVME_NS_DATA_LBAF_LBADS_SHIFT;
}

static void
pci_nvme_init_logpages(struct pci_nvme_softc *sc)
{

	memset(&sc->err_log, 0, sizeof(sc->err_log));
	memset(&sc->health_log, 0, sizeof(sc->health_log));
	memset(&sc->fw_log, 0, sizeof(sc->fw_log));
	memset(&sc->ns_log, 0, sizeof(sc->ns_log));

	/* Set read/write remainder to round up according to spec */
	sc->read_dunits_remainder = 999;
	sc->write_dunits_remainder = 999;

	/* Set nominal Health values checked by implementations */
	sc->health_log.temperature = NVME_TEMPERATURE;
	sc->health_log.available_spare = 100;
	sc->health_log.available_spare_threshold = 10;
}

static void
pci_nvme_init_features(struct pci_nvme_softc *sc)
{
	enum nvme_feature	fid;

	for (fid = 0; fid < NVME_FID_MAX; fid++) {
		switch (fid) {
		case NVME_FEAT_ARBITRATION:
		case NVME_FEAT_POWER_MANAGEMENT:
		case NVME_FEAT_INTERRUPT_COALESCING: //XXX
		case NVME_FEAT_WRITE_ATOMICITY:
			/* Mandatory but no special handling required */
		//XXX hang - case NVME_FEAT_PREDICTABLE_LATENCY_MODE_CONFIG:
		//XXX hang - case NVME_FEAT_HOST_BEHAVIOR_SUPPORT:
		//		  this returns a data buffer
			break;
		case NVME_FEAT_TEMPERATURE_THRESHOLD:
			sc->feat[fid].set = nvme_feature_temperature;
			break;
		case NVME_FEAT_ERROR_RECOVERY:
			sc->feat[fid].namespace_specific = true;
			break;
		case NVME_FEAT_NUMBER_OF_QUEUES:
			sc->feat[fid].set = nvme_feature_num_queues;
			break;
		case NVME_FEAT_INTERRUPT_VECTOR_CONFIGURATION:
			sc->feat[fid].set = nvme_feature_iv_config;
			break;
		case NVME_FEAT_ASYNC_EVENT_CONFIGURATION:
			/* Enable all AENs by default */
			sc->feat[fid].cdw11 = 0x31f;
			break;
		default:
			sc->feat[fid].set = nvme_feature_invalid_cb;
			sc->feat[fid].get = nvme_feature_invalid_cb;
		}
	}
}

static void
pci_nvme_aer_reset(struct pci_nvme_softc *sc)
{

	STAILQ_INIT(&sc->aer_list);
	sc->aer_count = 0;
}

static void
pci_nvme_aer_init(struct pci_nvme_softc *sc)
{

	pthread_mutex_init(&sc->aer_mtx, NULL);
	pci_nvme_aer_reset(sc);
}

static void
pci_nvme_aer_destroy(struct pci_nvme_softc *sc)
{
	struct pci_nvme_aer *aer = NULL;

	pthread_mutex_lock(&sc->aer_mtx);
	while (!STAILQ_EMPTY(&sc->aer_list)) {
		aer = STAILQ_FIRST(&sc->aer_list);
		STAILQ_REMOVE_HEAD(&sc->aer_list, link);
		free(aer);
	}
	pthread_mutex_unlock(&sc->aer_mtx);

	pci_nvme_aer_reset(sc);
}

static bool
pci_nvme_aer_available(struct pci_nvme_softc *sc)
{

	return (sc->aer_count != 0);
}

static bool
pci_nvme_aer_limit_reached(struct pci_nvme_softc *sc)
{
	struct nvme_controller_data *cd = &sc->ctrldata;

	/* AERL is a zero based value while aer_count is one's based */
	return (sc->aer_count == (cd->aerl + 1));
}

/*
 * Add an Async Event Request
 *
 * Stores an AER to be returned later if the Controller needs to notify the
 * host of an event.
 * Note that while the NVMe spec doesn't require Controllers to return AER's
 * in order, this implementation does preserve the order.
 */
static int
pci_nvme_aer_add(struct pci_nvme_softc *sc, uint16_t cid)
{
	struct pci_nvme_aer *aer = NULL;

	aer = calloc(1, sizeof(struct pci_nvme_aer));
	if (aer == NULL)
		return (-1);

	/* Save the Command ID for use in the completion message */
	aer->cid = cid;

	pthread_mutex_lock(&sc->aer_mtx);
	sc->aer_count++;
	STAILQ_INSERT_TAIL(&sc->aer_list, aer, link);
	pthread_mutex_unlock(&sc->aer_mtx);

	return (0);
}

/*
 * Get an Async Event Request structure
 *
 * Returns a pointer to an AER previously submitted by the host or NULL if
 * no AER's exist. Caller is responsible for freeing the returned struct.
 */
static struct pci_nvme_aer *
pci_nvme_aer_get(struct pci_nvme_softc *sc)
{
	struct pci_nvme_aer *aer = NULL;

	pthread_mutex_lock(&sc->aer_mtx);
	aer = STAILQ_FIRST(&sc->aer_list);
	if (aer != NULL) {
		STAILQ_REMOVE_HEAD(&sc->aer_list, link);
		sc->aer_count--;
	}
	pthread_mutex_unlock(&sc->aer_mtx);

	return (aer);
}

static void
pci_nvme_aen_reset(struct pci_nvme_softc *sc)
{
	uint32_t	atype;

	memset(sc->aen, 0, PCI_NVME_AE_TYPE_MAX * sizeof(struct pci_nvme_aen));

	for (atype = 0; atype < PCI_NVME_AE_TYPE_MAX; atype++) {
		sc->aen[atype].atype = atype;
	}
}

static void
pci_nvme_aen_init(struct pci_nvme_softc *sc)
{
	char nstr[80];

	pci_nvme_aen_reset(sc);

	pthread_mutex_init(&sc->aen_mtx, NULL);
	pthread_create(&sc->aen_tid, NULL, aen_thr, sc);
	snprintf(nstr, sizeof(nstr), "nvme-aen-%d:%d", sc->nsc_pi->pi_slot,
	    sc->nsc_pi->pi_func);
	pthread_set_name_np(sc->aen_tid, nstr);
}

static void
pci_nvme_aen_destroy(struct pci_nvme_softc *sc)
{

	pci_nvme_aen_reset(sc);
}

/* Notify the AEN thread of pending work */
static void
pci_nvme_aen_notify(struct pci_nvme_softc *sc)
{

	pthread_cond_signal(&sc->aen_cond);
}

/*
 * Post an Asynchronous Event Notification
 */
static int32_t
pci_nvme_aen_post(struct pci_nvme_softc *sc, pci_nvme_async_type atype,
		uint32_t event_data)
{
	struct pci_nvme_aen *aen;

	if (atype >= PCI_NVME_AE_TYPE_MAX) {
		return(EINVAL);
	}

	pthread_mutex_lock(&sc->aen_mtx);
	aen = &sc->aen[atype];

	/* Has the controller already posted an event of this type? */
	if (aen->posted) {
		pthread_mutex_unlock(&sc->aen_mtx);
		return(EALREADY);
	}

	aen->event_data = event_data;
	aen->posted = true;
	pthread_mutex_unlock(&sc->aen_mtx);

	pci_nvme_aen_notify(sc);

	return(0);
}

static void
pci_nvme_aen_process(struct pci_nvme_softc *sc)
{
	struct pci_nvme_aer *aer;
	struct pci_nvme_aen *aen;
	pci_nvme_async_type atype;
	uint32_t mask;
	uint16_t status;
	uint8_t lid;

	assert(pthread_mutex_isowned_np(&sc->aen_mtx));
	for (atype = 0; atype < PCI_NVME_AE_TYPE_MAX; atype++) {
		aen = &sc->aen[atype];
		/* Previous iterations may have depleted the available AER's */
		if (!pci_nvme_aer_available(sc)) {
			DPRINTF("%s: no AER", __func__);
			break;
		}

		if (!aen->posted) {
			DPRINTF("%s: no AEN posted for atype=%#x", __func__, atype);
			continue;
		}

		status = NVME_SC_SUCCESS;

		/* Is the event masked? */
		mask =
		    sc->feat[NVME_FEAT_ASYNC_EVENT_CONFIGURATION].cdw11;

		DPRINTF("%s: atype=%#x mask=%#x event_data=%#x", __func__, atype, mask, aen->event_data);
		switch (atype) {
		case PCI_NVME_AE_TYPE_ERROR:
			lid = NVME_LOG_ERROR;
			break;
		case PCI_NVME_AE_TYPE_SMART:
			mask &= 0xff;
			if ((mask & aen->event_data) == 0)
				continue;
			lid = NVME_LOG_HEALTH_INFORMATION;
			break;
		case PCI_NVME_AE_TYPE_NOTICE:
			if (aen->event_data >= PCI_NVME_AE_INFO_MAX) {
				EPRINTLN("%s unknown AEN notice type %u",
				    __func__, aen->event_data);
				status = NVME_SC_INTERNAL_DEVICE_ERROR;
				break;
			}
			mask >>= 8;
			if (((1 << aen->event_data) & mask) == 0)
				continue;
			switch (aen->event_data) {
			case PCI_NVME_AE_INFO_NS_ATTR_CHANGED:
				lid = NVME_LOG_CHANGED_NAMESPACE;
				break;
			case PCI_NVME_AE_INFO_FW_ACTIVATION:
				lid = NVME_LOG_FIRMWARE_SLOT;
				break;
			case PCI_NVME_AE_INFO_TELEMETRY_CHANGE:
				lid = NVME_LOG_TELEMETRY_CONTROLLER_INITIATED;
				break;
			case PCI_NVME_AE_INFO_ANA_CHANGE:
				lid = NVME_LOG_ASYMMETRIC_NAMESPAVE_ACCESS; //TODO spelling
				break;
			case PCI_NVME_AE_INFO_PREDICT_LATENCY_CHANGE:
				lid = NVME_LOG_PREDICTABLE_LATENCY_EVENT_AGGREGATE;
				break;
			case PCI_NVME_AE_INFO_LBA_STATUS_ALERT:
				lid = NVME_LOG_LBA_STATUS_INFORMATION;
				break;
			case PCI_NVME_AE_INFO_ENDURANCE_GROUP_CHANGE:
				lid = NVME_LOG_ENDURANCE_GROUP_EVENT_AGGREGATE;
				break;
			default:
				lid = 0;
			}
			break;
		default:
			/* bad type?!? */
			EPRINTLN("%s unknown AEN type %u", __func__, atype);
			status = NVME_SC_INTERNAL_DEVICE_ERROR;
			break;
		}

		aer = pci_nvme_aer_get(sc);
		assert(aer != NULL);

		DPRINTF("%s: CID=%#x CDW0=%#x", __func__, aer->cid, (lid << 16) | (aen->event_data << 8) | atype);
		pci_nvme_cq_update(sc, &sc->compl_queues[0],
		    (lid << 16) | (aen->event_data << 8) | atype, /* cdw0 */
		    aer->cid,
		    0,		/* SQID */
		    status);

		aen->event_data = 0;
		aen->posted = false;

		pci_generate_msix(sc->nsc_pi, 0);
	}
}

static void *
aen_thr(void *arg)
{
	struct pci_nvme_softc *sc;

	sc = arg;

	pthread_mutex_lock(&sc->aen_mtx);
	for (;;) {
		pci_nvme_aen_process(sc);
		pthread_cond_wait(&sc->aen_cond, &sc->aen_mtx);
	}
	pthread_mutex_unlock(&sc->aen_mtx);

	pthread_exit(NULL);
	return (NULL);
}

static void
pci_nvme_reset_locked(struct pci_nvme_softc *sc)
{
	uint32_t i;

	DPRINTF("%s", __func__);

	sc->regs.cap_lo = (ZERO_BASED(sc->max_qentries) & NVME_CAP_LO_REG_MQES_MASK) |
	    (1 << NVME_CAP_LO_REG_CQR_SHIFT) |
	    (60 << NVME_CAP_LO_REG_TO_SHIFT);

	sc->regs.cap_hi = 1 << NVME_CAP_HI_REG_CSS_NVM_SHIFT;

	sc->regs.vs = NVME_REV(1,4);	/* NVMe v1.4 */

	sc->regs.cc = 0;

	assert(sc->submit_queues != NULL);

	for (i = 0; i < sc->num_squeues + 1; i++) {
		sc->submit_queues[i].qbase = NULL;
		sc->submit_queues[i].size = 0;
		sc->submit_queues[i].cqid = 0;
		sc->submit_queues[i].tail = 0;
		sc->submit_queues[i].head = 0;
	}

	assert(sc->compl_queues != NULL);

	for (i = 0; i < sc->num_cqueues + 1; i++) {
		sc->compl_queues[i].qbase = NULL;
		sc->compl_queues[i].size = 0;
		sc->compl_queues[i].tail = 0;
		sc->compl_queues[i].head = 0;
	}

	sc->num_q_is_set = false;

	pci_nvme_aer_destroy(sc);
	pci_nvme_aen_destroy(sc);

	/*
	 * Clear CSTS.RDY last to prevent the host from enabling Controller
	 * before cleanup completes
	 */
	sc->regs.csts = 0;
}

static void
pci_nvme_reset(struct pci_nvme_softc *sc)
{
	pthread_mutex_lock(&sc->mtx);
	pci_nvme_reset_locked(sc);
	pthread_mutex_unlock(&sc->mtx);
}

static void
pci_nvme_init_controller(struct vmctx *ctx, struct pci_nvme_softc *sc)
{
	uint16_t acqs, asqs;

	DPRINTF("%s", __func__);

	asqs = (sc->regs.aqa & NVME_AQA_REG_ASQS_MASK) + 1;
	sc->submit_queues[0].size = asqs;
	sc->submit_queues[0].qbase = vm_map_gpa(ctx, sc->regs.asq,
	            sizeof(struct nvme_command) * asqs);

	DPRINTF("%s mapping Admin-SQ guest 0x%lx, host: %p",
	        __func__, sc->regs.asq, sc->submit_queues[0].qbase);

	acqs = ((sc->regs.aqa >> NVME_AQA_REG_ACQS_SHIFT) &
	    NVME_AQA_REG_ACQS_MASK) + 1;
	sc->compl_queues[0].size = acqs;
	sc->compl_queues[0].qbase = vm_map_gpa(ctx, sc->regs.acq,
	         sizeof(struct nvme_completion) * acqs);
	sc->compl_queues[0].intr_en = NVME_CQ_INTEN;

	DPRINTF("%s mapping Admin-CQ guest 0x%lx, host: %p",
	        __func__, sc->regs.acq, sc->compl_queues[0].qbase);
}

static int
nvme_prp_memcpy(struct vmctx *ctx, uint64_t prp1, uint64_t prp2, uint8_t *b,
	size_t len, enum nvme_copy_dir dir)
{
	uint8_t *p;
	size_t bytes;

	if (len > (8 * 1024)) {
		return (-1);
	}

	/* Copy from the start of prp1 to the end of the physical page */
	bytes = PAGE_SIZE - (prp1 & PAGE_MASK);
	bytes = MIN(bytes, len);

	p = vm_map_gpa(ctx, prp1, bytes);
	if (p == NULL) {
		return (-1);
	}

	if (dir == NVME_COPY_TO_PRP)
		memcpy(p, b, bytes);
	else
		memcpy(b, p, bytes);

	b += bytes;

	len -= bytes;
	if (len == 0) {
		return (0);
	}

	len = MIN(len, PAGE_SIZE);

	p = vm_map_gpa(ctx, prp2, len);
	if (p == NULL) {
		return (-1);
	}

	if (dir == NVME_COPY_TO_PRP)
		memcpy(p, b, len);
	else
		memcpy(b, p, len);

	return (0);
}

/*
 * Write a Completion Queue Entry update
 *
 * Write the completion and update the doorbell value
 */
static void
pci_nvme_cq_update(struct pci_nvme_softc *sc,
		struct nvme_completion_queue *cq,
		uint32_t cdw0,
		uint16_t cid,
		uint16_t sqid,
		uint16_t status)
{
	struct nvme_submission_queue *sq = &sc->submit_queues[sqid];
	struct nvme_completion *cqe;

	assert(cq->qbase != NULL);

	pthread_mutex_lock(&cq->mtx);

	cqe = &cq->qbase[cq->tail];

	/* Flip the phase bit */
	status |= (cqe->status ^ NVME_STATUS_P) & NVME_STATUS_P_MASK;

	cqe->cdw0 = cdw0;
	cqe->sqhd = sq->head;
	cqe->sqid = sqid;
	cqe->cid = cid;
	cqe->status = status;

	cq->tail++;
	if (cq->tail >= cq->size) {
		cq->tail = 0;
	}

	pthread_mutex_unlock(&cq->mtx);
}

static int
nvme_opc_delete_io_sq(struct pci_nvme_softc* sc, struct nvme_command* command,
	struct nvme_completion* compl)
{
	uint16_t qid = command->cdw10 & 0xffff;

	DPRINTF("%s DELETE_IO_SQ %u", __func__, qid);
	if (qid == 0 || qid > sc->num_squeues ||
	    (sc->submit_queues[qid].qbase == NULL)) {
		WPRINTF("%s NOT PERMITTED queue id %u / num_squeues %u",
		        __func__, qid, sc->num_squeues);
		pci_nvme_status_tc(&compl->status, NVME_SCT_COMMAND_SPECIFIC,
		    NVME_SC_INVALID_QUEUE_IDENTIFIER);
		return (1);
	}

	sc->submit_queues[qid].qbase = NULL;
	sc->submit_queues[qid].cqid = 0;
	pci_nvme_status_genc(&compl->status, NVME_SC_SUCCESS);
	return (1);
}

static int
nvme_opc_create_io_sq(struct pci_nvme_softc* sc, struct nvme_command* command,
	struct nvme_completion* compl)
{
	if (command->cdw11 & NVME_CMD_CDW11_PC) {
		uint16_t qid = command->cdw10 & 0xffff;
		struct nvme_submission_queue *nsq;

		if ((qid == 0) || (qid > sc->num_squeues) ||
		    (sc->submit_queues[qid].qbase != NULL)) {
			WPRINTF("%s queue index %u > num_squeues %u",
			        __func__, qid, sc->num_squeues);
			pci_nvme_status_tc(&compl->status,
			    NVME_SCT_COMMAND_SPECIFIC,
			    NVME_SC_INVALID_QUEUE_IDENTIFIER);
			return (1);
		}

		nsq = &sc->submit_queues[qid];
		nsq->size = ONE_BASED((command->cdw10 >> 16) & 0xffff);
		DPRINTF("%s size=%u (max=%u)", __func__, nsq->size, sc->max_qentries);
		if ((nsq->size < 2) || (nsq->size > sc->max_qentries)) {
			/*
			 * Queues must specify at least two entries
			 * NOTE: "MAXIMUM QUEUE SIZE EXCEEDED" was renamed to
			 * "INVALID QUEUE SIZE" in the NVM Express 1.3 Spec
			 */
			pci_nvme_status_tc(&compl->status,
			    NVME_SCT_COMMAND_SPECIFIC,
			    NVME_SC_MAXIMUM_QUEUE_SIZE_EXCEEDED);
			return (1);
		}
		nsq->head = nsq->tail = 0;

		nsq->cqid = (command->cdw11 >> 16) & 0xffff;
		if ((nsq->cqid == 0) || (nsq->cqid > sc->num_cqueues)) {
			pci_nvme_status_tc(&compl->status,
			    NVME_SCT_COMMAND_SPECIFIC,
			    NVME_SC_INVALID_QUEUE_IDENTIFIER);
			return (1);
		}

		if (sc->compl_queues[nsq->cqid].qbase == NULL) {
			pci_nvme_status_tc(&compl->status,
			    NVME_SCT_COMMAND_SPECIFIC,
			    NVME_SC_COMPLETION_QUEUE_INVALID);
			return (1);
		}

		nsq->qpriority = (command->cdw11 >> 1) & 0x03;

		nsq->qbase = vm_map_gpa(sc->nsc_pi->pi_vmctx, command->prp1,
		              sizeof(struct nvme_command) * (size_t)nsq->size);

		DPRINTF("%s sq %u size %u gaddr %p cqid %u", __func__,
		        qid, nsq->size, nsq->qbase, nsq->cqid);

		pci_nvme_status_genc(&compl->status, NVME_SC_SUCCESS);

		DPRINTF("%s completed creating IOSQ qid %u",
		         __func__, qid);
	} else {
		/*
		 * Guest sent non-cont submission queue request.
		 * This setting is unsupported by this emulation.
		 */
		WPRINTF("%s unsupported non-contig (list-based) "
		         "create i/o submission queue", __func__);

		pci_nvme_status_genc(&compl->status, NVME_SC_INVALID_FIELD);
	}
	return (1);
}

static int
nvme_opc_delete_io_cq(struct pci_nvme_softc* sc, struct nvme_command* command,
	struct nvme_completion* compl)
{
	uint16_t qid = command->cdw10 & 0xffff;
	uint16_t sqid;

	DPRINTF("%s DELETE_IO_CQ %u", __func__, qid);
	if (qid == 0 || qid > sc->num_cqueues ||
	    (sc->compl_queues[qid].qbase == NULL)) {
		WPRINTF("%s queue index %u / num_cqueues %u",
		        __func__, qid, sc->num_cqueues);
		pci_nvme_status_tc(&compl->status, NVME_SCT_COMMAND_SPECIFIC,
		    NVME_SC_INVALID_QUEUE_IDENTIFIER);
		return (1);
	}

	/* Deleting an Active CQ is an error */
	for (sqid = 1; sqid < sc->num_squeues + 1; sqid++)
		if (sc->submit_queues[sqid].cqid == qid) {
			pci_nvme_status_tc(&compl->status,
			    NVME_SCT_COMMAND_SPECIFIC,
			    NVME_SC_INVALID_QUEUE_DELETION);
			return (1);
		}

	sc->compl_queues[qid].qbase = NULL;
	pci_nvme_status_genc(&compl->status, NVME_SC_SUCCESS);
	return (1);
}

static int
nvme_opc_create_io_cq(struct pci_nvme_softc* sc, struct nvme_command* command,
	struct nvme_completion* compl)
{
	struct nvme_completion_queue *ncq;
	uint16_t qid = command->cdw10 & 0xffff;

	/* Only support Physically Contiguous queues */
	if ((command->cdw11 & NVME_CMD_CDW11_PC) == 0) {
		WPRINTF("%s unsupported non-contig (list-based) "
		         "create i/o completion queue",
		         __func__);

		pci_nvme_status_genc(&compl->status, NVME_SC_INVALID_FIELD);
		return (1);
	}

	if ((qid == 0) || (qid > sc->num_cqueues) ||
	    (sc->compl_queues[qid].qbase != NULL)) {
		WPRINTF("%s queue index %u > num_cqueues %u",
			__func__, qid, sc->num_cqueues);
		pci_nvme_status_tc(&compl->status,
		    NVME_SCT_COMMAND_SPECIFIC,
		    NVME_SC_INVALID_QUEUE_IDENTIFIER);
		return (1);
 	}

	ncq = &sc->compl_queues[qid];
	ncq->intr_en = (command->cdw11 & NVME_CMD_CDW11_IEN) >> 1;
	ncq->intr_vec = (command->cdw11 >> 16) & 0xffff;
	if (ncq->intr_vec > (sc->max_queues + 1)) {
		pci_nvme_status_tc(&compl->status,
		    NVME_SCT_COMMAND_SPECIFIC,
		    NVME_SC_INVALID_INTERRUPT_VECTOR);
		return (1);
	}

	ncq->size = ONE_BASED((command->cdw10 >> 16) & 0xffff);
	if ((ncq->size < 2) || (ncq->size > sc->max_qentries))  {
		/*
		 * Queues must specify at least two entries
		 * NOTE: "MAXIMUM QUEUE SIZE EXCEEDED" was renamed to
		 * "INVALID QUEUE SIZE" in the NVM Express 1.3 Spec
		 */
		pci_nvme_status_tc(&compl->status,
		    NVME_SCT_COMMAND_SPECIFIC,
		    NVME_SC_MAXIMUM_QUEUE_SIZE_EXCEEDED);
		return (1);
	}
	ncq->head = ncq->tail = 0;
	ncq->qbase = vm_map_gpa(sc->nsc_pi->pi_vmctx,
		     command->prp1,
		     sizeof(struct nvme_command) * (size_t)ncq->size);

	pci_nvme_status_genc(&compl->status, NVME_SC_SUCCESS);


	return (1);
}

static int
nvme_opc_get_log_page(struct pci_nvme_softc* sc, struct nvme_command* command,
	struct nvme_completion* compl)
{
	uint64_t logoff;
	uint32_t logsize;
	uint8_t logpage = command->cdw10 & 0xFF;

	DPRINTF("%s log page %u len %u", __func__, logpage, logsize);

	pci_nvme_status_genc(&compl->status, NVME_SC_SUCCESS);

	/*
	 * Command specifies the number of dwords to return in fields NUMDU
	 * and NUMDL. This is a zero-based value.
	 */
	logsize = ((command->cdw11 << 16) | (command->cdw10 >> 16)) + 1;
	logsize *= sizeof(uint32_t);
	logoff  = ((uint64_t)(command->cdw13) << 32) | command->cdw12;

	switch (logpage) {
	case NVME_LOG_ERROR:
		if (logoff >= sizeof(sc->err_log)) {
			pci_nvme_status_genc(&compl->status,
			    NVME_SC_INVALID_FIELD);
			break;
		}

		nvme_prp_memcpy(sc->nsc_pi->pi_vmctx, command->prp1,
		    command->prp2, (uint8_t *)&sc->err_log + logoff,
		    MIN(logsize - logoff, sizeof(sc->err_log)),
		    NVME_COPY_TO_PRP);
		break;
	case NVME_LOG_HEALTH_INFORMATION:
		if (logoff >= sizeof(sc->health_log)) {
			pci_nvme_status_genc(&compl->status,
			    NVME_SC_INVALID_FIELD);
			break;
		}

		pthread_mutex_lock(&sc->mtx);
		memcpy(&sc->health_log.data_units_read, &sc->read_data_units,
		    sizeof(sc->health_log.data_units_read));
		memcpy(&sc->health_log.data_units_written, &sc->write_data_units,
		    sizeof(sc->health_log.data_units_written));
		memcpy(&sc->health_log.host_read_commands, &sc->read_commands,
		    sizeof(sc->health_log.host_read_commands));
		memcpy(&sc->health_log.host_write_commands, &sc->write_commands,
		    sizeof(sc->health_log.host_write_commands));
		pthread_mutex_unlock(&sc->mtx);

		nvme_prp_memcpy(sc->nsc_pi->pi_vmctx, command->prp1,
		    command->prp2, (uint8_t *)&sc->health_log + logoff,
		    MIN(logsize - logoff, sizeof(sc->health_log)),
		    NVME_COPY_TO_PRP);
		break;
	case NVME_LOG_FIRMWARE_SLOT:
		if (logoff >= sizeof(sc->fw_log)) {
			pci_nvme_status_genc(&compl->status,
			    NVME_SC_INVALID_FIELD);
			break;
		}

		nvme_prp_memcpy(sc->nsc_pi->pi_vmctx, command->prp1,
		    command->prp2, (uint8_t *)&sc->fw_log + logoff,
		    MIN(logsize - logoff, sizeof(sc->fw_log)),
		    NVME_COPY_TO_PRP);
		break;
	case NVME_LOG_CHANGED_NAMESPACE:
		if (logoff >= sizeof(sc->ns_log)) {
			pci_nvme_status_genc(&compl->status,
			    NVME_SC_INVALID_FIELD);
			break;
		}

		nvme_prp_memcpy(sc->nsc_pi->pi_vmctx, command->prp1,
		    command->prp2, (uint8_t *)&sc->ns_log + logoff,
		    MIN(logsize - logoff, sizeof(sc->ns_log)),
		    NVME_COPY_TO_PRP);
		memset(&sc->ns_log, 0, sizeof(sc->ns_log));
		break;
	default:
		DPRINTF("%s get log page %x command not supported",
		        __func__, logpage);

		pci_nvme_status_tc(&compl->status, NVME_SCT_COMMAND_SPECIFIC,
		    NVME_SC_INVALID_LOG_PAGE);
	}

	return (1);
}

static int
nvme_opc_identify(struct pci_nvme_softc* sc, struct nvme_command* command,
	struct nvme_completion* compl)
{
	void *dest;
	uint16_t status;

	DPRINTF("%s identify 0x%x nsid 0x%x", __func__,
	        command->cdw10 & 0xFF, command->nsid);

	pci_nvme_status_genc(&status, NVME_SC_SUCCESS);

	switch (command->cdw10 & 0xFF) {
	case 0x00: /* return Identify Namespace data structure */
		nvme_prp_memcpy(sc->nsc_pi->pi_vmctx, command->prp1,
		    command->prp2, (uint8_t *)&sc->nsdata, sizeof(sc->nsdata),
		    NVME_COPY_TO_PRP);
		break;
	case 0x01: /* return Identify Controller data structure */
		nvme_prp_memcpy(sc->nsc_pi->pi_vmctx, command->prp1,
		    command->prp2, (uint8_t *)&sc->ctrldata,
		    sizeof(sc->ctrldata),
		    NVME_COPY_TO_PRP);
		break;
	case 0x02: /* list of 1024 active NSIDs > CDW1.NSID */
		dest = vm_map_gpa(sc->nsc_pi->pi_vmctx, command->prp1,
		                  sizeof(uint32_t) * 1024);
		/* All unused entries shall be zero */
		bzero(dest, sizeof(uint32_t) * 1024);
		((uint32_t *)dest)[0] = 1;
		break;
	case 0x03: /* list of NSID structures in CDW1.NSID, 4096 bytes */
		if (command->nsid != 1) {
			pci_nvme_status_genc(&status,
			    NVME_SC_INVALID_NAMESPACE_OR_FORMAT);
			break;
		}
		dest = vm_map_gpa(sc->nsc_pi->pi_vmctx, command->prp1,
		                  sizeof(uint32_t) * 1024);
		/* All bytes after the descriptor shall be zero */
		bzero(dest, sizeof(uint32_t) * 1024);

		/* Return NIDT=1 (i.e. EUI64) descriptor */
		((uint8_t *)dest)[0] = 1;
		((uint8_t *)dest)[1] = sizeof(uint64_t);
		bcopy(sc->nsdata.eui64, ((uint8_t *)dest) + 4, sizeof(uint64_t));
		break;
	default:
		DPRINTF("%s unsupported identify command requested 0x%x",
		         __func__, command->cdw10 & 0xFF);
		pci_nvme_status_genc(&status, NVME_SC_INVALID_FIELD);
		break;
	}

	compl->status = status;
	return (1);
}

static const char *
nvme_fid_to_name(uint8_t fid)
{
	const char *name;

	switch (fid) {
	case NVME_FEAT_ARBITRATION:
		name = "Arbitration";
		break;
	case NVME_FEAT_POWER_MANAGEMENT:
		name = "Power Management";
		break;
	case NVME_FEAT_LBA_RANGE_TYPE:
		name = "LBA Range Type";
		break;
	case NVME_FEAT_TEMPERATURE_THRESHOLD:
		name = "Temperature Threshold";
		break;
	case NVME_FEAT_ERROR_RECOVERY:
		name = "Error Recovery";
		break;
	case NVME_FEAT_VOLATILE_WRITE_CACHE:
		name = "Volatile Write Cache";
		break;
	case NVME_FEAT_NUMBER_OF_QUEUES:
		name = "Number of Queues";
		break;
	case NVME_FEAT_INTERRUPT_COALESCING:
		name = "Interrupt Coalescing";
		break;
	case NVME_FEAT_INTERRUPT_VECTOR_CONFIGURATION:
		name = "Interrupt Vector Configuration";
		break;
	case NVME_FEAT_WRITE_ATOMICITY:
		name = "Write Atomicity Normal";
		break;
	case NVME_FEAT_ASYNC_EVENT_CONFIGURATION:
		name = "Asynchronous Event Configuration";
		break;
	case NVME_FEAT_AUTONOMOUS_POWER_STATE_TRANSITION:
		name = "Autonomous Power State Transition";
		break;
	case NVME_FEAT_HOST_MEMORY_BUFFER:
		name = "Host Memory Buffer";
		break;
	case NVME_FEAT_TIMESTAMP:
		name = "Timestamp";
		break;
	case NVME_FEAT_KEEP_ALIVE_TIMER:
		name = "Keep Alive Timer";
		break;
	case NVME_FEAT_HOST_CONTROLLED_THERMAL_MGMT:
		name = "Host Controlled Thermal Management";
		break;
	case NVME_FEAT_NON_OP_POWER_STATE_CONFIG:
		name = "Non-Operation Power State Config";
		break;
	case NVME_FEAT_READ_RECOVERY_LEVEL_CONFIG:
		name = "Read Recovery Level Config";
		break;
	case NVME_FEAT_PREDICTABLE_LATENCY_MODE_CONFIG:
		name = "Predictable Latency Mode Config";
		break;
	case NVME_FEAT_PREDICTABLE_LATENCY_MODE_WINDOW:
		name = "Predictable Latency Mode Window";
		break;
	case NVME_FEAT_LBA_STATUS_INFORMATION_ATTRIBUTES:
		name = "LBA Status Information Report Interval";
		break;
	case NVME_FEAT_HOST_BEHAVIOR_SUPPORT:
		name = "Host Behavior Support";
		break;
	case NVME_FEAT_SANITIZE_CONFIG:
		name = "Sanitize Config";
		break;
	case NVME_FEAT_ENDURANCE_GROUP_EVENT_CONFIGURATION:
		name = "Endurance Group Event Configuration";
		break;
	case NVME_FEAT_SOFTWARE_PROGRESS_MARKER:
		name = "Software Progress Marker";
		break;
	case NVME_FEAT_HOST_IDENTIFIER:
		name = "Host Identifier";
		break;
	case NVME_FEAT_RESERVATION_NOTIFICATION_MASK:
		name = "Reservation Notification Mask";
		break;
	case NVME_FEAT_RESERVATION_PERSISTENCE:
		name = "Reservation Persistence";
		break;
	case NVME_FEAT_NAMESPACE_WRITE_PROTECTION_CONFIG:
		name = "Namespace Write Protection Config";
		break;
	default:
		name = "Unknown";
		break;
	}

	return (name);
}

static void
nvme_feature_invalid_cb(struct pci_nvme_softc *sc,
    struct nvme_feature_obj *feat,
    struct nvme_command *command,
    struct nvme_completion *compl)
{

	pci_nvme_status_genc(&compl->status, NVME_SC_INVALID_FIELD);
}

static void
nvme_feature_iv_config(struct pci_nvme_softc *sc,
    struct nvme_feature_obj *feat,
    struct nvme_command *command,
    struct nvme_completion *compl)
{
	uint32_t i;
	uint32_t cdw11 = command->cdw11;
	uint16_t iv;
	bool cd;

	pci_nvme_status_genc(&compl->status, NVME_SC_INVALID_FIELD);

	iv = cdw11 & 0xffff;
	cd = cdw11 & (1 << 16);

	if (iv > (sc->max_queues + 1)) {
		return;
	}

	/* No Interrupt Coalescing (i.e. not Coalescing Disable) for Admin Q */
	if ((iv == 0) && !cd)
		return;

	/* Requested Interrupt Vector must be used by a CQ */
	for (i = 0; i < sc->num_cqueues + 1; i++) {
		if (sc->compl_queues[i].intr_vec == iv) {
			pci_nvme_status_genc(&compl->status, NVME_SC_SUCCESS);
		}
	}
}

#define NVME_TEMP_THRESH_OVER	0
#define NVME_TEMP_THRESH_UNDER	1
static void
nvme_feature_temperature(struct pci_nvme_softc *sc,
    struct nvme_feature_obj *feat,
    struct nvme_command *command,
    struct nvme_completion *compl)
{
	uint16_t	tmpth;	/* Temperature Threshold */
	uint8_t		tmpsel; /* Threshold Temperature Select */
	uint8_t		thsel;  /* Threshold Type Select */
	bool		set_crit = false;

	tmpth  = command->cdw11 & 0xffff;
	tmpsel = (command->cdw11 >> 16) & 0xf;
	thsel  = (command->cdw11 >> 20) & 0x3;

	DPRINTF("%s: tmpth=%#x tmpsel=%#x thsel=%#x", __func__, tmpth, tmpsel, thsel);

	/* Check for unsupported values */
	if (((tmpsel != 0) && (tmpsel != 0xf)) ||
	    (thsel > NVME_TEMP_THRESH_UNDER)) {
		pci_nvme_status_genc(&compl->status, NVME_SC_INVALID_FIELD);
		return;
	}

	if (((thsel == NVME_TEMP_THRESH_OVER)  && (NVME_TEMPERATURE >= tmpth)) ||
	    ((thsel == NVME_TEMP_THRESH_UNDER) && (NVME_TEMPERATURE <= tmpth)))
		set_crit = true;

	pthread_mutex_lock(&sc->mtx);
	if (set_crit)
		sc->health_log.critical_warning |=
		    NVME_CRIT_WARN_ST_TEMPERATURE;
	else
		sc->health_log.critical_warning &=
		    ~NVME_CRIT_WARN_ST_TEMPERATURE;
	pthread_mutex_unlock(&sc->mtx);

	if (set_crit)
		pci_nvme_aen_post(sc, PCI_NVME_AE_TYPE_SMART,
		    sc->health_log.critical_warning);


	DPRINTF("%s: set_crit=%c critical_warning=%#x status=%#x", __func__, set_crit ? 'T':'F', sc->health_log.critical_warning, compl->status);
}

static void
nvme_feature_num_queues(struct pci_nvme_softc *sc,
    struct nvme_feature_obj *feat,
    struct nvme_command *command,
    struct nvme_completion *compl)
{
	uint16_t nqr;	/* Number of Queues Requested */

	if (sc->num_q_is_set) {
		WPRINTF("%s: Number of Queues already set", __func__);
		pci_nvme_status_genc(&compl->status,
		    NVME_SC_COMMAND_SEQUENCE_ERROR);
		return;
	}

	nqr = command->cdw11 & 0xFFFF;
	if (nqr == 0xffff) {
		WPRINTF("%s: Illegal NSQR value %#x", __func__, nqr);
		pci_nvme_status_genc(&compl->status, NVME_SC_INVALID_FIELD);
		return;
	}

	sc->num_squeues = ONE_BASED(nqr);
	if (sc->num_squeues > sc->max_queues) {
		DPRINTF("NSQR=%u is greater than max %u", sc->num_squeues,
					sc->max_queues);
		sc->num_squeues = sc->max_queues;
	}

	nqr = (command->cdw11 >> 16) & 0xFFFF;
	if (nqr == 0xffff) {
		WPRINTF("%s: Illegal NCQR value %#x", __func__, nqr);
		pci_nvme_status_genc(&compl->status, NVME_SC_INVALID_FIELD);
		return;
	}

	sc->num_cqueues = ONE_BASED(nqr);
	if (sc->num_cqueues > sc->max_queues) {
		DPRINTF("NCQR=%u is greater than max %u", sc->num_cqueues,
					sc->max_queues);
		sc->num_cqueues = sc->max_queues;
	}

	/* Patch the command value which will be saved on callback's return */
	command->cdw11 = NVME_FEATURE_NUM_QUEUES(sc);
	compl->cdw0 = NVME_FEATURE_NUM_QUEUES(sc);

	sc->num_q_is_set = true;
}

static int
nvme_opc_set_features(struct pci_nvme_softc *sc, struct nvme_command *command,
	struct nvme_completion *compl)
{
	struct nvme_feature_obj *feat;
	uint32_t nsid = command->nsid;
	uint8_t fid = command->cdw10 & 0xFF;

	DPRINTF("%s: Feature ID 0x%x (%s)", __func__, fid, nvme_fid_to_name(fid));

	if (fid >= NVME_FID_MAX) {
		DPRINTF("%s invalid feature 0x%x", __func__, fid);
		pci_nvme_status_genc(&compl->status, NVME_SC_INVALID_FIELD);
		return (1);
	}
	feat = &sc->feat[fid];

	if (feat->namespace_specific && (nsid == NVME_GLOBAL_NAMESPACE_TAG)) {
		pci_nvme_status_genc(&compl->status, NVME_SC_INVALID_FIELD);
		return (1);
	}

	if (!feat->namespace_specific &&
	    !((nsid == 0) || (nsid == NVME_GLOBAL_NAMESPACE_TAG))) {
		pci_nvme_status_tc(&compl->status, NVME_SCT_COMMAND_SPECIFIC,
		    NVME_SC_FEATURE_NOT_NS_SPECIFIC);
		return (1);
	}

	compl->cdw0 = 0;
	pci_nvme_status_genc(&compl->status, NVME_SC_SUCCESS);

	if (feat->set)
		feat->set(sc, feat, command, compl);

	DPRINTF("%s: status=%#x cdw11=%#x", __func__, compl->status, command->cdw11);
	if (compl->status == NVME_SC_SUCCESS) {
		feat->cdw11 = command->cdw11;
		if ((fid == NVME_FEAT_ASYNC_EVENT_CONFIGURATION) &&
		    (command->cdw11 != 0))
			pci_nvme_aen_notify(sc);
	}

	return (0);
}

static int
nvme_opc_get_features(struct pci_nvme_softc* sc, struct nvme_command* command,
	struct nvme_completion* compl)
{
	struct nvme_feature_obj *feat;
	uint8_t fid = command->cdw10 & 0xFF;

	DPRINTF("%s: Feature ID 0x%x (%s)", __func__, fid, nvme_fid_to_name(fid));

	if (fid >= NVME_FID_MAX) {
		DPRINTF("%s invalid feature 0x%x", __func__, fid);
		pci_nvme_status_genc(&compl->status, NVME_SC_INVALID_FIELD);
		return (1);
	}

	compl->cdw0 = 0;
	pci_nvme_status_genc(&compl->status, NVME_SC_SUCCESS);

	feat = &sc->feat[fid];
	if (feat->get) {
		feat->get(sc, feat, command, compl);
	}

	if (compl->status == NVME_SC_SUCCESS) {
		compl->cdw0 = feat->cdw11;
	}

	return (0);
}

static int
nvme_opc_format_nvm(struct pci_nvme_softc* sc, struct nvme_command* command,
	struct nvme_completion* compl)
{
	uint8_t	ses, lbaf, pi;

	/* Only supports Secure Erase Setting - User Data Erase */
	ses = (command->cdw10 >> 9) & 0x7;
	if (ses > 0x1) {
		pci_nvme_status_genc(&compl->status, NVME_SC_INVALID_FIELD);
		return (1);
	}

	/* Only supports a single LBA Format */
	lbaf = command->cdw10 & 0xf;
	if (lbaf != 0) {
		pci_nvme_status_tc(&compl->status, NVME_SCT_COMMAND_SPECIFIC,
		    NVME_SC_INVALID_FORMAT);
		return (1);
	}

	/* Doesn't support Protection Infomation */
	pi = (command->cdw10 >> 5) & 0x7;
	if (pi != 0) {
		pci_nvme_status_genc(&compl->status, NVME_SC_INVALID_FIELD);
		return (1);
	}

	if (sc->nvstore.type == NVME_STOR_RAM) {
		if (sc->nvstore.ctx)
			free(sc->nvstore.ctx);
		sc->nvstore.ctx = calloc(1, sc->nvstore.size);
		pci_nvme_status_genc(&compl->status, NVME_SC_SUCCESS);
	} else {
		struct pci_nvme_ioreq *req;
		int err;

		req = pci_nvme_get_ioreq(sc);
		if (req == NULL) {
			pci_nvme_status_genc(&compl->status,
			    NVME_SC_INTERNAL_DEVICE_ERROR);
			WPRINTF("%s: unable to allocate IO req", __func__);
			return (1);
		}
		req->nvme_sq = &sc->submit_queues[0];
		req->sqid = 0;
		req->opc = command->opc;
		req->cid = command->cid;
		req->nsid = command->nsid;

		req->io_req.br_offset = 0;
		req->io_req.br_resid = sc->nvstore.size;
		req->io_req.br_callback = pci_nvme_io_done;

		err = blockif_delete(sc->nvstore.ctx, &req->io_req);
		if (err) {
			pci_nvme_status_genc(&compl->status,
			    NVME_SC_INTERNAL_DEVICE_ERROR);
			pci_nvme_release_ioreq(sc, req);
		} else
			compl->status = NVME_NO_STATUS;
	}

	return (1);
}

static int
nvme_opc_abort(struct pci_nvme_softc* sc, struct nvme_command* command,
	struct nvme_completion* compl)
{
	DPRINTF("%s submission queue %u, command ID 0x%x", __func__,
	        command->cdw10 & 0xFFFF, (command->cdw10 >> 16) & 0xFFFF);

	/* TODO: search for the command ID and abort it */

	compl->cdw0 = 1;
	pci_nvme_status_genc(&compl->status, NVME_SC_SUCCESS);
	return (1);
}

static int
nvme_opc_async_event_req(struct pci_nvme_softc* sc,
	struct nvme_command* command, struct nvme_completion* compl)
{
	DPRINTF("%s async event request count=%u aerl=%u cid=%#x", __func__,
	    sc->aer_count, sc->ctrldata.aerl, command->cid);

	/* Don't exceed the Async Event Request Limit (AERL). */
	if (pci_nvme_aer_limit_reached(sc)) {
		pci_nvme_status_tc(&compl->status, NVME_SCT_COMMAND_SPECIFIC,
				NVME_SC_ASYNC_EVENT_REQUEST_LIMIT_EXCEEDED);
		return (1);
	}

	if (pci_nvme_aer_add(sc, command->cid)) {
		pci_nvme_status_tc(&compl->status, NVME_SCT_GENERIC,
				NVME_SC_INTERNAL_DEVICE_ERROR);
		return (1);
	}

	/*
	 * Raise events when they happen based on the Set Features cmd.
	 * These events happen async, so only set completion successful if
	 * there is an event reflective of the request to get event.
	 */
	compl->status = NVME_NO_STATUS;
	pci_nvme_aen_notify(sc);

	return (0);
}

static void
pci_nvme_handle_admin_cmd(struct pci_nvme_softc* sc, uint64_t value)
{
	struct nvme_completion compl;
	struct nvme_command *cmd;
	struct nvme_submission_queue *sq;
	struct nvme_completion_queue *cq;
	uint16_t sqhead;

	DPRINTF("%s index %u", __func__, (uint32_t)value);

	sq = &sc->submit_queues[0];
	cq = &sc->compl_queues[0];

	pthread_mutex_lock(&sq->mtx);

	sqhead = sq->head;
	DPRINTF("sqhead %u, tail %u", sqhead, sq->tail);

	while (sqhead != atomic_load_acq_short(&sq->tail)) {
		cmd = &(sq->qbase)[sqhead];
		compl.cdw0 = 0;
		compl.status = 0;

		switch (cmd->opc) {
		case NVME_OPC_DELETE_IO_SQ:
			DPRINTF("%s command DELETE_IO_SQ", __func__);
			nvme_opc_delete_io_sq(sc, cmd, &compl);
			break;
		case NVME_OPC_CREATE_IO_SQ:
			DPRINTF("%s command CREATE_IO_SQ", __func__);
			nvme_opc_create_io_sq(sc, cmd, &compl);
			break;
		case NVME_OPC_DELETE_IO_CQ:
			DPRINTF("%s command DELETE_IO_CQ", __func__);
			nvme_opc_delete_io_cq(sc, cmd, &compl);
			break;
		case NVME_OPC_CREATE_IO_CQ:
			DPRINTF("%s command CREATE_IO_CQ", __func__);
			nvme_opc_create_io_cq(sc, cmd, &compl);
			break;
		case NVME_OPC_GET_LOG_PAGE:
			DPRINTF("%s command GET_LOG_PAGE", __func__);
			nvme_opc_get_log_page(sc, cmd, &compl);
			break;
		case NVME_OPC_IDENTIFY:
			DPRINTF("%s command IDENTIFY", __func__);
			nvme_opc_identify(sc, cmd, &compl);
			break;
		case NVME_OPC_ABORT:
			DPRINTF("%s command ABORT", __func__);
			nvme_opc_abort(sc, cmd, &compl);
			break;
		case NVME_OPC_SET_FEATURES:
			DPRINTF("%s command SET_FEATURES", __func__);
			nvme_opc_set_features(sc, cmd, &compl);
			break;
		case NVME_OPC_GET_FEATURES:
			DPRINTF("%s command GET_FEATURES", __func__);
			nvme_opc_get_features(sc, cmd, &compl);
			break;
		case NVME_OPC_FIRMWARE_ACTIVATE:
			DPRINTF("%s command FIRMWARE_ACTIVATE", __func__);
			pci_nvme_status_tc(&compl.status,
			    NVME_SCT_COMMAND_SPECIFIC,
			    NVME_SC_INVALID_FIRMWARE_SLOT);
			break;
		case NVME_OPC_ASYNC_EVENT_REQUEST:
			DPRINTF("%s command ASYNC_EVENT_REQ", __func__);
			nvme_opc_async_event_req(sc, cmd, &compl);
			break;
		case NVME_OPC_FORMAT_NVM:
			DPRINTF("%s command FORMAT_NVM", __func__);
			if ((sc->ctrldata.oacs &
			    (1 << NVME_CTRLR_DATA_OACS_FORMAT_SHIFT)) == 0) {
				pci_nvme_status_genc(&compl.status, NVME_SC_INVALID_OPCODE);
				break;
			}
			nvme_opc_format_nvm(sc, cmd, &compl);
			break;
		case NVME_OPC_SECURITY_SEND:
		case NVME_OPC_SECURITY_RECEIVE:
		case NVME_OPC_SANITIZE:
		case NVME_OPC_GET_LBA_STATUS:
			DPRINTF("%s command OPC=%#x (unsupported)", __func__,
			    cmd->opc);
			/* Valid but unsupported opcodes */
			pci_nvme_status_genc(&compl.status, NVME_SC_INVALID_FIELD);
			break;
		default:
			DPRINTF("%s command OPC=%#X (not implemented)",
			    __func__,
			    cmd->opc);
			pci_nvme_status_genc(&compl.status, NVME_SC_INVALID_OPCODE);
		}
		sqhead = (sqhead + 1) % sq->size;

		if (NVME_COMPLETION_VALID(compl)) {
			pci_nvme_cq_update(sc, &sc->compl_queues[0],
			    compl.cdw0,
			    cmd->cid,
			    0,		/* SQID */
			    compl.status);
		}
	}

	DPRINTF("setting sqhead %u", sqhead);
	sq->head = sqhead;

	if (cq->head != cq->tail)
		pci_generate_msix(sc->nsc_pi, 0);

	pthread_mutex_unlock(&sq->mtx);
}

/*
 * Update the Write and Read statistics reported in SMART data
 *
 * NVMe defines "data unit" as thousand's of 512 byte blocks and is rounded up.
 * E.g. 1 data unit is 1 - 1,000 512 byte blocks. 3 data units are 2,001 - 3,000
 * 512 byte blocks. Rounding up is acheived by initializing the remainder to 999.
 */
static void
pci_nvme_stats_write_read_update(struct pci_nvme_softc *sc, uint8_t opc,
    size_t bytes, uint16_t status)
{

	pthread_mutex_lock(&sc->mtx);
	switch (opc) {
	case NVME_OPC_WRITE:
		sc->write_commands++;
		if (status != NVME_SC_SUCCESS)
			break;
		sc->write_dunits_remainder += (bytes / 512);
		while (sc->write_dunits_remainder >= 1000) {
			sc->write_data_units++;
			sc->write_dunits_remainder -= 1000;
		}
		break;
	case NVME_OPC_READ:
		sc->read_commands++;
		if (status != NVME_SC_SUCCESS)
			break;
		sc->read_dunits_remainder += (bytes / 512);
		while (sc->read_dunits_remainder >= 1000) {
			sc->read_data_units++;
			sc->read_dunits_remainder -= 1000;
		}
		break;
	default:
		DPRINTF("%s: Invalid OPC 0x%02x for stats", __func__, opc);
		break;
	}
	pthread_mutex_unlock(&sc->mtx);
}

/*
 * Check if the combination of Starting LBA (slba) and Number of Logical
 * Blocks (nlb) exceeds the range of the underlying storage.
 *
 * Because NVMe specifies the SLBA in blocks as a uint64_t and blockif stores
 * the capacity in bytes as a uint64_t, care must be taken to avoid integer
 * overflow.
 */
static bool
pci_nvme_out_of_range(struct pci_nvme_blockstore *nvstore, uint64_t slba,
    uint32_t nlb)
{
	size_t	offset, bytes;

	/* Overflow check of multiplying Starting LBA by the sector size */
	if (slba >> (64 - nvstore->sectsz_bits))
		return (true);

	offset = slba << nvstore->sectsz_bits;
	bytes = nlb << nvstore->sectsz_bits;

	/* Overflow check of Number of Logical Blocks */
	if ((nvstore->size - offset) < bytes)
		return (true);

	return (false);
}

static int
pci_nvme_append_iov_req(struct pci_nvme_softc *sc, struct pci_nvme_ioreq *req,
	uint64_t gpaddr, size_t size, int do_write, uint64_t lba)
{
	int iovidx;

	if (req == NULL)
		return (-1);

	if (req->io_req.br_iovcnt == NVME_MAX_IOVEC) {
		return (-1);
	}

	/* concatenate contig block-iovs to minimize number of iovs */
	if ((req->prev_gpaddr + req->prev_size) == gpaddr) {
		iovidx = req->io_req.br_iovcnt - 1;

		req->io_req.br_iov[iovidx].iov_base =
		    paddr_guest2host(req->sc->nsc_pi->pi_vmctx,
				     req->prev_gpaddr, size);

		req->prev_size += size;
		req->io_req.br_resid += size;

		req->io_req.br_iov[iovidx].iov_len = req->prev_size;
	} else {
		iovidx = req->io_req.br_iovcnt;
		if (iovidx == 0) {
			req->io_req.br_offset = lba;
			req->io_req.br_resid = 0;
			req->io_req.br_param = req;
		}

		req->io_req.br_iov[iovidx].iov_base =
		    paddr_guest2host(req->sc->nsc_pi->pi_vmctx,
				     gpaddr, size);

		req->io_req.br_iov[iovidx].iov_len = size;

		req->prev_gpaddr = gpaddr;
		req->prev_size = size;
		req->io_req.br_resid += size;

		req->io_req.br_iovcnt++;
	}

	return (0);
}

static void
pci_nvme_set_completion(struct pci_nvme_softc *sc,
	struct nvme_submission_queue *sq, int sqid, uint16_t cid,
	uint32_t cdw0, uint16_t status)
{
	struct nvme_completion_queue *cq = &sc->compl_queues[sq->cqid];

	DPRINTF("%s sqid %d cqid %u cid %u status: 0x%x 0x%x",
		 __func__, sqid, sq->cqid, cid, NVME_STATUS_GET_SCT(status),
		 NVME_STATUS_GET_SC(status));

	pci_nvme_cq_update(sc, cq,
	    0,		/* CDW0 */
	    cid,
	    sqid,
	    status);

	if (cq->head != cq->tail) {
		if (cq->intr_en & NVME_CQ_INTEN) {
			pci_generate_msix(sc->nsc_pi, cq->intr_vec);
		} else {
			DPRINTF("%s: CQ%u interrupt disabled",
						__func__, sq->cqid);
		}
	}
}

static void
pci_nvme_release_ioreq(struct pci_nvme_softc *sc, struct pci_nvme_ioreq *req)
{
	req->sc = NULL;
	req->nvme_sq = NULL;
	req->sqid = 0;

	pthread_mutex_lock(&sc->mtx);

	STAILQ_INSERT_TAIL(&sc->ioreqs_free, req, link);
	sc->pending_ios--;

	/* when no more IO pending, can set to ready if device reset/enabled */
	if (sc->pending_ios == 0 &&
	    NVME_CC_GET_EN(sc->regs.cc) && !(NVME_CSTS_GET_RDY(sc->regs.csts)))
		sc->regs.csts |= NVME_CSTS_RDY;

	pthread_mutex_unlock(&sc->mtx);

	sem_post(&sc->iosemlock);
}

static struct pci_nvme_ioreq *
pci_nvme_get_ioreq(struct pci_nvme_softc *sc)
{
	struct pci_nvme_ioreq *req = NULL;

	sem_wait(&sc->iosemlock);
	pthread_mutex_lock(&sc->mtx);

	req = STAILQ_FIRST(&sc->ioreqs_free);
	assert(req != NULL);
	STAILQ_REMOVE_HEAD(&sc->ioreqs_free, link);

	req->sc = sc;

	sc->pending_ios++;

	pthread_mutex_unlock(&sc->mtx);

	req->io_req.br_iovcnt = 0;
	req->io_req.br_offset = 0;
	req->io_req.br_resid = 0;
	req->io_req.br_param = req;
	req->prev_gpaddr = 0;
	req->prev_size = 0;

	return req;
}

static void
pci_nvme_io_done(struct blockif_req *br, int err)
{
	struct pci_nvme_ioreq *req = br->br_param;
	struct nvme_submission_queue *sq = req->nvme_sq;
	uint16_t code, status;

	DPRINTF("%s error %d %s", __func__, err, strerror(err));

	/* TODO return correct error */
	code = err ? NVME_SC_DATA_TRANSFER_ERROR : NVME_SC_SUCCESS;
	pci_nvme_status_genc(&status, code);

	pci_nvme_set_completion(req->sc, sq, req->sqid, req->cid, 0, status);
	pci_nvme_stats_write_read_update(req->sc, req->opc,
	    req->bytes, status);
	pci_nvme_release_ioreq(req->sc, req);
}

/*
 * Implements the Flush command. The specification states:
 *    If a volatile write cache is not present, Flush commands complete
 *    successfully and have no effect
 * in the description of the Volatile Write Cache (VWC) field of the Identify
 * Controller data. Therefore, set status to Success if the command is
 * not supported (i.e. RAM or as indicated by the blockif).
 */
static bool
nvme_opc_flush(struct pci_nvme_softc *sc,
    struct nvme_command *cmd,
    struct pci_nvme_blockstore *nvstore,
    struct pci_nvme_ioreq *req,
    uint16_t *status)
{
	bool pending = false;

	if (nvstore->type == NVME_STOR_RAM) {
		pci_nvme_status_genc(status, NVME_SC_SUCCESS);
	} else {
		int err;

		req->io_req.br_callback = pci_nvme_io_done;

		err = blockif_flush(nvstore->ctx, &req->io_req);
		switch (err) {
		case 0:
			pending = true;
			break;
		case EOPNOTSUPP:
			pci_nvme_status_genc(status, NVME_SC_SUCCESS);
			break;
		default:
			pci_nvme_status_genc(status, NVME_SC_INTERNAL_DEVICE_ERROR);
		}
	}

	return (pending);
}

static uint16_t
nvme_write_read_ram(struct pci_nvme_softc *sc,
    struct pci_nvme_blockstore *nvstore,
    uint64_t prp1, uint64_t prp2,
    size_t offset, uint64_t bytes,
    bool is_write)
{
	uint8_t *buf = nvstore->ctx;
	enum nvme_copy_dir dir;
	uint16_t status;

	if (is_write)
		dir = NVME_COPY_TO_PRP;
	else
		dir = NVME_COPY_FROM_PRP;

	if (nvme_prp_memcpy(sc->nsc_pi->pi_vmctx, prp1, prp2,
	    buf + offset, bytes, dir))
		pci_nvme_status_genc(&status,
		    NVME_SC_DATA_TRANSFER_ERROR);
	else
		pci_nvme_status_genc(&status, NVME_SC_SUCCESS);

	return (status);
}

static uint16_t
nvme_write_read_blockif(struct pci_nvme_softc *sc,
    struct pci_nvme_blockstore *nvstore,
    struct pci_nvme_ioreq *req,
    uint64_t prp1, uint64_t prp2,
    size_t offset, uint64_t bytes,
    bool is_write)
{
	uint64_t size;
	int err;
	uint16_t status = NVME_NO_STATUS;

	size = MIN(PAGE_SIZE - (prp1 % PAGE_SIZE), bytes);
	if (pci_nvme_append_iov_req(sc, req, prp1,
	    size, is_write, offset)) {
		pci_nvme_status_genc(&status,
		    NVME_SC_DATA_TRANSFER_ERROR);
		goto out;
	}

	offset += size;
	bytes  -= size;

	if (bytes == 0) {
		;
	} else if (bytes <= PAGE_SIZE) {
		size = bytes;
		if (pci_nvme_append_iov_req(sc, req, prp2,
		    size, is_write, offset)) {
			pci_nvme_status_genc(&status,
			    NVME_SC_DATA_TRANSFER_ERROR);
			goto out;
		}
	} else {
		void *vmctx = sc->nsc_pi->pi_vmctx;
		uint64_t *prp_list = &prp2;
		uint64_t *last = prp_list;

		/* PRP2 is pointer to a physical region page list */
		while (bytes) {
			/* Last entry in list points to the next list */
			if ((prp_list == last) && (bytes > PAGE_SIZE)) {
				uint64_t prp = *prp_list;

				prp_list = paddr_guest2host(vmctx, prp,
				    PAGE_SIZE - (prp % PAGE_SIZE));
				last = prp_list + (NVME_PRP2_ITEMS - 1);
			}

			size = MIN(bytes, PAGE_SIZE);

			if (pci_nvme_append_iov_req(sc, req, *prp_list,
			    size, is_write, offset)) {
				pci_nvme_status_genc(&status,
				    NVME_SC_DATA_TRANSFER_ERROR);
				goto out;
			}

			offset += size;
			bytes  -= size;

			prp_list++;
		}
	}
	req->io_req.br_callback = pci_nvme_io_done;
	if (is_write)
		err = blockif_write(nvstore->ctx, &req->io_req);
	else
		err = blockif_read(nvstore->ctx, &req->io_req);

	if (err)
		pci_nvme_status_genc(&status, NVME_SC_DATA_TRANSFER_ERROR);
out:
	return (status);
}

static bool
nvme_opc_write_read(struct pci_nvme_softc *sc,
    struct nvme_command *cmd,
    struct pci_nvme_blockstore *nvstore,
    struct pci_nvme_ioreq *req,
    uint16_t *status)
{
	uint64_t lba, nblocks, bytes;
	size_t offset;
	bool is_write = cmd->opc == NVME_OPC_WRITE;
	bool pending = false;

	lba = ((uint64_t)cmd->cdw11 << 32) | cmd->cdw10;
	nblocks = (cmd->cdw12 & 0xFFFF) + 1;

	if (pci_nvme_out_of_range(nvstore, lba, nblocks)) {
		WPRINTF("%s command would exceed LBA range", __func__);
		pci_nvme_status_genc(status, NVME_SC_LBA_OUT_OF_RANGE);
		goto out;
	}

	bytes  = nblocks << nvstore->sectsz_bits;
	if (bytes > NVME_MAX_DATA_SIZE) {
		WPRINTF("%s command would exceed MDTS", __func__);
		pci_nvme_status_genc(status, NVME_SC_INVALID_FIELD);
		goto out;
	}

	offset = lba << nvstore->sectsz_bits;

	req->bytes = bytes;
	req->io_req.br_offset = lba;

	/* PRP bits 1:0 must be zero */
	cmd->prp1 &= ~0x3UL;
	cmd->prp2 &= ~0x3UL;

	if (nvstore->type == NVME_STOR_RAM) {
		*status = nvme_write_read_ram(sc, nvstore, cmd->prp1,
		    cmd->prp2, offset, bytes, is_write);
	} else {
		*status = nvme_write_read_blockif(sc, nvstore, req,
		    cmd->prp1, cmd->prp2, offset, bytes, is_write);

		if (*status == NVME_NO_STATUS)
			pending = true;
	}
out:
	if (!pending)
		pci_nvme_stats_write_read_update(sc, cmd->opc, bytes, *status);

	return (pending);
}

static void
pci_nvme_dealloc_sm(struct blockif_req *br, int err)
{
	struct pci_nvme_ioreq *req = br->br_param;
	struct pci_nvme_softc *sc = req->sc;
	bool done = true;
	uint16_t status;

	if (err) {
		pci_nvme_status_genc(&status, NVME_SC_INTERNAL_DEVICE_ERROR);
	} else if ((req->prev_gpaddr + 1) == (req->prev_size)) {
		pci_nvme_status_genc(&status, NVME_SC_SUCCESS);
	} else {
		struct iovec *iov = req->io_req.br_iov;

		req->prev_gpaddr++;
		iov += req->prev_gpaddr;

		/* The iov_* values already include the sector size */
		req->io_req.br_offset = (off_t)iov->iov_base;
		req->io_req.br_resid = iov->iov_len;
		if (blockif_delete(sc->nvstore.ctx, &req->io_req)) {
			pci_nvme_status_genc(&status,
			    NVME_SC_INTERNAL_DEVICE_ERROR);
		} else
			done = false;
	}

	if (done) {
		pci_nvme_set_completion(sc, req->nvme_sq, req->sqid,
		    req->cid, 0, status);
		pci_nvme_release_ioreq(sc, req);
	}
}

static bool
nvme_opc_dataset_mgmt(struct pci_nvme_softc *sc,
    struct nvme_command *cmd,
    struct pci_nvme_blockstore *nvstore,
    struct pci_nvme_ioreq *req,
    uint16_t *status)
{
	struct nvme_dsm_range *range;
	uint32_t nr, r, non_zero, dr;
	int err;
	bool pending = false;

	if ((sc->ctrldata.oncs & NVME_ONCS_DSM) == 0) {
		pci_nvme_status_genc(status, NVME_SC_INVALID_OPCODE);
		goto out;
	}

	nr = cmd->cdw10 & 0xff;

	/* copy locally because a range entry could straddle PRPs */
	range = calloc(1, NVME_MAX_DSM_TRIM);
	if (range == NULL) {
		pci_nvme_status_genc(status, NVME_SC_INTERNAL_DEVICE_ERROR);
		goto out;
	}
	nvme_prp_memcpy(sc->nsc_pi->pi_vmctx, cmd->prp1, cmd->prp2,
	    (uint8_t *)range, NVME_MAX_DSM_TRIM, NVME_COPY_FROM_PRP);

	/* Check for invalid ranges and the number of non-zero lengths */
	non_zero = 0;
	for (r = 0; r <= nr; r++) {
		if (pci_nvme_out_of_range(nvstore,
		    range[r].starting_lba, range[r].length)) {
			pci_nvme_status_genc(status, NVME_SC_LBA_OUT_OF_RANGE);
			goto out;
		}
		if (range[r].length != 0)
			non_zero++;
	}

	if (cmd->cdw11 & NVME_DSM_ATTR_DEALLOCATE) {
		size_t offset, bytes;
		int sectsz_bits = sc->nvstore.sectsz_bits;

		/*
		 * DSM calls are advisory only, and compliant controllers
		 * may choose to take no actions (i.e. return Success).
		 */
		if (!nvstore->deallocate) {
			pci_nvme_status_genc(status, NVME_SC_SUCCESS);
			goto out;
		}

		/* If all ranges have a zero length, return Success */
		if (non_zero == 0) {
			pci_nvme_status_genc(status, NVME_SC_SUCCESS);
			goto out;
		}

		if (req == NULL) {
			pci_nvme_status_genc(status, NVME_SC_INTERNAL_DEVICE_ERROR);
			goto out;
		}

		offset = range[0].starting_lba << sectsz_bits;
		bytes = range[0].length << sectsz_bits;

		/*
		 * If the request is for more than a single range, store
		 * the ranges in the br_iov. Optimize for the common case
		 * of a single range.
		 *
		 * Note that NVMe Number of Ranges is a zero based value
		 */
		req->io_req.br_iovcnt = 0;
		req->io_req.br_offset = offset;
		req->io_req.br_resid = bytes;

		if (nr == 0) {
			req->io_req.br_callback = pci_nvme_io_done;
		} else {
			struct iovec *iov = req->io_req.br_iov;

			for (r = 0, dr = 0; r <= nr; r++) {
				offset = range[r].starting_lba << sectsz_bits;
				bytes = range[r].length << sectsz_bits;
				if (bytes == 0)
					continue;

				if ((nvstore->size - offset) < bytes) {
					pci_nvme_status_genc(status,
					    NVME_SC_LBA_OUT_OF_RANGE);
					goto out;
				}
				iov[dr].iov_base = (void *)offset;
				iov[dr].iov_len = bytes;
				dr++;
			}
			req->io_req.br_callback = pci_nvme_dealloc_sm;

			/*
			 * Use prev_gpaddr to track the current entry and
			 * prev_size to track the number of entries
			 */
			req->prev_gpaddr = 0;
			req->prev_size = dr;
		}

		err = blockif_delete(nvstore->ctx, &req->io_req);
		if (err)
			pci_nvme_status_genc(status, NVME_SC_INTERNAL_DEVICE_ERROR);
		else
			pending = true;
	}
out:
	free(range);
	return (pending);
}

static void
pci_nvme_handle_io_cmd(struct pci_nvme_softc* sc, uint16_t idx)
{
	struct nvme_submission_queue *sq;
	uint16_t status;
	uint16_t sqhead;

	/* handle all submissions up to sq->tail index */
	sq = &sc->submit_queues[idx];

	pthread_mutex_lock(&sq->mtx);

	sqhead = sq->head;
	DPRINTF("nvme_handle_io qid %u head %u tail %u cmdlist %p",
	         idx, sqhead, sq->tail, sq->qbase);

	while (sqhead != atomic_load_acq_short(&sq->tail)) {
		struct nvme_command *cmd;
		struct pci_nvme_ioreq *req;
		uint32_t nsid;
		bool pending;

		pending = false;
		req = NULL;
		status = 0;

		cmd = &sq->qbase[sqhead];
		sqhead = (sqhead + 1) % sq->size;

		nsid = le32toh(cmd->nsid);
		if ((nsid == 0) || (nsid > sc->ctrldata.nn)) {
			pci_nvme_status_genc(&status,
			    NVME_SC_INVALID_NAMESPACE_OR_FORMAT);
			status |=
			    NVME_STATUS_DNR_MASK << NVME_STATUS_DNR_SHIFT;
			goto complete;
 		}

		req = pci_nvme_get_ioreq(sc);
		if (req == NULL) {
			pci_nvme_status_genc(&status,
			    NVME_SC_INTERNAL_DEVICE_ERROR);
			WPRINTF("%s: unable to allocate IO req", __func__);
			goto complete;
		}
		req->nvme_sq = sq;
		req->sqid = idx;
		req->opc = cmd->opc;
		req->cid = cmd->cid;
		req->nsid = cmd->nsid;

		switch (cmd->opc) {
		case NVME_OPC_FLUSH:
			pending = nvme_opc_flush(sc, cmd, &sc->nvstore,
			    req, &status);
 			break;
		case NVME_OPC_WRITE:
		case NVME_OPC_READ:
			pending = nvme_opc_write_read(sc, cmd, &sc->nvstore,
			    req, &status);
			break;
		case NVME_OPC_WRITE_ZEROES:
			/* TODO: write zeroes
			WPRINTF("%s write zeroes lba 0x%lx blocks %u",
			        __func__, lba, cmd->cdw12 & 0xFFFF); */
			pci_nvme_status_genc(&status, NVME_SC_SUCCESS);
			break;
		case NVME_OPC_DATASET_MANAGEMENT:
 			pending = nvme_opc_dataset_mgmt(sc, cmd, &sc->nvstore,
			    req, &status);
			break;
 		default:
 			WPRINTF("%s unhandled io command 0x%x",
			    __func__, cmd->opc);
			pci_nvme_status_genc(&status, NVME_SC_INVALID_OPCODE);
		}
complete:
		if (!pending) {
			pci_nvme_set_completion(sc, sq, idx, cmd->cid, 0,
			    status);
			if (req != NULL)
				pci_nvme_release_ioreq(sc, req);
		}
	}

	sq->head = sqhead;

	pthread_mutex_unlock(&sq->mtx);
}

static void
pci_nvme_handle_doorbell(struct vmctx *ctx, struct pci_nvme_softc* sc,
	uint64_t idx, int is_sq, uint64_t value)
{
	DPRINTF("nvme doorbell %lu, %s, val 0x%lx",
	        idx, is_sq ? "SQ" : "CQ", value & 0xFFFF);

	if (is_sq) {
		if (idx > sc->num_squeues) {
			WPRINTF("%s queue index %lu overflow from "
			         "guest (max %u)",
			         __func__, idx, sc->num_squeues);
			return;
		}

		atomic_store_short(&sc->submit_queues[idx].tail,
		                   (uint16_t)value);

		if (idx == 0) {
			pci_nvme_handle_admin_cmd(sc, value);
		} else {
			/* submission queue; handle new entries in SQ */
			if (idx > sc->num_squeues) {
				WPRINTF("%s SQ index %lu overflow from "
				         "guest (max %u)",
				         __func__, idx, sc->num_squeues);
				return;
			}
			pci_nvme_handle_io_cmd(sc, (uint16_t)idx);
		}
	} else {
		if (idx > sc->num_cqueues) {
			WPRINTF("%s queue index %lu overflow from "
			         "guest (max %u)",
			         __func__, idx, sc->num_cqueues);
			return;
		}

		atomic_store_short(&sc->compl_queues[idx].head,
				(uint16_t)value);
	}
}

static void
pci_nvme_bar0_reg_dumps(const char *func, uint64_t offset, int iswrite)
{
	const char *s = iswrite ? "WRITE" : "READ";

	switch (offset) {
	case NVME_CR_CAP_LOW:
		DPRINTF("%s %s NVME_CR_CAP_LOW", func, s);
		break;
	case NVME_CR_CAP_HI:
		DPRINTF("%s %s NVME_CR_CAP_HI", func, s);
		break;
	case NVME_CR_VS:
		DPRINTF("%s %s NVME_CR_VS", func, s);
		break;
	case NVME_CR_INTMS:
		DPRINTF("%s %s NVME_CR_INTMS", func, s);
		break;
	case NVME_CR_INTMC:
		DPRINTF("%s %s NVME_CR_INTMC", func, s);
		break;
	case NVME_CR_CC:
		DPRINTF("%s %s NVME_CR_CC", func, s);
		break;
	case NVME_CR_CSTS:
		DPRINTF("%s %s NVME_CR_CSTS", func, s);
		break;
	case NVME_CR_NSSR:
		DPRINTF("%s %s NVME_CR_NSSR", func, s);
		break;
	case NVME_CR_AQA:
		DPRINTF("%s %s NVME_CR_AQA", func, s);
		break;
	case NVME_CR_ASQ_LOW:
		DPRINTF("%s %s NVME_CR_ASQ_LOW", func, s);
		break;
	case NVME_CR_ASQ_HI:
		DPRINTF("%s %s NVME_CR_ASQ_HI", func, s);
		break;
	case NVME_CR_ACQ_LOW:
		DPRINTF("%s %s NVME_CR_ACQ_LOW", func, s);
		break;
	case NVME_CR_ACQ_HI:
		DPRINTF("%s %s NVME_CR_ACQ_HI", func, s);
		break;
	default:
		DPRINTF("unknown nvme bar-0 offset 0x%lx", offset);
	}

}

static void
pci_nvme_write_bar_0(struct vmctx *ctx, struct pci_nvme_softc* sc,
	uint64_t offset, int size, uint64_t value)
{
	uint32_t ccreg;

	if (offset >= NVME_DOORBELL_OFFSET) {
		uint64_t belloffset = offset - NVME_DOORBELL_OFFSET;
		uint64_t idx = belloffset / 8; /* door bell size = 2*int */
		int is_sq = (belloffset % 8) < 4;

		if (belloffset > ((sc->max_queues+1) * 8 - 4)) {
			WPRINTF("guest attempted an overflow write offset "
			         "0x%lx, val 0x%lx in %s",
			         offset, value, __func__);
			return;
		}

		pci_nvme_handle_doorbell(ctx, sc, idx, is_sq, value);
		return;
	}

	DPRINTF("nvme-write offset 0x%lx, size %d, value 0x%lx",
	        offset, size, value);

	if (size != 4) {
		WPRINTF("guest wrote invalid size %d (offset 0x%lx, "
		         "val 0x%lx) to bar0 in %s",
		         size, offset, value, __func__);
		/* TODO: shutdown device */
		return;
	}

	pci_nvme_bar0_reg_dumps(__func__, offset, 1);

	pthread_mutex_lock(&sc->mtx);

	switch (offset) {
	case NVME_CR_CAP_LOW:
	case NVME_CR_CAP_HI:
		/* readonly */
		break;
	case NVME_CR_VS:
		/* readonly */
		break;
	case NVME_CR_INTMS:
		/* MSI-X, so ignore */
		break;
	case NVME_CR_INTMC:
		/* MSI-X, so ignore */
		break;
	case NVME_CR_CC:
		ccreg = (uint32_t)value;

		DPRINTF("%s NVME_CR_CC en %x css %x shn %x iosqes %u "
		         "iocqes %u",
		        __func__,
			 NVME_CC_GET_EN(ccreg), NVME_CC_GET_CSS(ccreg),
			 NVME_CC_GET_SHN(ccreg), NVME_CC_GET_IOSQES(ccreg),
			 NVME_CC_GET_IOCQES(ccreg));

		if (NVME_CC_GET_SHN(ccreg)) {
			/* perform shutdown - flush out data to backend */
			sc->regs.csts &= ~(NVME_CSTS_REG_SHST_MASK <<
			    NVME_CSTS_REG_SHST_SHIFT);
			sc->regs.csts |= NVME_SHST_COMPLETE <<
			    NVME_CSTS_REG_SHST_SHIFT;
		}
		if (NVME_CC_GET_EN(ccreg) != NVME_CC_GET_EN(sc->regs.cc)) {
			if (NVME_CC_GET_EN(ccreg) == 0)
				/* transition 1-> causes controller reset */
				pci_nvme_reset_locked(sc);
			else
				pci_nvme_init_controller(ctx, sc);
		}

		/* Insert the iocqes, iosqes and en bits from the write */
		sc->regs.cc &= ~NVME_CC_WRITE_MASK;
		sc->regs.cc |= ccreg & NVME_CC_WRITE_MASK;
		if (NVME_CC_GET_EN(ccreg) == 0) {
			/* Insert the ams, mps and css bit fields */
			sc->regs.cc &= ~NVME_CC_NEN_WRITE_MASK;
			sc->regs.cc |= ccreg & NVME_CC_NEN_WRITE_MASK;
			sc->regs.csts &= ~NVME_CSTS_RDY;
		} else if (sc->pending_ios == 0) {
			sc->regs.csts |= NVME_CSTS_RDY;
		}
		break;
	case NVME_CR_CSTS:
		break;
	case NVME_CR_NSSR:
		/* ignore writes; don't support subsystem reset */
		break;
	case NVME_CR_AQA:
		sc->regs.aqa = (uint32_t)value;
		break;
	case NVME_CR_ASQ_LOW:
		sc->regs.asq = (sc->regs.asq & (0xFFFFFFFF00000000)) |
		               (0xFFFFF000 & value);
		break;
	case NVME_CR_ASQ_HI:
		sc->regs.asq = (sc->regs.asq & (0x00000000FFFFFFFF)) |
		               (value << 32);
		break;
	case NVME_CR_ACQ_LOW:
		sc->regs.acq = (sc->regs.acq & (0xFFFFFFFF00000000)) |
		               (0xFFFFF000 & value);
		break;
	case NVME_CR_ACQ_HI:
		sc->regs.acq = (sc->regs.acq & (0x00000000FFFFFFFF)) |
		               (value << 32);
		break;
	default:
		DPRINTF("%s unknown offset 0x%lx, value 0x%lx size %d",
		         __func__, offset, value, size);
	}
	pthread_mutex_unlock(&sc->mtx);
}

static void
pci_nvme_write(struct vmctx *ctx, int vcpu, struct pci_devinst *pi,
                int baridx, uint64_t offset, int size, uint64_t value)
{
	struct pci_nvme_softc* sc = pi->pi_arg;

	if (baridx == pci_msix_table_bar(pi) ||
	    baridx == pci_msix_pba_bar(pi)) {
		DPRINTF("nvme-write baridx %d, msix: off 0x%lx, size %d, "
		         " value 0x%lx", baridx, offset, size, value);

		pci_emul_msix_twrite(pi, offset, size, value);
		return;
	}

	switch (baridx) {
	case 0:
		pci_nvme_write_bar_0(ctx, sc, offset, size, value);
		break;

	default:
		DPRINTF("%s unknown baridx %d, val 0x%lx",
		         __func__, baridx, value);
	}
}

static uint64_t pci_nvme_read_bar_0(struct pci_nvme_softc* sc,
	uint64_t offset, int size)
{
	uint64_t value;

	pci_nvme_bar0_reg_dumps(__func__, offset, 0);

	if (offset < NVME_DOORBELL_OFFSET) {
		void *p = &(sc->regs);
		pthread_mutex_lock(&sc->mtx);
		memcpy(&value, (void *)((uintptr_t)p + offset), size);
		pthread_mutex_unlock(&sc->mtx);
	} else {
		value = 0;
                WPRINTF("pci_nvme: read invalid offset %ld", offset);
	}

	switch (size) {
	case 1:
		value &= 0xFF;
		break;
	case 2:
		value &= 0xFFFF;
		break;
	case 4:
		value &= 0xFFFFFFFF;
		break;
	}

	DPRINTF("   nvme-read offset 0x%lx, size %d -> value 0x%x",
	         offset, size, (uint32_t)value);

	return (value);
}



static uint64_t
pci_nvme_read(struct vmctx *ctx, int vcpu, struct pci_devinst *pi, int baridx,
    uint64_t offset, int size)
{
	struct pci_nvme_softc* sc = pi->pi_arg;

	if (baridx == pci_msix_table_bar(pi) ||
	    baridx == pci_msix_pba_bar(pi)) {
		DPRINTF("nvme-read bar: %d, msix: regoff 0x%lx, size %d",
		        baridx, offset, size);

		return pci_emul_msix_tread(pi, offset, size);
	}

	switch (baridx) {
	case 0:
       		return pci_nvme_read_bar_0(sc, offset, size);

	default:
		DPRINTF("unknown bar %d, 0x%lx", baridx, offset);
	}

	return (0);
}

static int
pci_nvme_parse_config(struct pci_nvme_softc *sc, nvlist_t *nvl)
{
	char bident[sizeof("XX:X:X")];
	const char *value;
	uint32_t sectsz;

	sc->max_queues = NVME_QUEUES;
	sc->max_qentries = NVME_MAX_QENTRIES;
	sc->ioslots = NVME_IOSLOTS;
	sc->num_squeues = sc->max_queues;
	sc->num_cqueues = sc->max_queues;
	sc->dataset_management = NVME_DATASET_MANAGEMENT_AUTO;
	sectsz = 0;
	snprintf(sc->ctrldata.sn, sizeof(sc->ctrldata.sn),
	         "NVME-%d-%d", sc->nsc_pi->pi_slot, sc->nsc_pi->pi_func);

	value = get_config_value_node(nvl, "maxq");
	if (value != NULL)
		sc->max_queues = atoi(value);
	value = get_config_value_node(nvl, "qsz");
	if (value != NULL) {
		sc->max_qentries = atoi(value);
		if (sc->max_qentries <= 0) {
			EPRINTLN("nvme: Invalid qsz option %d",
			    sc->max_qentries);
			return (-1);
		}
	}
	value = get_config_value_node(nvl, "ioslots");
	if (value != NULL) {
		sc->ioslots = atoi(value);
		if (sc->ioslots <= 0) {
			EPRINTLN("Invalid ioslots option %d", sc->ioslots);
			return (-1);
		}
	}
	value = get_config_value_node(nvl, "sectsz");
	if (value != NULL)
		sectsz = atoi(value);
	value = get_config_value_node(nvl, "ser");
	if (value != NULL) {
		/*
		 * This field indicates the Product Serial Number in
		 * 7-bit ASCII, unused bytes should be space characters.
		 * Ref: NVMe v1.3c.
		 */
		cpywithpad((char *)sc->ctrldata.sn,
		    sizeof(sc->ctrldata.sn), value, ' ');
	}
	value = get_config_value_node(nvl, "eui64");
	if (value != NULL)
		sc->nvstore.eui64 = htobe64(strtoull(value, NULL, 0));
	value = get_config_value_node(nvl, "dsm");
	if (value != NULL) {
		if (strcmp(value, "auto") == 0)
			sc->dataset_management = NVME_DATASET_MANAGEMENT_AUTO;
		else if (strcmp(value, "enable") == 0)
			sc->dataset_management = NVME_DATASET_MANAGEMENT_ENABLE;
		else if (strcmp(value, "disable") == 0)
			sc->dataset_management = NVME_DATASET_MANAGEMENT_DISABLE;
	}

	value = get_config_value_node(nvl, "ram");
	if (value != NULL) {
		uint64_t sz = strtoull(value, NULL, 10);

		sc->nvstore.type = NVME_STOR_RAM;
		sc->nvstore.size = sz * 1024 * 1024;
		sc->nvstore.ctx = calloc(1, sc->nvstore.size);
		sc->nvstore.sectsz = 4096;
		sc->nvstore.sectsz_bits = 12;
		if (sc->nvstore.ctx == NULL) {
			EPRINTLN("nvme: Unable to allocate RAM");
			return (-1);
		}
	} else {
		snprintf(bident, sizeof(bident), "%d:%d",
		    sc->nsc_pi->pi_slot, sc->nsc_pi->pi_func);
		sc->nvstore.ctx = blockif_open(nvl, bident);
		if (sc->nvstore.ctx == NULL) {
			EPRINTLN("nvme: Could not open backing file: %s",
			    strerror(errno));
			return (-1);
		}
		sc->nvstore.type = NVME_STOR_BLOCKIF;
		sc->nvstore.size = blockif_size(sc->nvstore.ctx);
	}

	if (sectsz == 512 || sectsz == 4096 || sectsz == 8192)
		sc->nvstore.sectsz = sectsz;
	else if (sc->nvstore.type != NVME_STOR_RAM)
		sc->nvstore.sectsz = blockif_sectsz(sc->nvstore.ctx);
	for (sc->nvstore.sectsz_bits = 9;
	     (1 << sc->nvstore.sectsz_bits) < sc->nvstore.sectsz;
	     sc->nvstore.sectsz_bits++);

	if (sc->max_queues <= 0 || sc->max_queues > NVME_QUEUES)
		sc->max_queues = NVME_QUEUES;

	return (0);
}

static void
pci_nvme_resized(struct blockif_ctxt *bctxt, void *arg, size_t new_size)
{
	struct pci_nvme_softc *sc;
	struct pci_nvme_blockstore *nvstore;
	struct nvme_namespace_data *nd;

	sc = arg;
	nvstore = &sc->nvstore;
	nd = &sc->nsdata;

	nvstore->size = new_size;
	pci_nvme_init_nsdata_size(nvstore, nd);

	/* Add changed NSID to list */
	sc->ns_log.ns[0] = 1;
	sc->ns_log.ns[1] = 0;

	pci_nvme_aen_post(sc, PCI_NVME_AE_TYPE_NOTICE,
	    PCI_NVME_AE_INFO_NS_ATTR_CHANGED);
}

static int
pci_nvme_init(struct vmctx *ctx, struct pci_devinst *pi, nvlist_t *nvl)
{
	struct pci_nvme_softc *sc;
	uint32_t pci_membar_sz;
	int	error;

	error = 0;

	sc = calloc(1, sizeof(struct pci_nvme_softc));
	pi->pi_arg = sc;
	sc->nsc_pi = pi;

	error = pci_nvme_parse_config(sc, nvl);
	if (error < 0)
		goto done;
	else
		error = 0;

	STAILQ_INIT(&sc->ioreqs_free);
	sc->ioreqs = calloc(sc->ioslots, sizeof(struct pci_nvme_ioreq));
	for (int i = 0; i < sc->ioslots; i++) {
		STAILQ_INSERT_TAIL(&sc->ioreqs_free, &sc->ioreqs[i], link);
	}

	pci_set_cfgdata16(pi, PCIR_DEVICE, 0x0A0A);
	pci_set_cfgdata16(pi, PCIR_VENDOR, 0xFB5D);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_STORAGE);
	pci_set_cfgdata8(pi, PCIR_SUBCLASS, PCIS_STORAGE_NVM);
	pci_set_cfgdata8(pi, PCIR_PROGIF,
	                 PCIP_STORAGE_NVM_ENTERPRISE_NVMHCI_1_0);

	/*
	 * Allocate size of NVMe registers + doorbell space for all queues.
	 *
	 * The specification requires a minimum memory I/O window size of 16K.
	 * The Windows driver will refuse to start a device with a smaller
	 * window.
	 */
	pci_membar_sz = sizeof(struct nvme_registers) +
	    2 * sizeof(uint32_t) * (sc->max_queues + 1);
	pci_membar_sz = MAX(pci_membar_sz, NVME_MMIO_SPACE_MIN);

	DPRINTF("nvme membar size: %u", pci_membar_sz);

	error = pci_emul_alloc_bar(pi, 0, PCIBAR_MEM64, pci_membar_sz);
	if (error) {
		WPRINTF("%s pci alloc mem bar failed", __func__);
		goto done;
	}

	error = pci_emul_add_msixcap(pi, sc->max_queues + 1, NVME_MSIX_BAR);
	if (error) {
		WPRINTF("%s pci add msixcap failed", __func__);
		goto done;
	}

	error = pci_emul_add_pciecap(pi, PCIEM_TYPE_ROOT_INT_EP);
	if (error) {
		WPRINTF("%s pci add Express capability failed", __func__);
		goto done;
	}

	pthread_mutex_init(&sc->mtx, NULL);
	sem_init(&sc->iosemlock, 0, sc->ioslots);
	blockif_register_resize_callback(sc->nvstore.ctx, pci_nvme_resized, sc);

	pci_nvme_init_queues(sc, sc->max_queues, sc->max_queues);
	/*
	 * Controller data depends on Namespace data so initialize Namespace
	 * data first.
	 */
	pci_nvme_init_nsdata(sc, &sc->nsdata, 1, &sc->nvstore);
	pci_nvme_init_ctrldata(sc);
	pci_nvme_init_logpages(sc);
	pci_nvme_init_features(sc);

	pci_nvme_aer_init(sc);
	pci_nvme_aen_init(sc);

	pci_nvme_reset(sc);

	pci_lintr_request(pi);

done:
	return (error);
}

static int
pci_nvme_legacy_config(nvlist_t *nvl, const char *opts)
{
	char *cp, *ram;

	if (opts == NULL)
		return (0);

	if (strncmp(opts, "ram=", 4) == 0) {
		cp = strchr(opts, ',');
		if (cp == NULL) {
			set_config_value_node(nvl, "ram", opts + 4);
			return (0);
		}
		ram = strndup(opts + 4, cp - opts - 4);
		set_config_value_node(nvl, "ram", ram);
		free(ram);
		return (pci_parse_legacy_config(nvl, cp + 1));
	} else
		return (blockif_legacy_config(nvl, opts));
}

struct pci_devemu pci_de_nvme = {
	.pe_emu =	"nvme",
	.pe_init =	pci_nvme_init,
	.pe_legacy_config = pci_nvme_legacy_config,
	.pe_barwrite =	pci_nvme_write,
	.pe_barread =	pci_nvme_read
};
PCI_EMUL_SET(pci_de_nvme);
