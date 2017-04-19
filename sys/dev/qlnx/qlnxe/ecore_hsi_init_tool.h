/*
 * Copyright (c) 2017-2018 Cavium, Inc. 
 * All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */


#ifndef __ECORE_HSI_INIT_TOOL__
#define __ECORE_HSI_INIT_TOOL__ 
/**************************************/
/* Init Tool HSI constants and macros */
/**************************************/

/* Width of GRC address in bits (addresses are specified in dwords) */
#define GRC_ADDR_BITS			23
#define MAX_GRC_ADDR			((1 << GRC_ADDR_BITS) - 1)

/* indicates an init that should be applied to any phase ID */
#define ANY_PHASE_ID			0xffff

/* Max size in dwords of a zipped array */
#define MAX_ZIPPED_SIZE			8192


enum chip_ids
{
	CHIP_BB,
	CHIP_K2,
	CHIP_E5,
	MAX_CHIP_IDS
};


struct fw_asserts_ram_section
{
	__le16 section_ram_line_offset /* The offset of the section in the RAM in RAM lines (64-bit units) */;
	__le16 section_ram_line_size /* The size of the section in RAM lines (64-bit units) */;
	u8 list_dword_offset /* The offset of the asserts list within the section in dwords */;
	u8 list_element_dword_size /* The size of an assert list element in dwords */;
	u8 list_num_elements /* The number of elements in the asserts list */;
	u8 list_next_index_dword_offset /* The offset of the next list index field within the section in dwords */;
};


struct fw_ver_num
{
	u8 major /* Firmware major version number */;
	u8 minor /* Firmware minor version number */;
	u8 rev /* Firmware revision version number */;
	u8 eng /* Firmware engineering version number (for bootleg versions) */;
};

struct fw_ver_info
{
	__le16 tools_ver /* Tools version number */;
	u8 image_id /* FW image ID (e.g. main, l2b, kuku) */;
	u8 reserved1;
	struct fw_ver_num num /* FW version number */;
	__le32 timestamp /* FW Timestamp in unix time  (sec. since 1970) */;
	__le32 reserved2;
};

struct fw_info
{
	struct fw_ver_info ver /* FW version information */;
	struct fw_asserts_ram_section fw_asserts_section /* Info regarding the FW asserts section in the Storm RAM */;
};


struct fw_info_location
{
	__le32 grc_addr /* GRC address where the fw_info struct is located. */;
	__le32 size /* Size of the fw_info structure (thats located at the grc_addr). */;
};




enum init_modes
{
	MODE_BB_A0_DEPRECATED,
	MODE_BB,
	MODE_K2,
	MODE_ASIC,
	MODE_EMUL_REDUCED,
	MODE_EMUL_FULL,
	MODE_FPGA,
	MODE_CHIPSIM,
	MODE_SF,
	MODE_MF_SD,
	MODE_MF_SI,
	MODE_PORTS_PER_ENG_1,
	MODE_PORTS_PER_ENG_2,
	MODE_PORTS_PER_ENG_4,
	MODE_100G,
	MODE_E5,
	MAX_INIT_MODES
};


enum init_phases
{
	PHASE_ENGINE,
	PHASE_PORT,
	PHASE_PF,
	PHASE_VF,
	PHASE_QM_PF,
	MAX_INIT_PHASES
};


enum init_split_types
{
	SPLIT_TYPE_NONE,
	SPLIT_TYPE_PORT,
	SPLIT_TYPE_PF,
	SPLIT_TYPE_PORT_PF,
	SPLIT_TYPE_VF,
	MAX_INIT_SPLIT_TYPES
};


/*
 * Binary buffer header
 */
struct bin_buffer_hdr
{
	__le32 offset /* buffer offset in bytes from the beginning of the binary file */;
	__le32 length /* buffer length in bytes */;
};


/*
 * binary init buffer types
 */
enum bin_init_buffer_type
{
	BIN_BUF_INIT_FW_VER_INFO /* fw_ver_info struct */,
	BIN_BUF_INIT_CMD /* init commands */,
	BIN_BUF_INIT_VAL /* init data */,
	BIN_BUF_INIT_MODE_TREE /* init modes tree */,
	BIN_BUF_INIT_IRO /* internal RAM offsets */,
	MAX_BIN_INIT_BUFFER_TYPE
};


/*
 * init array header: raw
 */
struct init_array_raw_hdr
{
	__le32 data;
#define INIT_ARRAY_RAW_HDR_TYPE_MASK    0xF /* Init array type, from init_array_types enum */
#define INIT_ARRAY_RAW_HDR_TYPE_SHIFT   0
#define INIT_ARRAY_RAW_HDR_PARAMS_MASK  0xFFFFFFF /* init array params */
#define INIT_ARRAY_RAW_HDR_PARAMS_SHIFT 4
};

/*
 * init array header: standard
 */
struct init_array_standard_hdr
{
	__le32 data;
#define INIT_ARRAY_STANDARD_HDR_TYPE_MASK  0xF /* Init array type, from init_array_types enum */
#define INIT_ARRAY_STANDARD_HDR_TYPE_SHIFT 0
#define INIT_ARRAY_STANDARD_HDR_SIZE_MASK  0xFFFFFFF /* Init array size (in dwords) */
#define INIT_ARRAY_STANDARD_HDR_SIZE_SHIFT 4
};

/*
 * init array header: zipped
 */
struct init_array_zipped_hdr
{
	__le32 data;
#define INIT_ARRAY_ZIPPED_HDR_TYPE_MASK         0xF /* Init array type, from init_array_types enum */
#define INIT_ARRAY_ZIPPED_HDR_TYPE_SHIFT        0
#define INIT_ARRAY_ZIPPED_HDR_ZIPPED_SIZE_MASK  0xFFFFFFF /* Init array zipped size (in bytes) */
#define INIT_ARRAY_ZIPPED_HDR_ZIPPED_SIZE_SHIFT 4
};

/*
 * init array header: pattern
 */
struct init_array_pattern_hdr
{
	__le32 data;
#define INIT_ARRAY_PATTERN_HDR_TYPE_MASK          0xF /* Init array type, from init_array_types enum */
#define INIT_ARRAY_PATTERN_HDR_TYPE_SHIFT         0
#define INIT_ARRAY_PATTERN_HDR_PATTERN_SIZE_MASK  0xF /* pattern size in dword */
#define INIT_ARRAY_PATTERN_HDR_PATTERN_SIZE_SHIFT 4
#define INIT_ARRAY_PATTERN_HDR_REPETITIONS_MASK   0xFFFFFF /* pattern repetitions */
#define INIT_ARRAY_PATTERN_HDR_REPETITIONS_SHIFT  8
};

/*
 * init array header union
 */
union init_array_hdr
{
	struct init_array_raw_hdr raw /* raw init array header */;
	struct init_array_standard_hdr standard /* standard init array header */;
	struct init_array_zipped_hdr zipped /* zipped init array header */;
	struct init_array_pattern_hdr pattern /* pattern init array header */;
};





/*
 * init array types
 */
enum init_array_types
{
	INIT_ARR_STANDARD /* standard init array */,
	INIT_ARR_ZIPPED /* zipped init array */,
	INIT_ARR_PATTERN /* a repeated pattern */,
	MAX_INIT_ARRAY_TYPES
};



/*
 * init operation: callback
 */
struct init_callback_op
{
	__le32 op_data;
#define INIT_CALLBACK_OP_OP_MASK        0xF /* Init operation, from init_op_types enum */
#define INIT_CALLBACK_OP_OP_SHIFT       0
#define INIT_CALLBACK_OP_RESERVED_MASK  0xFFFFFFF
#define INIT_CALLBACK_OP_RESERVED_SHIFT 4
	__le16 callback_id /* Callback ID */;
	__le16 block_id /* Blocks ID */;
};


/*
 * init operation: delay
 */
struct init_delay_op
{
	__le32 op_data;
#define INIT_DELAY_OP_OP_MASK        0xF /* Init operation, from init_op_types enum */
#define INIT_DELAY_OP_OP_SHIFT       0
#define INIT_DELAY_OP_RESERVED_MASK  0xFFFFFFF
#define INIT_DELAY_OP_RESERVED_SHIFT 4
	__le32 delay /* delay in us */;
};


/*
 * init operation: if_mode
 */
struct init_if_mode_op
{
	__le32 op_data;
#define INIT_IF_MODE_OP_OP_MASK          0xF /* Init operation, from init_op_types enum */
#define INIT_IF_MODE_OP_OP_SHIFT         0
#define INIT_IF_MODE_OP_RESERVED1_MASK   0xFFF
#define INIT_IF_MODE_OP_RESERVED1_SHIFT  4
#define INIT_IF_MODE_OP_CMD_OFFSET_MASK  0xFFFF /* Commands to skip if the modes dont match */
#define INIT_IF_MODE_OP_CMD_OFFSET_SHIFT 16
	__le16 reserved2;
	__le16 modes_buf_offset /* offset (in bytes) in modes expression buffer */;
};


/*
 * init operation: if_phase
 */
struct init_if_phase_op
{
	__le32 op_data;
#define INIT_IF_PHASE_OP_OP_MASK           0xF /* Init operation, from init_op_types enum */
#define INIT_IF_PHASE_OP_OP_SHIFT          0
#define INIT_IF_PHASE_OP_DMAE_ENABLE_MASK  0x1 /* Indicates if DMAE is enabled in this phase */
#define INIT_IF_PHASE_OP_DMAE_ENABLE_SHIFT 4
#define INIT_IF_PHASE_OP_RESERVED1_MASK    0x7FF
#define INIT_IF_PHASE_OP_RESERVED1_SHIFT   5
#define INIT_IF_PHASE_OP_CMD_OFFSET_MASK   0xFFFF /* Commands to skip if the phases dont match */
#define INIT_IF_PHASE_OP_CMD_OFFSET_SHIFT  16
	__le32 phase_data;
#define INIT_IF_PHASE_OP_PHASE_MASK        0xFF /* Init phase */
#define INIT_IF_PHASE_OP_PHASE_SHIFT       0
#define INIT_IF_PHASE_OP_RESERVED2_MASK    0xFF
#define INIT_IF_PHASE_OP_RESERVED2_SHIFT   8
#define INIT_IF_PHASE_OP_PHASE_ID_MASK     0xFFFF /* Init phase ID */
#define INIT_IF_PHASE_OP_PHASE_ID_SHIFT    16
};


/*
 * init mode operators
 */
enum init_mode_ops
{
	INIT_MODE_OP_NOT /* init mode not operator */,
	INIT_MODE_OP_OR /* init mode or operator */,
	INIT_MODE_OP_AND /* init mode and operator */,
	MAX_INIT_MODE_OPS
};


/*
 * init operation: raw
 */
struct init_raw_op
{
	__le32 op_data;
#define INIT_RAW_OP_OP_MASK      0xF /* Init operation, from init_op_types enum */
#define INIT_RAW_OP_OP_SHIFT     0
#define INIT_RAW_OP_PARAM1_MASK  0xFFFFFFF /* init param 1 */
#define INIT_RAW_OP_PARAM1_SHIFT 4
	__le32 param2 /* Init param 2 */;
};

/*
 * init array params
 */
struct init_op_array_params
{
	__le16 size /* array size in dwords */;
	__le16 offset /* array start offset in dwords */;
};

/*
 * Write init operation arguments
 */
union init_write_args
{
	__le32 inline_val /* value to write, used when init source is INIT_SRC_INLINE */;
	__le32 zeros_count /* number of zeros to write, used when init source is INIT_SRC_ZEROS */;
	__le32 array_offset /* array offset to write, used when init source is INIT_SRC_ARRAY */;
	struct init_op_array_params runtime /* runtime array params to write, used when init source is INIT_SRC_RUNTIME */;
};

/*
 * init operation: write
 */
struct init_write_op
{
	__le32 data;
#define INIT_WRITE_OP_OP_MASK        0xF /* init operation, from init_op_types enum */
#define INIT_WRITE_OP_OP_SHIFT       0
#define INIT_WRITE_OP_SOURCE_MASK    0x7 /* init source type, taken from init_source_types enum */
#define INIT_WRITE_OP_SOURCE_SHIFT   4
#define INIT_WRITE_OP_RESERVED_MASK  0x1
#define INIT_WRITE_OP_RESERVED_SHIFT 7
#define INIT_WRITE_OP_WIDE_BUS_MASK  0x1 /* indicates if the register is wide-bus */
#define INIT_WRITE_OP_WIDE_BUS_SHIFT 8
#define INIT_WRITE_OP_ADDRESS_MASK   0x7FFFFF /* internal (absolute) GRC address, in dwords */
#define INIT_WRITE_OP_ADDRESS_SHIFT  9
	union init_write_args args /* Write init operation arguments */;
};

/*
 * init operation: read
 */
struct init_read_op
{
	__le32 op_data;
#define INIT_READ_OP_OP_MASK         0xF /* init operation, from init_op_types enum */
#define INIT_READ_OP_OP_SHIFT        0
#define INIT_READ_OP_POLL_TYPE_MASK  0xF /* polling type, from init_poll_types enum */
#define INIT_READ_OP_POLL_TYPE_SHIFT 4
#define INIT_READ_OP_RESERVED_MASK   0x1
#define INIT_READ_OP_RESERVED_SHIFT  8
#define INIT_READ_OP_ADDRESS_MASK    0x7FFFFF /* internal (absolute) GRC address, in dwords */
#define INIT_READ_OP_ADDRESS_SHIFT   9
	__le32 expected_val /* expected polling value, used only when polling is done */;
};

/*
 * Init operations union
 */
union init_op
{
	struct init_raw_op raw /* raw init operation */;
	struct init_write_op write /* write init operation */;
	struct init_read_op read /* read init operation */;
	struct init_if_mode_op if_mode /* if_mode init operation */;
	struct init_if_phase_op if_phase /* if_phase init operation */;
	struct init_callback_op callback /* callback init operation */;
	struct init_delay_op delay /* delay init operation */;
};



/*
 * Init command operation types
 */
enum init_op_types
{
	INIT_OP_READ /* GRC read init command */,
	INIT_OP_WRITE /* GRC write init command */,
	INIT_OP_IF_MODE /* Skip init commands if the init modes expression doesnt match */,
	INIT_OP_IF_PHASE /* Skip init commands if the init phase doesnt match */,
	INIT_OP_DELAY /* delay init command */,
	INIT_OP_CALLBACK /* callback init command */,
	MAX_INIT_OP_TYPES
};


/*
 * init polling types
 */
enum init_poll_types
{
	INIT_POLL_NONE /* No polling */,
	INIT_POLL_EQ /* init value is included in the init command */,
	INIT_POLL_OR /* init value is all zeros */,
	INIT_POLL_AND /* init value is an array of values */,
	MAX_INIT_POLL_TYPES
};




/*
 * init source types
 */
enum init_source_types
{
	INIT_SRC_INLINE /* init value is included in the init command */,
	INIT_SRC_ZEROS /* init value is all zeros */,
	INIT_SRC_ARRAY /* init value is an array of values */,
	INIT_SRC_RUNTIME /* init value is provided during runtime */,
	MAX_INIT_SOURCE_TYPES
};




/*
 * Internal RAM Offsets macro data
 */
struct iro
{
	__le32 base /* RAM field offset */;
	__le16 m1 /* multiplier 1 */;
	__le16 m2 /* multiplier 2 */;
	__le16 m3 /* multiplier 3 */;
	__le16 size /* RAM field size */;
};

#endif /* __ECORE_HSI_INIT_TOOL__ */
