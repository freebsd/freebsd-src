/*
 * Copyright (c) 2004-2007 Voltaire Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#ifndef _MAD_H_
#define _MAD_H_

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else /* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif /* __cplusplus */

BEGIN_C_DECLS

#define IB_SUBNET_PATH_HOPS_MAX	64
#define IB_DEFAULT_SUBN_PREFIX	0xfe80000000000000llu
#define IB_DEFAULT_QP1_QKEY	0x80010000

#define IB_MAD_SIZE		256

#define IB_SMP_DATA_OFFS	64
#define IB_SMP_DATA_SIZE	64

#define IB_VENDOR_RANGE1_DATA_OFFS	24
#define IB_VENDOR_RANGE1_DATA_SIZE	(IB_MAD_SIZE - IB_VENDOR_RANGE1_DATA_OFFS)

#define IB_VENDOR_RANGE2_DATA_OFFS	40
#define IB_VENDOR_RANGE2_DATA_SIZE	(IB_MAD_SIZE - IB_VENDOR_RANGE2_DATA_OFFS)

#define IB_SA_DATA_SIZE		200
#define IB_SA_DATA_OFFS		56

#define IB_PC_DATA_OFFS		64
#define IB_PC_DATA_SZ		(IB_MAD_SIZE - IB_PC_DATA_OFFS)

#define IB_SA_MCM_RECSZ		53
#define IB_SA_PR_RECSZ		64

enum MAD_CLASSES {
	IB_SMI_CLASS = 		0x1,
	IB_SMI_DIRECT_CLASS = 	0x81,
	IB_SA_CLASS = 		0x3,
	IB_PERFORMANCE_CLASS = 	0x4,
	IB_BOARD_MGMT_CLASS = 	0x5,
	IB_DEVICE_MGMT_CLASS =	0x6,
	IB_CM_CLASS =		0x7,
	IB_SNMP_CLASS =		0x8,
	IB_VENDOR_RANGE1_START_CLASS = 0x9,
	IB_VENDOR_RANGE1_END_CLASS = 0x0f,
	IB_CC_CLASS =		0x21,
	IB_VENDOR_RANGE2_START_CLASS = 0x30,
	IB_VENDOR_RANGE2_END_CLASS = 0x4f,
};

enum MAD_METHODS {
	IB_MAD_METHOD_GET = 		0x1,
	IB_MAD_METHOD_SET = 		0x2,
	IB_MAD_METHOD_GET_RESPONSE =	0x81,

	IB_MAD_METHOD_SEND = 		0x3,
	IB_MAD_METHOD_TRAP = 		0x5,
	IB_MAD_METHOD_TRAP_REPRESS = 	0x7,

	IB_MAD_METHOD_REPORT =		0x6,
	IB_MAD_METHOD_REPORT_RESPONSE = 0x86,
	IB_MAD_METHOD_GET_TABLE =  	0x12,
	IB_MAD_METHOD_GET_TABLE_RESPONSE = 0x92,
	IB_MAD_METHOD_GET_TRACE_TABLE = 0x13,
	IB_MAD_METHOD_GET_TRACE_TABLE_RESPONSE = 0x93,
	IB_MAD_METHOD_GETMULTI = 	0x14,
	IB_MAD_METHOD_GETMULTI_RESPONSE = 0x94,
	IB_MAD_METHOD_DELETE =		0x15,
	IB_MAD_METHOD_DELETE_RESPONSE = 0x95,

	IB_MAD_RESPONSE = 		0x80,
};

enum MAD_ATTR_ID {
	CLASS_PORT_INFO = 0x1,
	NOTICE = 0x2,
	INFORM_INFO = 0x3,
};

enum SMI_ATTR_ID {
	IB_ATTR_NODE_DESC = 0x10,
	IB_ATTR_NODE_INFO = 0x11,
	IB_ATTR_SWITCH_INFO = 0x12,
	IB_ATTR_GUID_INFO = 0x14,
	IB_ATTR_PORT_INFO = 0x15,
	IB_ATTR_PKEY_TBL = 0x16,
	IB_ATTR_SLVL_TABLE = 0x17,
	IB_ATTR_VL_ARBITRATION = 0x18,
	IB_ATTR_LINEARFORWTBL = 0x19,
	IB_ATTR_MULTICASTFORWTBL = 0x1b,
	IB_ATTR_SMINFO = 0x20,

	IB_ATTR_LAST
};

enum SA_ATTR_ID {
	IB_SA_ATTR_NOTICE = 0x02,
	IB_SA_ATTR_INFORMINFO = 0x03,
	IB_SA_ATTR_PORTINFORECORD = 0x12,
	IB_SA_ATTR_LINKRECORD = 0x20,
	IB_SA_ATTR_SERVICERECORD = 0x31,
	IB_SA_ATTR_PATHRECORD = 0x35,
	IB_SA_ATTR_MCRECORD = 0x38,
	IB_SA_ATTR_MULTIPATH = 0x3a,

	IB_SA_ATTR_LAST
};

enum GSI_ATTR_ID {
	IB_GSI_PORT_SAMPLES_CONTROL = 0x10,
	IB_GSI_PORT_SAMPLES_RESULT = 0x11,
	IB_GSI_PORT_COUNTERS = 0x12,
	IB_GSI_PORT_COUNTERS_EXT = 0x1D,

	IB_GSI_ATTR_LAST
};

#define IB_VENDOR_OPENIB_PING_CLASS	(IB_VENDOR_RANGE2_START_CLASS + 2)
#define IB_VENDOR_OPENIB_SYSSTAT_CLASS	(IB_VENDOR_RANGE2_START_CLASS + 3)
#define IB_OPENIB_OUI			(0x001405)

typedef uint8_t ibmad_gid_t[16];
#ifdef USE_DEPRECATED_IB_GID_T
typedef ibmad_gid_t ib_gid_t __attribute__((deprecated));
#endif

typedef struct {
	int cnt;
	uint8_t p[IB_SUBNET_PATH_HOPS_MAX];
	uint16_t drslid;
	uint16_t drdlid;
} ib_dr_path_t;

typedef struct {
	unsigned id;
	unsigned mod;
} ib_attr_t;

typedef struct {
	int mgtclass;
	int method;
	ib_attr_t attr;
	uint32_t rstatus;	/* return status */
	int dataoffs;
	int datasz;
	uint64_t mkey;
	uint64_t trid;	/* used for out mad if nonzero, return real val */
	uint64_t mask;	/* for sa mads */
	unsigned recsz;	/* for sa mads (attribute offset) */
	int timeout;
	uint32_t oui;	/* for vendor range 2 mads */
} ib_rpc_t;

typedef struct portid {
	int lid;		/* lid or 0 if directed route */
	ib_dr_path_t drpath;
	int grh_present;	/* flag */
	ibmad_gid_t gid;
	uint32_t qp;
	uint32_t qkey;
	uint8_t sl;
	unsigned pkey_idx;
} ib_portid_t;

typedef void (ib_mad_dump_fn)(char *buf, int bufsz, void *val, int valsz);

#define IB_FIELD_NAME_LEN	32

typedef struct ib_field {
	int bitoffs;
	int bitlen;
	char name[IB_FIELD_NAME_LEN];
	ib_mad_dump_fn *def_dump_fn;
} ib_field_t;

enum MAD_FIELDS {
	IB_NO_FIELD,

	IB_GID_PREFIX_F,
	IB_GID_GUID_F,

	/* first MAD word (0-3 bytes) */
	IB_MAD_METHOD_F,
	IB_MAD_RESPONSE_F,
	IB_MAD_CLASSVER_F,
	IB_MAD_MGMTCLASS_F,
	IB_MAD_BASEVER_F,

	/* second MAD word (4-7 bytes) */
	IB_MAD_STATUS_F,

	/* DRSMP only */
	IB_DRSMP_HOPCNT_F,
	IB_DRSMP_HOPPTR_F,
	IB_DRSMP_STATUS_F,
	IB_DRSMP_DIRECTION_F,

	/* words 3,4,5,6 (8-23 bytes) */
	IB_MAD_TRID_F,
	IB_MAD_ATTRID_F,
	IB_MAD_ATTRMOD_F,

	/* word 7,8 (24-31 bytes) */
	IB_MAD_MKEY_F,

	/* word 9 (32-37 bytes) */
	IB_DRSMP_DRSLID_F,
	IB_DRSMP_DRDLID_F,

	/* word 10,11 (36-43 bytes) */
	IB_SA_MKEY_F,

	/* word 12 (44-47 bytes) */
	IB_SA_ATTROFFS_F,

	/* word 13,14 (48-55 bytes) */
	IB_SA_COMPMASK_F,

	/* word 13,14 (56-255 bytes) */
	IB_SA_DATA_F,

	/* bytes 64 - 127 */
	IB_SM_DATA_F,

	/* bytes 64 - 256 */
	IB_GS_DATA_F,

	/* bytes 128 - 191 */
	IB_DRSMP_PATH_F,

	/* bytes 192 - 255 */
	IB_DRSMP_RPATH_F,

	/*
	 * PortInfo fields:
	 */
	IB_PORT_FIRST_F,
	IB_PORT_MKEY_F = IB_PORT_FIRST_F,
	IB_PORT_GID_PREFIX_F,
	IB_PORT_LID_F,
	IB_PORT_SMLID_F,
	IB_PORT_CAPMASK_F,
	IB_PORT_DIAG_F,
	IB_PORT_MKEY_LEASE_F,
	IB_PORT_LOCAL_PORT_F,
	IB_PORT_LINK_WIDTH_ENABLED_F,
	IB_PORT_LINK_WIDTH_SUPPORTED_F,
	IB_PORT_LINK_WIDTH_ACTIVE_F,
	IB_PORT_LINK_SPEED_SUPPORTED_F,
	IB_PORT_STATE_F,
	IB_PORT_PHYS_STATE_F,
	IB_PORT_LINK_DOWN_DEF_F,
	IB_PORT_MKEY_PROT_BITS_F,
	IB_PORT_LMC_F,
	IB_PORT_LINK_SPEED_ACTIVE_F,
	IB_PORT_LINK_SPEED_ENABLED_F,
	IB_PORT_NEIGHBOR_MTU_F,
	IB_PORT_SMSL_F,
	IB_PORT_VL_CAP_F,
	IB_PORT_INIT_TYPE_F,
	IB_PORT_VL_HIGH_LIMIT_F,
	IB_PORT_VL_ARBITRATION_HIGH_CAP_F,
	IB_PORT_VL_ARBITRATION_LOW_CAP_F,
	IB_PORT_INIT_TYPE_REPLY_F,
	IB_PORT_MTU_CAP_F,
	IB_PORT_VL_STALL_COUNT_F,
	IB_PORT_HOQ_LIFE_F,
	IB_PORT_OPER_VLS_F,
	IB_PORT_PART_EN_INB_F,
	IB_PORT_PART_EN_OUTB_F,
	IB_PORT_FILTER_RAW_INB_F,
	IB_PORT_FILTER_RAW_OUTB_F,
	IB_PORT_MKEY_VIOL_F,
	IB_PORT_PKEY_VIOL_F,
	IB_PORT_QKEY_VIOL_F,
	IB_PORT_GUID_CAP_F,
	IB_PORT_CLIENT_REREG_F,
	IB_PORT_SUBN_TIMEOUT_F,
	IB_PORT_RESP_TIME_VAL_F,
	IB_PORT_LOCAL_PHYS_ERR_F,
	IB_PORT_OVERRUN_ERR_F,
	IB_PORT_MAX_CREDIT_HINT_F,
	IB_PORT_LINK_ROUND_TRIP_F,
	IB_PORT_LAST_F,

	/*
	 * NodeInfo fields:
	 */
	IB_NODE_FIRST_F,
	IB_NODE_BASE_VERS_F = IB_NODE_FIRST_F,
	IB_NODE_CLASS_VERS_F,
	IB_NODE_TYPE_F,
	IB_NODE_NPORTS_F,
	IB_NODE_SYSTEM_GUID_F,
	IB_NODE_GUID_F,
	IB_NODE_PORT_GUID_F,
	IB_NODE_PARTITION_CAP_F,
	IB_NODE_DEVID_F,
	IB_NODE_REVISION_F,
	IB_NODE_LOCAL_PORT_F,
	IB_NODE_VENDORID_F,
	IB_NODE_LAST_F,

	/*
	 * SwitchInfo fields:
	 */
	IB_SW_FIRST_F,
	IB_SW_LINEAR_FDB_CAP_F = IB_SW_FIRST_F,
	IB_SW_RANDOM_FDB_CAP_F,
	IB_SW_MCAST_FDB_CAP_F,
	IB_SW_LINEAR_FDB_TOP_F,
	IB_SW_DEF_PORT_F,
	IB_SW_DEF_MCAST_PRIM_F,
	IB_SW_DEF_MCAST_NOT_PRIM_F,
	IB_SW_LIFE_TIME_F,
	IB_SW_STATE_CHANGE_F,
	IB_SW_LIDS_PER_PORT_F,
	IB_SW_PARTITION_ENFORCE_CAP_F,
	IB_SW_PARTITION_ENF_INB_F,
	IB_SW_PARTITION_ENF_OUTB_F,
	IB_SW_FILTER_RAW_INB_F,
	IB_SW_FILTER_RAW_OUTB_F,
	IB_SW_ENHANCED_PORT0_F,
	IB_SW_LAST_F,

	/*
	 * SwitchLinearForwardingTable fields:
	 */
	IB_LINEAR_FORW_TBL_F,

	/*
	 * SwitchMulticastForwardingTable fields:
	 */
	IB_MULTICAST_FORW_TBL_F,

	/*
	 * NodeDescription fields:
	 */
	IB_NODE_DESC_F,

	/*
	 * Notice/Trap fields
	 */
	IB_NOTICE_IS_GENERIC_F,
	IB_NOTICE_TYPE_F,
	IB_NOTICE_PRODUCER_F,
	IB_NOTICE_TRAP_NUMBER_F,
	IB_NOTICE_ISSUER_LID_F,
	IB_NOTICE_TOGGLE_F,
	IB_NOTICE_COUNT_F,
	IB_NOTICE_DATA_DETAILS_F,
	IB_NOTICE_DATA_LID_F,
	IB_NOTICE_DATA_144_LID_F,
	IB_NOTICE_DATA_144_CAPMASK_F,

	/*
	 * GS Performance
	 */
	IB_PC_FIRST_F,
	IB_PC_PORT_SELECT_F = IB_PC_FIRST_F,
	IB_PC_COUNTER_SELECT_F,
	IB_PC_ERR_SYM_F,
	IB_PC_LINK_RECOVERS_F,
	IB_PC_LINK_DOWNED_F,
	IB_PC_ERR_RCV_F,
	IB_PC_ERR_PHYSRCV_F,
	IB_PC_ERR_SWITCH_REL_F,
	IB_PC_XMT_DISCARDS_F,
	IB_PC_ERR_XMTCONSTR_F,
	IB_PC_ERR_RCVCONSTR_F,
	IB_PC_ERR_LOCALINTEG_F,
	IB_PC_ERR_EXCESS_OVR_F,
	IB_PC_VL15_DROPPED_F,
	IB_PC_XMT_BYTES_F,
	IB_PC_RCV_BYTES_F,
	IB_PC_XMT_PKTS_F,
	IB_PC_RCV_PKTS_F,
	IB_PC_LAST_F,

	/*
	 * SMInfo
	 */
	IB_SMINFO_GUID_F,
	IB_SMINFO_KEY_F,
	IB_SMINFO_ACT_F,
	IB_SMINFO_PRIO_F,
	IB_SMINFO_STATE_F,

	/*
	 * SA RMPP
	 */
	IB_SA_RMPP_VERS_F,
	IB_SA_RMPP_TYPE_F,
	IB_SA_RMPP_RESP_F,
	IB_SA_RMPP_FLAGS_F,
	IB_SA_RMPP_STATUS_F,

	/* data1 */
	IB_SA_RMPP_D1_F,
	IB_SA_RMPP_SEGNUM_F,
	/* data2 */
	IB_SA_RMPP_D2_F,
	IB_SA_RMPP_LEN_F,		/* DATA: Payload len */
	IB_SA_RMPP_NEWWIN_F,		/* ACK: new window last */

	/*
	 * SA Multi Path rec
	 */
	IB_SA_MP_NPATH_F,
	IB_SA_MP_NSRC_F,
	IB_SA_MP_NDEST_F,
	IB_SA_MP_GID0_F,

	/*
	 * SA Path rec
	 */
	IB_SA_PR_DGID_F,
	IB_SA_PR_SGID_F,
	IB_SA_PR_DLID_F,
	IB_SA_PR_SLID_F,
	IB_SA_PR_NPATH_F,

	/*
	 * MC Member rec
	 */
	IB_SA_MCM_MGID_F,
	IB_SA_MCM_PORTGID_F,
	IB_SA_MCM_QKEY_F,
	IB_SA_MCM_MLID_F,
	IB_SA_MCM_SL_F,
	IB_SA_MCM_MTU_F,
	IB_SA_MCM_RATE_F,
	IB_SA_MCM_TCLASS_F,
	IB_SA_MCM_PKEY_F,
	IB_SA_MCM_FLOW_LABEL_F,
	IB_SA_MCM_JOIN_STATE_F,
	IB_SA_MCM_PROXY_JOIN_F,

	/*
	 * Service record
	 */
	IB_SA_SR_ID_F,
	IB_SA_SR_GID_F,
	IB_SA_SR_PKEY_F,
	IB_SA_SR_LEASE_F,
	IB_SA_SR_KEY_F,
	IB_SA_SR_NAME_F,
	IB_SA_SR_DATA_F,

	/*
	 * ATS SM record - within SA_SR_DATA
	 */
	IB_ATS_SM_NODE_ADDR_F,
	IB_ATS_SM_MAGIC_KEY_F,
	IB_ATS_SM_NODE_TYPE_F,
	IB_ATS_SM_NODE_NAME_F,

	/*
	 * SLTOVL MAPPING TABLE
	 */
	IB_SLTOVL_MAPPING_TABLE_F,

	/*
	 * VL ARBITRATION TABLE
	 */
	IB_VL_ARBITRATION_TABLE_F,

	/*
	 * IB vendor class range 2
	 */
	IB_VEND2_OUI_F,
	IB_VEND2_DATA_F,

	/*
	 * PortCountersExtended
	 */
	IB_PC_EXT_FIRST_F,
	IB_PC_EXT_PORT_SELECT_F = IB_PC_EXT_FIRST_F,
	IB_PC_EXT_COUNTER_SELECT_F,
	IB_PC_EXT_XMT_BYTES_F,
	IB_PC_EXT_RCV_BYTES_F,
	IB_PC_EXT_XMT_PKTS_F,
	IB_PC_EXT_RCV_PKTS_F,
	IB_PC_EXT_XMT_UPKTS_F,
	IB_PC_EXT_RCV_UPKTS_F,
	IB_PC_EXT_XMT_MPKTS_F,
	IB_PC_EXT_RCV_MPKTS_F,
	IB_PC_EXT_LAST_F,

	/*
	 * GUIDInfo fields
	 */
	IB_GUID_GUID0_F,

	IB_FIELD_LAST_	/* must be last */
};

/*
 * SA RMPP section
 */
enum RMPP_TYPE_ENUM {
	IB_RMPP_TYPE_NONE,
	IB_RMPP_TYPE_DATA,
	IB_RMPP_TYPE_ACK,
	IB_RMPP_TYPE_STOP,
	IB_RMPP_TYPE_ABORT,
};

enum RMPP_FLAGS_ENUM {
	IB_RMPP_FLAG_ACTIVE = 1 << 0,
	IB_RMPP_FLAG_FIRST = 1 << 1,
	IB_RMPP_FLAG_LAST = 1 << 2,
};

typedef struct {
	int type;
	int flags;
	int status;
	union {
		uint32_t u;
		uint32_t segnum;
	} d1;
	union {
		uint32_t u;
		uint32_t len;
		uint32_t newwin;
	} d2;
} ib_rmpp_hdr_t;

enum SA_SIZES_ENUM {
	SA_HEADER_SZ = 20,
};

typedef struct ib_sa_call {
	unsigned attrid;
	unsigned mod;
	uint64_t mask;
	unsigned method;

	uint64_t trid;	/* used for out mad if nonzero, return real val */
	unsigned recsz;	/* return field */
	ib_rmpp_hdr_t rmpp;
} ib_sa_call_t;

typedef struct ib_vendor_call {
	unsigned method;
	unsigned mgmt_class;
	unsigned attrid;
	unsigned mod;
	uint32_t oui;
	unsigned timeout;
	ib_rmpp_hdr_t rmpp;
} ib_vendor_call_t;

#define IB_MIN_UCAST_LID	1
#define IB_MAX_UCAST_LID	(0xc000-1)
#define IB_MIN_MCAST_LID	0xc000
#define IB_MAX_MCAST_LID	(0xffff-1)

#define IB_LID_VALID(lid)	((lid) >= IB_MIN_UCAST_LID && lid <= IB_MAX_UCAST_LID)
#define IB_MLID_VALID(lid)	((lid) >= IB_MIN_MCAST_LID && lid <= IB_MAX_MCAST_LID)

#define MAD_DEF_RETRIES		3
#define MAD_DEF_TIMEOUT_MS	1000

enum {
	IB_DEST_LID,
	IB_DEST_DRPATH,
	IB_DEST_GUID,
	IB_DEST_DRSLID,
};

enum {
	IB_NODE_CA = 1,
	IB_NODE_SWITCH,
	IB_NODE_ROUTER,
	NODE_RNIC,

	IB_NODE_MAX = NODE_RNIC
};

/******************************************************************************/

/* portid.c */
char *	portid2str(ib_portid_t *portid);
int	portid2portnum(ib_portid_t *portid);
int	str2drpath(ib_dr_path_t *path, char *routepath, int drslid, int drdlid);
char *  drpath2str(ib_dr_path_t *path, char *dstr, size_t dstr_size);

static inline int
ib_portid_set(ib_portid_t *portid, int lid, int qp, int qkey)
{
	portid->lid = lid;
	portid->qp = qp;
	portid->qkey = qkey;
	portid->grh_present = 0;

	return 0;
}

/* fields.c */
extern ib_field_t ib_mad_f[];

void	_set_field(void *buf, int base_offs, ib_field_t *f, uint32_t val);
uint32_t _get_field(void *buf, int base_offs, ib_field_t *f);
void	_set_array(void *buf, int base_offs, ib_field_t *f, void *val);
void	_get_array(void *buf, int base_offs, ib_field_t *f, void *val);
void	_set_field64(void *buf, int base_offs, ib_field_t *f, uint64_t val);
uint64_t _get_field64(void *buf, int base_offs, ib_field_t *f);

/* mad.c */
static inline uint32_t
mad_get_field(void *buf, int base_offs, int field)
{
	return _get_field(buf, base_offs, ib_mad_f + field);
}

static inline void
mad_set_field(void *buf, int base_offs, int field, uint32_t val)
{
	_set_field(buf, base_offs, ib_mad_f + field, val);
}

/* field must be byte aligned */
static inline uint64_t
mad_get_field64(void *buf, int base_offs, int field)
{
	return _get_field64(buf, base_offs, ib_mad_f + field);
}

static inline void
mad_set_field64(void *buf, int base_offs, int field, uint64_t val)
{
	_set_field64(buf, base_offs, ib_mad_f + field, val);
}

static inline void
mad_set_array(void *buf, int base_offs, int field, void *val)
{
	_set_array(buf, base_offs, ib_mad_f + field, val);
}

static inline void
mad_get_array(void *buf, int base_offs, int field, void *val)
{
	_get_array(buf, base_offs, ib_mad_f + field, val);
}

void	mad_decode_field(uint8_t *buf, int field, void *val);
void	mad_encode_field(uint8_t *buf, int field, void *val);
void *	mad_encode(void *buf, ib_rpc_t *rpc, ib_dr_path_t *drpath, void *data);
uint64_t mad_trid(void);
int	mad_build_pkt(void *umad, ib_rpc_t *rpc, ib_portid_t *dport, ib_rmpp_hdr_t *rmpp, void *data);

/* register.c */
int	mad_register_port_client(int port_id, int mgmt, uint8_t rmpp_version);
int	mad_register_client(int mgmt, uint8_t rmpp_version);
int	mad_register_server(int mgmt, uint8_t rmpp_version,
			    long method_mask[16/sizeof(long)],
			    uint32_t class_oui);
int	mad_class_agent(int mgmt);
int	mad_agent_class(int agent);

/* serv.c */
int	mad_send(ib_rpc_t *rpc, ib_portid_t *dport, ib_rmpp_hdr_t *rmpp,
		 void *data);
void *	mad_receive(void *umad, int timeout);
int	mad_respond(void *umad, ib_portid_t *portid, uint32_t rstatus);
void *	mad_alloc(void);
void	mad_free(void *umad);

/* vendor.c */
uint8_t *ib_vendor_call(void *data, ib_portid_t *portid,
			ib_vendor_call_t *call);

static inline int
mad_is_vendor_range1(int mgmt)
{
	return mgmt >= 0x9 && mgmt <= 0xf;
}

static inline int
mad_is_vendor_range2(int mgmt)
{
	return mgmt >= 0x30 && mgmt <= 0x4f;
}

/* rpc.c */
int	madrpc_portid(void);
int	madrpc_set_retries(int retries);
int	madrpc_set_timeout(int timeout);
void *	madrpc(ib_rpc_t *rpc, ib_portid_t *dport, void *payload, void *rcvdata);
void *  madrpc_rmpp(ib_rpc_t *rpc, ib_portid_t *dport, ib_rmpp_hdr_t *rmpp,
		    void *data);
void	madrpc_init(char *dev_name, int dev_port, int *mgmt_classes,
		    int num_classes);
void	madrpc_save_mad(void *madbuf, int len);
void	madrpc_lock(void);
void	madrpc_unlock(void);
void	madrpc_show_errors(int set);

void *	mad_rpc_open_port(char *dev_name, int dev_port, int *mgmt_classes,
			  int num_classes);
void	mad_rpc_close_port(void *ibmad_port);
void *	mad_rpc(const void *ibmad_port, ib_rpc_t *rpc, ib_portid_t *dport,
		void *payload, void *rcvdata);
void *  mad_rpc_rmpp(const void *ibmad_port, ib_rpc_t *rpc, ib_portid_t *dport,
		     ib_rmpp_hdr_t *rmpp, void *data);

/* smp.c */
uint8_t * smp_query(void *buf, ib_portid_t *id, unsigned attrid, unsigned mod,
		    unsigned timeout);
uint8_t * smp_set(void *buf, ib_portid_t *id, unsigned attrid, unsigned mod,
		  unsigned timeout);
uint8_t * smp_query_via(void *buf, ib_portid_t *id, unsigned attrid,
			unsigned mod, unsigned timeout, const void *srcport);
uint8_t * smp_set_via(void *buf, ib_portid_t *id, unsigned attrid, unsigned mod,
		      unsigned timeout, const void *srcport);

inline static uint8_t *
safe_smp_query(void *rcvbuf, ib_portid_t *portid, unsigned attrid, unsigned mod,
	       unsigned timeout)
{
	uint8_t *p;

	madrpc_lock();
	p = smp_query(rcvbuf, portid, attrid, mod, timeout);
	madrpc_unlock();

	return p;
}

inline static uint8_t *
safe_smp_set(void *rcvbuf, ib_portid_t *portid, unsigned attrid, unsigned mod,
	     unsigned timeout)
{
	uint8_t *p;

	madrpc_lock();
	p = smp_set(rcvbuf, portid, attrid, mod, timeout);
	madrpc_unlock();

	return p;
}

/* sa.c */
uint8_t * sa_call(void *rcvbuf, ib_portid_t *portid, ib_sa_call_t *sa,
		  unsigned timeout);
uint8_t * sa_rpc_call(const void *ibmad_port, void *rcvbuf, ib_portid_t *portid,
                      ib_sa_call_t *sa, unsigned timeout);
int	ib_path_query(ibmad_gid_t srcgid, ibmad_gid_t destgid, ib_portid_t *sm_id,
		      void *buf);	/* returns lid */
int	ib_path_query_via(const void *srcport, ibmad_gid_t srcgid,
			  ibmad_gid_t destgid, ib_portid_t *sm_id, void *buf);

inline static uint8_t *
safe_sa_call(void *rcvbuf, ib_portid_t *portid, ib_sa_call_t *sa,
	     unsigned timeout)
{
	uint8_t *p;

	madrpc_lock();
	p = sa_call(rcvbuf, portid, sa, timeout);
	madrpc_unlock();

	return p;
}

/* resolve.c */
int	ib_resolve_smlid(ib_portid_t *sm_id, int timeout);
int	ib_resolve_guid(ib_portid_t *portid, uint64_t *guid,
			ib_portid_t *sm_id, int timeout);
int	ib_resolve_portid_str(ib_portid_t *portid, char *addr_str,
			      int dest_type, ib_portid_t *sm_id);
int	ib_resolve_self(ib_portid_t *portid, int *portnum, ibmad_gid_t *gid);

int	ib_resolve_smlid_via(ib_portid_t *sm_id, int timeout,
			     const void *srcport);
int	ib_resolve_guid_via(ib_portid_t *portid, uint64_t *guid,
			    ib_portid_t *sm_id, int timeout,
			    const void *srcport);
int	ib_resolve_portid_str_via(ib_portid_t *portid, char *addr_str,
			          int dest_type, ib_portid_t *sm_id,
				  const void *srcport);
int	ib_resolve_self_via(ib_portid_t *portid, int *portnum, ibmad_gid_t *gid,
			    const void *srcport);

/* gs.c */
uint8_t *perf_classportinfo_query(void *rcvbuf, ib_portid_t *dest, int port,
				  unsigned timeout);
uint8_t *port_performance_query(void *rcvbuf, ib_portid_t *dest, int port,
				unsigned timeout);
uint8_t *port_performance_reset(void *rcvbuf, ib_portid_t *dest, int port,
				unsigned mask, unsigned timeout);
uint8_t *port_performance_ext_query(void *rcvbuf, ib_portid_t *dest, int port,
				    unsigned timeout);
uint8_t *port_performance_ext_reset(void *rcvbuf, ib_portid_t *dest, int port,
				    unsigned mask, unsigned timeout);
uint8_t *port_samples_control_query(void *rcvbuf, ib_portid_t *dest, int port,
				    unsigned timeout);
uint8_t *port_samples_result_query(void *rcvbuf, ib_portid_t *dest, int port,
				   unsigned timeout);

uint8_t *perf_classportinfo_query_via(void *rcvbuf, ib_portid_t *dest, int port,
				  unsigned timeout, const void *srcport);
uint8_t *port_performance_query_via(void *rcvbuf, ib_portid_t *dest, int port,
				unsigned timeout, const void *srcport);
uint8_t *port_performance_reset_via(void *rcvbuf, ib_portid_t *dest, int port,
				unsigned mask, unsigned timeout, const void *srcport);
uint8_t *port_performance_ext_query_via(void *rcvbuf, ib_portid_t *dest, int port,
				    unsigned timeout, const void *srcport);
uint8_t *port_performance_ext_reset_via(void *rcvbuf, ib_portid_t *dest, int port,
				    unsigned mask, unsigned timeout, const void *srcport);
uint8_t *port_samples_control_query_via(void *rcvbuf, ib_portid_t *dest, int port,
				    unsigned timeout, const void *srcport);
uint8_t *port_samples_result_query_via(void *rcvbuf, ib_portid_t *dest, int port,
				   unsigned timeout, const void *srcport);
/* dump.c */
ib_mad_dump_fn
	mad_dump_int, mad_dump_uint, mad_dump_hex, mad_dump_rhex,
	mad_dump_bitfield, mad_dump_array, mad_dump_string,
	mad_dump_linkwidth, mad_dump_linkwidthsup, mad_dump_linkwidthen,
	mad_dump_linkdowndefstate,
	mad_dump_linkspeed, mad_dump_linkspeedsup, mad_dump_linkspeeden,
	mad_dump_portstate, mad_dump_portstates,
	mad_dump_physportstate, mad_dump_portcapmask,
	mad_dump_mtu, mad_dump_vlcap, mad_dump_opervls,
	mad_dump_node_type,
	mad_dump_sltovl, mad_dump_vlarbitration,
	mad_dump_nodedesc, mad_dump_nodeinfo, mad_dump_portinfo, mad_dump_switchinfo,
	mad_dump_perfcounters, mad_dump_perfcounters_ext;

int	_mad_dump(ib_mad_dump_fn *fn, char *name, void *val, int valsz);
char *	_mad_dump_field(ib_field_t *f, char *name, char *buf, int bufsz,
			void *val);
int	_mad_print_field(ib_field_t *f, char *name, void *val, int valsz);
char *	_mad_dump_val(ib_field_t *f, char *buf, int bufsz, void *val);

static inline int
mad_print_field(int field, char *name, void *val)
{
	if (field <= IB_NO_FIELD || field >= IB_FIELD_LAST_)
		return -1;
	return _mad_print_field(ib_mad_f + field, name, val, 0);
}

static inline char *
mad_dump_field(int field, char *buf, int bufsz, void *val)
{
	if (field <= IB_NO_FIELD || field >= IB_FIELD_LAST_)
		return 0;
	return _mad_dump_field(ib_mad_f + field, 0, buf, bufsz, val);
}

static inline char *
mad_dump_val(int field, char *buf, int bufsz, void *val)
{
	if (field <= IB_NO_FIELD || field >= IB_FIELD_LAST_)
		return 0;
	return _mad_dump_val(ib_mad_f + field, buf, bufsz, val);
}

extern int ibdebug;

END_C_DECLS

#endif /* _MAD_H_ */
