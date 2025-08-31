/*-
 * Copyright (c) 2025, Samsung Electronics Co., Ltd.
 * Written by Jaeyoon Choi
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef __UFSHCI_H__
#define __UFSHCI_H__

#include <sys/param.h>
#include <sys/endian.h>

/*
 * Note: This driver currently assumes a little-endian architecture.
 * Big-endian support is not yet implemented.
 */

/* MIPI UniPro spec 2.0, section 5.8.1 "PHY Adapter Common Attributes" */
#define PA_AvailTxDataLanes 0x1520
#define PA_AvailRxDataLanes 0x1540

/*
 * MIPI UniPro spec 2.0, section 5.8.2 "PHY Adapter M-PHY-Specific
 * Attributes"
 */
#define PA_ConnectedTxDataLanes 0x1561
#define PA_ConnectedRxDataLanes 0x1581
#define PA_MaxRxHSGear		0x1587
#define PA_Granularity		0x15AA
#define PA_TActivate		0x15A8

#define PA_RemoteVerInfo	0x15A0
#define PA_LocalVerInfo		0x15A9

/* UFSHCI spec 4.1, section 7.4 "UIC Power Mode Change" */
#define PA_ActiveTxDataLanes		 0x1560
#define PA_ActiveRxDataLanes		 0x1580
#define PA_TxGear			 0x1568
#define PA_RxGear			 0x1583
#define PA_TxTermination		 0x1569
#define PA_RxTermination		 0x1584
#define PA_HSSeries			 0x156A
#define PA_PWRModeUserData0		 0x15B0
#define PA_PWRModeUserData1		 0x15B1
#define PA_PWRModeUserData2		 0x15B2
#define PA_PWRModeUserData3		 0x15B3
#define PA_PWRModeUserData4		 0x15B4
#define PA_PWRModeUserData5		 0x15B5

#define PA_TxHsAdaptType		 0x15D4
#define PA_PWRMode			 0x1571

#define DME_LocalFC0ProtectionTimeOutVal 0xD041
#define DME_LocalTC0ReplayTimeOutVal	 0xD042
#define DME_LocalAFC0ReqTimeOutVal	 0xD043

/* Currently, UFS uses TC0 only. */
#define DL_FC0ProtectionTimeOutVal_Default 8191
#define DL_TC0ReplayTimeOutVal_Default	   65535
#define DL_AFC0ReqTimeOutVal_Default	   32767

/* UFS Spec 4.1, section 6.4 "Reference Clock" */
enum ufshci_attribute_reference_clock {
	UFSHCI_REF_CLK_19_2MHz = 0x0,
	UFSHCI_REF_CLK_26MHz = 0x1,
	UFSHCI_REF_CLK_38_4MHz = 0x2,
	UFSHCI_REF_CLK_OBSOLETE = 0x3,
};

/* UFS spec 4.1, section 9 "UFS UIC Layer: MIPI Unipro" */
enum ufshci_uic_cmd_opcode {
	/* Configuration */
	UFSHCI_DME_GET = 0x01,
	UFSHCI_DME_SET = 0x02,
	UFSHCI_DME_PEER_GET = 0x03,
	UFSHCI_DME_PEER_SET = 0x04,
	/* Controll */
	UFSHCI_DME_POWER_ON = 0x10,
	UFSHCI_DME_POWER_OFF = 0x11,
	UFSHCI_DME_ENABLE = 0x12,
	UFSHCI_DME_RESET = 0x14,
	UFSHCI_DME_ENDPOINT_RESET = 0x15,
	UFSHCI_DME_LINK_STARTUP = 0x16,
	UFSHCI_DME_HIBERNATE_ENTER = 0x17,
	UFSHCI_DME_HIBERNATE_EXIT = 0x18,
	UFSHCI_DME_TEST_MODE = 0x1a,
};

/* UFSHCI spec 4.1, section 5.6.3 "Offset 98h: UICCMDARG2 – UIC Command
 * Argument" */
enum ufshci_uic_cmd_attr_set_type {
	UFSHCI_ATTR_SET_TYPE_NORMAL = 0, /* volatile value */
	UFSHCI_ATTR_SET_TYPE_STATIC = 1, /* non-volatile reset value */
};

struct ufshci_uic_cmd {
	uint8_t opcode;
	uint32_t argument1;
	uint32_t argument2;
	uint32_t argument3;
};

/* UFS spec 4.1, section 10.5 "UPIU Transactions" */
enum transaction_code {
	UFSHCI_UPIU_TRANSACTION_CODE_NOP_OUT = 0x00,
	UFSHCI_UPIU_TRANSACTION_CODE_COMMAND = 0x01,
	UFSHCI_UPIU_TRANSACTION_CODE_DATA_OUT = 0x02,
	UFSHCI_UPIU_TRANSACTION_CODE_TASK_MANAGEMENT_REQUEST = 0x04,
	UFSHCI_UPIU_TRANSACTION_CODE_QUERY_REQUEST = 0x16,
	UFSHCI_UPIU_TRANSACTION_CODE_NOP_IN = 0x20,
	UFSHCI_UPIU_TRANSACTION_CODE_RESPONSE = 0x21,
	UFSHCI_UPIU_TRANSACTION_CODE_DATA_IN = 0x22,
	UFSHCI_UPIU_TRANSACTION_CODE_TASK_MANAGEMENT_RESPONSE = 0x24,
	UFSHCI_UPIU_TRANSACTION_CODE_READY_TO_TRANSFER = 0x31,
	UFSHCI_UPIU_TRANSACTION_CODE_QUERY_RESPONSE = 0x36,
	UFSHCI_UPIU_TRANSACTION_CODE_REJECT_UPIU = 0x3f,
};

enum overall_command_status {
	UFSHCI_DESC_SUCCESS = 0x0,
	UFSHCI_DESC_INVALID_COMMAND_TABLE_ATTRIBUTES = 0x01,
	UFSHCI_DESC_INVALID_PRDT_ATTRIBUTES = 0x02,
	UFSHCI_DESC_MISMATCH_DATA_BUFFER_SIZE = 0x03,
	UFSHCI_DESC_MISMATCH_RESPONSE_UPIU_SIZE = 0x04,
	UFSHCI_DESC_COMMUNICATION_FAILURE_WITHIN_UIC_LAYERS = 0x05,
	UFSHCI_DESC_ABORTED = 0x06,
	UFSHCI_DESC_HOST_CONTROLLER_FATAL_ERROR = 0x07,
	UFSHCI_DESC_DEVICEFATALERROR = 0x08,
	UFSHCI_DESC_INVALID_CRYPTO_CONFIGURATION = 0x09,
	UFSHCI_DESC_GENERAL_CRYPTO_ERROR = 0x0A,
	UFSHCI_DESC_INVALID = 0x0F,
};

enum response_code {
	UFSHCI_RESPONSE_CODE_TARGET_SUCCESS = 0x00,
	UFSHCI_RESPONSE_CODE_TARGET_FAILURE = 0x01,
	UFSHCI_RESPONSE_CODE_PARAMETER_NOTREADABLE = 0xF6,
	UFSHCI_RESPONSE_CODE_PARAMETER_NOTWRITEABLE = 0xF7,
	UFSHCI_RESPONSE_CODE_PARAMETER_ALREADYWRITTEN = 0xF8,
	UFSHCI_RESPONSE_CODE_INVALID_LENGTH = 0xF9,
	UFSHCI_RESPONSE_CODE_INVALID_VALUE = 0xFA,
	UFSHCI_RESPONSE_CODE_INVALID_SELECTOR = 0xFB,
	UFSHCI_RESPONSE_CODE_INVALID_INDEX = 0xFC,
	UFSHCI_RESPONSE_CODE_INVALID_IDN = 0xFD,
	UFSHCI_RESPONSE_CODE_INVALID_OPCODE = 0xFE,
	UFSHCI_RESPONSE_CODE_GENERAL_FAILURE = 0xFF,
};

/* UFSHCI spec 4.1, section 6.1.1 "UTP Transfer Request Descriptor" */
enum ufshci_command_type {
	UFSHCI_COMMAND_TYPE_UFS_STORAGE = 0x01,
	UFSHCI_COMMAND_TYPE_NULLIFIED_UTRD = 0x0F,
};

enum ufshci_data_direction {
	UFSHCI_DATA_DIRECTION_NO_DATA_TRANSFER = 0x00,
	UFSHCI_DATA_DIRECTION_FROM_SYS_TO_TGT = 0x01,
	UFSHCI_DATA_DIRECTION_FROM_TGT_TO_SYS = 0x10,
	UFSHCI_DATA_DIRECTION_RESERVED = 0b11,
};

enum ufshci_utr_overall_command_status {
	UFSHCI_UTR_OCS_SUCCESS = 0x0,
	UFSHCI_UTR_OCS_INVALID_COMMAND_TABLE_ATTRIBUTES = 0x01,
	UFSHCI_UTR_OCS_INVALID_PRDT_ATTRIBUTES = 0x02,
	UFSHCI_UTR_OCS_MISMATCH_DATA_BUFFER_SIZE = 0x03,
	UFSHCI_UTR_OCS_MISMATCH_RESPONSE_UPIU_SIZE = 0x04,
	UFSHCI_UTR_OCS_COMMUNICATION_FAILURE_WITHIN_UIC_LAYERS = 0x05,
	UFSHCI_UTR_OCS_ABORTED = 0x06,
	UFSHCI_UTR_OCS_HOST_CONTROLLER_FATAL_ERROR = 0x07,
	UFSHCI_UTR_OCS_DEVICE_FATAL_ERROR = 0x08,
	UFSHCI_UTR_OCS_INVALID_CRYPTO_CONFIGURATION = 0x09,
	UFSHCI_UTR_OCS_GENERAL_CRYPTO_ERROR = 0x0A,
	UFSHCI_UTR_OCS_INVALID = 0xF,
};

struct ufshci_utp_xfer_req_desc {
	/* dword 0 */
	uint32_t cci : 8;	       /* [7:0] */
	uint32_t total_ehs_length : 8; /* [15:8] */
	uint32_t reserved0 : 7;	       /* [22:16] */
	uint32_t ce : 1;	       /* [23] */
	uint32_t interrupt : 1;	       /* [24] */
	uint32_t data_direction : 2;   /* [26:25] */
	uint32_t reserved1 : 1;	       /* [27] */
	uint32_t command_type : 4;     /* [31:28] */

	/* dword 1 */
	uint32_t data_unit_number_lower; /* [31:0] */

	/* dword 2 */
	uint8_t overall_command_status; /* [7:0] */
	uint8_t common_data_size;	/* [15:8] */
	uint16_t last_data_byte_count;	/* [31:16] */

	/* dword 3 */
	uint32_t data_unit_number_upper; /* [31:0] */

	/* dword 4 */
	uint32_t utp_command_descriptor_base_address; /* [31:0] */

	/* dword 5 */
	uint32_t utp_command_descriptor_base_address_upper; /* [31:0] */

	/* dword 6 */
	uint16_t response_upiu_length; /* [15:0] */
	uint16_t response_upiu_offset; /* [31:16] */

	/* dword 7 */
	uint16_t prdt_length; /* [15:0] */
	uint16_t prdt_offset; /* [31:16] */
} __packed __aligned(8);

_Static_assert(sizeof(struct ufshci_utp_xfer_req_desc) == 32,
    "ufshci_utp_xfer_req_desc must be 32 bytes");

/*
 * According to the UFSHCI specification, the size of the UTP command
 * descriptor is as follows. The size of the transfer request is not limited,
 * a transfer response can be as long as 65535 * dwords, and a PRDT can be as
 * long as 65565 * PRDT entry size(16 bytes). However, for ease of use, this
 * UFSHCI Driver imposes the following limits. The size of the transfer
 * request and the transfer response is 1024 bytes or less. The PRDT region
 * limits the number of scatter gathers to 256 + 1, using a total of 4096 +
 * 16 bytes. Therefore, only 8KB size is allocated for the UTP command
 * descriptor.
 */
#define UFSHCI_UTP_COMMAND_DESCRIPTOR_SIZE 8192
#define UFSHCI_UTP_XFER_REQ_SIZE	   512
#define UFSHCI_UTP_XFER_RESP_SIZE	   512

/*
 * To reduce the size of the UTP Command Descriptor(8KB), we must use only
 * 256 + 1 PRDT entries. The reason for adding the 1 is that if the data is
 * not aligned, one additional PRDT_ENTRY is used.
 */
#define UFSHCI_MAX_PRDT_ENTRY_COUNT (256 + 1)

/* UFSHCI spec 4.1, section 6.1.2 "UTP Command Descriptor" */
struct ufshci_prdt_entry {
	/* dword 0 */
	uint32_t data_base_address; /* [31:0] */

	/* dword 1 */
	uint32_t data_base_address_upper; /* [31:0] */

	/* dword 2 */
	uint32_t reserved; /* [31:0] */

	/* dword 3 */
	uint32_t data_byte_count; /* [17:0] Maximum byte
				   * count is 256KB */
} __packed __aligned(8);

_Static_assert(sizeof(struct ufshci_prdt_entry) == 16,
    "ufshci_prdt_entry must be 16 bytes");

struct ufshci_utp_cmd_desc {
	uint8_t command_upiu[UFSHCI_UTP_XFER_REQ_SIZE];
	uint8_t response_upiu[UFSHCI_UTP_XFER_RESP_SIZE];
	uint8_t prd_table[sizeof(struct ufshci_prdt_entry) *
	    UFSHCI_MAX_PRDT_ENTRY_COUNT];
	uint8_t padding[3072 - sizeof(struct ufshci_prdt_entry)];
} __packed __aligned(128);

_Static_assert(sizeof(struct ufshci_utp_cmd_desc) ==
	UFSHCI_UTP_COMMAND_DESCRIPTOR_SIZE,
    "ufshci_utp_cmd_desc must be 8192 bytes");

#define UFSHCI_UTP_TASK_MGMT_REQ_SIZE  32
#define UFSHCI_UTP_TASK_MGMT_RESP_SIZE 32

enum ufshci_utmr_overall_command_status {
	UFSHCI_UTMR_OCS_SUCCESS = 0x0,
	UFSHCI_UTMR_OCS_INVALID_TASK_MANAGEMENT_FUNCTION_ATTRIBUTES = 0x01,
	UFSHCI_UTMR_OCS_MISMATCH_TASK_MANAGEMENT_REQUEST_SIZE = 0x02,
	UFSHCI_UTMR_OCS_MISMATCH_TASK_MANAGEMENT_RESPONSE_SIZE = 0x03,
	UFSHCI_UTMR_OCS_PEER_COMMUNICATION_FAILURE = 0x04,
	UFSHCI_UTMR_OCS_ABORTED = 0x05,
	UFSHCI_UTMR_OCS_FATAL_ERROR = 0x06,
	UFSHCI_UTMR_OCS_DEVICE_FATAL_ERROR = 0x07,
	UFSHCI_UTMR_OCS_INVALID = 0xF,
};

/* UFSHCI spec 4.1, section 6.3.1 "UTP Task Management Request Descriptor" */
struct ufshci_utp_task_mgmt_req_desc {
	/* dword 0 */
	uint32_t reserved0 : 24; /* [23:0] */
	uint32_t interrupt : 1;	 /* [24] */
	uint32_t reserved1 : 7;	 /* [31:25] */

	/* dword 1 */
	uint32_t reserved2; /* [31:0] */

	/* dword 2 */
	uint8_t overall_command_status; /* [7:0] */
	uint8_t reserved3;		/* [15:8] */
	uint16_t reserved4;		/* [31:16] */

	/* dword 3 */
	uint32_t reserved5; /* [31:0] */

	/* dword 4-11 */
	uint8_t request_upiu[UFSHCI_UTP_TASK_MGMT_REQ_SIZE];

	/* dword 12-19 */
	uint8_t response_upiu[UFSHCI_UTP_TASK_MGMT_RESP_SIZE];

} __packed __aligned(8);

_Static_assert(sizeof(struct ufshci_utp_task_mgmt_req_desc) == 80,
    "ufshci_utp_task_mgmt_req_desc must be 80 bytes");

/* UFS spec 4.1, section 10.6.2 "Basic Header Format" */
struct ufshci_upiu_header {
	/* dword 0 */
	union {
		struct {
			uint8_t trans_code : 6; /* [5:0] */
			uint8_t dd : 1;		/* [6] */
			uint8_t hd : 1;		/* [7] */
		};
		uint8_t trans_type;
	};
	union {
		struct {
			uint8_t task_attribute : 2;	  /* [1:0] */
			uint8_t cp : 1;			  /* [2] */
			uint8_t retransmit_indicator : 1; /* [3] */
#define UFSHCI_OPERATIONAL_FLAG_W 0x2
#define UFSHCI_OPERATIONAL_FLAG_R 0x4
			uint8_t operational_flags : 4; /* [7:4] */
		};
		uint8_t flags;
	};
	uint8_t lun;
	uint8_t task_tag;

	/* dword 1 */
#define UFSHCI_COMMAND_SET_TYPE_SCSI 0
	uint8_t cmd_set_type : 4; /* [3:0] */
	uint8_t iid : 4;	  /* [7:4] */
	uint8_t ext_iid_or_function;
	uint8_t response;
	uint8_t ext_iid_or_status;

	/* dword 2 */
	uint8_t ehs_length;
	uint8_t device_infomation;
	uint16_t data_segment_length; /* (Big-endian) */
} __packed __aligned(4);

_Static_assert(sizeof(struct ufshci_upiu_header) == 12,
    "ufshci_upiu_header must be 12 bytes");

#define UFSHCI_MAX_UPIU_SIZE  512
#define UFSHCI_UPIU_ALIGNMENT 8 /* UPIU requires 64-bit alignment. */

struct ufshci_upiu {
	/* dword 0-2 */
	struct ufshci_upiu_header header;
	/* dword 3-127 */
	uint8_t
	    reserved[UFSHCI_MAX_UPIU_SIZE - sizeof(struct ufshci_upiu_header)];
} __packed __aligned(8);

_Static_assert(sizeof(struct ufshci_upiu) == 512,
    "ufshci_upiu must be 512 bytes");

/* UFS Spec 4.1, section 10.7.1 "COMMAND UPIU" */
struct ufshci_cmd_command_upiu {
	/* dword 0-2 */
	struct ufshci_upiu_header header;
	/* dword 3 */
	uint32_t expected_data_transfer_length; /* (Big-endian) */

	/* dword 4-7 */
	uint8_t cdb[16];

} __packed __aligned(4);

_Static_assert(sizeof(struct ufshci_cmd_command_upiu) == 32,
    "bad size for ufshci_cmd_command_upiu");
_Static_assert(sizeof(struct ufshci_cmd_command_upiu) <=
	UFSHCI_UTP_XFER_REQ_SIZE,
    "bad size for ufshci_cmd_command_upiu");
_Static_assert(sizeof(struct ufshci_cmd_command_upiu) % UFSHCI_UPIU_ALIGNMENT ==
	0,
    "UPIU requires 64-bit alignment");

/* UFS Spec 4.1, section 10.7.2 "RESPONSE UPIU" */
struct ufshci_cmd_response_upiu {
	/* dword 0-2 */
	struct ufshci_upiu_header header;
	/* dword 3 */
	uint32_t residual_transfer_count; /* (Big-endian) */

	/* dword 4-7 */
	uint8_t reserved[16];

	/* Sense Data */
	uint16_t sense_data_len; /* (Big-endian) */
	uint8_t sense_data[18];

	/* Add padding to align the kUpiuAlignment. */
	uint8_t padding[4];
} __packed __aligned(4);

_Static_assert(sizeof(struct ufshci_cmd_response_upiu) == 56,
    "bad size for ufshci_cmd_response_upiu");
_Static_assert(sizeof(struct ufshci_cmd_response_upiu) <=
	UFSHCI_UTP_XFER_RESP_SIZE,
    "bad size for ufshci_cmd_response_upiu");
_Static_assert(sizeof(struct ufshci_cmd_response_upiu) %
	    UFSHCI_UPIU_ALIGNMENT ==
	0,
    "UPIU requires 64-bit alignment");

enum task_management_function {
	UFSHCI_TASK_MGMT_FUNCTION_ABORT_TASK = 0x01,
	UFSHCI_TASK_MGMT_FUNCTION_ABORT_TASK_SET = 0x02,
	UFSHCI_TASK_MGMT_FUNCTION_CLEAR_TASK_SET = 0x04,
	UFSHCI_TASK_MGMT_FUNCTION_LOGICAL_UNIT_RESET = 0x08,
	UFSHCI_TASK_MGMT_FUNCTION_QUERY_TASK = 0x80,
	UFSHCI_TASK_MGMT_FUNCTION_QUERY_TASKSET = 0x81,
};

/* UFS Spec 4.1, section 10.7.6 "TASK MANAGEMENT REQUEST UPIU" */
struct ufshci_task_mgmt_request_upiu {
	/* dword 0-2 */
	struct ufshci_upiu_header header;
	/* dword 3 */
	uint32_t input_param1; /* (Big-endian) */
	/* dword 4 */
	uint32_t input_param2; /* (Big-endian) */
	/* dword 5 */
	uint32_t input_param3; /* (Big-endian) */
	/* dword 6-7 */
	uint8_t reserved[8];
} __packed __aligned(4);

_Static_assert(sizeof(struct ufshci_task_mgmt_request_upiu) == 32,
    "bad size for ufshci_task_mgmt_request_upiu");
_Static_assert(sizeof(struct ufshci_task_mgmt_request_upiu) <=
	UFSHCI_UTP_XFER_RESP_SIZE,
    "bad size for ufshci_task_mgmt_request_upiu");
_Static_assert(sizeof(struct ufshci_task_mgmt_request_upiu) %
	    UFSHCI_UPIU_ALIGNMENT ==
	0,
    "UPIU requires 64-bit alignment");

enum task_management_service_response {
	UFSHCI_TASK_MGMT_SERVICE_RESPONSE_FUNCTION_COMPLETE = 0x00,
	UFSHCI_TASK_MGMT_SERVICE_RESPONSE_FUNCTION_NOT_SUPPORTED = 0x04,
	UFSHCI_TASK_MGMT_SERVICE_RESPONSE_FUNCTION_FAILED = 0x05,
	UFSHCI_TASK_MGMT_SERVICE_RESPONSE_FUNCTION_SUCCEEDED = 0x08,
	UFSHCI_TASK_MGMT_SERVICE_RESPONSE_INCORRECT_LUN = 0x09,
};

/* UFS Spec 4.1, section 10.7.7 "TASK MANAGEMENT RESPONSE UPIU" */
struct ufshci_task_mgmt_response_upiu {
	/* dword 0-2 */
	struct ufshci_upiu_header header;
	/* dword 3 */
	uint32_t output_param1; /* (Big-endian) */
	/* dword 4 */
	uint32_t output_param2; /* (Big-endian) */
	/* dword 5-7 */
	uint8_t reserved[12];
} __packed __aligned(4);

_Static_assert(sizeof(struct ufshci_task_mgmt_response_upiu) == 32,
    "bad size for ufshci_task_mgmt_response_upiu");
_Static_assert(sizeof(struct ufshci_task_mgmt_response_upiu) <=
	UFSHCI_UTP_XFER_RESP_SIZE,
    "bad size for ufshci_task_mgmt_response_upiu");
_Static_assert(sizeof(struct ufshci_task_mgmt_response_upiu) %
	    UFSHCI_UPIU_ALIGNMENT ==
	0,
    "UPIU requires 64-bit alignment");

/* UFS Spec 4.1, section 10.7.8 "QUERY REQUEST UPIU" */
enum ufshci_query_function {
	UFSHCI_QUERY_FUNC_STANDARD_READ_REQUEST = 0x01,
	UFSHCI_QUERY_FUNC_STANDARD_WRITE_REQUEST = 0x81,
};

enum ufshci_query_opcode {
	UFSHCI_QUERY_OPCODE_NOP = 0,
	UFSHCI_QUERY_OPCODE_READ_DESCRIPTOR,
	UFSHCI_QUERY_OPCODE_WRITE_DESCRIPTOR,
	UFSHCI_QUERY_OPCODE_READ_ATTRIBUTE,
	UFSHCI_QUERY_OPCODE_WRITE_ATTRIBUTE,
	UFSHCI_QUERY_OPCODE_READ_FLAG,
	UFSHCI_QUERY_OPCODE_SET_FLAG,
	UFSHCI_QUERY_OPCODE_CLEAR_FLAG,
	UFSHCI_QUERY_OPCODE_TOGGLE_FLAG,
};

struct ufshci_query_param {
	enum ufshci_query_function function;
	enum ufshci_query_opcode opcode;
	uint8_t type;
	uint8_t index;
	uint8_t selector;
	uint64_t value;
	size_t desc_size;
};

struct ufshci_query_request_upiu {
	/* dword 0-2 */
	struct ufshci_upiu_header header;
	/* dword 3 */
	uint8_t opcode;
	uint8_t idn;
	uint8_t index;
	uint8_t selector;

	/* dword 4-5 */
	union {
		/* The Write Attribute opcode uses 64 - bit value. */
		uint64_t value_64; /* (Big-endian) */
		struct {
			uint8_t reserved1[2];
			uint16_t length;   /* (Big-endian) */
			uint32_t value_32; /* (Big-endian) */
		};
	} __packed __aligned(4);

	/* dword 6 */
	uint32_t reserved2;

	/* dword 7 */
	uint32_t reserved3;

	uint8_t command_data[256];
} __packed __aligned(4);

_Static_assert(sizeof(struct ufshci_query_request_upiu) == 288,
    "bad size for ufshci_query_request_upiu");
_Static_assert(sizeof(struct ufshci_query_request_upiu) <=
	UFSHCI_UTP_XFER_REQ_SIZE,
    "bad size for ufshci_query_request_upiu");
_Static_assert(sizeof(struct ufshci_query_request_upiu) %
	    UFSHCI_UPIU_ALIGNMENT ==
	0,
    "UPIU requires 64-bit alignment");

/* UFS Spec 4.1, section 10.7.9 "QUERY RESPONSE UPIU" */
enum ufshci_query_response_code {
	UFSHCI_QUERY_RESP_CODE_SUCCESS = 0x00,
	UFSHCI_QUERY_RESP_CODE_PARAMETER_NOT_READABLE = 0xf6,
	UFSHCI_QUERY_RESP_CODE_PARAMETER_NOT_WRITEABLE = 0xf7,
	UFSHCI_QUERY_RESP_CODE_PARAMETER_ALREADY_WRITTEN = 0xf8,
	UFSHCI_QUERY_RESP_CODE_INVALID_LENGTH = 0xf9,
	UFSHCI_QUERY_RESP_CODE_INVALID_VALUE = 0xfa,
	UFSHCI_QUERY_RESP_CODE_INVALID_SELECTOR = 0xfb,
	UFSHCI_QUERY_RESP_CODE_INVALID_INDEX = 0xfc,
	UFSHCI_QUERY_RESP_CODE_INVALID_IDN = 0xfd,
	UFSHCI_QUERY_RESP_CODE_INVALID_OPCODE = 0xfe,
	UFSHCI_QUERY_RESP_CODE_GENERAL_FAILURE = 0xff,
};

struct ufshci_query_response_upiu {
	/* dword 0-2 */
	struct ufshci_upiu_header header;
	/* dword 3 */
	uint8_t opcode;
	uint8_t idn;
	uint8_t index;
	uint8_t selector;

	/* dword 4-5 */
	union {
		/* The Read / Write Attribute opcodes use 64 - bit value. */
		uint64_t value_64; /* (Big-endian) */
		struct {
			uint8_t reserved1[2];
			uint16_t length; /* (Big-endian) */
			union {
				uint32_t value_32; /* (Big-endian) */
				struct {
					uint8_t reserved2[3];
					uint8_t flag_value;
				};
			};
		};
	} __packed __aligned(4);

	/* dword 6 */
	uint8_t reserved3[4];

	/* dword 7 */
	uint8_t reserved4[4];

	uint8_t command_data[256];
} __packed __aligned(4);

_Static_assert(sizeof(struct ufshci_query_response_upiu) == 288,
    "bad size for ufshci_query_response_upiu");
_Static_assert(sizeof(struct ufshci_query_response_upiu) <=
	UFSHCI_UTP_XFER_RESP_SIZE,
    "bad size for ufshci_query_response_upiu");
_Static_assert(sizeof(struct ufshci_query_response_upiu) %
	    UFSHCI_UPIU_ALIGNMENT ==
	0,
    "UPIU requires 64-bit alignment");

/* UFS 4.1, section 10.7.11 "NOP OUT UPIU" */
struct ufshci_nop_out_upiu {
	/* dword 0-2 */
	struct ufshci_upiu_header header;
	/* dword 3-7 */
	uint8_t reserved[20];
} __packed __aligned(8);
_Static_assert(sizeof(struct ufshci_nop_out_upiu) == 32,
    "ufshci_upiu_nop_out must be 32 bytes");

/* UFS 4.1, section 10.7.12 "NOP IN UPIU" */
struct ufshci_nop_in_upiu {
	/* dword 0-2 */
	struct ufshci_upiu_header header;
	/* dword 3-7 */
	uint8_t reserved[20];
} __packed __aligned(8);
_Static_assert(sizeof(struct ufshci_nop_in_upiu) == 32,
    "ufshci_upiu_nop_in must be 32 bytes");

union ufshci_reponse_upiu {
	struct ufshci_upiu_header header;
	struct ufshci_cmd_response_upiu cmd_response_upiu;
	struct ufshci_query_response_upiu query_response_upiu;
	struct ufshci_task_mgmt_response_upiu task_mgmt_response_upiu;
	struct ufshci_nop_in_upiu nop_in_upiu;
};

struct ufshci_completion {
	union ufshci_reponse_upiu response_upiu;
	size_t size;
};

typedef void (*ufshci_cb_fn_t)(void *, const struct ufshci_completion *, bool);

/*
 * UFS Spec 4.1, section 14.1 "UFS Descriptors"
 * All descriptors use big-endian byte ordering.
 */
enum ufshci_descriptor_type {
	UFSHCI_DESC_TYPE_DEVICE = 0x00,
	UFSHCI_DESC_TYPE_CONFIGURATION = 0x01,
	UFSHCI_DESC_TYPE_UNIT = 0x02,
	UFSHCI_DESC_TYPE_INTERCONNECT = 0x04,
	UFSHCI_DESC_TYPE_STRING = 0x05,
	UFSHCI_DESC_TYPE_GEOMETRY = 0X07,
	UFSHCI_DESC_TYPE_POWER = 0x08,
	UFSHCI_DESC_TYPE_DEVICE_HEALTH = 0x09,
	UFSHCI_DESC_TYPE_FBO_EXTENSION_SPECIFICATION = 0x0a,
};

/*
 * UFS Spec 4.1, section 14.1.5.2 "Device Descriptor"
 * DeviceDescriptor use big-endian byte ordering.
 */
struct ufshci_device_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorIDN;
	uint8_t bDevice;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bProtocol;
	uint8_t bNumberLU;
	uint8_t bNumberWLU;
	uint8_t bBootEnable;
	uint8_t bDescrAccessEn;
	uint8_t bInitPowerMode;
	uint8_t bHighPriorityLUN;
	uint8_t bSecureRemovalType;
	uint8_t bSecurityLU;
	uint8_t bBackgroundOpsTermLat;
	uint8_t bInitActiveICCLevel;
	/* 0x10 */
	uint16_t wSpecVersion;
	uint16_t wManufactureDate;
	uint8_t iManufacturerName;
	uint8_t iProductName;
	uint8_t iSerialNumber;
	uint8_t iOemID;
	uint16_t wManufacturerID;
	uint8_t bUD0BaseOffset;
	uint8_t bUDConfigPLength;
	uint8_t bDeviceRTTCap;
	uint16_t wPeriodicRTCUpdate;
	uint8_t bUfsFeaturesSupport;
	/* 0x20 */
	uint8_t bFFUTimeout;
	uint8_t bQueueDepth;
	uint16_t wDeviceVersion;
	uint8_t bNumSecureWPArea;
	uint32_t dPSAMaxDataSize;
	uint8_t bPSAStateTimeout;
	uint8_t iProductRevisionLevel;
	uint8_t Reserved[5];
	/* 0x2a */
	/* 0x30 */
	uint8_t ReservedUME[16];
	/* 0x40 */
	uint8_t ReservedHpb[3];
	uint8_t Reserved2[12];
	uint32_t dExtendedUfsFeaturesSupport;
	uint8_t bWriteBoosterBufferPreserveUserSpaceEn;
	uint8_t bWriteBoosterBufferType;
	uint32_t dNumSharedWriteBoosterBufferAllocUnits;
} __packed;

_Static_assert(sizeof(struct ufshci_device_descriptor) == 89,
    "bad size for ufshci_device_descriptor");

/* Defines the bit field of dExtendedUfsFeaturesSupport. */
enum ufshci_desc_wb_ext_ufs_feature {
	UFSHCI_DESC_EXT_UFS_FEATURE_FFU = (1 << 0),
	UFSHCI_DESC_EXT_UFS_FEATURE_PSA = (1 << 1),
	UFSHCI_DESC_EXT_UFS_FEATURE_DEV_LIFE_SPAN = (1 << 2),
	UFSHCI_DESC_EXT_UFS_FEATURE_REFRESH_OP = (1 << 3),
	UFSHCI_DESC_EXT_UFS_FEATURE_TOO_HIGH_TEMP = (1 << 4),
	UFSHCI_DESC_EXT_UFS_FEATURE_TOO_LOW_TEMP = (1 << 5),
	UFSHCI_DESC_EXT_UFS_FEATURE_EXT_TEMP = (1 << 6),
	UFSHCI_DESC_EXT_UFS_FEATURE_HPB_SUPPORT = (1 << 7),
	UFSHCI_DESC_EXT_UFS_FEATURE_WRITE_BOOSTER = (1 << 8),
	UFSHCI_DESC_EXT_UFS_FEATURE_PERF_THROTTLING = (1 << 9),
	UFSHCI_DESC_EXT_UFS_FEATURE_ADVANCED_RPMB = (1 << 10),
	UFSHCI_DESC_EXT_UFS_FEATURE_ZONED_UFS_EXTENSION = (1 << 11),
	UFSHCI_DESC_EXT_UFS_FEATURE_DEV_LEVEL_EXCEPTION = (1 << 12),
	UFSHCI_DESC_EXT_UFS_FEATURE_HID = (1 << 13),
	UFSHCI_DESC_EXT_UFS_FEATURE_BARRIER = (1 << 14),
	UFSHCI_DESC_EXT_UFS_FEATURE_CLEAR_ERROR_HISTORY = (1 << 15),
	UFSHCI_DESC_EXT_UFS_FEATURE_EXT_IID = (1 << 16),
	UFSHCI_DESC_EXT_UFS_FEATURE_FBO = (1 << 17),
	UFSHCI_DESC_EXT_UFS_FEATURE_FAST_RECOVERY_MODE = (1 << 18),
	UFSHCI_DESC_EXT_UFS_FEATURE_RPMB_VENDOR_CMD = (1 << 19),
};

/* Defines the bit field of bWriteBoosterBufferType. */
enum ufshci_desc_wb_buffer_type {
	UFSHCI_DESC_WB_BUF_TYPE_LU_DEDICATED = 0x00,
	UFSHCI_DESC_WB_BUF_TYPE_SINGLE_SHARED = 0x01,
};

/* Defines the bit field of bWriteBoosterBufferPreserveUserSpaceEn. */
enum ufshci_desc_user_space_config {
	UFSHCI_DESC_WB_BUF_USER_SPACE_REDUCTION = 0x00,
	UFSHCI_DESC_WB_BUF_PRESERVE_USER_SPACE = 0x01,
};

/*
 * UFS Spec 4.1, section 14.1.5.3 "Configuration Descriptor"
 * ConfigurationDescriptor use big-endian byte ordering.
 */
struct ufshci_unit_descriptor_configurable_parameters {
	uint8_t bLUEnable;
	uint8_t bBootLunID;
	uint8_t bLUWriteProtect;
	uint8_t bMemoryType;
	uint32_t dNumAllocUnits;
	uint8_t bDataReliability;
	uint8_t bLogicalBlockSize;
	uint8_t bProvisioningType;
	uint16_t wContextCapabilities;
	union {
		struct {
			uint8_t Reserved[3];
			uint8_t ReservedHpb[6];
		} __packed;
		uint16_t wZoneBufferAllocUnits;
	};
	uint32_t dLUNumWriteBoosterBufferAllocUnits;
} __packed;

_Static_assert(sizeof(struct ufshci_unit_descriptor_configurable_parameters) ==
	27,
    "bad size for ufshci_unit_descriptor_configurable_parameters");

#define UFSHCI_CONFIGURATION_DESCEIPTOR_LU_NUM 8

struct ufshci_configuration_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorIDN;
	uint8_t bConfDescContinue;
	uint8_t bBootEnable;
	uint8_t bDescrAccessEn;
	uint8_t bInitPowerMode;
	uint8_t bHighPriorityLUN;
	uint8_t bSecureRemovalType;
	uint8_t bInitActiveICCLevel;
	uint16_t wPeriodicRTCUpdate;
	uint8_t Reserved;
	uint8_t bRPMBRegionEnable;
	uint8_t bRPMBRegion1Size;
	uint8_t bRPMBRegion2Size;
	uint8_t bRPMBRegion3Size;
	uint8_t bWriteBoosterBufferPreserveUserSpaceEn;
	uint8_t bWriteBoosterBufferType;
	uint32_t dNumSharedWriteBoosterBufferAllocUnits;
	/* 0x16 */
	struct ufshci_unit_descriptor_configurable_parameters
	    unit_config_params[UFSHCI_CONFIGURATION_DESCEIPTOR_LU_NUM];
} __packed;

_Static_assert(sizeof(struct ufshci_configuration_descriptor) == (22 + 27 * 8),
    "bad size for ufshci_configuration_descriptor");

/*
 * UFS Spec 4.1, section 14.1.5.4 "Geometry Descriptor"
 * GeometryDescriptor use big-endian byte ordering.
 */
struct ufshci_geometry_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorIDN;
	uint8_t bMediaTechnology;
	uint8_t Reserved;
	uint64_t qTotalRawDeviceCapacity;
	uint8_t bMaxNumberLU;
	uint32_t dSegmentSize;
	/* 0x11 */
	uint8_t bAllocationUnitSize;
	uint8_t bMinAddrBlockSize;
	uint8_t bOptimalReadBlockSize;
	uint8_t bOptimalWriteBlockSize;
	uint8_t bMaxInBufferSize;
	uint8_t bMaxOutBufferSize;
	uint8_t bRPMB_ReadWriteSize;
	uint8_t bDynamicCapacityResourcePolicy;
	uint8_t bDataOrdering;
	uint8_t bMaxContexIDNumber;
	uint8_t bSysDataTagUnitSize;
	uint8_t bSysDataTagResSize;
	uint8_t bSupportedSecRTypes;
	uint16_t wSupportedMemoryTypes;
	/* 0x20 */
	uint32_t dSystemCodeMaxNAllocU;
	uint16_t wSystemCodeCapAdjFac;
	uint32_t dNonPersistMaxNAllocU;
	uint16_t wNonPersistCapAdjFac;
	uint32_t dEnhanced1MaxNAllocU;
	/* 0x30 */
	uint16_t wEnhanced1CapAdjFac;
	uint32_t dEnhanced2MaxNAllocU;
	uint16_t wEnhanced2CapAdjFac;
	uint32_t dEnhanced3MaxNAllocU;
	uint16_t wEnhanced3CapAdjFac;
	uint32_t dEnhanced4MaxNAllocU;
	/* 0x42 */
	uint16_t wEnhanced4CapAdjFac;
	uint32_t dOptimalLogicalBlockSize;
	uint8_t ReservedHpb[5];
	uint8_t Reserved2[2];
	uint32_t dWriteBoosterBufferMaxNAllocUnits;
	uint8_t bDeviceMaxWriteBoosterLUs;
	uint8_t bWriteBoosterBufferCapAdjFac;
	uint8_t bSupportedWriteBoosterBufferUserSpaceReductionTypes;
	uint8_t bSupportedWriteBoosterBufferTypes;
} __packed;

_Static_assert(sizeof(struct ufshci_geometry_descriptor) == 87,
    "bad size for ufshci_geometry_descriptor");

/*
 * UFS Spec 4.1, section 14.1.5.5 "Unit Descriptor"
 * UnitDescriptor use big-endian byte ordering.
 */
struct ufshci_unit_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorIDN;
	uint8_t bUnitIndex;
	uint8_t bLUEnable;
	uint8_t bBootLunID;
	uint8_t bLUWriteProtect;
	uint8_t bLUQueueDepth;
	uint8_t bPSASensitive;
	uint8_t bMemoryType;
	uint8_t bDataReliability;
	uint8_t bLogicalBlockSize;
	uint64_t qLogicalBlockCount;
	/* 0x13 */
	uint32_t dEraseBlockSize;
	uint8_t bProvisioningType;
	uint64_t qPhyMemResourceCount;
	/* 0x20 */
	uint16_t wContextCapabilities;
	uint8_t bLargeUnitGranularity_M1;
	uint8_t ReservedHpb[6];
	uint32_t dLUNumWriteBoosterBufferAllocUnits;
} __packed;
_Static_assert(sizeof(struct ufshci_unit_descriptor) == 45,
    "bad size for ufshci_unit_descriptor");

enum LUWriteProtect {
	kNoWriteProtect = 0x00,
	kPowerOnWriteProtect = 0x01,
	kPermanentWriteProtect = 0x02,
};

/*
 * UFS Spec 4.1, section 14.1.5.6 "RPMB Unit Descriptor"
 * RpmbUnitDescriptor use big-endian byte ordering.
 */
struct ufshci_rpmb_unit_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorIDN;
	uint8_t bUnitIndex;
	uint8_t bLUEnable;
	uint8_t bBootLunID;
	uint8_t bLUWriteProtect;
	uint8_t bLUQueueDepth;
	uint8_t bPSASensitive;
	uint8_t bMemoryType;
	uint8_t Reserved;
	uint8_t bLogicalBlockSize;
	uint64_t qLogicalBlockCount;
	/* 0x13 */
	uint32_t dEraseBlockSize;
	uint8_t bProvisioningType;
	uint64_t qPhyMemResourceCount;
	/* 0x20 */
	uint8_t Reserved1[3];
} __packed;
_Static_assert(sizeof(struct ufshci_rpmb_unit_descriptor) == 35,
    "bad size for RpmbUnitDescriptor");

/*
 * UFS Spec 4.1, section 14.1.5.7 "Power Parameters Descriptor"
 * PowerParametersDescriptor use big-endian byte ordering.
 */
struct ufshci_power_parameters_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorIDN;
	uint16_t wActiveICCLevelsVCC[16];
	uint16_t wActiveICCLevelsVCCQ[16];
	uint16_t wActiveICCLevelsVCCQ2[16];
} __packed;
_Static_assert(sizeof(struct ufshci_power_parameters_descriptor) == 98,
    "bad size for PowerParametersDescriptor");

/*
 * UFS Spec 4.1, section 14.1.5.8 "Interconnect Descriptor"
 * InterconnectDescriptor use big-endian byte ordering.
 */
struct ufshci_interconnect_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorIDN;
	uint16_t bcdUniproVersion;
	uint16_t bcdMphyVersion;
} __packed;
_Static_assert(sizeof(struct ufshci_interconnect_descriptor) == 6,
    "bad size for InterconnectDescriptor");

/*
 * UFS Spec 4.1, section 14.1.5.9-13 "String Descriptor"
 * StringDescriptor use big-endian byte ordering.
 */
struct ufshci_string_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorIDN;
	uint16_t UC[126];
} __packed;
_Static_assert(sizeof(struct ufshci_string_descriptor) == 254,
    "bad size for StringDescriptor");

/*
 * UFS Spec 4.1, section 14.1.5.14 "Device Health Descriptor"
 * DeviceHealthDescriptor use big-endian byte ordering.
 */
struct ufshci_device_healthd_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorIDN;
	uint8_t bPreEOLInfo;
	uint8_t bDeviceLifeTimeEstA;
	uint8_t bDeviceLifeTimeEstB;
	uint8_t VendorPropInfo[32];
	uint32_t dRefreshTotalCount;
	uint32_t dRefreshProgress;
} __packed;
_Static_assert(sizeof(struct ufshci_device_healthd_descriptor) == 45,
    "bad size for DeviceHealthDescriptor");

/*
 * UFS Spec 4.1, section 14.1.5.15 "Vendor Specific Descriptor"
 * VendorSpecificDescriptor use big-endian byte ordering.
 */
struct ufshci_vendor_specific_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorIDN;
	uint8_t DATA[254];
} __packed;
_Static_assert(sizeof(struct ufshci_vendor_specific_descriptor) == 256,
    "bad size for VendorSpecificDescriptor");

/* UFS Spec 4.1, section 14.2 "Flags" */
enum ufshci_flags {
	UFSHCI_FLAG_F_RESERVED = 0x00,
	UFSHCI_FLAG_F_DEVICE_INIT = 0x01,
	UFSHCI_FLAG_F_PERMANENT_WP_EN = 0x02,
	UFSHCI_FLAS_F_POWER_ON_WP_EN = 0x03,
	UFSHCI_FLAG_F_BACKGROUND_OPS_EN = 0x04,
	UFSHCI_FLAG_F_DEVICE_LIFE_SPAN_MODE_EN = 0x05,
	UFSHCI_FLAG_F_PURGE_ENABLE = 0x06,
	UFSHCI_FLAG_F_REFRESH_ENABLE = 0x07,
	UFSHCI_FLAG_F_PHY_RESOURCE_REMOVAL = 0x08,
	UFSHCI_FLAG_F_BUSY_RTC = 0x09,
	UFSHCI_FLAG_F_PERMANENTLY_DISABLE_FW_UPDATE = 0x0b,
	UFSHCI_FLAG_F_WRITE_BOOSTER_EN = 0x0e,
	UFSHCI_FLAG_F_WB_BUFFER_FLUSH_EN = 0x0f,
	UFSHCI_FLAG_F_WB_BUFFER_FLUSH_DURING_HIBERNATE = 0x10,
	UFSHCI_FLAG_F_UNPIN_EN = 0x13,
};

/* UFS Spec 4.1, section 14.3 "Attributes" */
enum ufshci_attributes {
	UFSHCI_ATTR_B_BOOT_LUN_EN = 0x00,
	UFSHCI_ATTR_B_CURRENT_POWER_MODE = 0x02,
	UFSHCI_ATTR_B_ACTIVE_ICC_LEVEL = 0x03,
	UFSHCI_ATTR_B_OUT_OF_ORDER_DATA_EN = 0x04,
	UFSHCI_ATTR_B_BACKGROUND_OP_STATUS = 0x05,
	UFSHCI_ATTR_B_PURGE_STATUS = 0x06,
	UFSHCI_ATTR_B_MAX_DATA_IN_SIZE = 0x07,
	UFSHCI_ATTR_B_MAX_DATA_OUT_SIZE = 0x08,
	UFSHCI_ATTR_D_DYN_CAP_NEEDED = 0x09,
	UFSHCI_ATTR_B_REF_CLK_FREQ = 0x0a,
	UFSHCI_ATTR_B_CONFIG_DESCR_LOCK = 0x0b,
	UFSHCI_ATTR_B_MAX_NUM_OF_RTT = 0x0c,
	UFSHCI_ATTR_W_EXCEPTION_EVENT_CONTROL = 0x0d,
	UFSHCI_ATTR_W_EXCEPTION_EVENT_STATUS = 0x0e,
	UFSHCI_ATTR_D_SECONDS_PASSED = 0x0f,
	UFSHCI_ATTR_W_CONTEXT_CONF = 0x10,
	UFSHCI_ATTR_B_DEVICE_FFU_STATUS = 0x14,
	UFSHCI_ATTR_B_PSA_STATE = 0x15,
	UFSHCI_ATTR_D_PSA_DATA_SIZE = 0x16,
	UFSHCI_ATTR_B_REF_CLK_GATING_WAIT_TIME = 0x17,
	UFSHCI_ATTR_B_DEVICE_CASE_ROUGH_TEMPERAURE = 0x18,
	UFSHCI_ATTR_B_DEVICE_TOO_HIGH_TEMP_BOUNDARY = 0x19,
	UFSHCI_ATTR_B_DEVICE_TOO_LOW_TEMP_BOUNDARY = 0x1a,
	UFSHCI_ATTR_B_THROTTLING_STATUS = 0x1b,
	UFSHCI_ATTR_B_WB_BUFFER_FLUSH_STATUS = 0x1c,
	UFSHCI_ATTR_B_AVAILABLE_WB_BUFFER_SIZE = 0x1d,
	UFSHCI_ATTR_B_WB_BUFFER_LIFE_TIME_EST = 0x1e,
	UFSHCI_ATTR_D_CURRENT_WB_BUFFER_SIZE = 0x1f,
	UFSHCI_ATTR_B_REFRESH_STATUS = 0x2c,
	UFSHCI_ATTR_B_REFRESH_FREQ = 0x2d,
	UFSHCI_ATTR_B_REFRESH_UNIT = 0x2e,
	UFSHCI_ATTR_B_REFRESH_METHOD = 0x2f,
};

/* bAvailableWriteBoosterBufferSize codes (UFS WriteBooster abailable buffer
 * left %) */
enum ufshci_wb_available_buffer_Size {
	UFSHCI_ATTR_WB_AVAILABLE_0 = 0x00,   /* 0% buffer remains  */
	UFSHCI_ATTR_WB_AVAILABLE_10 = 0x01,  /* 10% buffer remains  */
	UFSHCI_ATTR_WB_AVAILABLE_20 = 0x02,  /* 20% buffer remains  */
	UFSHCI_ATTR_WB_AVAILABLE_30 = 0x03,  /* 30% buffer remains  */
	UFSHCI_ATTR_WB_AVAILABLE_40 = 0x04,  /* 40% buffer remains  */
	UFSHCI_ATTR_WB_AVAILABLE_50 = 0x05,  /* 50% buffer remains  */
	UFSHCI_ATTR_WB_AVAILABLE_60 = 0x06,  /* 60% buffer remains  */
	UFSHCI_ATTR_WB_AVAILABLE_70 = 0x07,  /* 70% buffer remains  */
	UFSHCI_ATTR_WB_AVAILABLE_80 = 0x08,  /* 80% buffer remains  */
	UFSHCI_ATTR_WB_AVAILABLE_90 = 0x09,  /* 90% buffer remains  */
	UFSHCI_ATTR_WB_AVAILABLE_100 = 0x0A, /* 100% buffer remains  */
};

/* bWriteBoosterBufferLifeTimeEst codes (UFS WriteBooster buffer life %) */
enum ufshci_wb_lifetime {
	UFSHCI_ATTR_WB_LIFE_DISABLED = 0x00, /* Info not available */
	UFSHCI_ATTR_WB_LIFE_0_10 = 0x01,     /*   0%–10% used  */
	UFSHCI_ATTR_WB_LIFE_10_20 = 0x02,    /*  10%–20% used  */
	UFSHCI_ATTR_WB_LIFE_20_30 = 0x03,    /*  20%–30% used  */
	UFSHCI_ATTR_WB_LIFE_30_40 = 0x04,    /*  30%–40% used  */
	UFSHCI_ATTR_WB_LIFE_40_50 = 0x05,    /*  40%–50% used  */
	UFSHCI_ATTR_WB_LIFE_50_60 = 0x06,    /*  50%–60% used  */
	UFSHCI_ATTR_WB_LIFE_60_70 = 0x07,    /*  60%–70% used  */
	UFSHCI_ATTR_WB_LIFE_70_80 = 0x08,    /*  70%–80% used  */
	UFSHCI_ATTR_WB_LIFE_80_90 = 0x09,    /*  80%–90% used  */
	UFSHCI_ATTR_WB_LIFE_90_100 = 0x0A,   /* 90%–100% used  */
	UFSHCI_ATTR_WB_LIFE_EXCEEDED =
	    0x0B, /* Exceeded estimated life (treat as WB disabled) */
};

#endif /* __UFSHCI_H__ */
