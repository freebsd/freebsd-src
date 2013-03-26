/*-
 * Copyright (C) 2012 Intel Corporation
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

#ifndef __NVME_H__
#define __NVME_H__

#ifdef _KERNEL
#include <sys/types.h>
#endif

#define	NVME_IDENTIFY_CONTROLLER	_IOR('n', 0, struct nvme_controller_data)
#define	NVME_IDENTIFY_NAMESPACE		_IOR('n', 1, struct nvme_namespace_data)
#define	NVME_IO_TEST			_IOWR('n', 2, struct nvme_io_test)
#define	NVME_BIO_TEST			_IOWR('n', 4, struct nvme_io_test)

/*
 * Use to mark a command to apply to all namespaces, or to retrieve global
 *  log pages.
 */
#define NVME_GLOBAL_NAMESPACE_TAG	((uint32_t)0xFFFFFFFF)

union cap_lo_register {
	uint32_t	raw;
	struct {
		/** maximum queue entries supported */
		uint32_t mqes		: 16;

		/** contiguous queues required */
		uint32_t cqr		: 1;

		/** arbitration mechanism supported */
		uint32_t ams		: 2;

		uint32_t reserved1	: 5;

		/** timeout */
		uint32_t to		: 8;
	} bits __packed;
} __packed;

union cap_hi_register {
	uint32_t	raw;
	struct {
		/** doorbell stride */
		uint32_t dstrd		: 4;

		uint32_t reserved3	: 1;

		/** command sets supported */
		uint32_t css_nvm	: 1;

		uint32_t css_reserved	: 3;
		uint32_t reserved2	: 7;

		/** memory page size minimum */
		uint32_t mpsmin		: 4;

		/** memory page size maximum */
		uint32_t mpsmax		: 4;

		uint32_t reserved1	: 8;
	} bits __packed;
} __packed;

union cc_register {
	uint32_t	raw;
	struct {
		/** enable */
		uint32_t en		: 1;

		uint32_t reserved1	: 3;

		/** i/o command set selected */
		uint32_t css		: 3;

		/** memory page size */
		uint32_t mps		: 4;

		/** arbitration mechanism selected */
		uint32_t ams		: 3;

		/** shutdown notification */
		uint32_t shn		: 2;

		/** i/o submission queue entry size */
		uint32_t iosqes		: 4;

		/** i/o completion queue entry size */
		uint32_t iocqes		: 4;

		uint32_t reserved2	: 8;
	} bits __packed;
} __packed;

enum shn_value {
	NVME_SHN_NORMAL		= 0x1,
	NVME_SHN_ABRUPT		= 0x2,
};

union csts_register {
	uint32_t	raw;
	struct {
		/** ready */
		uint32_t rdy		: 1;

		/** controller fatal status */
		uint32_t cfs		: 1;

		/** shutdown status */
		uint32_t shst		: 2;

		uint32_t reserved1	: 28;
	} bits __packed;
} __packed;

enum shst_value {
	NVME_SHST_NORMAL	= 0x0,
	NVME_SHST_OCCURRING	= 0x1,
	NVME_SHST_COMPLETE	= 0x2,
};

union aqa_register {
	uint32_t	raw;
	struct {
		/** admin submission queue size */
		uint32_t asqs		: 12;

		uint32_t reserved1	: 4;

		/** admin completion queue size */
		uint32_t acqs		: 12;

		uint32_t reserved2	: 4;
	} bits __packed;
} __packed;

struct nvme_registers
{
	/** controller capabilities */
	union cap_lo_register	cap_lo;
	union cap_hi_register	cap_hi;

	uint32_t	vs;		/* version */
	uint32_t	intms;		/* interrupt mask set */
	uint32_t	intmc;		/* interrupt mask clear */

	/** controller configuration */
	union cc_register	cc;

	uint32_t	reserved1;
	uint32_t	csts;		/* controller status */
	uint32_t	reserved2;

	/** admin queue attributes */
	union aqa_register	aqa;

	uint64_t	asq;		/* admin submission queue base addr */
	uint64_t	acq;		/* admin completion queue base addr */
	uint32_t	reserved3[0x3f2];

	struct {
	    uint32_t	sq_tdbl;	/* submission queue tail doorbell */
	    uint32_t	cq_hdbl;	/* completion queue head doorbell */
	} doorbell[1] __packed;
} __packed;

struct nvme_command
{
	/* dword 0 */
	uint16_t opc	:  8;	/* opcode */
	uint16_t fuse	:  2;	/* fused operation */
	uint16_t rsvd1	:  6;
	uint16_t cid;		/* command identifier */

	/* dword 1 */
	uint32_t nsid;		/* namespace identifier */

	/* dword 2-3 */
	uint32_t rsvd2;
	uint32_t rsvd3;

	/* dword 4-5 */
	uint64_t mptr;		/* metadata pointer */

	/* dword 6-7 */
	uint64_t prp1;		/* prp entry 1 */

	/* dword 8-9 */
	uint64_t prp2;		/* prp entry 2 */

	/* dword 10-15 */
	uint32_t cdw10;		/* command-specific */
	uint32_t cdw11;		/* command-specific */
	uint32_t cdw12;		/* command-specific */
	uint32_t cdw13;		/* command-specific */
	uint32_t cdw14;		/* command-specific */
	uint32_t cdw15;		/* command-specific */
} __packed;

struct nvme_completion {

	/* dword 0 */
	uint32_t cdw0;		/* command-specific */

	/* dword 1 */
	uint32_t rsvd1;

	/* dword 2 */
	uint16_t sqhd;		/* submission queue head pointer */
	uint16_t sqid;		/* submission queue identifier */

	/* dword 3 */
	uint16_t cid;		/* command identifier */
	uint16_t p	:  1;	/* phase tag */
	uint16_t sf_sc	:  8;	/* status field - status code */
	uint16_t sf_sct	:  3;	/* status field - status code type */
	uint16_t rsvd2	:  2;
	uint16_t sf_m	:  1;	/* status field - more */
	uint16_t sf_dnr	:  1;	/* status field - do not retry */
} __packed;

struct nvme_dsm_range {

	uint32_t attributes;
	uint32_t length;
	uint64_t starting_lba;
} __packed;

/* status code types */
enum nvme_status_code_type {
	NVME_SCT_GENERIC		= 0x0,
	NVME_SCT_COMMAND_SPECIFIC	= 0x1,
	NVME_SCT_MEDIA_ERROR		= 0x2,
	/* 0x3-0x6 - reserved */
	NVME_SCT_VENDOR_SPECIFIC	= 0x7,
};

/* generic command status codes */
enum nvme_generic_command_status_code {
	NVME_SC_SUCCESS				= 0x00,
	NVME_SC_INVALID_OPCODE			= 0x01,
	NVME_SC_INVALID_FIELD			= 0x02,
	NVME_SC_COMMAND_ID_CONFLICT		= 0x03,
	NVME_SC_DATA_TRANSFER_ERROR		= 0x04,
	NVME_SC_ABORTED_POWER_LOSS		= 0x05,
	NVME_SC_INTERNAL_DEVICE_ERROR		= 0x06,
	NVME_SC_ABORTED_BY_REQUEST		= 0x07,
	NVME_SC_ABORTED_SQ_DELETION		= 0x08,
	NVME_SC_ABORTED_FAILED_FUSED		= 0x09,
	NVME_SC_ABORTED_MISSING_FUSED		= 0x0a,
	NVME_SC_INVALID_NAMESPACE_OR_FORMAT	= 0x0b,
	NVME_SC_COMMAND_SEQUENCE_ERROR		= 0x0c,

	NVME_SC_LBA_OUT_OF_RANGE		= 0x80,
	NVME_SC_CAPACITY_EXCEEDED		= 0x81,
	NVME_SC_NAMESPACE_NOT_READY		= 0x82,
};

/* command specific status codes */
enum nvme_command_specific_status_code {
	NVME_SC_COMPLETION_QUEUE_INVALID	= 0x00,
	NVME_SC_INVALID_QUEUE_IDENTIFIER	= 0x01,
	NVME_SC_MAXIMUM_QUEUE_SIZE_EXCEEDED	= 0x02,
	NVME_SC_ABORT_COMMAND_LIMIT_EXCEEDED	= 0x03,
	/* 0x04 - reserved */
	NVME_SC_ASYNC_EVENT_REQUEST_LIMIT_EXCEEDED = 0x05,
	NVME_SC_INVALID_FIRMWARE_SLOT		= 0x06,
	NVME_SC_INVALID_FIRMWARE_IMAGE		= 0x07,
	NVME_SC_INVALID_INTERRUPT_VECTOR	= 0x08,
	NVME_SC_INVALID_LOG_PAGE		= 0x09,
	NVME_SC_INVALID_FORMAT			= 0x0a,
	NVME_SC_FIRMWARE_REQUIRES_RESET		= 0x0b,

	NVME_SC_CONFLICTING_ATTRIBUTES		= 0x80,
	NVME_SC_INVALID_PROTECTION_INFO		= 0x81,
	NVME_SC_ATTEMPTED_WRITE_TO_RO_PAGE	= 0x82,
};

/* media error status codes */
enum nvme_media_error_status_code {
	NVME_SC_WRITE_FAULTS			= 0x80,
	NVME_SC_UNRECOVERED_READ_ERROR		= 0x81,
	NVME_SC_GUARD_CHECK_ERROR		= 0x82,
	NVME_SC_APPLICATION_TAG_CHECK_ERROR	= 0x83,
	NVME_SC_REFERENCE_TAG_CHECK_ERROR	= 0x84,
	NVME_SC_COMPARE_FAILURE			= 0x85,
	NVME_SC_ACCESS_DENIED			= 0x86,
};

/* admin opcodes */
enum nvme_admin_opcode {
	NVME_OPC_DELETE_IO_SQ			= 0x00,
	NVME_OPC_CREATE_IO_SQ			= 0x01,
	NVME_OPC_GET_LOG_PAGE			= 0x02,
	/* 0x03 - reserved */
	NVME_OPC_DELETE_IO_CQ			= 0x04,
	NVME_OPC_CREATE_IO_CQ			= 0x05,
	NVME_OPC_IDENTIFY			= 0x06,
	/* 0x07 - reserved */
	NVME_OPC_ABORT				= 0x08,
	NVME_OPC_SET_FEATURES			= 0x09,
	NVME_OPC_GET_FEATURES			= 0x0a,
	/* 0x0b - reserved */
	NVME_OPC_ASYNC_EVENT_REQUEST		= 0x0c,
	/* 0x0d-0x0f - reserved */
	NVME_OPC_FIRMWARE_ACTIVATE		= 0x10,
	NVME_OPC_FIRMWARE_IMAGE_DOWNLOAD	= 0x11,

	NVME_OPC_FORMAT_NVM			= 0x80,
	NVME_OPC_SECURITY_SEND			= 0x81,
	NVME_OPC_SECURITY_RECEIVE		= 0x82,
};

/* nvme nvm opcodes */
enum nvme_nvm_opcode {
	NVME_OPC_FLUSH				= 0x00,
	NVME_OPC_WRITE				= 0x01,
	NVME_OPC_READ				= 0x02,
	/* 0x03 - reserved */
	NVME_OPC_WRITE_UNCORRECTABLE		= 0x04,
	NVME_OPC_COMPARE			= 0x05,
	/* 0x06-0x07 - reserved */
	NVME_OPC_DATASET_MANAGEMENT		= 0x09,
};

enum nvme_feature {
	/* 0x00 - reserved */
	NVME_FEAT_ARBITRATION			= 0x01,
	NVME_FEAT_POWER_MANAGEMENT		= 0x02,
	NVME_FEAT_LBA_RANGE_TYPE		= 0x03,
	NVME_FEAT_TEMPERATURE_THRESHOLD		= 0x04,
	NVME_FEAT_ERROR_RECOVERY		= 0x05,
	NVME_FEAT_VOLATILE_WRITE_CACHE		= 0x06,
	NVME_FEAT_NUMBER_OF_QUEUES		= 0x07,
	NVME_FEAT_INTERRUPT_COALESCING		= 0x08,
	NVME_FEAT_INTERRUPT_VECTOR_CONFIGURATION = 0x09,
	NVME_FEAT_WRITE_ATOMICITY		= 0x0A,
	NVME_FEAT_ASYNC_EVENT_CONFIGURATION	= 0x0B,
	/* 0x0C-0x7F - reserved */
	NVME_FEAT_SOFTWARE_PROGRESS_MARKER	= 0x80,
	/* 0x81-0xBF - command set specific (reserved) */
	/* 0xC0-0xFF - vendor specific */
};

enum nvme_dsm_attribute {
	NVME_DSM_ATTR_INTEGRAL_READ		= 0x1,
	NVME_DSM_ATTR_INTEGRAL_WRITE		= 0x2,
	NVME_DSM_ATTR_DEALLOCATE		= 0x4,
};

struct nvme_controller_data {

	/* bytes 0-255: controller capabilities and features */

	/** pci vendor id */
	uint16_t		vid;

	/** pci subsystem vendor id */
	uint16_t		ssvid;

	/** serial number */
	int8_t			sn[20];

	/** model number */
	int8_t			mn[40];

	/** firmware revision */
	uint8_t			fr[8];

	/** recommended arbitration burst */
	uint8_t			rab;

	/** ieee oui identifier */
	uint8_t			ieee[3];

	/** multi-interface capabilities */
	uint8_t			mic;

	/** maximum data transfer size */
	uint8_t			mdts;

	uint8_t			reserved1[178];

	/* bytes 256-511: admin command set attributes */

	/** optional admin command support */
	struct {
		/* supports security send/receive commands */
		uint16_t	security  : 1;

		/* supports format nvm command */
		uint16_t	format    : 1;

		/* supports firmware activate/download commands */
		uint16_t	firmware  : 1;

		uint16_t	oacs_rsvd : 13;
	} __packed oacs;

	/** abort command limit */
	uint8_t			acl;

	/** asynchronous event request limit */
	uint8_t			aerl;

	/** firmware updates */
	struct {
		/* first slot is read-only */
		uint8_t		slot1_ro  : 1;

		/* number of firmware slots */
		uint8_t		num_slots : 3;

		uint8_t		frmw_rsvd : 4;
	} __packed frmw;

	/** log page attributes */
	struct {
		/* per namespace smart/health log page */
		uint8_t		ns_smart : 1;

		uint8_t		lpa_rsvd : 7;
	} __packed lpa;

	/** error log page entries */
	uint8_t			elpe;

	/** number of power states supported */
	uint8_t			npss;

	/** admin vendor specific command configuration */
	struct {
		/* admin vendor specific commands use spec format */
		uint8_t		spec_format : 1;

		uint8_t		avscc_rsvd  : 7;
	} __packed avscc;

	uint8_t			reserved2[247];

	/* bytes 512-703: nvm command set attributes */

	/** submission queue entry size */
	struct {
		uint8_t		min : 4;
		uint8_t		max : 4;
	} __packed sqes;

	/** completion queue entry size */
	struct {
		uint8_t		min : 4;
		uint8_t		max : 4;
	} __packed cqes;

	uint8_t			reserved3[2];

	/** number of namespaces */
	uint32_t		nn;

	/** optional nvm command support */
	struct {
		uint16_t	compare : 1;
		uint16_t	write_unc : 1;
		uint16_t	dsm: 1;
		uint16_t	reserved: 13;
	} __packed oncs;

	/** fused operation support */
	uint16_t		fuses;

	/** format nvm attributes */
	uint8_t			fna;

	/** volatile write cache */
	struct {
		uint8_t		present : 1;
		uint8_t		reserved : 7;
	} __packed vwc;

	/* TODO: flesh out remaining nvm command set attributes */
	uint8_t			reserved4[178];

	/* bytes 704-2047: i/o command set attributes */
	uint8_t			reserved5[1344];

	/* bytes 2048-3071: power state descriptors */
	uint8_t			reserved6[1024];

	/* bytes 3072-4095: vendor specific */
	uint8_t			reserved7[1024];
} __packed __aligned(4);

struct nvme_namespace_data {

	/** namespace size */
	uint64_t		nsze;

	/** namespace capacity */
	uint64_t		ncap;

	/** namespace utilization */
	uint64_t		nuse;

	/** namespace features */
	struct {
		/** thin provisioning */
		uint8_t		thin_prov : 1;
		uint8_t		reserved1 : 7;
	} __packed nsfeat;

	/** number of lba formats */
	uint8_t			nlbaf;

	/** formatted lba size */
	struct {
		uint8_t		format    : 4;
		uint8_t		extended  : 1;
		uint8_t		reserved2 : 3;
	} __packed flbas;

	/** metadata capabilities */
	struct {
		/* metadata can be transferred as part of data prp list */
		uint8_t		extended  : 1;

		/* metadata can be transferred with separate metadata pointer */
		uint8_t		pointer   : 1;

		uint8_t		reserved3 : 6;
	} __packed mc;

	/** end-to-end data protection capabilities */
	struct {
		/* protection information type 1 */
		uint8_t		pit1     : 1;

		/* protection information type 2 */
		uint8_t		pit2     : 1;

		/* protection information type 3 */
		uint8_t		pit3     : 1;

		/* first eight bytes of metadata */
		uint8_t		md_start : 1;

		/* last eight bytes of metadata */
		uint8_t		md_end   : 1;
	} __packed dpc;

	/** end-to-end data protection type settings */
	struct {
		/* protection information type */
		uint8_t		pit       : 3;

		/* 1 == protection info transferred at start of metadata */
		/* 0 == protection info transferred at end of metadata */
		uint8_t		md_start  : 1;

		uint8_t		reserved4 : 4;
	} __packed dps;

	uint8_t			reserved5[98];

	/** lba format support */
	struct {
		/** metadata size */
		uint32_t	ms	  : 16;

		/** lba data size */
		uint32_t	lbads	  : 8;

		/** relative performance */
		uint32_t	rp	  : 2;

		uint32_t	reserved6 : 6;
	} __packed lbaf[16];

	uint8_t			reserved6[192];

	uint8_t			vendor_specific[3712];
} __packed __aligned(4);

enum nvme_log_page {

	/* 0x00 - reserved */
	NVME_LOG_ERROR			= 0x01,
	NVME_LOG_HEALTH_INFORMATION	= 0x02,
	NVME_LOG_FIRMWARE_SLOT		= 0x03,
	/* 0x04-0x7F - reserved */
	/* 0x80-0xBF - I/O command set specific */
	/* 0xC0-0xFF - vendor specific */
};

union nvme_critical_warning_state {

	uint8_t		raw;

	struct {
		uint8_t	available_spare		: 1;
		uint8_t	temperature		: 1;
		uint8_t	device_reliability	: 1;
		uint8_t	read_only		: 1;
		uint8_t	volatile_memory_backup	: 1;
		uint8_t	reserved		: 3;
	} __packed bits;
} __packed;

struct nvme_health_information_page {

	union nvme_critical_warning_state	critical_warning;

	uint16_t		temperature;
	uint8_t			available_spare;
	uint8_t			available_spare_threshold;
	uint8_t			percentage_used;

	uint8_t			reserved[26];

	/*
	 * Note that the following are 128-bit values, but are
	 *  defined as an array of 2 64-bit values.
	 */
	/* Data Units Read is always in 512-byte units. */
	uint64_t		data_units_read[2];
	/* Data Units Written is always in 512-byte units. */
	uint64_t		data_units_written[2];
	/* For NVM command set, this includes Compare commands. */
	uint64_t		host_read_commands[2];
	uint64_t		host_write_commands[2];
	/* Controller Busy Time is reported in minutes. */
	uint64_t		controller_busy_time[2];
	uint64_t		power_cycles[2];
	uint64_t		power_on_hours[2];
	uint64_t		unsafe_shutdowns[2];
	uint64_t		media_errors[2];
	uint64_t		num_error_info_log_entries[2];

	uint8_t			reserved2[320];
} __packed __aligned(4);

#define NVME_TEST_MAX_THREADS	128

struct nvme_io_test {

	enum nvme_nvm_opcode	opc;
	uint32_t		size;
	uint32_t		time;	/* in seconds */
	uint32_t		num_threads;
	uint32_t		flags;
	uint32_t		io_completed[NVME_TEST_MAX_THREADS];
};

enum nvme_io_test_flags {

	/*
	 * Specifies whether dev_refthread/dev_relthread should be
	 *  called during NVME_BIO_TEST.  Ignored for other test
	 *  types.
	 */
	NVME_TEST_FLAG_REFTHREAD =	0x1,
};

#ifdef _KERNEL

struct bio;

struct nvme_namespace;
struct nvme_consumer;

typedef void (*nvme_cb_fn_t)(void *, const struct nvme_completion *);
typedef void (*nvme_consumer_cb_fn_t)(void *, struct nvme_namespace *);

enum nvme_namespace_flags {
	NVME_NS_DEALLOCATE_SUPPORTED	= 0x1,
	NVME_NS_FLUSH_SUPPORTED		= 0x2,
};

/* NVM I/O functions */
int	nvme_ns_cmd_write(struct nvme_namespace *ns, void *payload,
			  uint64_t lba, uint32_t lba_count, nvme_cb_fn_t cb_fn,
			  void *cb_arg);
int	nvme_ns_cmd_read(struct nvme_namespace *ns, void *payload,
			 uint64_t lba, uint32_t lba_count, nvme_cb_fn_t cb_fn,
			 void *cb_arg);
int	nvme_ns_cmd_deallocate(struct nvme_namespace *ns, void *payload,
			       uint8_t num_ranges, nvme_cb_fn_t cb_fn,
			       void *cb_arg);
int	nvme_ns_cmd_flush(struct nvme_namespace *ns, nvme_cb_fn_t cb_fn,
			  void *cb_arg);

/* Registration functions */
struct nvme_consumer *	nvme_register_consumer(nvme_consumer_cb_fn_t cb_fn,
					       void *cb_arg);
void		nvme_unregister_consumer(struct nvme_consumer *consumer);

/* Namespace helper functions */
uint32_t	nvme_ns_get_max_io_xfer_size(struct nvme_namespace *ns);
uint32_t	nvme_ns_get_sector_size(struct nvme_namespace *ns);
uint64_t	nvme_ns_get_num_sectors(struct nvme_namespace *ns);
uint64_t	nvme_ns_get_size(struct nvme_namespace *ns);
uint32_t	nvme_ns_get_flags(struct nvme_namespace *ns);
const char *	nvme_ns_get_serial_number(struct nvme_namespace *ns);
const char *	nvme_ns_get_model_number(struct nvme_namespace *ns);

int	nvme_ns_bio_process(struct nvme_namespace *ns, struct bio *bp,
			    nvme_cb_fn_t cb_fn);

#endif /* _KERNEL */

#endif /* __NVME_H__ */
