/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright (C) 2016 Intel Corporation.
 *   All rights reserved.
 */

#ifndef SPDK_NVMF_SPEC_H
#define SPDK_NVMF_SPEC_H

#include "spdk/stdinc.h"

#include "spdk/assert.h"
#include "spdk/nvme_spec.h"

/**
 * \file
 * NVMe over Fabrics specification definitions
 */

#pragma pack(push, 1)

struct spdk_nvmf_capsule_cmd {
	uint8_t		opcode;
	uint8_t		reserved1;
	uint16_t	cid;
	uint8_t		fctype;
	uint8_t		reserved2[35];
	uint8_t		fabric_specific[24];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_capsule_cmd) == 64, "Incorrect size");

/* Fabric Command Set */
#define SPDK_NVME_OPC_FABRIC 0x7f

enum spdk_nvmf_fabric_cmd_types {
	SPDK_NVMF_FABRIC_COMMAND_PROPERTY_SET			= 0x00,
	SPDK_NVMF_FABRIC_COMMAND_CONNECT			= 0x01,
	SPDK_NVMF_FABRIC_COMMAND_PROPERTY_GET			= 0x04,
	SPDK_NVMF_FABRIC_COMMAND_AUTHENTICATION_SEND		= 0x05,
	SPDK_NVMF_FABRIC_COMMAND_AUTHENTICATION_RECV		= 0x06,
	SPDK_NVMF_FABRIC_COMMAND_START_VENDOR_SPECIFIC		= 0xC0,
};

enum spdk_nvmf_fabric_cmd_status_code {
	SPDK_NVMF_FABRIC_SC_INCOMPATIBLE_FORMAT		= 0x80,
	SPDK_NVMF_FABRIC_SC_CONTROLLER_BUSY		= 0x81,
	SPDK_NVMF_FABRIC_SC_INVALID_PARAM		= 0x82,
	SPDK_NVMF_FABRIC_SC_RESTART_DISCOVERY		= 0x83,
	SPDK_NVMF_FABRIC_SC_INVALID_HOST		= 0x84,
	SPDK_NVMF_FABRIC_SC_LOG_RESTART_DISCOVERY	= 0x90,
	SPDK_NVMF_FABRIC_SC_AUTH_REQUIRED		= 0x91,
};

/**
 * RDMA Queue Pair service types
 */
enum spdk_nvmf_rdma_qptype {
	/** Reliable connected */
	SPDK_NVMF_RDMA_QPTYPE_RELIABLE_CONNECTED	= 0x1,

	/** Reliable datagram */
	SPDK_NVMF_RDMA_QPTYPE_RELIABLE_DATAGRAM		= 0x2,
};

/**
 * RDMA provider types
 */
enum spdk_nvmf_rdma_prtype {
	/** No provider specified */
	SPDK_NVMF_RDMA_PRTYPE_NONE	= 0x1,

	/** InfiniBand */
	SPDK_NVMF_RDMA_PRTYPE_IB	= 0x2,

	/** RoCE v1 */
	SPDK_NVMF_RDMA_PRTYPE_ROCE	= 0x3,

	/** RoCE v2 */
	SPDK_NVMF_RDMA_PRTYPE_ROCE2	= 0x4,

	/** iWARP */
	SPDK_NVMF_RDMA_PRTYPE_IWARP	= 0x5,
};

/**
 * RDMA connection management service types
 */
enum spdk_nvmf_rdma_cms {
	/** Sockets based endpoint addressing */
	SPDK_NVMF_RDMA_CMS_RDMA_CM	= 0x1,
};

/**
 * NVMe over Fabrics transport types
 */
enum spdk_nvmf_trtype {
	/** RDMA */
	SPDK_NVMF_TRTYPE_RDMA		= 0x1,

	/** Fibre Channel */
	SPDK_NVMF_TRTYPE_FC		= 0x2,

	/** TCP */
	SPDK_NVMF_TRTYPE_TCP		= 0x3,

	/** Intra-host transport (loopback) */
	SPDK_NVMF_TRTYPE_INTRA_HOST	= 0xfe,
};

/**
 * Address family types
 */
enum spdk_nvmf_adrfam {
	/** IPv4 (AF_INET) */
	SPDK_NVMF_ADRFAM_IPV4		= 0x1,

	/** IPv6 (AF_INET6) */
	SPDK_NVMF_ADRFAM_IPV6		= 0x2,

	/** InfiniBand (AF_IB) */
	SPDK_NVMF_ADRFAM_IB		= 0x3,

	/** Fibre Channel address family */
	SPDK_NVMF_ADRFAM_FC		= 0x4,

	/** Intra-host transport (loopback) */
	SPDK_NVMF_ADRFAM_INTRA_HOST	= 0xfe,
};

/**
 * NVM subsystem types
 */
enum spdk_nvmf_subtype {
	/** Referral to a discovery service */
	SPDK_NVMF_SUBTYPE_DISCOVERY		= 0x1,

	/** NVM Subsystem */
	SPDK_NVMF_SUBTYPE_NVME			= 0x2,

	/** Current Discovery Subsystem */
	SPDK_NVMF_SUBTYPE_DISCOVERY_CURRENT	= 0x3
};

/* Discovery Log Entry Flags - Duplicate Returned Information */
#define SPDK_NVMF_DISCOVERY_LOG_EFLAGS_DUPRETINFO (1u << 0u)

/* Discovery Log Entry Flags - Explicit Persistent Connection Support for Discovery */
#define SPDK_NVMF_DISCOVERY_LOG_EFLAGS_EPCSD (1u << 1u)

/**
 * Connections shall be made over a fabric secure channel
 */
enum spdk_nvmf_treq_secure_channel {
	/** Not specified */
	SPDK_NVMF_TREQ_SECURE_CHANNEL_NOT_SPECIFIED	= 0x0,

	/** Required */
	SPDK_NVMF_TREQ_SECURE_CHANNEL_REQUIRED		= 0x1,

	/** Not required */
	SPDK_NVMF_TREQ_SECURE_CHANNEL_NOT_REQUIRED	= 0x2,
};

struct spdk_nvmf_fabric_auth_recv_cmd {
	uint8_t		opcode;
	uint8_t		reserved1;
	uint16_t	cid;
	uint8_t		fctype; /* NVMF_FABRIC_COMMAND_AUTHENTICATION_RECV (0x06) */
	uint8_t		reserved2[19];
	struct spdk_nvme_sgl_descriptor sgl1;
	uint8_t		reserved3;
	uint8_t		spsp0;
	uint8_t		spsp1;
	uint8_t		secp;
	uint32_t	al;
	uint8_t		reserved4[16];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_fabric_auth_recv_cmd) == 64, "Incorrect size");

struct spdk_nvmf_fabric_auth_send_cmd {
	uint8_t		opcode;
	uint8_t		reserved1;
	uint16_t	cid;
	uint8_t		fctype; /* NVMF_FABRIC_COMMAND_AUTHENTICATION_SEND (0x05) */
	uint8_t		reserved2[19];
	struct spdk_nvme_sgl_descriptor sgl1;
	uint8_t		reserved3;
	uint8_t		spsp0;
	uint8_t		spsp1;
	uint8_t		secp;
	uint32_t	tl;
	uint8_t		reserved4[16];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_fabric_auth_send_cmd) == 64, "Incorrect size");

struct spdk_nvmf_fabric_connect_data {
	uint8_t		hostid[16];
	uint16_t	cntlid;
	uint8_t		reserved5[238];
	uint8_t		subnqn[SPDK_NVME_NQN_FIELD_SIZE];
	uint8_t		hostnqn[SPDK_NVME_NQN_FIELD_SIZE];
	uint8_t		reserved6[256];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_fabric_connect_data) == 1024, "Incorrect size");

struct spdk_nvmf_fabric_connect_cmd {
	uint8_t		opcode;
	uint8_t		reserved1;
	uint16_t	cid;
	uint8_t		fctype;
	uint8_t		reserved2[19];
	struct spdk_nvme_sgl_descriptor sgl1;
	uint16_t	recfmt; /* Connect Record Format */
	uint16_t	qid; /* Queue Identifier */
	uint16_t	sqsize; /* Submission Queue Size */
	uint8_t		cattr; /* queue attributes */
	uint8_t		reserved3;
	uint32_t	kato; /* keep alive timeout */
	uint8_t		reserved4[12];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_fabric_connect_cmd) == 64, "Incorrect size");

struct spdk_nvmf_fabric_connect_rsp {
	union {
		struct {
			uint16_t cntlid;
			uint16_t authreq;
		} success;

		struct {
			uint16_t	ipo;
			uint8_t		iattr;
			uint8_t		reserved;
		} invalid;

		uint32_t raw;
	} status_code_specific;

	uint32_t	reserved0;
	uint16_t	sqhd;
	uint16_t	reserved1;
	uint16_t	cid;
	struct spdk_nvme_status status;
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_fabric_connect_rsp) == 16, "Incorrect size");

#define SPDK_NVMF_PROP_SIZE_4	0
#define SPDK_NVMF_PROP_SIZE_8	1

struct spdk_nvmf_fabric_prop_get_cmd {
	uint8_t		opcode;
	uint8_t		reserved1;
	uint16_t	cid;
	uint8_t		fctype;
	uint8_t		reserved2[35];
	struct {
		uint8_t size		: 3;
		uint8_t reserved	: 5;
	} attrib;
	uint8_t		reserved3[3];
	uint32_t	ofst;
	uint8_t		reserved4[16];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_fabric_prop_get_cmd) == 64, "Incorrect size");

struct spdk_nvmf_fabric_prop_get_rsp {
	union {
		uint64_t u64;
		struct {
			uint32_t low;
			uint32_t high;
		} u32;
	} value;

	uint16_t	sqhd;
	uint16_t	reserved0;
	uint16_t	cid;
	struct spdk_nvme_status status;
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_fabric_prop_get_rsp) == 16, "Incorrect size");

struct spdk_nvmf_fabric_prop_set_cmd {
	uint8_t		opcode;
	uint8_t		reserved0;
	uint16_t	cid;
	uint8_t		fctype;
	uint8_t		reserved1[35];
	struct {
		uint8_t size		: 3;
		uint8_t reserved	: 5;
	} attrib;
	uint8_t		reserved2[3];
	uint32_t	ofst;

	union {
		uint64_t u64;
		struct {
			uint32_t low;
			uint32_t high;
		} u32;
	} value;

	uint8_t		reserved4[8];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_fabric_prop_set_cmd) == 64, "Incorrect size");

#define SPDK_NVMF_NQN_MIN_LEN 11 /* The prefix in the spec is 11 characters */
#define SPDK_NVMF_NQN_MAX_LEN 223
#define SPDK_NVMF_NQN_UUID_PRE_LEN 32
#define SPDK_NVMF_UUID_STRING_LEN 36
#define SPDK_NVMF_NQN_UUID_PRE "nqn.2014-08.org.nvmexpress:uuid:"
#define SPDK_NVMF_DISCOVERY_NQN "nqn.2014-08.org.nvmexpress.discovery"

#define SPDK_DOMAIN_LABEL_MAX_LEN 63 /* RFC 1034 max domain label length */

#define SPDK_NVMF_TRSTRING_MAX_LEN 32
#define SPDK_NVMF_TRADDR_MAX_LEN 256
#define SPDK_NVMF_TRSVCID_MAX_LEN 32

/** RDMA transport-specific address subtype */
struct spdk_nvmf_rdma_transport_specific_address_subtype {
	/** RDMA QP service type (\ref spdk_nvmf_rdma_qptype) */
	uint8_t		rdma_qptype;

	/** RDMA provider type (\ref spdk_nvmf_rdma_prtype) */
	uint8_t		rdma_prtype;

	/** RDMA connection management service (\ref spdk_nvmf_rdma_cms) */
	uint8_t		rdma_cms;

	uint8_t		reserved0[5];

	/** RDMA partition key for AF_IB */
	uint16_t	rdma_pkey;

	uint8_t		reserved2[246];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_rdma_transport_specific_address_subtype) == 256,
		   "Incorrect size");

/** TCP Secure Socket Type */
enum spdk_nvme_tcp_secure_socket_type {
	/** No security */
	SPDK_NVME_TCP_SECURITY_NONE			= 0,

	/** TLS (Secure Sockets) version 1.2 */
	SPDK_NVME_TCP_SECURITY_TLS_1_2			= 1,

	/** TLS (Secure Sockets) version 1.3 */
	SPDK_NVME_TCP_SECURITY_TLS_1_3			= 2,
};

/** TCP transport-specific address subtype */
struct spdk_nvme_tcp_transport_specific_address_subtype {
	/** Security type (\ref spdk_nvme_tcp_secure_socket_type) */
	uint8_t		sectype;

	uint8_t		reserved0[255];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvme_tcp_transport_specific_address_subtype) == 256,
		   "Incorrect size");

/** Transport-specific address subtype */
union spdk_nvmf_transport_specific_address_subtype {
	uint8_t raw[256];

	/** RDMA */
	struct spdk_nvmf_rdma_transport_specific_address_subtype rdma;

	/** TCP */
	struct spdk_nvme_tcp_transport_specific_address_subtype tcp;
};
SPDK_STATIC_ASSERT(sizeof(union spdk_nvmf_transport_specific_address_subtype) == 256,
		   "Incorrect size");

#define SPDK_NVMF_MIN_ADMIN_MAX_SQ_SIZE 32

/**
 * Discovery Log Page entry
 */
struct spdk_nvmf_discovery_log_page_entry {
	/** Transport type (\ref spdk_nvmf_trtype) */
	uint8_t		trtype;

	/** Address family (\ref spdk_nvmf_adrfam) */
	uint8_t		adrfam;

	/** Subsystem type (\ref spdk_nvmf_subtype) */
	uint8_t		subtype;

	/** Transport requirements */
	struct {
		/** Secure channel requirements (\ref spdk_nvmf_treq_secure_channel) */
		uint8_t secure_channel : 2;

		uint8_t reserved : 6;
	} treq;

	/** NVM subsystem port ID */
	uint16_t	portid;

	/** Controller ID */
	uint16_t	cntlid;

	/** Admin max SQ size */
	uint16_t	asqsz;

	/** Entry Flags */
	uint16_t	eflags;

	uint8_t		reserved0[20];

	/** Transport service identifier */
	uint8_t		trsvcid[SPDK_NVMF_TRSVCID_MAX_LEN];

	uint8_t		reserved1[192];

	/** NVM subsystem qualified name */
	uint8_t		subnqn[256];

	/** Transport address */
	uint8_t		traddr[SPDK_NVMF_TRADDR_MAX_LEN];

	/** Transport-specific address subtype */
	union spdk_nvmf_transport_specific_address_subtype tsas;
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_discovery_log_page_entry) == 1024, "Incorrect size");

struct spdk_nvmf_discovery_log_page {
	uint64_t	genctr;
	uint64_t	numrec;
	uint16_t	recfmt;
	uint8_t		reserved0[1006];
	struct spdk_nvmf_discovery_log_page_entry entries[0];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_discovery_log_page) == 1024, "Incorrect size");

/* RDMA Fabric specific definitions below */

#define SPDK_NVME_SGL_SUBTYPE_INVALIDATE_KEY	0xF

struct spdk_nvmf_rdma_request_private_data {
	uint16_t	recfmt; /* record format */
	uint16_t	qid;	/* queue id */
	uint16_t	hrqsize;	/* host receive queue size */
	uint16_t	hsqsize;	/* host send queue size */
	uint16_t	cntlid;		/* controller id */
	uint8_t		reserved[22];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_rdma_request_private_data) == 32, "Incorrect size");

struct spdk_nvmf_rdma_accept_private_data {
	uint16_t	recfmt; /* record format */
	uint16_t	crqsize;	/* controller receive queue size */
	uint8_t		reserved[28];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_rdma_accept_private_data) == 32, "Incorrect size");

struct spdk_nvmf_rdma_reject_private_data {
	uint16_t	recfmt; /* record format */
	uint16_t	sts; /* status */
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvmf_rdma_reject_private_data) == 4, "Incorrect size");

union spdk_nvmf_rdma_private_data {
	struct spdk_nvmf_rdma_request_private_data	pd_request;
	struct spdk_nvmf_rdma_accept_private_data	pd_accept;
	struct spdk_nvmf_rdma_reject_private_data	pd_reject;
};
SPDK_STATIC_ASSERT(sizeof(union spdk_nvmf_rdma_private_data) == 32, "Incorrect size");

enum spdk_nvmf_rdma_transport_error {
	SPDK_NVMF_RDMA_ERROR_INVALID_PRIVATE_DATA_LENGTH	= 0x1,
	SPDK_NVMF_RDMA_ERROR_INVALID_RECFMT			= 0x2,
	SPDK_NVMF_RDMA_ERROR_INVALID_QID			= 0x3,
	SPDK_NVMF_RDMA_ERROR_INVALID_HSQSIZE			= 0x4,
	SPDK_NVMF_RDMA_ERROR_INVALID_HRQSIZE			= 0x5,
	SPDK_NVMF_RDMA_ERROR_NO_RESOURCES			= 0x6,
	SPDK_NVMF_RDMA_ERROR_INVALID_IRD			= 0x7,
	SPDK_NVMF_RDMA_ERROR_INVALID_ORD			= 0x8,
};

/* TCP transport specific definitions below */

/** NVMe/TCP PDU type */
enum spdk_nvme_tcp_pdu_type {
	/** Initialize Connection Request (ICReq) */
	SPDK_NVME_TCP_PDU_TYPE_IC_REQ			= 0x00,

	/** Initialize Connection Response (ICResp) */
	SPDK_NVME_TCP_PDU_TYPE_IC_RESP			= 0x01,

	/** Terminate Connection Request (TermReq) */
	SPDK_NVME_TCP_PDU_TYPE_H2C_TERM_REQ		= 0x02,

	/** Terminate Connection Response (TermResp) */
	SPDK_NVME_TCP_PDU_TYPE_C2H_TERM_REQ		= 0x03,

	/** Command Capsule (CapsuleCmd) */
	SPDK_NVME_TCP_PDU_TYPE_CAPSULE_CMD		= 0x04,

	/** Response Capsule (CapsuleRsp) */
	SPDK_NVME_TCP_PDU_TYPE_CAPSULE_RESP		= 0x05,

	/** Host To Controller Data (H2CData) */
	SPDK_NVME_TCP_PDU_TYPE_H2C_DATA			= 0x06,

	/** Controller To Host Data (C2HData) */
	SPDK_NVME_TCP_PDU_TYPE_C2H_DATA			= 0x07,

	/** Ready to Transfer (R2T) */
	SPDK_NVME_TCP_PDU_TYPE_R2T			= 0x09,
};

/** Common NVMe/TCP PDU header */
struct spdk_nvme_tcp_common_pdu_hdr {
	/** PDU type (\ref spdk_nvme_tcp_pdu_type) */
	uint8_t				pdu_type;

	/** pdu_type-specific flags */
	uint8_t				flags;

	/** Length of PDU header (not including the Header Digest) */
	uint8_t				hlen;

	/** PDU Data Offset from the start of the PDU */
	uint8_t				pdo;

	/** Total number of bytes in PDU, including pdu_hdr */
	uint32_t			plen;
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvme_tcp_common_pdu_hdr) == 8, "Incorrect size");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_common_pdu_hdr, pdu_type) == 0,
		   "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_common_pdu_hdr, flags) == 1, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_common_pdu_hdr, hlen) == 2, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_common_pdu_hdr, pdo) == 3, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_common_pdu_hdr, plen) == 4, "Incorrect offset");

#define SPDK_NVME_TCP_CH_FLAGS_HDGSTF		(1u << 0)
#define SPDK_NVME_TCP_CH_FLAGS_DDGSTF		(1u << 1)

/**
 * ICReq
 *
 * common.pdu_type == SPDK_NVME_TCP_PDU_TYPE_IC_REQ
 */
struct spdk_nvme_tcp_ic_req {
	struct spdk_nvme_tcp_common_pdu_hdr	common;
	uint16_t				pfv;
	/** Specifies the data alignment for all PDUs transferred from the controller to the host that contain data */
	uint8_t					hpda;
	union {
		uint8_t				raw;
		struct {
			uint8_t			hdgst_enable : 1;
			uint8_t			ddgst_enable : 1;
			uint8_t			reserved : 6;
		} bits;
	} dgst;
	uint32_t				maxr2t;
	uint8_t					reserved16[112];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvme_tcp_ic_req) == 128, "Incorrect size");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_ic_req, pfv) == 8, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_ic_req, hpda) == 10, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_ic_req, maxr2t) == 12, "Incorrect offset");

#define SPDK_NVME_TCP_HPDA_MAX 31
#define SPDK_NVME_TCP_CPDA_MAX 31
#define SPDK_NVME_TCP_PDU_PDO_MAX_OFFSET     ((SPDK_NVME_TCP_CPDA_MAX + 1) << 2)

/**
 * ICResp
 *
 * common.pdu_type == SPDK_NVME_TCP_PDU_TYPE_IC_RESP
 */
struct spdk_nvme_tcp_ic_resp {
	struct spdk_nvme_tcp_common_pdu_hdr	common;
	uint16_t				pfv;
	/** Specifies the data alignment for all PDUs transferred from the host to the controller that contain data */
	uint8_t					cpda;
	union {
		uint8_t				raw;
		struct {
			uint8_t			hdgst_enable : 1;
			uint8_t			ddgst_enable : 1;
			uint8_t			reserved : 6;
		} bits;
	} dgst;
	/** Specifies the maximum number of PDU-Data bytes per H2C Data Transfer PDU */
	uint32_t				maxh2cdata;
	uint8_t					reserved16[112];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvme_tcp_ic_resp) == 128, "Incorrect size");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_ic_resp, pfv) == 8, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_ic_resp, cpda) == 10, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_ic_resp, maxh2cdata) == 12, "Incorrect offset");

/**
 * TermReq
 *
 * common.pdu_type == SPDK_NVME_TCP_PDU_TYPE_TERM_REQ
 */
struct spdk_nvme_tcp_term_req_hdr {
	struct spdk_nvme_tcp_common_pdu_hdr	common;
	uint16_t				fes;
	uint8_t					fei[4];
	uint8_t					reserved14[10];
};

SPDK_STATIC_ASSERT(sizeof(struct spdk_nvme_tcp_term_req_hdr) == 24, "Incorrect size");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_term_req_hdr, fes) == 8, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_term_req_hdr, fei) == 10, "Incorrect offset");

enum spdk_nvme_tcp_term_req_fes {
	SPDK_NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD				= 0x01,
	SPDK_NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR				= 0x02,
	SPDK_NVME_TCP_TERM_REQ_FES_HDGST_ERROR					= 0x03,
	SPDK_NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_OUT_OF_RANGE			= 0x04,
	SPDK_NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_LIMIT_EXCEEDED			= 0x05,
	SPDK_NVME_TCP_TERM_REQ_FES_R2T_LIMIT_EXCEEDED				= 0x05,
	SPDK_NVME_TCP_TERM_REQ_FES_INVALID_DATA_UNSUPPORTED_PARAMETER		= 0x06,
};

/* Total length of term req PDU (including PDU header and DATA) in bytes shall not exceed a limit of 152 bytes. */
#define SPDK_NVME_TCP_TERM_REQ_ERROR_DATA_MAX_SIZE	128
#define SPDK_NVME_TCP_TERM_REQ_PDU_MAX_SIZE		(SPDK_NVME_TCP_TERM_REQ_ERROR_DATA_MAX_SIZE + sizeof(struct spdk_nvme_tcp_term_req_hdr))

/**
 * CapsuleCmd
 *
 * common.pdu_type == SPDK_NVME_TCP_PDU_TYPE_CAPSULE_CMD
 */
struct spdk_nvme_tcp_cmd {
	struct spdk_nvme_tcp_common_pdu_hdr	common;
	struct spdk_nvme_cmd			ccsqe;
	/**< icdoff hdgst padding + in-capsule data + ddgst (if enabled) */
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvme_tcp_cmd) == 72, "Incorrect size");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_cmd, ccsqe) == 8, "Incorrect offset");

/**
 * CapsuleResp
 *
 * common.pdu_type == SPDK_NVME_TCP_PDU_TYPE_CAPSULE_RESP
 */
struct spdk_nvme_tcp_rsp {
	struct spdk_nvme_tcp_common_pdu_hdr	common;
	struct spdk_nvme_cpl			rccqe;
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvme_tcp_rsp) == 24, "incorrect size");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_rsp, rccqe) == 8, "Incorrect offset");


/**
 * H2CData
 *
 * hdr.pdu_type == SPDK_NVME_TCP_PDU_TYPE_H2C_DATA
 */
struct spdk_nvme_tcp_h2c_data_hdr {
	struct spdk_nvme_tcp_common_pdu_hdr	common;
	uint16_t				cccid;
	uint16_t				ttag;
	uint32_t				datao;
	uint32_t				datal;
	uint8_t					reserved20[4];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvme_tcp_h2c_data_hdr) == 24, "Incorrect size");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_h2c_data_hdr, cccid) == 8, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_h2c_data_hdr, ttag) == 10, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_h2c_data_hdr, datao) == 12, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_h2c_data_hdr, datal) == 16, "Incorrect offset");

#define SPDK_NVME_TCP_H2C_DATA_FLAGS_LAST_PDU	(1u << 2)
#define SPDK_NVME_TCP_H2C_DATA_FLAGS_SUCCESS	(1u << 3)
#define SPDK_NVME_TCP_H2C_DATA_PDO_MULT		8u

/**
 * C2HData
 *
 * hdr.pdu_type == SPDK_NVME_TCP_PDU_TYPE_C2H_DATA
 */
struct spdk_nvme_tcp_c2h_data_hdr {
	struct spdk_nvme_tcp_common_pdu_hdr	common;
	uint16_t				cccid;
	uint8_t					reserved10[2];
	uint32_t				datao;
	uint32_t				datal;
	uint8_t					reserved20[4];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvme_tcp_c2h_data_hdr) == 24, "Incorrect size");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_c2h_data_hdr, cccid) == 8, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_c2h_data_hdr, datao) == 12, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_c2h_data_hdr, datal) == 16, "Incorrect offset");

#define SPDK_NVME_TCP_C2H_DATA_FLAGS_SUCCESS	(1u << 3)
#define SPDK_NVME_TCP_C2H_DATA_FLAGS_LAST_PDU	(1u << 2)
#define SPDK_NVME_TCP_C2H_DATA_PDO_MULT		8u

/**
 * R2T
 *
 * common.pdu_type == SPDK_NVME_TCP_PDU_TYPE_R2T
 */
struct spdk_nvme_tcp_r2t_hdr {
	struct spdk_nvme_tcp_common_pdu_hdr	common;
	uint16_t				cccid;
	uint16_t				ttag;
	uint32_t				r2to;
	uint32_t				r2tl;
	uint8_t					reserved20[4];
};
SPDK_STATIC_ASSERT(sizeof(struct spdk_nvme_tcp_r2t_hdr) == 24, "Incorrect size");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_r2t_hdr, cccid) == 8, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_r2t_hdr, ttag) == 10, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_r2t_hdr, r2to) == 12, "Incorrect offset");
SPDK_STATIC_ASSERT(offsetof(struct spdk_nvme_tcp_r2t_hdr, r2tl) == 16, "Incorrect offset");

#pragma pack(pop)

#endif /* __NVMF_SPEC_H__ */
