/*-
 * Copyright (c) 2013-2020, Mellanox Technologies.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef MLX5_IFC_H
#define MLX5_IFC_H

#include <dev/mlx5/mlx5_fpga/mlx5_ifc_fpga.h>

enum {
	MLX5_EVENT_TYPE_NOTIFY_ANY				   = 0x0,
	MLX5_EVENT_TYPE_COMP                                       = 0x0,
	MLX5_EVENT_TYPE_PATH_MIG                                   = 0x1,
	MLX5_EVENT_TYPE_COMM_EST                                   = 0x2,
	MLX5_EVENT_TYPE_SQ_DRAINED                                 = 0x3,
	MLX5_EVENT_TYPE_SRQ_LAST_WQE                               = 0x13,
	MLX5_EVENT_TYPE_SRQ_RQ_LIMIT                               = 0x14,
	MLX5_EVENT_TYPE_DCT_DRAINED                                = 0x1c,
	MLX5_EVENT_TYPE_DCT_KEY_VIOLATION                          = 0x1d,
	MLX5_EVENT_TYPE_CQ_ERROR                                   = 0x4,
	MLX5_EVENT_TYPE_WQ_CATAS_ERROR                             = 0x5,
	MLX5_EVENT_TYPE_PATH_MIG_FAILED                            = 0x7,
	MLX5_EVENT_TYPE_PAGE_FAULT                                 = 0xc,
	MLX5_EVENT_TYPE_WQ_INVAL_REQ_ERROR                         = 0x10,
	MLX5_EVENT_TYPE_WQ_ACCESS_ERROR                            = 0x11,
	MLX5_EVENT_TYPE_SRQ_CATAS_ERROR                            = 0x12,
	MLX5_EVENT_TYPE_INTERNAL_ERROR                             = 0x8,
	MLX5_EVENT_TYPE_PORT_CHANGE                                = 0x9,
	MLX5_EVENT_TYPE_GPIO_EVENT                                 = 0x15,
	MLX5_EVENT_TYPE_CODING_PORT_MODULE_EVENT                   = 0x16,
	MLX5_EVENT_TYPE_TEMP_WARN_EVENT                            = 0x17,
	MLX5_EVENT_TYPE_XRQ_ERROR				   = 0x18,
	MLX5_EVENT_TYPE_REMOTE_CONFIG                              = 0x19,
	MLX5_EVENT_TYPE_CODING_DCBX_CHANGE_EVENT                   = 0x1e,
	MLX5_EVENT_TYPE_CODING_PPS_EVENT                           = 0x25,
	MLX5_EVENT_TYPE_CODING_GENERAL_NOTIFICATION_EVENT          = 0x22,
	MLX5_EVENT_TYPE_DB_BF_CONGESTION                           = 0x1a,
	MLX5_EVENT_TYPE_STALL_EVENT                                = 0x1b,
	MLX5_EVENT_TYPE_DROPPED_PACKET_LOGGED_EVENT                = 0x1f,
	MLX5_EVENT_TYPE_CMD                                        = 0xa,
	MLX5_EVENT_TYPE_PAGE_REQUEST                               = 0xb,
	MLX5_EVENT_TYPE_NIC_VPORT_CHANGE                           = 0xd,
	MLX5_EVENT_TYPE_FPGA_ERROR                                 = 0x20,
	MLX5_EVENT_TYPE_FPGA_QP_ERROR                              = 0x21,
	MLX5_EVENT_TYPE_OBJECT_CHANGE                              = 0x27,
};

enum {
	MLX5_MODIFY_TIR_BITMASK_LRO                                = 0x0,
	MLX5_MODIFY_TIR_BITMASK_INDIRECT_TABLE                     = 0x1,
	MLX5_MODIFY_TIR_BITMASK_HASH                               = 0x2,
	MLX5_MODIFY_TIR_BITMASK_TUNNELED_OFFLOAD_EN                = 0x3,
	MLX5_MODIFY_TIR_BITMASK_SELF_LB_EN                         = 0x4
};

enum {
	MLX5_MODIFY_RQT_BITMASK_RQN_LIST          = 0x1,
};

enum {
	MLX5_SET_HCA_CAP_OP_MOD_GENERAL_DEVICE        = 0x0,
	MLX5_SET_HCA_CAP_OP_MOD_ATOMIC                = 0x3,
};

enum {
	MLX5_OBJ_TYPE_GENEVE_TLV_OPT = 0x000b,
	MLX5_OBJ_TYPE_MKEY = 0xff01,
	MLX5_OBJ_TYPE_QP = 0xff02,
	MLX5_OBJ_TYPE_PSV = 0xff03,
	MLX5_OBJ_TYPE_RMP = 0xff04,
	MLX5_OBJ_TYPE_XRC_SRQ = 0xff05,
	MLX5_OBJ_TYPE_RQ = 0xff06,
	MLX5_OBJ_TYPE_SQ = 0xff07,
	MLX5_OBJ_TYPE_TIR = 0xff08,
	MLX5_OBJ_TYPE_TIS = 0xff09,
	MLX5_OBJ_TYPE_DCT = 0xff0a,
	MLX5_OBJ_TYPE_XRQ = 0xff0b,
	MLX5_OBJ_TYPE_RQT = 0xff0e,
	MLX5_OBJ_TYPE_FLOW_COUNTER = 0xff0f,
	MLX5_OBJ_TYPE_CQ = 0xff10,
};

enum {
	MLX5_CMD_OP_QUERY_HCA_CAP                 = 0x100,
	MLX5_CMD_OP_QUERY_ADAPTER                 = 0x101,
	MLX5_CMD_OP_INIT_HCA                      = 0x102,
	MLX5_CMD_OP_TEARDOWN_HCA                  = 0x103,
	MLX5_CMD_OP_ENABLE_HCA                    = 0x104,
	MLX5_CMD_OP_DISABLE_HCA                   = 0x105,
	MLX5_CMD_OP_QUERY_PAGES                   = 0x107,
	MLX5_CMD_OP_MANAGE_PAGES                  = 0x108,
	MLX5_CMD_OP_SET_HCA_CAP                   = 0x109,
	MLX5_CMD_OP_QUERY_ISSI                    = 0x10a,
	MLX5_CMD_OP_SET_ISSI                      = 0x10b,
	MLX5_CMD_OP_SET_DRIVER_VERSION            = 0x10d,
	MLX5_CMD_OP_QUERY_OTHER_HCA_CAP           = 0x10e,
	MLX5_CMD_OP_MODIFY_OTHER_HCA_CAP          = 0x10f,
	MLX5_CMD_OP_CREATE_MKEY                   = 0x200,
	MLX5_CMD_OP_QUERY_MKEY                    = 0x201,
	MLX5_CMD_OP_DESTROY_MKEY                  = 0x202,
	MLX5_CMD_OP_QUERY_SPECIAL_CONTEXTS        = 0x203,
	MLX5_CMD_OP_PAGE_FAULT_RESUME             = 0x204,
	MLX5_CMD_OP_CREATE_EQ                     = 0x301,
	MLX5_CMD_OP_DESTROY_EQ                    = 0x302,
	MLX5_CMD_OP_QUERY_EQ                      = 0x303,
	MLX5_CMD_OP_GEN_EQE                       = 0x304,
	MLX5_CMD_OP_CREATE_CQ                     = 0x400,
	MLX5_CMD_OP_DESTROY_CQ                    = 0x401,
	MLX5_CMD_OP_QUERY_CQ                      = 0x402,
	MLX5_CMD_OP_MODIFY_CQ                     = 0x403,
	MLX5_CMD_OP_CREATE_QP                     = 0x500,
	MLX5_CMD_OP_DESTROY_QP                    = 0x501,
	MLX5_CMD_OP_RST2INIT_QP                   = 0x502,
	MLX5_CMD_OP_INIT2RTR_QP                   = 0x503,
	MLX5_CMD_OP_RTR2RTS_QP                    = 0x504,
	MLX5_CMD_OP_RTS2RTS_QP                    = 0x505,
	MLX5_CMD_OP_SQERR2RTS_QP                  = 0x506,
	MLX5_CMD_OP_2ERR_QP                       = 0x507,
	MLX5_CMD_OP_2RST_QP                       = 0x50a,
	MLX5_CMD_OP_QUERY_QP                      = 0x50b,
	MLX5_CMD_OP_SQD_RTS_QP                    = 0x50c,
	MLX5_CMD_OP_INIT2INIT_QP                  = 0x50e,
	MLX5_CMD_OP_CREATE_PSV                    = 0x600,
	MLX5_CMD_OP_DESTROY_PSV                   = 0x601,
	MLX5_CMD_OP_CREATE_SRQ                    = 0x700,
	MLX5_CMD_OP_DESTROY_SRQ                   = 0x701,
	MLX5_CMD_OP_QUERY_SRQ                     = 0x702,
	MLX5_CMD_OP_ARM_RQ                        = 0x703,
	MLX5_CMD_OP_CREATE_XRC_SRQ                = 0x705,
	MLX5_CMD_OP_DESTROY_XRC_SRQ               = 0x706,
	MLX5_CMD_OP_QUERY_XRC_SRQ                 = 0x707,
	MLX5_CMD_OP_ARM_XRC_SRQ                   = 0x708,
	MLX5_CMD_OP_CREATE_DCT                    = 0x710,
	MLX5_CMD_OP_DESTROY_DCT                   = 0x711,
	MLX5_CMD_OP_DRAIN_DCT                     = 0x712,
	MLX5_CMD_OP_QUERY_DCT                     = 0x713,
	MLX5_CMD_OP_ARM_DCT_FOR_KEY_VIOLATION     = 0x714,
	MLX5_CMD_OP_SET_DC_CNAK_TRACE             = 0x715,
	MLX5_CMD_OP_QUERY_DC_CNAK_TRACE           = 0x716,
	MLX5_CMD_OP_CREATE_XRQ                    = 0x717,
	MLX5_CMD_OP_DESTROY_XRQ                   = 0x718,
	MLX5_CMD_OP_QUERY_XRQ                     = 0x719,
	MLX5_CMD_OP_ARM_XRQ                       = 0x71a,
	MLX5_CMD_OP_QUERY_XRQ_DC_PARAMS_ENTRY     = 0x725,
	MLX5_CMD_OP_SET_XRQ_DC_PARAMS_ENTRY       = 0x726,
	MLX5_CMD_OP_QUERY_XRQ_ERROR_PARAMS        = 0x727,
	MLX5_CMD_OP_RELEASE_XRQ_ERROR             = 0x729,
	MLX5_CMD_OP_MODIFY_XRQ                    = 0x72a,

	MLX5_CMD_OP_QUERY_VPORT_STATE             = 0x750,
	MLX5_CMD_OP_MODIFY_VPORT_STATE            = 0x751,
	MLX5_CMD_OP_QUERY_ESW_VPORT_CONTEXT       = 0x752,
	MLX5_CMD_OP_MODIFY_ESW_VPORT_CONTEXT      = 0x753,
	MLX5_CMD_OP_QUERY_NIC_VPORT_CONTEXT       = 0x754,
	MLX5_CMD_OP_MODIFY_NIC_VPORT_CONTEXT      = 0x755,
	MLX5_CMD_OP_QUERY_ROCE_ADDRESS            = 0x760,
	MLX5_CMD_OP_SET_ROCE_ADDRESS              = 0x761,
	MLX5_CMD_OP_QUERY_HCA_VPORT_CONTEXT       = 0x762,
	MLX5_CMD_OP_MODIFY_HCA_VPORT_CONTEXT      = 0x763,
	MLX5_CMD_OP_QUERY_HCA_VPORT_GID           = 0x764,
	MLX5_CMD_OP_QUERY_HCA_VPORT_PKEY          = 0x765,
	MLX5_CMD_OP_QUERY_VNIC_ENV                = 0x76f,
	MLX5_CMD_OP_QUERY_VPORT_COUNTER           = 0x770,
	MLX5_CMD_OP_ALLOC_Q_COUNTER               = 0x771,
	MLX5_CMD_OP_DEALLOC_Q_COUNTER             = 0x772,
	MLX5_CMD_OP_QUERY_Q_COUNTER               = 0x773,
	MLX5_CMD_OP_SET_RATE_LIMIT                = 0x780,
	MLX5_CMD_OP_QUERY_RATE_LIMIT              = 0x781,
	MLX5_CMD_OP_CREATE_SCHEDULING_ELEMENT     = 0x782,
	MLX5_CMD_OP_DESTROY_SCHEDULING_ELEMENT    = 0x783,
	MLX5_CMD_OP_QUERY_SCHEDULING_ELEMENT      = 0x784,
	MLX5_CMD_OP_MODIFY_SCHEDULING_ELEMENT     = 0x785,
	MLX5_CMD_OP_CREATE_QOS_PARA_VPORT         = 0x786,
	MLX5_CMD_OP_DESTROY_QOS_PARA_VPORT        = 0x787,
	MLX5_CMD_OP_ALLOC_PD                      = 0x800,
	MLX5_CMD_OP_DEALLOC_PD                    = 0x801,
	MLX5_CMD_OP_ALLOC_UAR                     = 0x802,
	MLX5_CMD_OP_DEALLOC_UAR                   = 0x803,
	MLX5_CMD_OP_CONFIG_INT_MODERATION         = 0x804,
	MLX5_CMD_OP_ACCESS_REG                    = 0x805,
	MLX5_CMD_OP_ATTACH_TO_MCG                 = 0x806,
	MLX5_CMD_OP_DETACH_FROM_MCG               = 0x807,
	MLX5_CMD_OP_GET_DROPPED_PACKET_LOG        = 0x80a,
	MLX5_CMD_OP_MAD_IFC                       = 0x50d,
	MLX5_CMD_OP_QUERY_MAD_DEMUX               = 0x80b,
	MLX5_CMD_OP_SET_MAD_DEMUX                 = 0x80c,
	MLX5_CMD_OP_NOP                           = 0x80d,
	MLX5_CMD_OP_ALLOC_XRCD                    = 0x80e,
	MLX5_CMD_OP_DEALLOC_XRCD                  = 0x80f,
	MLX5_CMD_OP_SET_BURST_SIZE                = 0x812,
	MLX5_CMD_OP_QUERY_BURST_SIZE              = 0x813,
	MLX5_CMD_OP_ACTIVATE_TRACER               = 0x814,
	MLX5_CMD_OP_DEACTIVATE_TRACER             = 0x815,
	MLX5_CMD_OP_ALLOC_TRANSPORT_DOMAIN        = 0x816,
	MLX5_CMD_OP_DEALLOC_TRANSPORT_DOMAIN      = 0x817,
	MLX5_CMD_OP_QUERY_DIAGNOSTIC_PARAMS       = 0x819,
	MLX5_CMD_OP_SET_DIAGNOSTICS               = 0x820,
	MLX5_CMD_OP_QUERY_DIAGNOSTICS             = 0x821,
	MLX5_CMD_OP_QUERY_CONG_STATUS             = 0x822,
	MLX5_CMD_OP_MODIFY_CONG_STATUS            = 0x823,
	MLX5_CMD_OP_QUERY_CONG_PARAMS             = 0x824,
	MLX5_CMD_OP_MODIFY_CONG_PARAMS            = 0x825,
	MLX5_CMD_OP_QUERY_CONG_STATISTICS         = 0x826,
	MLX5_CMD_OP_ADD_VXLAN_UDP_DPORT           = 0x827,
	MLX5_CMD_OP_DELETE_VXLAN_UDP_DPORT        = 0x828,
	MLX5_CMD_OP_SET_L2_TABLE_ENTRY            = 0x829,
	MLX5_CMD_OP_QUERY_L2_TABLE_ENTRY          = 0x82a,
	MLX5_CMD_OP_DELETE_L2_TABLE_ENTRY         = 0x82b,
	MLX5_CMD_OP_SET_WOL_ROL                   = 0x830,
	MLX5_CMD_OP_QUERY_WOL_ROL                 = 0x831,
	MLX5_CMD_OP_CREATE_LAG                    = 0x840,
	MLX5_CMD_OP_MODIFY_LAG                    = 0x841,
	MLX5_CMD_OP_QUERY_LAG                     = 0x842,
	MLX5_CMD_OP_DESTROY_LAG                   = 0x843,
	MLX5_CMD_OP_CREATE_VPORT_LAG              = 0x844,
	MLX5_CMD_OP_DESTROY_VPORT_LAG             = 0x845,
	MLX5_CMD_OP_CREATE_TIR                    = 0x900,
	MLX5_CMD_OP_MODIFY_TIR                    = 0x901,
	MLX5_CMD_OP_DESTROY_TIR                   = 0x902,
	MLX5_CMD_OP_QUERY_TIR                     = 0x903,
	MLX5_CMD_OP_CREATE_SQ                     = 0x904,
	MLX5_CMD_OP_MODIFY_SQ                     = 0x905,
	MLX5_CMD_OP_DESTROY_SQ                    = 0x906,
	MLX5_CMD_OP_QUERY_SQ                      = 0x907,
	MLX5_CMD_OP_CREATE_RQ                     = 0x908,
	MLX5_CMD_OP_MODIFY_RQ                     = 0x909,
	MLX5_CMD_OP_DESTROY_RQ                    = 0x90a,
	MLX5_CMD_OP_QUERY_RQ                      = 0x90b,
	MLX5_CMD_OP_CREATE_RMP                    = 0x90c,
	MLX5_CMD_OP_MODIFY_RMP                    = 0x90d,
	MLX5_CMD_OP_DESTROY_RMP                   = 0x90e,
	MLX5_CMD_OP_QUERY_RMP                     = 0x90f,
	MLX5_CMD_OP_SET_DELAY_DROP_PARAMS         = 0x910,
	MLX5_CMD_OP_QUERY_DELAY_DROP_PARAMS       = 0x911,
	MLX5_CMD_OP_CREATE_TIS                    = 0x912,
	MLX5_CMD_OP_MODIFY_TIS                    = 0x913,
	MLX5_CMD_OP_DESTROY_TIS                   = 0x914,
	MLX5_CMD_OP_QUERY_TIS                     = 0x915,
	MLX5_CMD_OP_CREATE_RQT                    = 0x916,
	MLX5_CMD_OP_MODIFY_RQT                    = 0x917,
	MLX5_CMD_OP_DESTROY_RQT                   = 0x918,
	MLX5_CMD_OP_QUERY_RQT                     = 0x919,
	MLX5_CMD_OP_SET_FLOW_TABLE_ROOT           = 0x92f,
	MLX5_CMD_OP_CREATE_FLOW_TABLE             = 0x930,
	MLX5_CMD_OP_DESTROY_FLOW_TABLE            = 0x931,
	MLX5_CMD_OP_QUERY_FLOW_TABLE              = 0x932,
	MLX5_CMD_OP_CREATE_FLOW_GROUP             = 0x933,
	MLX5_CMD_OP_DESTROY_FLOW_GROUP            = 0x934,
	MLX5_CMD_OP_QUERY_FLOW_GROUP              = 0x935,
	MLX5_CMD_OP_SET_FLOW_TABLE_ENTRY          = 0x936,
	MLX5_CMD_OP_QUERY_FLOW_TABLE_ENTRY        = 0x937,
	MLX5_CMD_OP_DELETE_FLOW_TABLE_ENTRY       = 0x938,
	MLX5_CMD_OP_ALLOC_FLOW_COUNTER            = 0x939,
	MLX5_CMD_OP_DEALLOC_FLOW_COUNTER          = 0x93a,
	MLX5_CMD_OP_QUERY_FLOW_COUNTER            = 0x93b,
	MLX5_CMD_OP_MODIFY_FLOW_TABLE             = 0x93c,
	MLX5_CMD_OP_ALLOC_PACKET_REFORMAT_CONTEXT = 0x93d,
	MLX5_CMD_OP_DEALLOC_PACKET_REFORMAT_CONTEXT = 0x93e,
	MLX5_CMD_OP_QUERY_PACKET_REFORMAT_CONTEXT = 0x93f,
	MLX5_CMD_OP_ALLOC_MODIFY_HEADER_CONTEXT   = 0x940,
	MLX5_CMD_OP_DEALLOC_MODIFY_HEADER_CONTEXT = 0x941,
	MLX5_CMD_OP_QUERY_MODIFY_HEADER_CONTEXT   = 0x942,
	MLX5_CMD_OP_FPGA_CREATE_QP                = 0x960,
	MLX5_CMD_OP_FPGA_MODIFY_QP                = 0x961,
	MLX5_CMD_OP_FPGA_QUERY_QP                 = 0x962,
	MLX5_CMD_OP_FPGA_DESTROY_QP               = 0x963,
	MLX5_CMD_OP_FPGA_QUERY_QP_COUNTERS        = 0x964,
	MLX5_CMD_OP_CREATE_GENERAL_OBJ            = 0xa00,
	MLX5_CMD_OP_MODIFY_GENERAL_OBJ            = 0xa01,
	MLX5_CMD_OP_QUERY_GENERAL_OBJ             = 0xa02,
	MLX5_CMD_OP_DESTROY_GENERAL_OBJ           = 0xa03,
	MLX5_CMD_OP_CREATE_UCTX                   = 0xa04,
	MLX5_CMD_OP_DESTROY_UCTX                  = 0xa06,
	MLX5_CMD_OP_CREATE_UMEM                   = 0xa08,
	MLX5_CMD_OP_DESTROY_UMEM                  = 0xa0a,
};

/* Valid range for general commands that don't work over an object */
enum {
	MLX5_CMD_OP_GENERAL_START = 0xb00,
	MLX5_CMD_OP_GENERAL_END = 0xd00,
};

enum {
	MLX5_FT_NIC_RX_2_NIC_RX_RDMA = BIT(0),
	MLX5_FT_NIC_TX_RDMA_2_NIC_TX = BIT(1),
};

enum {
	MLX5_ICMD_CMDS_OPCODE_ICMD_OPCODE_QUERY_FW_INFO     = 0x8007,
	MLX5_ICMD_CMDS_OPCODE_ICMD_QUERY_CAPABILITY         = 0x8400,
	MLX5_ICMD_CMDS_OPCODE_ICMD_ACCESS_REGISTER          = 0x9001,
	MLX5_ICMD_CMDS_OPCODE_ICMD_QUERY_VIRTUAL_MAC        = 0x9003,
	MLX5_ICMD_CMDS_OPCODE_ICMD_SET_VIRTUAL_MAC          = 0x9004,
	MLX5_ICMD_CMDS_OPCODE_ICMD_QUERY_WOL_ROL            = 0x9005,
	MLX5_ICMD_CMDS_OPCODE_ICMD_SET_WOL_ROL              = 0x9006,
	MLX5_ICMD_CMDS_OPCODE_ICMD_OCBB_INIT                = 0x9007,
	MLX5_ICMD_CMDS_OPCODE_ICMD_OCBB_QUERY_HEADER_STATUS = 0x9008,
	MLX5_ICMD_CMDS_OPCODE_ICMD_OCBB_QUERY_ETOC_STATUS   = 0x9009,
	MLX5_ICMD_CMDS_OPCODE_ICMD_OCBB_SET_EVENT           = 0x900a,
	MLX5_ICMD_CMDS_OPCODE_ICMD_OPCODE_INIT_OCSD         = 0xf004
};

enum {
	MLX5_HCA_CAP_GENERAL_OBJECT_TYPES_IPSEC = 1ULL << 0x13,
};

enum {
	MLX5_GENERAL_OBJECT_TYPES_ENCRYPTION_KEY = 0xc,
	MLX5_GENERAL_OBJECT_TYPES_IPSEC = 0x13,
};

enum {
	MLX5_HCA_CAP_GENERAL_OBJ_TYPES_ENCRYPTION_KEY = 1 << 0xc,
};

enum {
	MLX5_GENERAL_OBJECT_TYPE_ENCRYPTION_KEY_KEY_SIZE_128 = 0x0,
	MLX5_GENERAL_OBJECT_TYPE_ENCRYPTION_KEY_KEY_SIZE_256 = 0x1,
};

enum {
	MLX5_GENERAL_OBJECT_TYPE_ENCRYPTION_KEY_TYPE_TLS = 0x1,
	MLX5_GENERAL_OBJECT_TYPE_ENCRYPTION_KEY_TYPE_IPSEC = 0x2,
};

struct mlx5_ifc_flow_table_fields_supported_bits {
	u8         outer_dmac[0x1];
	u8         outer_smac[0x1];
	u8         outer_ether_type[0x1];
	u8         outer_ip_version[0x1];
	u8         outer_first_prio[0x1];
	u8         outer_first_cfi[0x1];
	u8         outer_first_vid[0x1];
	u8         reserved_1[0x1];
	u8         outer_second_prio[0x1];
	u8         outer_second_cfi[0x1];
	u8         outer_second_vid[0x1];
	u8         outer_ipv6_flow_label[0x1];
	u8         outer_sip[0x1];
	u8         outer_dip[0x1];
	u8         outer_frag[0x1];
	u8         outer_ip_protocol[0x1];
	u8         outer_ip_ecn[0x1];
	u8         outer_ip_dscp[0x1];
	u8         outer_udp_sport[0x1];
	u8         outer_udp_dport[0x1];
	u8         outer_tcp_sport[0x1];
	u8         outer_tcp_dport[0x1];
	u8         outer_tcp_flags[0x1];
	u8         outer_gre_protocol[0x1];
	u8         outer_gre_key[0x1];
	u8         outer_vxlan_vni[0x1];
	u8         outer_geneve_vni[0x1];
	u8         outer_geneve_oam[0x1];
	u8         outer_geneve_protocol_type[0x1];
	u8         outer_geneve_opt_len[0x1];
	u8         reserved_2[0x1];
	u8         source_eswitch_port[0x1];

	u8         inner_dmac[0x1];
	u8         inner_smac[0x1];
	u8         inner_ether_type[0x1];
	u8         inner_ip_version[0x1];
	u8         inner_first_prio[0x1];
	u8         inner_first_cfi[0x1];
	u8         inner_first_vid[0x1];
	u8         reserved_4[0x1];
	u8         inner_second_prio[0x1];
	u8         inner_second_cfi[0x1];
	u8         inner_second_vid[0x1];
	u8         inner_ipv6_flow_label[0x1];
	u8         inner_sip[0x1];
	u8         inner_dip[0x1];
	u8         inner_frag[0x1];
	u8         inner_ip_protocol[0x1];
	u8         inner_ip_ecn[0x1];
	u8         inner_ip_dscp[0x1];
	u8         inner_udp_sport[0x1];
	u8         inner_udp_dport[0x1];
	u8         inner_tcp_sport[0x1];
	u8         inner_tcp_dport[0x1];
	u8         inner_tcp_flags[0x1];
	u8         reserved_5[0x9];

	u8         reserved_6[0x1a];
	u8         bth_dst_qp[0x1];
	u8         reserved_7[0x4];
	u8         source_sqn[0x1];

	u8         reserved_8[0x20];
};

struct mlx5_ifc_eth_discard_cntrs_grp_bits {
	u8         ingress_general_high[0x20];

	u8         ingress_general_low[0x20];

	u8         ingress_policy_engine_high[0x20];

	u8         ingress_policy_engine_low[0x20];

	u8         ingress_vlan_membership_high[0x20];

	u8         ingress_vlan_membership_low[0x20];

	u8         ingress_tag_frame_type_high[0x20];

	u8         ingress_tag_frame_type_low[0x20];

	u8         egress_vlan_membership_high[0x20];

	u8         egress_vlan_membership_low[0x20];

	u8         loopback_filter_high[0x20];

	u8         loopback_filter_low[0x20];

	u8         egress_general_high[0x20];

	u8         egress_general_low[0x20];

	u8         reserved_at_1c0[0x40];

	u8         egress_hoq_high[0x20];

	u8         egress_hoq_low[0x20];

	u8         port_isolation_high[0x20];

	u8         port_isolation_low[0x20];

	u8         egress_policy_engine_high[0x20];

	u8         egress_policy_engine_low[0x20];

	u8         ingress_tx_link_down_high[0x20];

	u8         ingress_tx_link_down_low[0x20];

	u8         egress_stp_filter_high[0x20];

	u8         egress_stp_filter_low[0x20];

	u8         egress_hoq_stall_high[0x20];

	u8         egress_hoq_stall_low[0x20];

	u8         reserved_at_340[0x440];
};

struct mlx5_ifc_flow_table_prop_layout_bits {
	u8         ft_support[0x1];
	u8         reserved_at_1[0x1];
	u8         flow_counter[0x1];
	u8         flow_modify_en[0x1];
	u8         modify_root[0x1];
	u8         identified_miss_table_mode[0x1];
	u8         flow_table_modify[0x1];
	u8         reformat[0x1];
	u8         decap[0x1];
	u8         reserved_at_9[0x1];
	u8         pop_vlan[0x1];
	u8         push_vlan[0x1];
	u8         reserved_at_c[0x1];
	u8         pop_vlan_2[0x1];
	u8         push_vlan_2[0x1];
	u8         reformat_and_vlan_action[0x1];
	u8         reserved_at_10[0x1];
	u8         sw_owner[0x1];
	u8         reformat_l3_tunnel_to_l2[0x1];
	u8         reformat_l2_to_l3_tunnel[0x1];
	u8         reformat_and_modify_action[0x1];
	u8         ignore_flow_level[0x1];
	u8         reserved_at_16[0x1];
	u8         table_miss_action_domain[0x1];
	u8         termination_table[0x1];
	u8         reformat_and_fwd_to_table[0x1];
	u8         reserved_at_1a[0x2];
	u8         ipsec_encrypt[0x1];
	u8         ipsec_decrypt[0x1];
	u8         sw_owner_v2[0x1];
	u8         reserved_at_1f[0x1];
	u8         termination_table_raw_traffic[0x1];
	u8         reserved_at_21[0x1];
	u8         log_max_ft_size[0x6];
	u8         log_max_modify_header_context[0x8];
        u8         max_modify_header_actions[0x8];
	u8         max_ft_level[0x8];

	u8         reformat_add_esp_trasport[0x1];
	u8         reformat_l2_to_l3_esp_tunnel[0x1];
	u8         reformat_add_esp_transport_over_udp[0x1];
	u8         reformat_del_esp_trasport[0x1];
	u8         reformat_l3_esp_tunnel_to_l2[0x1];
	u8         reformat_del_esp_transport_over_udp[0x1];
	u8         execute_aso[0x1];
	u8         reserved_at_47[0x19];
	u8         reserved_at_60[0x2];
	u8         reformat_insert[0x1];
	u8         reformat_remove[0x1];
	u8         macsec_encrypt[0x1];
	u8         macsec_decrypt[0x1];
	u8         reserved_at_66[0x2];
	u8         reformat_add_macsec[0x1];
	u8         reformat_remove_macsec[0x1];
	u8         reserved_at_6a[0xe];
	u8         log_max_ft_num[0x8];
	u8         reserved_at_80[0x10];
	u8         log_max_flow_counter[0x8];
	u8         log_max_destination[0x8];
	u8         reserved_at_a0[0x18];
	u8         log_max_flow[0x8];
	u8         reserved_at_c0[0x40];
	struct mlx5_ifc_flow_table_fields_supported_bits ft_field_support;

	struct mlx5_ifc_flow_table_fields_supported_bits ft_field_bitmask_support;
};

struct mlx5_ifc_odp_per_transport_service_cap_bits {
	u8         send[0x1];
	u8         receive[0x1];
	u8         write[0x1];
	u8         read[0x1];
	u8         atomic[0x1];
	u8         srq_receive[0x1];
	u8         reserved_0[0x1a];
};

struct mlx5_ifc_flow_counter_list_bits {
	u8         reserved_0[0x10];
	u8         flow_counter_id[0x10];

	u8         reserved_1[0x20];
};

struct mlx5_ifc_dest_format_struct_bits {
        u8         destination_type[0x8];
        u8         destination_id[0x18];

        u8         destination_eswitch_owner_vhca_id_valid[0x1];
        u8         packet_reformat[0x1];
        u8         reserved_at_22[0x6];
        u8         destination_table_type[0x8];
        u8         destination_eswitch_owner_vhca_id[0x10];
};

struct mlx5_ifc_ipv4_layout_bits {
	u8         reserved_at_0[0x60];

	u8         ipv4[0x20];
};

struct mlx5_ifc_ipv6_layout_bits {
	u8         ipv6[16][0x8];
};

union mlx5_ifc_ipv6_layout_ipv4_layout_auto_bits {
	struct mlx5_ifc_ipv6_layout_bits ipv6_layout;
	struct mlx5_ifc_ipv4_layout_bits ipv4_layout;
	u8         reserved_at_0[0x80];
};

struct mlx5_ifc_fte_match_set_lyr_2_4_bits {
	u8         smac_47_16[0x20];

	u8         smac_15_0[0x10];
	u8         ethertype[0x10];

	u8         dmac_47_16[0x20];

	u8         dmac_15_0[0x10];
	u8         first_prio[0x3];
	u8         first_cfi[0x1];
	u8         first_vid[0xc];

	u8         ip_protocol[0x8];
	u8         ip_dscp[0x6];
	u8         ip_ecn[0x2];
	u8         cvlan_tag[0x1];
	u8         svlan_tag[0x1];
	u8         frag[0x1];
	u8         ip_version[0x4];
	u8         tcp_flags[0x9];

	u8         tcp_sport[0x10];
	u8         tcp_dport[0x10];

	u8         reserved_2[0x20];

	u8         udp_sport[0x10];
	u8         udp_dport[0x10];

	union mlx5_ifc_ipv6_layout_ipv4_layout_auto_bits src_ipv4_src_ipv6;

	union mlx5_ifc_ipv6_layout_ipv4_layout_auto_bits dst_ipv4_dst_ipv6;
};

struct mlx5_ifc_nvgre_key_bits {
	u8 hi[0x18];
	u8 lo[0x8];
};

union mlx5_ifc_gre_key_bits {
	struct mlx5_ifc_nvgre_key_bits nvgre;
	u8 key[0x20];
};

struct mlx5_ifc_fte_match_set_misc_bits {
	u8         gre_c_present[0x1];
	u8         reserved_at_1[0x1];
	u8         gre_k_present[0x1];
	u8         gre_s_present[0x1];
	u8         source_vhca_port[0x4];
	u8         source_sqn[0x18];

	u8         source_eswitch_owner_vhca_id[0x10];
	u8         source_port[0x10];

	u8         outer_second_prio[0x3];
	u8         outer_second_cfi[0x1];
	u8         outer_second_vid[0xc];
	u8         inner_second_prio[0x3];
	u8         inner_second_cfi[0x1];
	u8         inner_second_vid[0xc];

	u8         outer_second_cvlan_tag[0x1];
	u8         inner_second_cvlan_tag[0x1];
	u8         outer_second_svlan_tag[0x1];
	u8         inner_second_svlan_tag[0x1];
	u8         reserved_at_64[0xc];
	u8         gre_protocol[0x10];

	union mlx5_ifc_gre_key_bits gre_key;

	u8         vxlan_vni[0x18];
	u8         bth_opcode[0x8];

	u8         geneve_vni[0x18];
	u8         reserved_at_d8[0x6];
	u8         geneve_tlv_option_0_exist[0x1];
	u8         geneve_oam[0x1];

	u8         reserved_at_e0[0xc];
	u8         outer_ipv6_flow_label[0x14];

	u8         reserved_at_100[0xc];
	u8         inner_ipv6_flow_label[0x14];

	u8         reserved_at_120[0xa];
	u8         geneve_opt_len[0x6];
	u8         geneve_protocol_type[0x10];

	u8         reserved_at_140[0x8];
	u8         bth_dst_qp[0x18];
	u8         inner_esp_spi[0x20];
	u8         outer_esp_spi[0x20];
	u8         reserved_at_1a0[0x60];
};

struct mlx5_ifc_fte_match_mpls_bits {
	u8         mpls_label[0x14];
	u8         mpls_exp[0x3];
	u8         mpls_s_bos[0x1];
	u8         mpls_ttl[0x8];
};

struct mlx5_ifc_fte_match_set_misc2_bits {
	struct mlx5_ifc_fte_match_mpls_bits outer_first_mpls;

	struct mlx5_ifc_fte_match_mpls_bits inner_first_mpls;

	struct mlx5_ifc_fte_match_mpls_bits outer_first_mpls_over_gre;

	struct mlx5_ifc_fte_match_mpls_bits outer_first_mpls_over_udp;

	u8         metadata_reg_c_7[0x20];

	u8         metadata_reg_c_6[0x20];

	u8         metadata_reg_c_5[0x20];

	u8         metadata_reg_c_4[0x20];

	u8         metadata_reg_c_3[0x20];

	u8         metadata_reg_c_2[0x20];

	u8         metadata_reg_c_1[0x20];

	u8         metadata_reg_c_0[0x20];

	u8         metadata_reg_a[0x20];

	u8         reserved_at_1a0[0x8];

	u8         macsec_syndrome[0x8];
	u8         ipsec_syndrome[0x8];
	u8         reserved_at_1b8[0x8];

	u8         reserved_at_1c0[0x40];
};

struct mlx5_ifc_fte_match_set_misc3_bits {
	u8         inner_tcp_seq_num[0x20];

	u8         outer_tcp_seq_num[0x20];

	u8         inner_tcp_ack_num[0x20];

	u8         outer_tcp_ack_num[0x20];

	u8         reserved_at_80[0x8];
	u8         outer_vxlan_gpe_vni[0x18];

	u8         outer_vxlan_gpe_next_protocol[0x8];
	u8         outer_vxlan_gpe_flags[0x8];
	u8         reserved_at_b0[0x10];

	u8         icmp_header_data[0x20];

	u8         icmpv6_header_data[0x20];

	u8         icmp_type[0x8];
	u8         icmp_code[0x8];
	u8         icmpv6_type[0x8];
	u8         icmpv6_code[0x8];

	u8         geneve_tlv_option_0_data[0x20];

	u8         gtpu_teid[0x20];

	u8         gtpu_msg_type[0x8];
	u8         gtpu_msg_flags[0x8];
	u8         reserved_at_170[0x10];

	u8         gtpu_dw_2[0x20];

	u8         gtpu_first_ext_dw_0[0x20];

	u8         gtpu_dw_0[0x20];

	u8         reserved_at_1e0[0x20];
};

struct mlx5_ifc_fte_match_set_misc4_bits {
        u8         prog_sample_field_value_0[0x20];

        u8         prog_sample_field_id_0[0x20];

        u8         prog_sample_field_value_1[0x20];

        u8         prog_sample_field_id_1[0x20];

        u8         prog_sample_field_value_2[0x20];

        u8         prog_sample_field_id_2[0x20];

        u8         prog_sample_field_value_3[0x20];

        u8         prog_sample_field_id_3[0x20];

        u8         reserved_at_100[0x100];
};

struct mlx5_ifc_fte_match_set_misc5_bits {
        u8         macsec_tag_0[0x20];

        u8         macsec_tag_1[0x20];

        u8         macsec_tag_2[0x20];

        u8         macsec_tag_3[0x20];

        u8         tunnel_header_0[0x20];

        u8         tunnel_header_1[0x20];

        u8         tunnel_header_2[0x20];

        u8         tunnel_header_3[0x20];

        u8         reserved_at_100[0x100];
};

struct mlx5_ifc_cmd_pas_bits {
	u8         pa_h[0x20];

	u8         pa_l[0x14];
	u8         reserved_0[0xc];
};

struct mlx5_ifc_uint64_bits {
	u8         hi[0x20];

	u8         lo[0x20];
};

struct mlx5_ifc_application_prio_entry_bits {
	u8         reserved_0[0x8];
	u8         priority[0x3];
	u8         reserved_1[0x2];
	u8         sel[0x3];
	u8         protocol_id[0x10];
};

struct mlx5_ifc_nodnic_ring_doorbell_bits {
	u8         reserved_0[0x8];
	u8         ring_pi[0x10];
	u8         reserved_1[0x8];
};

enum {
	MLX5_ADS_STAT_RATE_NO_LIMIT  = 0x0,
	MLX5_ADS_STAT_RATE_2_5GBPS   = 0x7,
	MLX5_ADS_STAT_RATE_10GBPS    = 0x8,
	MLX5_ADS_STAT_RATE_30GBPS    = 0x9,
	MLX5_ADS_STAT_RATE_5GBPS     = 0xa,
	MLX5_ADS_STAT_RATE_20GBPS    = 0xb,
	MLX5_ADS_STAT_RATE_40GBPS    = 0xc,
	MLX5_ADS_STAT_RATE_60GBPS    = 0xd,
	MLX5_ADS_STAT_RATE_80GBPS    = 0xe,
	MLX5_ADS_STAT_RATE_120GBPS   = 0xf,
};

struct mlx5_ifc_ads_bits {
	u8         fl[0x1];
	u8         free_ar[0x1];
	u8         reserved_0[0xe];
	u8         pkey_index[0x10];

	u8         reserved_1[0x8];
	u8         grh[0x1];
	u8         mlid[0x7];
	u8         rlid[0x10];

	u8         ack_timeout[0x5];
	u8         reserved_2[0x3];
	u8         src_addr_index[0x8];
	u8         log_rtm[0x4];
	u8         stat_rate[0x4];
	u8         hop_limit[0x8];

	u8         reserved_3[0x4];
	u8         tclass[0x8];
	u8         flow_label[0x14];

	u8         rgid_rip[16][0x8];

	u8         reserved_4[0x4];
	u8         f_dscp[0x1];
	u8         f_ecn[0x1];
	u8         reserved_5[0x1];
	u8         f_eth_prio[0x1];
	u8         ecn[0x2];
	u8         dscp[0x6];
	u8         udp_sport[0x10];

	u8         dei_cfi[0x1];
	u8         eth_prio[0x3];
	u8         sl[0x4];
	u8         port[0x8];
	u8         rmac_47_32[0x10];

	u8         rmac_31_0[0x20];
};

struct mlx5_ifc_diagnostic_counter_cap_bits {
	u8         sync[0x1];
	u8         reserved_0[0xf];
	u8         counter_id[0x10];
};

struct mlx5_ifc_debug_cap_bits {
	u8         reserved_0[0x18];
	u8         log_max_samples[0x8];

	u8         single[0x1];
	u8         repetitive[0x1];
	u8         health_mon_rx_activity[0x1];
	u8         reserved_1[0x15];
	u8         log_min_sample_period[0x8];

	u8         reserved_2[0x1c0];

	struct mlx5_ifc_diagnostic_counter_cap_bits diagnostic_counter[0x1f0];
};

struct mlx5_ifc_qos_cap_bits {
	u8         packet_pacing[0x1];
	u8         esw_scheduling[0x1];
	u8         esw_bw_share[0x1];
	u8         esw_rate_limit[0x1];
	u8         hll[0x1];
	u8         packet_pacing_burst_bound[0x1];
	u8         packet_pacing_typical_size[0x1];
	u8         reserved_at_7[0x19];

	u8 	   reserved_at_20[0xA];
	u8	   qos_remap_pp[0x1];
	u8         reserved_at_2b[0x15];

	u8         packet_pacing_max_rate[0x20];

	u8         packet_pacing_min_rate[0x20];

	u8         reserved_at_80[0x10];
	u8         packet_pacing_rate_table_size[0x10];

	u8         esw_element_type[0x10];
	u8         esw_tsar_type[0x10];

	u8         reserved_at_c0[0x10];
	u8         max_qos_para_vport[0x10];

	u8         max_tsar_bw_share[0x20];

	u8         reserved_at_100[0x700];
};

struct mlx5_ifc_snapshot_cap_bits {
	u8         reserved_0[0x1d];
	u8         suspend_qp_uc[0x1];
	u8         suspend_qp_ud[0x1];
	u8         suspend_qp_rc[0x1];

	u8         reserved_1[0x1c];
	u8         restore_pd[0x1];
	u8         restore_uar[0x1];
	u8         restore_mkey[0x1];
	u8         restore_qp[0x1];

	u8         reserved_2[0x1e];
	u8         named_mkey[0x1];
	u8         named_qp[0x1];

	u8         reserved_3[0x7a0];
};

struct mlx5_ifc_e_switch_cap_bits {
        u8         vport_svlan_strip[0x1];
        u8         vport_cvlan_strip[0x1];
        u8         vport_svlan_insert[0x1];
        u8         vport_cvlan_insert_if_not_exist[0x1];
        u8         vport_cvlan_insert_overwrite[0x1];
        u8         reserved_at_5[0x1];
        u8         vport_cvlan_insert_always[0x1];
        u8         esw_shared_ingress_acl[0x1];
        u8         esw_uplink_ingress_acl[0x1];
        u8         root_ft_on_other_esw[0x1];
        u8         reserved_at_a[0xf];
        u8         esw_functions_changed[0x1];
        u8         reserved_at_1a[0x1];
        u8         ecpf_vport_exists[0x1];
        u8         counter_eswitch_affinity[0x1];
        u8         merged_eswitch[0x1];
        u8         nic_vport_node_guid_modify[0x1];
        u8         nic_vport_port_guid_modify[0x1];

        u8         vxlan_encap_decap[0x1];
        u8         nvgre_encap_decap[0x1];
        u8         reserved_at_22[0x1];
        u8         log_max_fdb_encap_uplink[0x5];
        u8         reserved_at_21[0x3];
        u8         log_max_packet_reformat_context[0x5];
        u8         reserved_2b[0x6];
        u8         max_encap_header_size[0xa];

        u8         reserved_at_40[0xb];
        u8         log_max_esw_sf[0x5];
        u8         esw_sf_base_id[0x10];

        u8         reserved_at_60[0x7a0];

};

struct mlx5_ifc_flow_table_eswitch_cap_bits {
	u8         reserved_0[0x200];

	struct mlx5_ifc_flow_table_prop_layout_bits flow_table_properties_nic_esw_fdb;

	struct mlx5_ifc_flow_table_prop_layout_bits flow_table_properties_esw_acl_ingress;

	struct mlx5_ifc_flow_table_prop_layout_bits flow_table_properties_esw_acl_egress;

	u8         reserved_1[0x7800];
};

struct mlx5_ifc_flow_table_nic_cap_bits {
	u8         nic_rx_multi_path_tirs[0x1];
        u8         nic_rx_multi_path_tirs_fts[0x1];
        u8         allow_sniffer_and_nic_rx_shared_tir[0x1];
        u8         reserved_at_3[0x4];
        u8         sw_owner_reformat_supported[0x1];
        u8         reserved_at_8[0x18];

        u8         encap_general_header[0x1];
        u8         reserved_at_21[0xa];
        u8         log_max_packet_reformat_context[0x5];
        u8         reserved_at_30[0x6];
        u8         max_encap_header_size[0xa];
        u8         reserved_at_40[0x1c0];

	struct mlx5_ifc_flow_table_prop_layout_bits flow_table_properties_nic_receive;

	struct mlx5_ifc_flow_table_prop_layout_bits flow_table_properties_nic_receive_rdma;

	struct mlx5_ifc_flow_table_prop_layout_bits flow_table_properties_nic_receive_sniffer;

	struct mlx5_ifc_flow_table_prop_layout_bits flow_table_properties_nic_transmit;

	struct mlx5_ifc_flow_table_prop_layout_bits flow_table_properties_nic_transmit_rdma;

	struct mlx5_ifc_flow_table_prop_layout_bits flow_table_properties_nic_transmit_sniffer;

	u8         reserved_1[0x7200];
};

struct mlx5_ifc_port_selection_cap_bits {
        u8         reserved_at_0[0x10];
        u8         port_select_flow_table[0x1];
        u8         reserved_at_11[0x1];
        u8         port_select_flow_table_bypass[0x1];
        u8         reserved_at_13[0xd];

        u8         reserved_at_20[0x1e0];

        struct mlx5_ifc_flow_table_prop_layout_bits flow_table_properties_port_selection;

        u8         reserved_at_400[0x7c00];
};

struct mlx5_ifc_pddr_module_info_bits {
	u8         cable_technology[0x8];
	u8         cable_breakout[0x8];
	u8         ext_ethernet_compliance_code[0x8];
	u8         ethernet_compliance_code[0x8];

	u8         cable_type[0x4];
	u8         cable_vendor[0x4];
	u8         cable_length[0x8];
	u8         cable_identifier[0x8];
	u8         cable_power_class[0x8];

	u8         reserved_at_40[0x8];
	u8         cable_rx_amp[0x8];
	u8         cable_rx_emphasis[0x8];
	u8         cable_tx_equalization[0x8];

	u8         reserved_at_60[0x8];
	u8         cable_attenuation_12g[0x8];
	u8         cable_attenuation_7g[0x8];
	u8         cable_attenuation_5g[0x8];

	u8         reserved_at_80[0x8];
	u8         rx_cdr_cap[0x4];
	u8         tx_cdr_cap[0x4];
	u8         reserved_at_90[0x4];
	u8         rx_cdr_state[0x4];
	u8         reserved_at_98[0x4];
	u8         tx_cdr_state[0x4];

	u8         vendor_name[16][0x8];

	u8         vendor_pn[16][0x8];

	u8         vendor_rev[0x20];

	u8         fw_version[0x20];

	u8         vendor_sn[16][0x8];

	u8         temperature[0x10];
	u8         voltage[0x10];

	u8         rx_power_lane0[0x10];
	u8         rx_power_lane1[0x10];

	u8         rx_power_lane2[0x10];
	u8         rx_power_lane3[0x10];

	u8         reserved_at_2c0[0x40];

	u8         tx_power_lane0[0x10];
	u8         tx_power_lane1[0x10];

	u8         tx_power_lane2[0x10];
	u8         tx_power_lane3[0x10];

	u8         reserved_at_340[0x40];

	u8         tx_bias_lane0[0x10];
	u8         tx_bias_lane1[0x10];

	u8         tx_bias_lane2[0x10];
	u8         tx_bias_lane3[0x10];

	u8         reserved_at_3c0[0x40];

	u8         temperature_high_th[0x10];
	u8         temperature_low_th[0x10];

	u8         voltage_high_th[0x10];
	u8         voltage_low_th[0x10];

	u8         rx_power_high_th[0x10];
	u8         rx_power_low_th[0x10];

	u8         tx_power_high_th[0x10];
	u8         tx_power_low_th[0x10];

	u8         tx_bias_high_th[0x10];
	u8         tx_bias_low_th[0x10];

	u8         reserved_at_4a0[0x10];
	u8         wavelength[0x10];

	u8         reserved_at_4c0[0x300];
};

struct mlx5_ifc_per_protocol_networking_offload_caps_bits {
	u8         csum_cap[0x1];
	u8         vlan_cap[0x1];
	u8         lro_cap[0x1];
	u8         lro_psh_flag[0x1];
	u8         lro_time_stamp[0x1];
	u8         lro_max_msg_sz_mode[0x2];
	u8         wqe_vlan_insert[0x1];
	u8         self_lb_en_modifiable[0x1];
	u8         self_lb_mc[0x1];
	u8         self_lb_uc[0x1];
	u8         max_lso_cap[0x5];
	u8         multi_pkt_send_wqe[0x2];
	u8         wqe_inline_mode[0x2];
	u8         rss_ind_tbl_cap[0x4];
	u8	   reg_umr_sq[0x1];
	u8         scatter_fcs[0x1];
	u8	   enhanced_multi_pkt_send_wqe[0x1];
	u8         tunnel_lso_const_out_ip_id[0x1];
	u8         tunnel_lro_gre[0x1];
	u8         tunnel_lro_vxlan[0x1];
	u8         tunnel_statless_gre[0x1];
	u8         tunnel_stateless_vxlan[0x1];

	u8         swp[0x1];
	u8         swp_csum[0x1];
	u8         swp_lso[0x1];
	u8         reserved_2[0x1b];
	u8         max_geneve_opt_len[0x1];
	u8         tunnel_stateless_geneve_rx[0x1];

	u8         reserved_3[0x10];
	u8         lro_min_mss_size[0x10];

	u8         reserved_4[0x120];

	u8         lro_timer_supported_periods[4][0x20];

	u8         reserved_5[0x600];
};

enum {
	MLX5_ROCE_CAP_L3_TYPE_GRH   = 0x1,
	MLX5_ROCE_CAP_L3_TYPE_IPV4  = 0x2,
	MLX5_ROCE_CAP_L3_TYPE_IPV6  = 0x4,
};

enum {
	MLX5_QP_TIMESTAMP_FORMAT_CAP_FREE_RUNNING               = 0x0,
	MLX5_QP_TIMESTAMP_FORMAT_CAP_REAL_TIME                  = 0x1,
	MLX5_QP_TIMESTAMP_FORMAT_CAP_FREE_RUNNING_AND_REAL_TIME = 0x2,
};

struct mlx5_ifc_roce_cap_bits {
	u8         roce_apm[0x1];
	u8         rts2rts_primary_eth_prio[0x1];
	u8         roce_rx_allow_untagged[0x1];
	u8         rts2rts_src_addr_index_for_vlan_valid_vlan_id[0x1];
	u8         reserved_at_4[0x1a];
	u8         qp_ts_format[0x2];

	u8         reserved_1[0x60];

	u8         reserved_2[0xc];
	u8         l3_type[0x4];
	u8         reserved_3[0x8];
	u8         roce_version[0x8];

	u8         reserved_4[0x10];
	u8         r_roce_dest_udp_port[0x10];

	u8         r_roce_max_src_udp_port[0x10];
	u8         r_roce_min_src_udp_port[0x10];

	u8         reserved_5[0x10];
	u8         roce_address_table_size[0x10];

	u8         reserved_6[0x700];
};

struct mlx5_ifc_device_event_cap_bits {
	u8         user_affiliated_events[4][0x40];

	u8         user_unaffiliated_events[4][0x40];
};

enum {
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_QP_1_BYTE     = 0x1,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_QP_2_BYTES    = 0x2,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_QP_4_BYTES    = 0x4,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_QP_8_BYTES    = 0x8,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_QP_16_BYTES   = 0x10,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_QP_32_BYTES   = 0x20,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_QP_64_BYTES   = 0x40,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_QP_128_BYTES  = 0x80,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_QP_256_BYTES  = 0x100,
};

enum {
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_DC_1_BYTE     = 0x1,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_DC_2_BYTES    = 0x2,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_DC_4_BYTES    = 0x4,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_DC_8_BYTES    = 0x8,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_DC_16_BYTES   = 0x10,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_DC_32_BYTES   = 0x20,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_DC_64_BYTES   = 0x40,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_DC_128_BYTES  = 0x80,
	MLX5_ATOMIC_CAPS_ATOMIC_SIZE_DC_256_BYTES  = 0x100,
};

struct mlx5_ifc_atomic_caps_bits {
	u8         reserved_0[0x40];

	u8         atomic_req_8B_endianess_mode[0x2];
	u8         reserved_1[0x4];
	u8         supported_atomic_req_8B_endianess_mode_1[0x1];

	u8         reserved_2[0x19];

	u8         reserved_3[0x20];

	u8         reserved_4[0x10];
	u8         atomic_operations[0x10];

	u8         reserved_5[0x10];
	u8         atomic_size_qp[0x10];

	u8         reserved_6[0x10];
	u8         atomic_size_dc[0x10];

	u8         reserved_7[0x720];
};

struct mlx5_ifc_odp_cap_bits {
	u8         reserved_0[0x40];

	u8         sig[0x1];
	u8         reserved_1[0x1f];

	u8         reserved_2[0x20];

	struct mlx5_ifc_odp_per_transport_service_cap_bits rc_odp_caps;

	struct mlx5_ifc_odp_per_transport_service_cap_bits uc_odp_caps;

	struct mlx5_ifc_odp_per_transport_service_cap_bits ud_odp_caps;

	struct mlx5_ifc_odp_per_transport_service_cap_bits xrc_odp_caps;

	struct mlx5_ifc_odp_per_transport_service_cap_bits dc_odp_caps;

	u8         reserved_3[0x6e0];
};

enum {
	MLX5_CMD_HCA_CAP_GID_TABLE_SIZE_8_GID_ENTRIES    = 0x0,
	MLX5_CMD_HCA_CAP_GID_TABLE_SIZE_16_GID_ENTRIES   = 0x1,
	MLX5_CMD_HCA_CAP_GID_TABLE_SIZE_32_GID_ENTRIES   = 0x2,
	MLX5_CMD_HCA_CAP_GID_TABLE_SIZE_64_GID_ENTRIES   = 0x3,
	MLX5_CMD_HCA_CAP_GID_TABLE_SIZE_128_GID_ENTRIES  = 0x4,
};

enum {
	MLX5_CMD_HCA_CAP_PKEY_TABLE_SIZE_128_ENTRIES  = 0x0,
	MLX5_CMD_HCA_CAP_PKEY_TABLE_SIZE_256_ENTRIES  = 0x1,
	MLX5_CMD_HCA_CAP_PKEY_TABLE_SIZE_512_ENTRIES  = 0x2,
	MLX5_CMD_HCA_CAP_PKEY_TABLE_SIZE_1K_ENTRIES   = 0x3,
	MLX5_CMD_HCA_CAP_PKEY_TABLE_SIZE_2K_ENTRIES   = 0x4,
	MLX5_CMD_HCA_CAP_PKEY_TABLE_SIZE_4K_ENTRIES   = 0x5,
};

enum {
	MLX5_CMD_HCA_CAP_PORT_TYPE_IB        = 0x0,
	MLX5_CMD_HCA_CAP_PORT_TYPE_ETHERNET  = 0x1,
};

enum {
	MLX5_CMD_HCA_CAP_CMDIF_CHECKSUM_DISABLED       = 0x0,
	MLX5_CMD_HCA_CAP_CMDIF_CHECKSUM_INITIAL_STATE  = 0x1,
	MLX5_CMD_HCA_CAP_CMDIF_CHECKSUM_ENABLED        = 0x3,
};

enum {
	MLX5_UCTX_CAP_RAW_TX = 1UL << 0,
	MLX5_UCTX_CAP_INTERNAL_DEV_RES = 1UL << 1,
};

enum {
	MLX5_SQ_TIMESTAMP_FORMAT_CAP_FREE_RUNNING               = 0x0,
	MLX5_SQ_TIMESTAMP_FORMAT_CAP_REAL_TIME                  = 0x1,
	MLX5_SQ_TIMESTAMP_FORMAT_CAP_FREE_RUNNING_AND_REAL_TIME = 0x2,
};

enum {
	MLX5_RQ_TIMESTAMP_FORMAT_CAP_FREE_RUNNING               = 0x0,
	MLX5_RQ_TIMESTAMP_FORMAT_CAP_REAL_TIME                  = 0x1,
	MLX5_RQ_TIMESTAMP_FORMAT_CAP_FREE_RUNNING_AND_REAL_TIME = 0x2,
};

struct mlx5_ifc_cmd_hca_cap_bits {
	u8         reserved_0[0x20];

	u8         hca_cap_2[0x1];
	u8         create_lag_when_not_master_up[0x1];
        u8         dtor[0x1];
        u8         event_on_vhca_state_teardown_request[0x1];
        u8         event_on_vhca_state_in_use[0x1];
        u8         event_on_vhca_state_active[0x1];
        u8         event_on_vhca_state_allocated[0x1];
        u8         event_on_vhca_state_invalid[0x1];
        u8         reserved_at_28[0x8];
        u8         vhca_id[0x10];

	u8         reserved_at_40[0x40];

	u8         log_max_srq_sz[0x8];
	u8         log_max_qp_sz[0x8];
	u8         event_cap[0x1];
	u8         reserved_1[0xa];
	u8         log_max_qp[0x5];

	u8         reserved_2[0xb];
	u8         log_max_srq[0x5];
	u8         reserved_3[0x10];

	u8         reserved_4[0x8];
	u8         log_max_cq_sz[0x8];
	u8         relaxed_ordering_write_umr[0x1];
	u8         relaxed_ordering_read_umr[0x1];
	u8         reserved_5[0x9];
	u8         log_max_cq[0x5];

	u8         log_max_eq_sz[0x8];
	u8         relaxed_ordering_write[0x1];
	u8         relaxed_ordering_read[0x1];
	u8         log_max_mkey[0x6];
	u8         reserved_7[0xb];
	u8         fast_teardown[0x1];
	u8         log_max_eq[0x4];

	u8         max_indirection[0x8];
	u8         reserved_8[0x1];
	u8         log_max_mrw_sz[0x7];
	u8	   force_teardown[0x1];
	u8         reserved_9[0x1];
	u8         log_max_bsf_list_size[0x6];
	u8         reserved_10[0x2];
	u8         log_max_klm_list_size[0x6];

	u8         reserved_11[0xa];
	u8         log_max_ra_req_dc[0x6];
	u8         reserved_12[0xa];
	u8         log_max_ra_res_dc[0x6];

	u8         reserved_13[0xa];
	u8         log_max_ra_req_qp[0x6];
	u8         reserved_14[0xa];
	u8         log_max_ra_res_qp[0x6];

	u8         pad_cap[0x1];
	u8         cc_query_allowed[0x1];
	u8         cc_modify_allowed[0x1];
	u8         start_pad[0x1];
	u8         cache_line_128byte[0x1];
	u8         reserved_at_165[0xa];
	u8         qcam_reg[0x1];
	u8         gid_table_size[0x10];

	u8         out_of_seq_cnt[0x1];
	u8         vport_counters[0x1];
	u8         retransmission_q_counters[0x1];
	u8         debug[0x1];
	u8         modify_rq_counters_set_id[0x1];
	u8         rq_delay_drop[0x1];
	u8         max_qp_cnt[0xa];
	u8         pkey_table_size[0x10];

	u8         vport_group_manager[0x1];
	u8         vhca_group_manager[0x1];
	u8         ib_virt[0x1];
	u8         eth_virt[0x1];
	u8         reserved_17[0x1];
	u8         ets[0x1];
	u8         nic_flow_table[0x1];
	u8         eswitch_flow_table[0x1];
	u8         reserved_18[0x1];
	u8         mcam_reg[0x1];
	u8         pcam_reg[0x1];
	u8         local_ca_ack_delay[0x5];
	u8         port_module_event[0x1];
	u8         reserved_19[0x5];
	u8         port_type[0x2];
	u8         num_ports[0x8];

	u8         snapshot[0x1];
	u8         reserved_20[0x2];
	u8         log_max_msg[0x5];
	u8         reserved_21[0x4];
	u8         max_tc[0x4];
	u8         temp_warn_event[0x1];
	u8         dcbx[0x1];
	u8         general_notification_event[0x1];
	u8         reserved_at_1d3[0x2];
	u8         fpga[0x1];
	u8         rol_s[0x1];
	u8         rol_g[0x1];
	u8         reserved_23[0x1];
	u8         wol_s[0x1];
	u8         wol_g[0x1];
	u8         wol_a[0x1];
	u8         wol_b[0x1];
	u8         wol_m[0x1];
	u8         wol_u[0x1];
	u8         wol_p[0x1];

	u8         stat_rate_support[0x10];
	u8         reserved_24[0xc];
	u8         cqe_version[0x4];

	u8         compact_address_vector[0x1];
	u8         striding_rq[0x1];
	u8         reserved_25[0x1];
	u8         ipoib_enhanced_offloads[0x1];
	u8         ipoib_ipoib_offloads[0x1];
	u8         reserved_26[0x8];
	u8         dc_connect_qp[0x1];
	u8         dc_cnak_trace[0x1];
	u8         drain_sigerr[0x1];
	u8         cmdif_checksum[0x2];
	u8         sigerr_cqe[0x1];
	u8         reserved_27[0x1];
	u8         wq_signature[0x1];
	u8         sctr_data_cqe[0x1];
	u8         reserved_28[0x1];
	u8         sho[0x1];
	u8         tph[0x1];
	u8         rf[0x1];
	u8         dct[0x1];
	u8         qos[0x1];
	u8         eth_net_offloads[0x1];
	u8         roce[0x1];
	u8         atomic[0x1];
	u8         reserved_30[0x1];

	u8         cq_oi[0x1];
	u8         cq_resize[0x1];
	u8         cq_moderation[0x1];
	u8         cq_period_mode_modify[0x1];
	u8         cq_invalidate[0x1];
	u8         reserved_at_225[0x1];
	u8         cq_eq_remap[0x1];
	u8         pg[0x1];
	u8         block_lb_mc[0x1];
	u8         exponential_backoff[0x1];
	u8         scqe_break_moderation[0x1];
	u8         cq_period_start_from_cqe[0x1];
	u8         cd[0x1];
	u8         atm[0x1];
	u8         apm[0x1];
	u8	   imaicl[0x1];
	u8         reserved_32[0x6];
	u8         qkv[0x1];
	u8         pkv[0x1];
	u8	   set_deth_sqpn[0x1];
	u8         reserved_33[0x3];
	u8         xrc[0x1];
	u8         ud[0x1];
	u8         uc[0x1];
	u8         rc[0x1];

	u8         uar_4k[0x1];
	u8         reserved_at_241[0x9];
	u8         uar_sz[0x6];
	u8         reserved_35[0x8];
	u8         log_pg_sz[0x8];

	u8         bf[0x1];
	u8         driver_version[0x1];
	u8         pad_tx_eth_packet[0x1];
	u8         reserved_36[0x8];
	u8         log_bf_reg_size[0x5];
	u8         reserved_37[0x10];

	u8         num_of_diagnostic_counters[0x10];
	u8         max_wqe_sz_sq[0x10];

	u8         reserved_38[0x10];
	u8         max_wqe_sz_rq[0x10];

	u8         reserved_39[0x10];
	u8         max_wqe_sz_sq_dc[0x10];

	u8         reserved_40[0x7];
	u8         max_qp_mcg[0x19];

	u8         reserved_41[0x10];
	u8         flow_counter_bulk_alloc[0x8];
	u8         log_max_mcg[0x8];

	u8         reserved_42[0x3];
	u8         log_max_transport_domain[0x5];
	u8         reserved_43[0x3];
	u8         log_max_pd[0x5];
	u8         reserved_44[0xb];
	u8         log_max_xrcd[0x5];

	u8         nic_receive_steering_discard[0x1];
	u8	   reserved_45[0x7];
	u8         log_max_flow_counter_bulk[0x8];
	u8         max_flow_counter[0x10];

	u8         reserved_46[0x3];
	u8         log_max_rq[0x5];
	u8         reserved_47[0x3];
	u8         log_max_sq[0x5];
	u8         reserved_48[0x3];
	u8         log_max_tir[0x5];
	u8         reserved_49[0x3];
	u8         log_max_tis[0x5];

	u8         basic_cyclic_rcv_wqe[0x1];
	u8         reserved_50[0x2];
	u8         log_max_rmp[0x5];
	u8         reserved_51[0x3];
	u8         log_max_rqt[0x5];
	u8         reserved_52[0x3];
	u8         log_max_rqt_size[0x5];
	u8         reserved_53[0x3];
	u8         log_max_tis_per_sq[0x5];

	u8         reserved_54[0x3];
	u8         log_max_stride_sz_rq[0x5];
	u8         reserved_55[0x3];
	u8         log_min_stride_sz_rq[0x5];
	u8         reserved_56[0x3];
	u8         log_max_stride_sz_sq[0x5];
	u8         reserved_57[0x3];
	u8         log_min_stride_sz_sq[0x5];

	u8         reserved_58[0x1b];
	u8         log_max_wq_sz[0x5];

	u8         nic_vport_change_event[0x1];
	u8         disable_local_lb_uc[0x1];
	u8         disable_local_lb_mc[0x1];
	u8         reserved_59[0x8];
	u8         log_max_vlan_list[0x5];
	u8         reserved_60[0x3];
	u8         log_max_current_mc_list[0x5];
	u8         reserved_61[0x3];
	u8         log_max_current_uc_list[0x5];

	u8         general_obj_types[0x40];

	u8         sq_ts_format[0x2];
	u8         rq_ts_format[0x2];
	u8         reserved_at_444[0x4];
	u8         create_qp_start_hint[0x18];

	u8         reserved_at_460[0x3];
	u8         log_max_uctx[0x5];
	u8         reserved_at_468[0x2];
	u8         ipsec_offload[0x1];
	u8         log_max_umem[0x5];
	u8         max_num_eqs[0x10];

	u8         reserved_at_480[0x1];
	u8         tls_tx[0x1];
	u8         tls_rx[0x1];
	u8         log_max_l2_table[0x5];
	u8         reserved_64[0x8];
	u8         log_uar_page_sz[0x10];

	u8         reserved_65[0x20];

	u8         device_frequency_mhz[0x20];

	u8         device_frequency_khz[0x20];

	u8         reserved_at_500[0x20];
	u8	   num_of_uars_per_page[0x20];
	u8         reserved_at_540[0x40];

	u8         log_max_atomic_size_qp[0x8];
	u8         reserved_67[0x10];
	u8         log_max_atomic_size_dc[0x8];

	u8         reserved_at_5a0[0x13];
	u8         log_max_dek[0x5];
	u8         reserved_at_5b8[0x4];
	u8         mini_cqe_resp_stride_index[0x1];
	u8         cqe_128_always[0x1];
	u8         cqe_compression_128b[0x1];

	u8         cqe_compression[0x1];

	u8         cqe_compression_timeout[0x10];
	u8         cqe_compression_max_num[0x10];

	u8         reserved_5e0[0xc0];

	u8         uctx_cap[0x20];

	u8         reserved_6c0[0xc0];

	u8	   vhca_tunnel_commands[0x40];
	u8	   reserved_at_7c0[0x40];
};

struct mlx5_ifc_cmd_hca_cap_2_bits {
	u8	   reserved_at_0[0x80];

	u8         migratable[0x1];
	u8         reserved_at_81[0x1f];

	u8	   max_reformat_insert_size[0x8];
	u8	   max_reformat_insert_offset[0x8];
	u8	   max_reformat_remove_size[0x8];
	u8	   max_reformat_remove_offset[0x8];

	u8	   reserved_at_c0[0x8];
	u8	   migration_multi_load[0x1];
	u8	   migration_tracking_state[0x1];
	u8	   reserved_at_ca[0x16];

	u8	   reserved_at_e0[0xc0];

	u8	   flow_table_type_2_type[0x8];
	u8	   reserved_at_1a8[0x3];
	u8	   log_min_mkey_entity_size[0x5];
	u8	   reserved_at_1b0[0x10];

	u8	   reserved_at_1c0[0x60];

	u8	   reserved_at_220[0x1];
	u8	   sw_vhca_id_valid[0x1];
	u8	   sw_vhca_id[0xe];
	u8	   reserved_at_230[0x10];

	u8	   reserved_at_240[0xb];
	u8	   ts_cqe_metadata_size2wqe_counter[0x5];
	u8	   reserved_at_250[0x10];

	u8	   reserved_at_260[0x5a0];
};

enum mlx5_ifc_flow_destination_type {
        MLX5_IFC_FLOW_DESTINATION_TYPE_VPORT        = 0x0,
        MLX5_IFC_FLOW_DESTINATION_TYPE_FLOW_TABLE   = 0x1,
        MLX5_IFC_FLOW_DESTINATION_TYPE_TIR          = 0x2,
        MLX5_IFC_FLOW_DESTINATION_TYPE_FLOW_SAMPLER = 0x6,
        MLX5_IFC_FLOW_DESTINATION_TYPE_UPLINK       = 0x8,
        MLX5_IFC_FLOW_DESTINATION_TYPE_TABLE_TYPE   = 0xA,
};

enum mlx5_flow_table_miss_action {
        MLX5_FLOW_TABLE_MISS_ACTION_DEF,
        MLX5_FLOW_TABLE_MISS_ACTION_FWD,
        MLX5_FLOW_TABLE_MISS_ACTION_SWITCH_DOMAIN,
};

struct mlx5_ifc_extended_dest_format_bits {
        struct mlx5_ifc_dest_format_struct_bits destination_entry;

        u8         packet_reformat_id[0x20];

        u8         reserved_at_60[0x20];
};

union mlx5_ifc_dest_format_struct_flow_counter_list_auto_bits {
	struct mlx5_ifc_dest_format_struct_bits dest_format_struct;
	struct mlx5_ifc_flow_counter_list_bits flow_counter_list;
	u8         reserved_0[0x40];
};

struct mlx5_ifc_fte_match_param_bits {
        struct mlx5_ifc_fte_match_set_lyr_2_4_bits outer_headers;

        struct mlx5_ifc_fte_match_set_misc_bits misc_parameters;

        struct mlx5_ifc_fte_match_set_lyr_2_4_bits inner_headers;

        struct mlx5_ifc_fte_match_set_misc2_bits misc_parameters_2;

        struct mlx5_ifc_fte_match_set_misc3_bits misc_parameters_3;

        struct mlx5_ifc_fte_match_set_misc4_bits misc_parameters_4;

        struct mlx5_ifc_fte_match_set_misc5_bits misc_parameters_5;

        u8         reserved_at_e00[0x200];
};

enum {
	MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_SRC_IP     = 0x0,
	MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_DST_IP     = 0x1,
	MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_L4_SPORT   = 0x2,
	MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_L4_DPORT   = 0x3,
	MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_IPSEC_SPI  = 0x4,
};

struct mlx5_ifc_rx_hash_field_select_bits {
	u8         l3_prot_type[0x1];
	u8         l4_prot_type[0x1];
	u8         selected_fields[0x1e];
};

struct mlx5_ifc_tls_capabilities_bits {
	u8         tls_1_2_aes_gcm_128[0x1];
	u8         tls_1_3_aes_gcm_128[0x1];
	u8         tls_1_2_aes_gcm_256[0x1];
	u8         tls_1_3_aes_gcm_256[0x1];
	u8         reserved_at_4[0x1c];

	u8         reserved_at_20[0x7e0];
};

enum {
	MLX5_WQ_TYPE_LINKED_LIST                 = 0x0,
	MLX5_WQ_TYPE_CYCLIC                      = 0x1,
	MLX5_WQ_TYPE_STRQ_LINKED_LIST            = 0x2,
	MLX5_WQ_TYPE_STRQ_CYCLIC                 = 0x3,
};

enum rq_type {
	RQ_TYPE_NONE,
	RQ_TYPE_STRIDE,
};

enum {
	MLX5_WQ_END_PAD_MODE_NONE               = 0x0,
	MLX5_WQ_END_PAD_MODE_ALIGN              = 0x1,
};

struct mlx5_ifc_wq_bits {
	u8         wq_type[0x4];
	u8         wq_signature[0x1];
	u8         end_padding_mode[0x2];
	u8         cd_slave[0x1];
	u8         reserved_0[0x18];

	u8         hds_skip_first_sge[0x1];
	u8         log2_hds_buf_size[0x3];
	u8         reserved_1[0x7];
	u8         page_offset[0x5];
	u8         lwm[0x10];

	u8         reserved_2[0x8];
	u8         pd[0x18];

	u8         reserved_3[0x8];
	u8         uar_page[0x18];

	u8         dbr_addr[0x40];

	u8         hw_counter[0x20];

	u8         sw_counter[0x20];

	u8         reserved_4[0xc];
	u8         log_wq_stride[0x4];
	u8         reserved_5[0x3];
	u8         log_wq_pg_sz[0x5];
	u8         reserved_6[0x3];
	u8         log_wq_sz[0x5];

	u8         dbr_umem_valid[0x1];
	u8         wq_umem_valid[0x1];
	u8         reserved_7[0x13];
	u8         single_wqe_log_num_of_strides[0x3];
	u8         two_byte_shift_en[0x1];
	u8         reserved_8[0x4];
	u8         single_stride_log_num_of_bytes[0x3];

	u8         reserved_9[0x4c0];

	struct mlx5_ifc_cmd_pas_bits pas[0];
};

struct mlx5_ifc_rq_num_bits {
	u8         reserved_0[0x8];
	u8         rq_num[0x18];
};

struct mlx5_ifc_mac_address_layout_bits {
	u8         reserved_0[0x10];
	u8         mac_addr_47_32[0x10];

	u8         mac_addr_31_0[0x20];
};

struct mlx5_ifc_cong_control_r_roce_ecn_np_bits {
	u8         reserved_0[0xa0];

	u8         min_time_between_cnps[0x20];

	u8         reserved_1[0x12];
	u8         cnp_dscp[0x6];
	u8         reserved_2[0x4];
	u8         cnp_prio_mode[0x1];
	u8         cnp_802p_prio[0x3];

	u8         reserved_3[0x720];
};

struct mlx5_ifc_cong_control_r_roce_ecn_rp_bits {
	u8         reserved_0[0x60];

	u8         reserved_1[0x4];
	u8         clamp_tgt_rate[0x1];
	u8         reserved_2[0x3];
	u8         clamp_tgt_rate_after_time_inc[0x1];
	u8         reserved_3[0x17];

	u8         reserved_4[0x20];

	u8         rpg_time_reset[0x20];

	u8         rpg_byte_reset[0x20];

	u8         rpg_threshold[0x20];

	u8         rpg_max_rate[0x20];

	u8         rpg_ai_rate[0x20];

	u8         rpg_hai_rate[0x20];

	u8         rpg_gd[0x20];

	u8         rpg_min_dec_fac[0x20];

	u8         rpg_min_rate[0x20];

	u8         reserved_5[0xe0];

	u8         rate_to_set_on_first_cnp[0x20];

	u8         dce_tcp_g[0x20];

	u8         dce_tcp_rtt[0x20];

	u8         rate_reduce_monitor_period[0x20];

	u8         reserved_6[0x20];

	u8         initial_alpha_value[0x20];

	u8         reserved_7[0x4a0];
};

struct mlx5_ifc_cong_control_802_1qau_rp_bits {
	u8         reserved_0[0x80];

	u8         rppp_max_rps[0x20];

	u8         rpg_time_reset[0x20];

	u8         rpg_byte_reset[0x20];

	u8         rpg_threshold[0x20];

	u8         rpg_max_rate[0x20];

	u8         rpg_ai_rate[0x20];

	u8         rpg_hai_rate[0x20];

	u8         rpg_gd[0x20];

	u8         rpg_min_dec_fac[0x20];

	u8         rpg_min_rate[0x20];

	u8         reserved_1[0x640];
};

enum {
	MLX5_RESIZE_FIELD_SELECT_RESIZE_FIELD_SELECT_LOG_CQ_SIZE    = 0x1,
	MLX5_RESIZE_FIELD_SELECT_RESIZE_FIELD_SELECT_PAGE_OFFSET    = 0x2,
	MLX5_RESIZE_FIELD_SELECT_RESIZE_FIELD_SELECT_LOG_PAGE_SIZE  = 0x4,
};

struct mlx5_ifc_resize_field_select_bits {
	u8         resize_field_select[0x20];
};

enum {
	MLX5_MODIFY_FIELD_SELECT_MODIFY_FIELD_SELECT_CQ_PERIOD     = 0x1,
	MLX5_MODIFY_FIELD_SELECT_MODIFY_FIELD_SELECT_CQ_MAX_COUNT  = 0x2,
	MLX5_MODIFY_FIELD_SELECT_MODIFY_FIELD_SELECT_OI            = 0x4,
	MLX5_MODIFY_FIELD_SELECT_MODIFY_FIELD_SELECT_C_EQN         = 0x8,
	MLX5_MODIFY_FIELD_SELECT_MODIFY_FIELD_SELECT_CQ_PERIOD_MODE  = 0x10,
	MLX5_MODIFY_FIELD_SELECT_MODIFY_FIELD_SELECT_STATUS          = 0x20,
};

struct mlx5_ifc_modify_field_select_bits {
	u8         modify_field_select[0x20];
};

struct mlx5_ifc_field_select_r_roce_np_bits {
	u8         field_select_r_roce_np[0x20];
};

enum {
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_CLAMP_TGT_RATE                 = 0x2,
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_CLAMP_TGT_RATE_AFTER_TIME_INC  = 0x4,
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_RPG_TIME_RESET                 = 0x8,
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_RPG_BYTE_RESET                 = 0x10,
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_RPG_THRESHOLD                  = 0x20,
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_RPG_MAX_RATE                   = 0x40,
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_RPG_AI_RATE                    = 0x80,
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_RPG_HAI_RATE                   = 0x100,
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_RPG_MIN_DEC_FAC                = 0x200,
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_RPG_MIN_RATE                   = 0x400,
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_RATE_TO_SET_ON_FIRST_CNP       = 0x800,
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_DCE_TCP_G                      = 0x1000,
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_DCE_TCP_RTT                    = 0x2000,
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_RATE_REDUCE_MONITOR_PERIOD     = 0x4000,
	MLX5_FIELD_SELECT_R_ROCE_RP_FIELD_SELECT_R_ROCE_RP_INITIAL_ALPHA_VALUE            = 0x8000,
};

struct mlx5_ifc_field_select_r_roce_rp_bits {
	u8         field_select_r_roce_rp[0x20];
};

enum {
	MLX5_FIELD_SELECT_802_1QAU_RP_FIELD_SELECT_8021QAURP_RPPP_MAX_RPS     = 0x4,
	MLX5_FIELD_SELECT_802_1QAU_RP_FIELD_SELECT_8021QAURP_RPG_TIME_RESET   = 0x8,
	MLX5_FIELD_SELECT_802_1QAU_RP_FIELD_SELECT_8021QAURP_RPG_BYTE_RESET   = 0x10,
	MLX5_FIELD_SELECT_802_1QAU_RP_FIELD_SELECT_8021QAURP_RPG_THRESHOLD    = 0x20,
	MLX5_FIELD_SELECT_802_1QAU_RP_FIELD_SELECT_8021QAURP_RPG_MAX_RATE     = 0x40,
	MLX5_FIELD_SELECT_802_1QAU_RP_FIELD_SELECT_8021QAURP_RPG_AI_RATE      = 0x80,
	MLX5_FIELD_SELECT_802_1QAU_RP_FIELD_SELECT_8021QAURP_RPG_HAI_RATE     = 0x100,
	MLX5_FIELD_SELECT_802_1QAU_RP_FIELD_SELECT_8021QAURP_RPG_GD           = 0x200,
	MLX5_FIELD_SELECT_802_1QAU_RP_FIELD_SELECT_8021QAURP_RPG_MIN_DEC_FAC  = 0x400,
	MLX5_FIELD_SELECT_802_1QAU_RP_FIELD_SELECT_8021QAURP_RPG_MIN_RATE     = 0x800,
};

struct mlx5_ifc_field_select_802_1qau_rp_bits {
	u8         field_select_8021qaurp[0x20];
};

struct mlx5_ifc_pptb_reg_bits {
	u8         reserved_at_0[0x2];
	u8         mm[0x2];
	u8         reserved_at_4[0x4];
	u8         local_port[0x8];
	u8         reserved_at_10[0x6];
	u8         cm[0x1];
	u8         um[0x1];
	u8         pm[0x8];

	u8         prio_x_buff[0x20];

	u8         pm_msb[0x8];
	u8         reserved_at_48[0x10];
	u8         ctrl_buff[0x4];
	u8         untagged_buff[0x4];
};

struct mlx5_ifc_dcbx_app_reg_bits {
	u8         reserved_0[0x8];
	u8         port_number[0x8];
	u8         reserved_1[0x10];

	u8         reserved_2[0x1a];
	u8         num_app_prio[0x6];

	u8         reserved_3[0x40];

	struct mlx5_ifc_application_prio_entry_bits app_prio[0];
};

struct mlx5_ifc_dcbx_param_reg_bits {
	u8         dcbx_cee_cap[0x1];
	u8         dcbx_ieee_cap[0x1];
	u8         dcbx_standby_cap[0x1];
	u8         reserved_0[0x5];
	u8         port_number[0x8];
	u8         reserved_1[0xa];
	u8         max_application_table_size[0x6];

	u8         reserved_2[0x15];
	u8         version_oper[0x3];
	u8         reserved_3[0x5];
	u8         version_admin[0x3];

	u8         willing_admin[0x1];
	u8         reserved_4[0x3];
	u8         pfc_cap_oper[0x4];
	u8         reserved_5[0x4];
	u8         pfc_cap_admin[0x4];
	u8         reserved_6[0x4];
	u8         num_of_tc_oper[0x4];
	u8         reserved_7[0x4];
	u8         num_of_tc_admin[0x4];

	u8         remote_willing[0x1];
	u8         reserved_8[0x3];
	u8         remote_pfc_cap[0x4];
	u8         reserved_9[0x14];
	u8         remote_num_of_tc[0x4];

	u8         reserved_10[0x18];
	u8         error[0x8];

	u8         reserved_11[0x160];
};

struct mlx5_ifc_qhll_bits {
	u8         reserved_at_0[0x8];
	u8         local_port[0x8];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x1b];
	u8         hll_time[0x5];

	u8         stall_en[0x1];
	u8         reserved_at_41[0x1c];
	u8         stall_cnt[0x3];
};

struct mlx5_ifc_qetcr_reg_bits {
	u8         operation_type[0x2];
	u8         cap_local_admin[0x1];
	u8         cap_remote_admin[0x1];
	u8         reserved_0[0x4];
	u8         port_number[0x8];
	u8         reserved_1[0x10];

	u8         reserved_2[0x20];

	u8         tc[8][0x40];

	u8         global_configuration[0x40];
};

struct mlx5_ifc_nodnic_ring_config_reg_bits {
	u8         queue_address_63_32[0x20];

	u8         queue_address_31_12[0x14];
	u8         reserved_0[0x6];
	u8         log_size[0x6];

	struct mlx5_ifc_nodnic_ring_doorbell_bits doorbell;

	u8         reserved_1[0x8];
	u8         queue_number[0x18];

	u8         q_key[0x20];

	u8         reserved_2[0x10];
	u8         pkey_index[0x10];

	u8         reserved_3[0x40];
};

struct mlx5_ifc_nodnic_cq_arming_word_bits {
	u8         reserved_0[0x8];
	u8         cq_ci[0x10];
	u8         reserved_1[0x8];
};

enum {
	MLX5_NODNIC_EVENT_WORD_LINK_TYPE_INFINIBAND  = 0x0,
	MLX5_NODNIC_EVENT_WORD_LINK_TYPE_ETHERNET    = 0x1,
};

enum {
	MLX5_NODNIC_EVENT_WORD_PORT_STATE_DOWN        = 0x0,
	MLX5_NODNIC_EVENT_WORD_PORT_STATE_INITIALIZE  = 0x1,
	MLX5_NODNIC_EVENT_WORD_PORT_STATE_ARMED       = 0x2,
	MLX5_NODNIC_EVENT_WORD_PORT_STATE_ACTIVE      = 0x3,
};

struct mlx5_ifc_nodnic_event_word_bits {
	u8         driver_reset_needed[0x1];
	u8         port_management_change_event[0x1];
	u8         reserved_0[0x19];
	u8         link_type[0x1];
	u8         port_state[0x4];
};

struct mlx5_ifc_nic_vport_change_event_bits {
	u8         reserved_0[0x10];
	u8         vport_num[0x10];

	u8         reserved_1[0xc0];
};

struct mlx5_ifc_pages_req_event_bits {
	u8         reserved_0[0x10];
	u8         function_id[0x10];

	u8         num_pages[0x20];

	u8         reserved_1[0xa0];
};

struct mlx5_ifc_cmd_inter_comp_event_bits {
	u8         command_completion_vector[0x20];

	u8         reserved_0[0xc0];
};

struct mlx5_ifc_stall_vl_event_bits {
	u8         reserved_0[0x18];
	u8         port_num[0x1];
	u8         reserved_1[0x3];
	u8         vl[0x4];

	u8         reserved_2[0xa0];
};

struct mlx5_ifc_db_bf_congestion_event_bits {
	u8         event_subtype[0x8];
	u8         reserved_0[0x8];
	u8         congestion_level[0x8];
	u8         reserved_1[0x8];

	u8         reserved_2[0xa0];
};

struct mlx5_ifc_gpio_event_bits {
	u8         reserved_0[0x60];

	u8         gpio_event_hi[0x20];

	u8         gpio_event_lo[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_port_state_change_event_bits {
	u8         reserved_0[0x40];

	u8         port_num[0x4];
	u8         reserved_1[0x1c];

	u8         reserved_2[0x80];
};

struct mlx5_ifc_dropped_packet_logged_bits {
	u8         reserved_0[0xe0];
};

enum {
	MLX5_CQ_ERROR_SYNDROME_CQ_OVERRUN                 = 0x1,
	MLX5_CQ_ERROR_SYNDROME_CQ_ACCESS_VIOLATION_ERROR  = 0x2,
};

struct mlx5_ifc_cq_error_bits {
	u8         reserved_0[0x8];
	u8         cqn[0x18];

	u8         reserved_1[0x20];

	u8         reserved_2[0x18];
	u8         syndrome[0x8];

	u8         reserved_3[0x80];
};

struct mlx5_ifc_rdma_page_fault_event_bits {
	u8         bytes_commited[0x20];

	u8         r_key[0x20];

	u8         reserved_0[0x10];
	u8         packet_len[0x10];

	u8         rdma_op_len[0x20];

	u8         rdma_va[0x40];

	u8         reserved_1[0x5];
	u8         rdma[0x1];
	u8         write[0x1];
	u8         requestor[0x1];
	u8         qp_number[0x18];
};

struct mlx5_ifc_wqe_associated_page_fault_event_bits {
	u8         bytes_committed[0x20];

	u8         reserved_0[0x10];
	u8         wqe_index[0x10];

	u8         reserved_1[0x10];
	u8         len[0x10];

	u8         reserved_2[0x60];

	u8         reserved_3[0x5];
	u8         rdma[0x1];
	u8         write_read[0x1];
	u8         requestor[0x1];
	u8         qpn[0x18];
};

enum {
	MLX5_QP_EVENTS_TYPE_QP  = 0x0,
	MLX5_QP_EVENTS_TYPE_RQ  = 0x1,
	MLX5_QP_EVENTS_TYPE_SQ  = 0x2,
};

struct mlx5_ifc_qp_events_bits {
	u8         reserved_0[0xa0];

	u8         type[0x8];
	u8         reserved_1[0x18];

	u8         reserved_2[0x8];
	u8         qpn_rqn_sqn[0x18];
};

struct mlx5_ifc_dct_events_bits {
	u8         reserved_0[0xc0];

	u8         reserved_1[0x8];
	u8         dct_number[0x18];
};

struct mlx5_ifc_comp_event_bits {
	u8         reserved_0[0xc0];

	u8         reserved_1[0x8];
	u8         cq_number[0x18];
};

struct mlx5_ifc_fw_version_bits {
	u8         major[0x10];
	u8         reserved_0[0x10];

	u8         minor[0x10];
	u8         subminor[0x10];

	u8         second[0x8];
	u8         minute[0x8];
	u8         hour[0x8];
	u8         reserved_1[0x8];

	u8         year[0x10];
	u8         month[0x8];
	u8         day[0x8];
};

enum {
	MLX5_QPC_STATE_RST        = 0x0,
	MLX5_QPC_STATE_INIT       = 0x1,
	MLX5_QPC_STATE_RTR        = 0x2,
	MLX5_QPC_STATE_RTS        = 0x3,
	MLX5_QPC_STATE_SQER       = 0x4,
	MLX5_QPC_STATE_SQD        = 0x5,
	MLX5_QPC_STATE_ERR        = 0x6,
	MLX5_QPC_STATE_SUSPENDED  = 0x9,
};

enum {
	MLX5_QPC_ST_RC            = 0x0,
	MLX5_QPC_ST_UC            = 0x1,
	MLX5_QPC_ST_UD            = 0x2,
	MLX5_QPC_ST_XRC           = 0x3,
	MLX5_QPC_ST_DCI           = 0x5,
	MLX5_QPC_ST_QP0           = 0x7,
	MLX5_QPC_ST_QP1           = 0x8,
	MLX5_QPC_ST_RAW_DATAGRAM  = 0x9,
	MLX5_QPC_ST_REG_UMR       = 0xc,
};

enum {
	MLX5_QP_PM_ARMED            = 0x0,
	MLX5_QP_PM_REARM            = 0x1,
	MLX5_QPC_PM_STATE_RESERVED  = 0x2,
	MLX5_QP_PM_MIGRATED         = 0x3,
};

enum {
	MLX5_QPC_END_PADDING_MODE_SCATTER_AS_IS                = 0x0,
	MLX5_QPC_END_PADDING_MODE_PAD_TO_CACHE_LINE_ALIGNMENT  = 0x1,
};

enum {
	MLX5_QPC_MTU_256_BYTES        = 0x1,
	MLX5_QPC_MTU_512_BYTES        = 0x2,
	MLX5_QPC_MTU_1K_BYTES         = 0x3,
	MLX5_QPC_MTU_2K_BYTES         = 0x4,
	MLX5_QPC_MTU_4K_BYTES         = 0x5,
	MLX5_QPC_MTU_RAW_ETHERNET_QP  = 0x7,
};

enum {
	MLX5_QPC_ATOMIC_MODE_IB_SPEC     = 0x1,
	MLX5_QPC_ATOMIC_MODE_ONLY_8B     = 0x2,
	MLX5_QPC_ATOMIC_MODE_UP_TO_8B    = 0x3,
	MLX5_QPC_ATOMIC_MODE_UP_TO_16B   = 0x4,
	MLX5_QPC_ATOMIC_MODE_UP_TO_32B   = 0x5,
	MLX5_QPC_ATOMIC_MODE_UP_TO_64B   = 0x6,
	MLX5_QPC_ATOMIC_MODE_UP_TO_128B  = 0x7,
	MLX5_QPC_ATOMIC_MODE_UP_TO_256B  = 0x8,
};

enum {
	MLX5_QPC_CS_REQ_DISABLE    = 0x0,
	MLX5_QPC_CS_REQ_UP_TO_32B  = 0x11,
	MLX5_QPC_CS_REQ_UP_TO_64B  = 0x22,
};

enum {
	MLX5_QPC_CS_RES_DISABLE    = 0x0,
	MLX5_QPC_CS_RES_UP_TO_32B  = 0x1,
	MLX5_QPC_CS_RES_UP_TO_64B  = 0x2,
};

enum {
	MLX5_QPC_TIMESTAMP_FORMAT_FREE_RUNNING = 0x0,
	MLX5_QPC_TIMESTAMP_FORMAT_DEFAULT      = 0x1,
	MLX5_QPC_TIMESTAMP_FORMAT_REAL_TIME    = 0x2,
};

struct mlx5_ifc_qpc_bits {
	u8         state[0x4];
	u8         lag_tx_port_affinity[0x4];
	u8         st[0x8];
	u8         reserved_1[0x3];
	u8         pm_state[0x2];
	u8         reserved_2[0x7];
	u8         end_padding_mode[0x2];
	u8         reserved_3[0x2];

	u8         wq_signature[0x1];
	u8         block_lb_mc[0x1];
	u8         atomic_like_write_en[0x1];
	u8         latency_sensitive[0x1];
	u8         reserved_4[0x1];
	u8         drain_sigerr[0x1];
	u8         reserved_5[0x2];
	u8         pd[0x18];

	u8         mtu[0x3];
	u8         log_msg_max[0x5];
	u8         reserved_6[0x1];
	u8         log_rq_size[0x4];
	u8         log_rq_stride[0x3];
	u8         no_sq[0x1];
	u8         log_sq_size[0x4];
	u8         reserved_at_55[0x3];
	u8         ts_format[0x2];
	u8         reserved_at_5a[0x1];
	u8         rlky[0x1];
	u8         ulp_stateless_offload_mode[0x4];

	u8         counter_set_id[0x8];
	u8         uar_page[0x18];

	u8         reserved_8[0x8];
	u8         user_index[0x18];

	u8         reserved_9[0x3];
	u8         log_page_size[0x5];
	u8         remote_qpn[0x18];

	struct mlx5_ifc_ads_bits primary_address_path;

	struct mlx5_ifc_ads_bits secondary_address_path;

	u8         log_ack_req_freq[0x4];
	u8         reserved_10[0x4];
	u8         log_sra_max[0x3];
	u8         reserved_11[0x2];
	u8         retry_count[0x3];
	u8         rnr_retry[0x3];
	u8         reserved_12[0x1];
	u8         fre[0x1];
	u8         cur_rnr_retry[0x3];
	u8         cur_retry_count[0x3];
	u8         reserved_13[0x5];

	u8         reserved_14[0x20];

	u8         reserved_15[0x8];
	u8         next_send_psn[0x18];

	u8         reserved_16[0x8];
	u8         cqn_snd[0x18];

	u8         reserved_at_400[0x8];

	u8         deth_sqpn[0x18];
	u8         reserved_17[0x20];

	u8         reserved_18[0x8];
	u8         last_acked_psn[0x18];

	u8         reserved_19[0x8];
	u8         ssn[0x18];

	u8         reserved_20[0x8];
	u8         log_rra_max[0x3];
	u8         reserved_21[0x1];
	u8         atomic_mode[0x4];
	u8         rre[0x1];
	u8         rwe[0x1];
	u8         rae[0x1];
	u8         reserved_22[0x1];
	u8         page_offset[0x6];
	u8         reserved_23[0x3];
	u8         cd_slave_receive[0x1];
	u8         cd_slave_send[0x1];
	u8         cd_master[0x1];

	u8         reserved_24[0x3];
	u8         min_rnr_nak[0x5];
	u8         next_rcv_psn[0x18];

	u8         reserved_25[0x8];
	u8         xrcd[0x18];

	u8         reserved_26[0x8];
	u8         cqn_rcv[0x18];

	u8         dbr_addr[0x40];

	u8         q_key[0x20];

	u8         reserved_27[0x5];
	u8         rq_type[0x3];
	u8         srqn_rmpn[0x18];

	u8         reserved_28[0x8];
	u8         rmsn[0x18];

	u8         hw_sq_wqebb_counter[0x10];
	u8         sw_sq_wqebb_counter[0x10];

	u8         hw_rq_counter[0x20];

	u8         sw_rq_counter[0x20];

	u8         reserved_29[0x20];

	u8         reserved_30[0xf];
	u8         cgs[0x1];
	u8         cs_req[0x8];
	u8         cs_res[0x8];

	u8         dc_access_key[0x40];

	u8         reserved_at_680[0x3];
	u8         dbr_umem_valid[0x1];

	u8         reserved_at_684[0xbc];
};

struct mlx5_ifc_roce_addr_layout_bits {
	u8         source_l3_address[16][0x8];

	u8         reserved_0[0x3];
	u8         vlan_valid[0x1];
	u8         vlan_id[0xc];
	u8         source_mac_47_32[0x10];

	u8         source_mac_31_0[0x20];

	u8         reserved_1[0x14];
	u8         roce_l3_type[0x4];
	u8         roce_version[0x8];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_rdbc_bits {
	u8         reserved_0[0x1c];
	u8         type[0x4];

	u8         reserved_1[0x20];

	u8         reserved_2[0x8];
	u8         psn[0x18];

	u8         rkey[0x20];

	u8         address[0x40];

	u8         byte_count[0x20];

	u8         reserved_3[0x20];

	u8         atomic_resp[32][0x8];
};

struct mlx5_ifc_vlan_bits {
	u8         ethtype[0x10];
	u8         prio[0x3];
	u8         cfi[0x1];
	u8         vid[0xc];
};

enum {
	MLX5_FLOW_METER_COLOR_RED       = 0x0,
	MLX5_FLOW_METER_COLOR_YELLOW    = 0x1,
	MLX5_FLOW_METER_COLOR_GREEN     = 0x2,
	MLX5_FLOW_METER_COLOR_UNDEFINED = 0x3,
};

enum {
	MLX5_EXE_ASO_FLOW_METER         = 0x2,
};

struct mlx5_ifc_exe_aso_ctrl_flow_meter_bits {
	u8        return_reg_id[0x4];
	u8        aso_type[0x4];
	u8        reserved_at_8[0x14];
	u8        action[0x1];
	u8        init_color[0x2];
	u8        meter_id[0x1];
};

union mlx5_ifc_exe_aso_ctrl {
	struct mlx5_ifc_exe_aso_ctrl_flow_meter_bits exe_aso_ctrl_flow_meter;
};

struct mlx5_ifc_execute_aso_bits {
	u8        valid[0x1];
	u8        reserved_at_1[0x7];
	u8        aso_object_id[0x18];

	union mlx5_ifc_exe_aso_ctrl exe_aso_ctrl;
};

enum {
	MLX5_FLOW_CONTEXT_ENCRYPT_DECRYPT_TYPE_IPSEC   = 0x0,
};

struct mlx5_ifc_flow_context_bits {
	struct mlx5_ifc_vlan_bits push_vlan;

	u8         group_id[0x20];

	u8         reserved_at_40[0x8];
	u8         flow_tag[0x18];

	u8         reserved_at_60[0x10];
	u8         action[0x10];

	u8         extended_destination[0x1];
	u8         reserved_at_81[0x1];
	u8         flow_source[0x2];
	u8         encrypt_decrypt_type[0x4];
	u8         destination_list_size[0x18];

	u8         reserved_at_a0[0x8];
	u8         flow_counter_list_size[0x18];

	u8         packet_reformat_id[0x20];

	u8         modify_header_id[0x20];

	struct mlx5_ifc_vlan_bits push_vlan_2;

	u8         encrypt_decrypt_obj_id[0x20];
	u8         reserved_at_140[0xc0];

	struct mlx5_ifc_fte_match_param_bits match_value;

	struct mlx5_ifc_execute_aso_bits execute_aso[4];

	u8         reserved_at_1300[0x500];

	union mlx5_ifc_dest_format_struct_flow_counter_list_auto_bits destination[];
};

enum {
	MLX5_XRC_SRQC_STATE_GOOD   = 0x0,
	MLX5_XRC_SRQC_STATE_ERROR  = 0x1,
};

struct mlx5_ifc_xrc_srqc_bits {
	u8         state[0x4];
	u8         log_xrc_srq_size[0x4];
	u8         reserved_0[0x18];

	u8         wq_signature[0x1];
	u8         cont_srq[0x1];
	u8         reserved_1[0x1];
	u8         rlky[0x1];
	u8         basic_cyclic_rcv_wqe[0x1];
	u8         log_rq_stride[0x3];
	u8         xrcd[0x18];

	u8         page_offset[0x6];
	u8         reserved_at_46[0x1];
	u8         dbr_umem_valid[0x1];
	u8         cqn[0x18];

	u8         reserved_3[0x20];

	u8         reserved_4[0x2];
	u8         log_page_size[0x6];
	u8         user_index[0x18];

	u8         reserved_5[0x20];

	u8         reserved_6[0x8];
	u8         pd[0x18];

	u8         lwm[0x10];
	u8         wqe_cnt[0x10];

	u8         reserved_7[0x40];

	u8         db_record_addr_h[0x20];

	u8         db_record_addr_l[0x1e];
	u8         reserved_8[0x2];

	u8         reserved_9[0x80];
};

struct mlx5_ifc_vnic_diagnostic_statistics_bits {
	u8         counter_error_queues[0x20];

	u8         total_error_queues[0x20];

	u8         send_queue_priority_update_flow[0x20];

	u8         reserved_at_60[0x20];

	u8         nic_receive_steering_discard[0x40];

	u8         receive_discard_vport_down[0x40];

	u8         transmit_discard_vport_down[0x40];

	u8         reserved_at_140[0xec0];
};

struct mlx5_ifc_traffic_counter_bits {
	u8         packets[0x40];

	u8         octets[0x40];
};

struct mlx5_ifc_tisc_bits {
	u8         strict_lag_tx_port_affinity[0x1];
	u8         tls_en[0x1];
	u8         reserved_at_2[0x2];
	u8         lag_tx_port_affinity[0x04];

	u8         reserved_at_8[0x4];
	u8         prio[0x4];
	u8         reserved_1[0x10];

	u8         reserved_2[0x100];

	u8         reserved_3[0x8];
	u8         transport_domain[0x18];

	u8         reserved_4[0x8];
	u8         underlay_qpn[0x18];

	u8         reserved_5[0x8];
	u8         pd[0x18];

	u8         reserved_6[0x380];
};

enum {
	MLX5_TIRC_DISP_TYPE_DIRECT    = 0x0,
	MLX5_TIRC_DISP_TYPE_INDIRECT  = 0x1,
};

enum {
	MLX5_TIRC_LRO_ENABLE_MASK_IPV4_LRO  = 0x1,
	MLX5_TIRC_LRO_ENABLE_MASK_IPV6_LRO  = 0x2,
};

enum {
	MLX5_TIRC_RX_HASH_FN_HASH_NONE           = 0x0,
	MLX5_TIRC_RX_HASH_FN_HASH_INVERTED_XOR8  = 0x1,
	MLX5_TIRC_RX_HASH_FN_HASH_TOEPLITZ       = 0x2,
};

enum {
	MLX5_TIRC_SELF_LB_EN_ENABLE_UNICAST    = 0x1,
	MLX5_TIRC_SELF_LB_EN_ENABLE_MULTICAST  = 0x2,
};

struct mlx5_ifc_tirc_bits {
	u8         reserved_0[0x20];

	u8         disp_type[0x4];
	u8         tls_en[0x1];
	u8         reserved_at_25[0x1b];

	u8         reserved_2[0x40];

	u8         reserved_3[0x4];
	u8         lro_timeout_period_usecs[0x10];
	u8         lro_enable_mask[0x4];
	u8         lro_max_msg_sz[0x8];

	u8         reserved_4[0x40];

	u8         reserved_5[0x8];
	u8         inline_rqn[0x18];

	u8         rx_hash_symmetric[0x1];
	u8         reserved_6[0x1];
	u8         tunneled_offload_en[0x1];
	u8         reserved_7[0x5];
	u8         indirect_table[0x18];

	u8         rx_hash_fn[0x4];
	u8         reserved_8[0x2];
	u8         self_lb_en[0x2];
	u8         transport_domain[0x18];

	u8         rx_hash_toeplitz_key[10][0x20];

	struct mlx5_ifc_rx_hash_field_select_bits rx_hash_field_selector_outer;

	struct mlx5_ifc_rx_hash_field_select_bits rx_hash_field_selector_inner;

	u8         reserved_9[0x4c0];
};

enum {
	MLX5_SRQC_STATE_GOOD   = 0x0,
	MLX5_SRQC_STATE_ERROR  = 0x1,
};

struct mlx5_ifc_srqc_bits {
	u8         state[0x4];
	u8         log_srq_size[0x4];
	u8         reserved_0[0x18];

	u8         wq_signature[0x1];
	u8         cont_srq[0x1];
	u8         reserved_1[0x1];
	u8         rlky[0x1];
	u8         reserved_2[0x1];
	u8         log_rq_stride[0x3];
	u8         xrcd[0x18];

	u8         page_offset[0x6];
	u8         reserved_3[0x2];
	u8         cqn[0x18];

	u8         reserved_4[0x20];

	u8         reserved_5[0x2];
	u8         log_page_size[0x6];
	u8         reserved_6[0x18];

	u8         reserved_7[0x20];

	u8         reserved_8[0x8];
	u8         pd[0x18];

	u8         lwm[0x10];
	u8         wqe_cnt[0x10];

	u8         reserved_9[0x40];

	u8	   dbr_addr[0x40];

	u8	   reserved_10[0x80];
};

enum {
	MLX5_SQC_STATE_RST  = 0x0,
	MLX5_SQC_STATE_RDY  = 0x1,
	MLX5_SQC_STATE_ERR  = 0x3,
};

enum {
	MLX5_SQC_TIMESTAMP_FORMAT_FREE_RUNNING = 0x0,
	MLX5_SQC_TIMESTAMP_FORMAT_DEFAULT      = 0x1,
	MLX5_SQC_TIMESTAMP_FORMAT_REAL_TIME    = 0x2,
};

struct mlx5_ifc_sqc_bits {
	u8         rlkey[0x1];
	u8         cd_master[0x1];
	u8         fre[0x1];
	u8         flush_in_error_en[0x1];
	u8         allow_multi_pkt_send_wqe[0x1];
	u8         min_wqe_inline_mode[0x3];
	u8         state[0x4];
	u8         reg_umr[0x1];
	u8         allow_swp[0x1];
	u8         reserved_at_e[0x4];
	u8	   qos_remap_en[0x1];
	u8	   reserved_at_d[0x7];
	u8         ts_format[0x2];
	u8         reserved_at_1c[0x4];

	u8         reserved_1[0x8];
	u8         user_index[0x18];

	u8         reserved_2[0x8];
	u8         cqn[0x18];

	u8         reserved_3[0x80];

	u8         qos_para_vport_number[0x10];
	u8         packet_pacing_rate_limit_index[0x10];

	u8         tis_lst_sz[0x10];
	u8         qos_queue_group_id[0x10];

	u8	   reserved_4[0x8];
	u8	   queue_handle[0x18];

	u8         reserved_5[0x20];

	u8         reserved_6[0x8];
	u8         tis_num_0[0x18];

	struct mlx5_ifc_wq_bits wq;
};

struct mlx5_ifc_query_pp_rate_limit_in_bits {
	u8	   opcode[0x10];
	u8	   uid[0x10];

	u8	   reserved1[0x10];
	u8         op_mod[0x10];

	u8         reserved2[0x10];
        u8         rate_limit_index[0x10];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_pp_context_bits {
	u8	   rate_limit[0x20];

	u8	   burst_upper_bound[0x20];

	u8	   reserved_1[0xc];
	u8	   rate_mode[0x4];
	u8	   typical_packet_size[0x10];

	u8	   reserved_2[0x8];
	u8	   qos_handle[0x18];

	u8	   reserved_3[0x40];
};

struct mlx5_ifc_query_pp_rate_limit_out_bits {
        u8	   status[0x8];
	u8         reserved_1[0x18];

        u8         syndrome[0x20];

        u8         reserved_2[0x40];

	struct mlx5_ifc_pp_context_bits pp_context;
};

enum {
	MLX5_TSAR_TYPE_DWRR = 0,
	MLX5_TSAR_TYPE_ROUND_ROUBIN = 1,
	MLX5_TSAR_TYPE_ETS = 2
};

struct mlx5_ifc_tsar_element_attributes_bits {
	u8         reserved_0[0x8];
	u8         tsar_type[0x8];
	u8	   reserved_1[0x10];
};

struct mlx5_ifc_vport_element_attributes_bits {
	u8         reserved_0[0x10];
	u8         vport_number[0x10];
};

struct mlx5_ifc_vport_tc_element_attributes_bits {
	u8         traffic_class[0x10];
	u8         vport_number[0x10];
};

struct mlx5_ifc_para_vport_tc_element_attributes_bits {
	u8         reserved_0[0x0C];
	u8         traffic_class[0x04];
	u8         qos_para_vport_number[0x10];
};

enum {
	MLX5_SCHEDULING_CONTEXT_ELEMENT_TYPE_TSAR           = 0x0,
	MLX5_SCHEDULING_CONTEXT_ELEMENT_TYPE_VPORT          = 0x1,
	MLX5_SCHEDULING_CONTEXT_ELEMENT_TYPE_VPORT_TC       = 0x2,
	MLX5_SCHEDULING_CONTEXT_ELEMENT_TYPE_PARA_VPORT_TC  = 0x3,
};

struct mlx5_ifc_scheduling_context_bits {
	u8         element_type[0x8];
	u8         reserved_at_8[0x18];

	u8         element_attributes[0x20];

	u8         parent_element_id[0x20];

	u8         reserved_at_60[0x40];

	u8         bw_share[0x20];

	u8         max_average_bw[0x20];

	u8         reserved_at_e0[0x120];
};

struct mlx5_ifc_rqtc_bits {
	u8         reserved_0[0xa0];

	u8         reserved_1[0x10];
	u8         rqt_max_size[0x10];

	u8         reserved_2[0x10];
	u8         rqt_actual_size[0x10];

	u8         reserved_3[0x6a0];

	struct mlx5_ifc_rq_num_bits rq_num[0];
};

enum {
	MLX5_RQC_RQ_TYPE_MEMORY_RQ_INLINE      = 0x0,
	MLX5_RQC_RQ_TYPE_MEMORY_RQ_RMP         = 0x1,
};

enum {
	MLX5_RQC_STATE_RST  = 0x0,
	MLX5_RQC_STATE_RDY  = 0x1,
	MLX5_RQC_STATE_ERR  = 0x3,
};

enum {
	MLX5_RQC_DROPLESS_MODE_DISABLE        = 0x0,
	MLX5_RQC_DROPLESS_MODE_ENABLE         = 0x1,
};

enum {
	MLX5_RQC_TIMESTAMP_FORMAT_FREE_RUNNING = 0x0,
	MLX5_RQC_TIMESTAMP_FORMAT_DEFAULT      = 0x1,
	MLX5_RQC_TIMESTAMP_FORMAT_REAL_TIME    = 0x2,
};

struct mlx5_ifc_rqc_bits {
	u8         rlkey[0x1];
	u8         delay_drop_en[0x1];
	u8         scatter_fcs[0x1];
	u8         vlan_strip_disable[0x1];
	u8         mem_rq_type[0x4];
	u8         state[0x4];
	u8         reserved_1[0x1];
	u8         flush_in_error_en[0x1];
	u8         reserved_at_e[0xc];
	u8         ts_format[0x2];
	u8         reserved_at_1c[0x4];

	u8         reserved_3[0x8];
	u8         user_index[0x18];

	u8         reserved_4[0x8];
	u8         cqn[0x18];

	u8         counter_set_id[0x8];
	u8         reserved_5[0x18];

	u8         reserved_6[0x8];
	u8         rmpn[0x18];

	u8         reserved_7[0xe0];

	struct mlx5_ifc_wq_bits wq;
};

enum {
	MLX5_RMPC_STATE_RDY  = 0x1,
	MLX5_RMPC_STATE_ERR  = 0x3,
};

struct mlx5_ifc_rmpc_bits {
	u8         reserved_0[0x8];
	u8         state[0x4];
	u8         reserved_1[0x14];

	u8         basic_cyclic_rcv_wqe[0x1];
	u8         reserved_2[0x1f];

	u8         reserved_3[0x140];

	struct mlx5_ifc_wq_bits wq;
};

enum {
	MLX5_NIC_VPORT_CONTEXT_ALLOWED_LIST_TYPE_CURRENT_UC_MAC_ADDRESS  = 0x0,
	MLX5_NIC_VPORT_CONTEXT_ALLOWED_LIST_TYPE_CURRENT_MC_MAC_ADDRESS  = 0x1,
	MLX5_NIC_VPORT_CONTEXT_ALLOWED_LIST_TYPE_VLAN_LIST               = 0x2,
};

struct mlx5_ifc_nic_vport_context_bits {
	u8         reserved_0[0x5];
	u8         min_wqe_inline_mode[0x3];
	u8         reserved_1[0x15];
	u8         disable_mc_local_lb[0x1];
	u8         disable_uc_local_lb[0x1];
	u8         roce_en[0x1];

	u8         arm_change_event[0x1];
	u8         reserved_2[0x1a];
	u8         event_on_mtu[0x1];
	u8         event_on_promisc_change[0x1];
	u8         event_on_vlan_change[0x1];
	u8         event_on_mc_address_change[0x1];
	u8         event_on_uc_address_change[0x1];

	u8         reserved_3[0xe0];

	u8         reserved_4[0x10];
	u8         mtu[0x10];

	u8         system_image_guid[0x40];

	u8         port_guid[0x40];

	u8         node_guid[0x40];

	u8         reserved_5[0x140];

	u8         qkey_violation_counter[0x10];
	u8         reserved_6[0x10];

	u8         reserved_7[0x420];

	u8         promisc_uc[0x1];
	u8         promisc_mc[0x1];
	u8         promisc_all[0x1];
	u8         reserved_8[0x2];
	u8         allowed_list_type[0x3];
	u8         reserved_9[0xc];
	u8         allowed_list_size[0xc];

	struct mlx5_ifc_mac_address_layout_bits permanent_address;

	u8         reserved_10[0x20];

	u8         current_uc_mac_address[0][0x40];
};

enum {
	MLX5_ACCESS_MODE_PA        = 0x0,
	MLX5_ACCESS_MODE_MTT       = 0x1,
	MLX5_ACCESS_MODE_KLM       = 0x2,
	MLX5_ACCESS_MODE_KSM       = 0x3,
	MLX5_ACCESS_MODE_SW_ICM    = 0x4,
	MLX5_ACCESS_MODE_MEMIC     = 0x5,
};

struct mlx5_ifc_mkc_bits {
	u8         reserved_at_0[0x1];
	u8         free[0x1];
	u8         reserved_at_2[0x1];
	u8         access_mode_4_2[0x3];
	u8         reserved_at_6[0x7];
	u8         relaxed_ordering_write[0x1];
	u8         reserved_at_e[0x1];
	u8         small_fence_on_rdma_read_response[0x1];
	u8         umr_en[0x1];
	u8         a[0x1];
	u8         rw[0x1];
	u8         rr[0x1];
	u8         lw[0x1];
	u8         lr[0x1];
	u8         access_mode[0x2];
	u8         reserved_2[0x8];

	u8         qpn[0x18];
	u8         mkey_7_0[0x8];

	u8         reserved_3[0x20];

	u8         length64[0x1];
	u8         bsf_en[0x1];
	u8         sync_umr[0x1];
	u8         reserved_4[0x2];
	u8         expected_sigerr_count[0x1];
	u8         reserved_5[0x1];
	u8         en_rinval[0x1];
	u8         pd[0x18];

	u8         start_addr[0x40];

	u8         len[0x40];

	u8         bsf_octword_size[0x20];

	u8         reserved_6[0x80];

	u8         translations_octword_size[0x20];

	u8         reserved_at_1c0[0x19];
	u8         relaxed_ordering_read[0x1];
	u8         reserved_at_1d9[0x1];
	u8         log_page_size[0x5];

	u8         reserved_8[0x20];
};

struct mlx5_ifc_pkey_bits {
	u8         reserved_0[0x10];
	u8         pkey[0x10];
};

struct mlx5_ifc_array128_auto_bits {
	u8         array128_auto[16][0x8];
};

enum {
	MLX5_HCA_VPORT_CONTEXT_FIELD_SELECT_PORT_GUID           = 0x0,
	MLX5_HCA_VPORT_CONTEXT_FIELD_SELECT_NODE_GUID           = 0x1,
	MLX5_HCA_VPORT_CONTEXT_FIELD_SELECT_VPORT_STATE_POLICY  = 0x2,
};

enum {
	MLX5_HCA_VPORT_CONTEXT_PORT_PHYSICAL_STATE_SLEEP                      = 0x1,
	MLX5_HCA_VPORT_CONTEXT_PORT_PHYSICAL_STATE_POLLING                    = 0x2,
	MLX5_HCA_VPORT_CONTEXT_PORT_PHYSICAL_STATE_DISABLED                   = 0x3,
	MLX5_HCA_VPORT_CONTEXT_PORT_PHYSICAL_STATE_PORTCONFIGURATIONTRAINING  = 0x4,
	MLX5_HCA_VPORT_CONTEXT_PORT_PHYSICAL_STATE_LINKUP                     = 0x5,
	MLX5_HCA_VPORT_CONTEXT_PORT_PHYSICAL_STATE_LINKERRORRECOVERY          = 0x6,
	MLX5_HCA_VPORT_CONTEXT_PORT_PHYSICAL_STATE_PHYTEST                    = 0x7,
};

enum {
	MLX5_HCA_VPORT_CONTEXT_VPORT_STATE_POLICY_DOWN    = 0x0,
	MLX5_HCA_VPORT_CONTEXT_VPORT_STATE_POLICY_UP      = 0x1,
	MLX5_HCA_VPORT_CONTEXT_VPORT_STATE_POLICY_FOLLOW  = 0x2,
};

enum {
	MLX5_HCA_VPORT_CONTEXT_PORT_STATE_DOWN    = 0x1,
	MLX5_HCA_VPORT_CONTEXT_PORT_STATE_INIT    = 0x2,
	MLX5_HCA_VPORT_CONTEXT_PORT_STATE_ARM     = 0x3,
	MLX5_HCA_VPORT_CONTEXT_PORT_STATE_ACTIVE  = 0x4,
};

enum {
	MLX5_HCA_VPORT_CONTEXT_VPORT_STATE_DOWN    = 0x1,
	MLX5_HCA_VPORT_CONTEXT_VPORT_STATE_INIT    = 0x2,
	MLX5_HCA_VPORT_CONTEXT_VPORT_STATE_ARM     = 0x3,
	MLX5_HCA_VPORT_CONTEXT_VPORT_STATE_ACTIVE  = 0x4,
};

struct mlx5_ifc_hca_vport_context_bits {
	u8         field_select[0x20];

	u8         reserved_0[0xe0];

	u8         sm_virt_aware[0x1];
	u8         has_smi[0x1];
	u8         has_raw[0x1];
	u8         grh_required[0x1];
	u8         reserved_1[0x1];
	u8         min_wqe_inline_mode[0x3];
	u8         reserved_2[0x8];
	u8         port_physical_state[0x4];
	u8         vport_state_policy[0x4];
	u8         port_state[0x4];
	u8         vport_state[0x4];

	u8         reserved_3[0x20];

	u8         system_image_guid[0x40];

	u8         port_guid[0x40];

	u8         node_guid[0x40];

	u8         cap_mask1[0x20];

	u8         cap_mask1_field_select[0x20];

	u8         cap_mask2[0x20];

	u8         cap_mask2_field_select[0x20];

	u8         reserved_4[0x80];

	u8         lid[0x10];
	u8         reserved_5[0x4];
	u8         init_type_reply[0x4];
	u8         lmc[0x3];
	u8         subnet_timeout[0x5];

	u8         sm_lid[0x10];
	u8         sm_sl[0x4];
	u8         reserved_6[0xc];

	u8         qkey_violation_counter[0x10];
	u8         pkey_violation_counter[0x10];

	u8         reserved_7[0xca0];
};

union mlx5_ifc_hca_cap_union_bits {
	struct mlx5_ifc_cmd_hca_cap_bits cmd_hca_cap;
	struct mlx5_ifc_cmd_hca_cap_2_bits cmd_hca_cap_2;
	struct mlx5_ifc_odp_cap_bits odp_cap;
	struct mlx5_ifc_atomic_caps_bits atomic_caps;
	struct mlx5_ifc_roce_cap_bits roce_cap;
	struct mlx5_ifc_per_protocol_networking_offload_caps_bits per_protocol_networking_offload_caps;
	struct mlx5_ifc_flow_table_nic_cap_bits flow_table_nic_cap;
	struct mlx5_ifc_flow_table_eswitch_cap_bits flow_table_eswitch_cap;
	struct mlx5_ifc_e_switch_cap_bits e_switch_cap;
	struct mlx5_ifc_snapshot_cap_bits snapshot_cap;
	struct mlx5_ifc_debug_cap_bits diagnostic_counters_cap;
	struct mlx5_ifc_qos_cap_bits qos_cap;
	struct mlx5_ifc_tls_capabilities_bits tls_capabilities;
	u8         reserved_0[0x8000];
};

enum {
	MLX5_FLOW_TABLE_CONTEXT_TABLE_MISS_ACTION_DEFAULT = 0x0,
	MLX5_FLOW_TABLE_CONTEXT_TABLE_MISS_ACTION_IDENTIFIED = 0x1,
};

struct mlx5_ifc_flow_table_context_bits {
        u8         reformat_en[0x1];
        u8         decap_en[0x1];
        u8         sw_owner[0x1];
        u8         termination_table[0x1];
        u8         table_miss_action[0x4];
        u8         level[0x8];
        u8         reserved_at_10[0x8];
        u8         log_size[0x8];

        u8         reserved_at_20[0x8];
        u8         table_miss_id[0x18];

        u8         reserved_at_40[0x8];
        u8         lag_master_next_table_id[0x18];

        u8         reserved_at_60[0x60];

        u8         sw_owner_icm_root_1[0x40];

        u8         sw_owner_icm_root_0[0x40];

};

struct mlx5_ifc_esw_vport_context_bits {
	u8         reserved_0[0x3];
	u8         vport_svlan_strip[0x1];
	u8         vport_cvlan_strip[0x1];
	u8         vport_svlan_insert[0x1];
	u8         vport_cvlan_insert[0x2];
	u8         reserved_1[0x18];

	u8         reserved_2[0x20];

	u8         svlan_cfi[0x1];
	u8         svlan_pcp[0x3];
	u8         svlan_id[0xc];
	u8         cvlan_cfi[0x1];
	u8         cvlan_pcp[0x3];
	u8         cvlan_id[0xc];

	u8         reserved_3[0x7a0];
};

enum {
	MLX5_EQC_STATUS_OK                = 0x0,
	MLX5_EQC_STATUS_EQ_WRITE_FAILURE  = 0xa,
};

enum {
	MLX5_EQ_STATE_ARMED = 0x9,
	MLX5_EQ_STATE_FIRED = 0xa,
};

struct mlx5_ifc_eqc_bits {
	u8         status[0x4];
	u8         reserved_0[0x9];
	u8         ec[0x1];
	u8         oi[0x1];
	u8         reserved_1[0x5];
	u8         st[0x4];
	u8         reserved_2[0x8];

	u8         reserved_3[0x20];

	u8         reserved_4[0x14];
	u8         page_offset[0x6];
	u8         reserved_5[0x6];

	u8         reserved_6[0x3];
	u8         log_eq_size[0x5];
	u8         uar_page[0x18];

	u8         reserved_7[0x20];

	u8         reserved_8[0x18];
	u8         intr[0x8];

	u8         reserved_9[0x3];
	u8         log_page_size[0x5];
	u8         reserved_10[0x18];

	u8         reserved_11[0x60];

	u8         reserved_12[0x8];
	u8         consumer_counter[0x18];

	u8         reserved_13[0x8];
	u8         producer_counter[0x18];

	u8         reserved_14[0x80];
};

enum {
	MLX5_DCTC_STATE_ACTIVE    = 0x0,
	MLX5_DCTC_STATE_DRAINING  = 0x1,
	MLX5_DCTC_STATE_DRAINED   = 0x2,
};

enum {
	MLX5_DCTC_CS_RES_DISABLE    = 0x0,
	MLX5_DCTC_CS_RES_NA         = 0x1,
	MLX5_DCTC_CS_RES_UP_TO_64B  = 0x2,
};

enum {
	MLX5_DCTC_MTU_256_BYTES  = 0x1,
	MLX5_DCTC_MTU_512_BYTES  = 0x2,
	MLX5_DCTC_MTU_1K_BYTES   = 0x3,
	MLX5_DCTC_MTU_2K_BYTES   = 0x4,
	MLX5_DCTC_MTU_4K_BYTES   = 0x5,
};

struct mlx5_ifc_dctc_bits {
	u8         reserved_0[0x4];
	u8         state[0x4];
	u8         reserved_1[0x18];

	u8         reserved_2[0x8];
	u8         user_index[0x18];

	u8         reserved_3[0x8];
	u8         cqn[0x18];

	u8         counter_set_id[0x8];
	u8         atomic_mode[0x4];
	u8         rre[0x1];
	u8         rwe[0x1];
	u8         rae[0x1];
	u8         atomic_like_write_en[0x1];
	u8         latency_sensitive[0x1];
	u8         rlky[0x1];
	u8         reserved_4[0xe];

	u8         reserved_5[0x8];
	u8         cs_res[0x8];
	u8         reserved_6[0x3];
	u8         min_rnr_nak[0x5];
	u8         reserved_7[0x8];

	u8         reserved_8[0x8];
	u8         srqn[0x18];

	u8         reserved_9[0x8];
	u8         pd[0x18];

	u8         tclass[0x8];
	u8         reserved_10[0x4];
	u8         flow_label[0x14];

	u8         dc_access_key[0x40];

	u8         reserved_11[0x5];
	u8         mtu[0x3];
	u8         port[0x8];
	u8         pkey_index[0x10];

	u8         reserved_12[0x8];
	u8         my_addr_index[0x8];
	u8         reserved_13[0x8];
	u8         hop_limit[0x8];

	u8         dc_access_key_violation_count[0x20];

	u8         reserved_14[0x14];
	u8         dei_cfi[0x1];
	u8         eth_prio[0x3];
	u8         ecn[0x2];
	u8         dscp[0x6];

	u8         reserved_15[0x40];
};

enum {
	MLX5_CQC_STATUS_OK             = 0x0,
	MLX5_CQC_STATUS_CQ_OVERFLOW    = 0x9,
	MLX5_CQC_STATUS_CQ_WRITE_FAIL  = 0xa,
};

enum {
	CQE_SIZE_64                = 0x0,
	CQE_SIZE_128               = 0x1,
};

enum {
	MLX5_CQ_PERIOD_MODE_START_FROM_EQE  = 0x0,
	MLX5_CQ_PERIOD_MODE_START_FROM_CQE  = 0x1,
};

enum {
	MLX5_CQ_STATE_SOLICITED_ARMED                     = 0x6,
	MLX5_CQ_STATE_ARMED                               = 0x9,
	MLX5_CQ_STATE_FIRED                               = 0xa,
};

struct mlx5_ifc_cqc_bits {
	u8         status[0x4];
	u8         reserved_at_4[0x2];
	u8         dbr_umem_valid[0x1];
	u8         reserved_at_7[0x1];
	u8         cqe_sz[0x3];
	u8         cc[0x1];
	u8         reserved_1[0x1];
	u8         scqe_break_moderation_en[0x1];
	u8         oi[0x1];
	u8         cq_period_mode[0x2];
	u8         cqe_compression_en[0x1];
	u8         mini_cqe_res_format[0x2];
	u8         st[0x4];
	u8         reserved_2[0x8];

	u8         reserved_3[0x20];

	u8         reserved_4[0x14];
	u8         page_offset[0x6];
	u8         reserved_5[0x6];

	u8         reserved_6[0x3];
	u8         log_cq_size[0x5];
	u8         uar_page[0x18];

	u8         reserved_7[0x4];
	u8         cq_period[0xc];
	u8         cq_max_count[0x10];

	u8         reserved_8[0x18];
	u8         c_eqn[0x8];

	u8         reserved_9[0x3];
	u8         log_page_size[0x5];
	u8         reserved_10[0x18];

	u8         reserved_11[0x20];

	u8         reserved_12[0x8];
	u8         last_notified_index[0x18];

	u8         reserved_13[0x8];
	u8         last_solicit_index[0x18];

	u8         reserved_14[0x8];
	u8         consumer_counter[0x18];

	u8         reserved_15[0x8];
	u8         producer_counter[0x18];

	u8         reserved_16[0x40];

	u8         dbr_addr[0x40];
};

union mlx5_ifc_cong_control_roce_ecn_auto_bits {
	struct mlx5_ifc_cong_control_802_1qau_rp_bits cong_control_802_1qau_rp;
	struct mlx5_ifc_cong_control_r_roce_ecn_rp_bits cong_control_r_roce_ecn_rp;
	struct mlx5_ifc_cong_control_r_roce_ecn_np_bits cong_control_r_roce_ecn_np;
	u8         reserved_0[0x800];
};

struct mlx5_ifc_query_adapter_param_block_bits {
	u8         reserved_0[0xc0];

	u8         reserved_1[0x8];
	u8         ieee_vendor_id[0x18];

	u8         reserved_2[0x10];
	u8         vsd_vendor_id[0x10];

	u8         vsd[208][0x8];

	u8         vsd_contd_psid[16][0x8];
};

union mlx5_ifc_modify_field_select_resize_field_select_auto_bits {
	struct mlx5_ifc_modify_field_select_bits modify_field_select;
	struct mlx5_ifc_resize_field_select_bits resize_field_select;
	u8         reserved_0[0x20];
};

union mlx5_ifc_field_select_802_1_r_roce_auto_bits {
	struct mlx5_ifc_field_select_802_1qau_rp_bits field_select_802_1qau_rp;
	struct mlx5_ifc_field_select_r_roce_rp_bits field_select_r_roce_rp;
	struct mlx5_ifc_field_select_r_roce_np_bits field_select_r_roce_np;
	u8         reserved_0[0x20];
};

struct mlx5_ifc_bufferx_reg_bits {
	u8         reserved_0[0x6];
	u8         lossy[0x1];
	u8         epsb[0x1];
	u8         reserved_1[0xc];
	u8         size[0xc];

	u8         xoff_threshold[0x10];
	u8         xon_threshold[0x10];
};

struct mlx5_ifc_config_item_bits {
	u8         valid[0x2];
	u8         reserved_0[0x2];
	u8         header_type[0x2];
	u8         reserved_1[0x2];
	u8         default_location[0x1];
	u8         reserved_2[0x7];
	u8         version[0x4];
	u8         reserved_3[0x3];
	u8         length[0x9];

	u8         type[0x20];

	u8         reserved_4[0x10];
	u8         crc16[0x10];
};

enum {
	MLX5_XRQC_STATE_GOOD   = 0x0,
	MLX5_XRQC_STATE_ERROR  = 0x1,
};

enum {
	MLX5_XRQC_TOPOLOGY_NO_SPECIAL_TOPOLOGY = 0x0,
	MLX5_XRQC_TOPOLOGY_TAG_MATCHING        = 0x1,
};

enum {
	MLX5_XRQC_OFFLOAD_RNDV = 0x1,
};

struct mlx5_ifc_tag_matching_topology_context_bits {
	u8         log_matching_list_sz[0x4];
	u8         reserved_at_4[0xc];
	u8         append_next_index[0x10];

	u8         sw_phase_cnt[0x10];
	u8         hw_phase_cnt[0x10];

	u8         reserved_at_40[0x40];
};

struct mlx5_ifc_xrqc_bits {
	u8         state[0x4];
	u8         rlkey[0x1];
	u8         reserved_at_5[0xf];
	u8         topology[0x4];
	u8         reserved_at_18[0x4];
	u8         offload[0x4];

	u8         reserved_at_20[0x8];
	u8         user_index[0x18];

	u8         reserved_at_40[0x8];
	u8         cqn[0x18];

	u8         reserved_at_60[0xa0];

	struct mlx5_ifc_tag_matching_topology_context_bits tag_matching_topology_context;

	u8         reserved_at_180[0x280];

	struct mlx5_ifc_wq_bits wq;
};

struct mlx5_ifc_nodnic_port_config_reg_bits {
	struct mlx5_ifc_nodnic_event_word_bits event;

	u8         network_en[0x1];
	u8         dma_en[0x1];
	u8         promisc_en[0x1];
	u8         promisc_multicast_en[0x1];
	u8         reserved_0[0x17];
	u8         receive_filter_en[0x5];

	u8         reserved_1[0x10];
	u8         mac_47_32[0x10];

	u8         mac_31_0[0x20];

	u8         receive_filters_mgid_mac[64][0x8];

	u8         gid[16][0x8];

	u8         reserved_2[0x10];
	u8         lid[0x10];

	u8         reserved_3[0xc];
	u8         sm_sl[0x4];
	u8         sm_lid[0x10];

	u8         completion_address_63_32[0x20];

	u8         completion_address_31_12[0x14];
	u8         reserved_4[0x6];
	u8         log_cq_size[0x6];

	u8         working_buffer_address_63_32[0x20];

	u8         working_buffer_address_31_12[0x14];
	u8         reserved_5[0xc];

	struct mlx5_ifc_nodnic_cq_arming_word_bits arm_cq;

	u8         pkey_index[0x10];
	u8         pkey[0x10];

	struct mlx5_ifc_nodnic_ring_config_reg_bits send_ring0;

	struct mlx5_ifc_nodnic_ring_config_reg_bits send_ring1;

	struct mlx5_ifc_nodnic_ring_config_reg_bits receive_ring0;

	struct mlx5_ifc_nodnic_ring_config_reg_bits receive_ring1;

	u8         reserved_6[0x400];
};

union mlx5_ifc_event_auto_bits {
	struct mlx5_ifc_comp_event_bits comp_event;
	struct mlx5_ifc_dct_events_bits dct_events;
	struct mlx5_ifc_qp_events_bits qp_events;
	struct mlx5_ifc_wqe_associated_page_fault_event_bits wqe_associated_page_fault_event;
	struct mlx5_ifc_rdma_page_fault_event_bits rdma_page_fault_event;
	struct mlx5_ifc_cq_error_bits cq_error;
	struct mlx5_ifc_dropped_packet_logged_bits dropped_packet_logged;
	struct mlx5_ifc_port_state_change_event_bits port_state_change_event;
	struct mlx5_ifc_gpio_event_bits gpio_event;
	struct mlx5_ifc_db_bf_congestion_event_bits db_bf_congestion_event;
	struct mlx5_ifc_stall_vl_event_bits stall_vl_event;
	struct mlx5_ifc_cmd_inter_comp_event_bits cmd_inter_comp_event;
	struct mlx5_ifc_pages_req_event_bits pages_req_event;
	struct mlx5_ifc_nic_vport_change_event_bits nic_vport_change_event;
	u8         reserved_0[0xe0];
};

struct mlx5_ifc_health_buffer_bits {
	u8         reserved_0[0x100];

	u8         assert_existptr[0x20];

	u8         assert_callra[0x20];

	u8         reserved_1[0x40];

	u8         fw_version[0x20];

	u8         hw_id[0x20];

	u8         reserved_2[0x20];

	u8         irisc_index[0x8];
	u8         synd[0x8];
	u8         ext_synd[0x10];
};

struct mlx5_ifc_register_loopback_control_bits {
	u8         no_lb[0x1];
	u8         reserved_0[0x7];
	u8         port[0x8];
	u8         reserved_1[0x10];

	u8         reserved_2[0x60];
};

struct mlx5_ifc_lrh_bits {
	u8	vl[4];
	u8	lver[4];
	u8	sl[4];
	u8	reserved2[2];
	u8	lnh[2];
	u8	dlid[16];
	u8	reserved5[5];
	u8	pkt_len[11];
	u8	slid[16];
};

struct mlx5_ifc_icmd_set_wol_rol_out_bits {
	u8         reserved_0[0x40];

	u8         reserved_1[0x10];
	u8         rol_mode[0x8];
	u8         wol_mode[0x8];
};

struct mlx5_ifc_icmd_set_wol_rol_in_bits {
	u8         reserved_0[0x40];

	u8         rol_mode_valid[0x1];
	u8         wol_mode_valid[0x1];
	u8         reserved_1[0xe];
	u8         rol_mode[0x8];
	u8         wol_mode[0x8];

	u8         reserved_2[0x7a0];
};

struct mlx5_ifc_icmd_set_virtual_mac_in_bits {
	u8         virtual_mac_en[0x1];
	u8         mac_aux_v[0x1];
	u8         reserved_0[0x1e];

	u8         reserved_1[0x40];

	struct mlx5_ifc_mac_address_layout_bits virtual_mac;

	u8         reserved_2[0x760];
};

struct mlx5_ifc_icmd_query_virtual_mac_out_bits {
	u8         virtual_mac_en[0x1];
	u8         mac_aux_v[0x1];
	u8         reserved_0[0x1e];

	struct mlx5_ifc_mac_address_layout_bits permanent_mac;

	struct mlx5_ifc_mac_address_layout_bits virtual_mac;

	u8         reserved_1[0x760];
};

struct mlx5_ifc_icmd_query_fw_info_out_bits {
	struct mlx5_ifc_fw_version_bits fw_version;

	u8         reserved_0[0x10];
	u8         hash_signature[0x10];

	u8         psid[16][0x8];

	u8         reserved_1[0x6e0];
};

struct mlx5_ifc_icmd_query_cap_in_bits {
	u8         reserved_0[0x10];
	u8         capability_group[0x10];
};

struct mlx5_ifc_icmd_query_cap_general_bits {
	u8         nv_access[0x1];
	u8         fw_info_psid[0x1];
	u8         reserved_0[0x1e];

	u8         reserved_1[0x16];
	u8         rol_s[0x1];
	u8         rol_g[0x1];
	u8         reserved_2[0x1];
	u8         wol_s[0x1];
	u8         wol_g[0x1];
	u8         wol_a[0x1];
	u8         wol_b[0x1];
	u8         wol_m[0x1];
	u8         wol_u[0x1];
	u8         wol_p[0x1];
};

struct mlx5_ifc_icmd_ocbb_query_header_stats_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         reserved_1[0x7e0];
};

struct mlx5_ifc_icmd_ocbb_query_etoc_stats_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         reserved_1[0x7e0];
};

struct mlx5_ifc_icmd_ocbb_init_in_bits {
	u8         address_hi[0x20];

	u8         address_lo[0x20];

	u8         reserved_0[0x7c0];
};

struct mlx5_ifc_icmd_init_ocsd_in_bits {
	u8         reserved_0[0x20];

	u8         address_hi[0x20];

	u8         address_lo[0x20];

	u8         reserved_1[0x7a0];
};

struct mlx5_ifc_icmd_access_reg_out_bits {
	u8         reserved_0[0x11];
	u8         status[0x7];
	u8         reserved_1[0x8];

	u8         register_id[0x10];
	u8         reserved_2[0x10];

	u8         reserved_3[0x40];

	u8         reserved_4[0x5];
	u8         len[0xb];
	u8         reserved_5[0x10];

	u8         register_data[0][0x20];
};

enum {
	MLX5_ICMD_ACCESS_REG_IN_METHOD_QUERY  = 0x1,
	MLX5_ICMD_ACCESS_REG_IN_METHOD_WRITE  = 0x2,
};

struct mlx5_ifc_icmd_access_reg_in_bits {
	u8         constant_1[0x5];
	u8         constant_2[0xb];
	u8         reserved_0[0x10];

	u8         register_id[0x10];
	u8         reserved_1[0x1];
	u8         method[0x7];
	u8         constant_3[0x8];

	u8         reserved_2[0x40];

	u8         constant_4[0x5];
	u8         len[0xb];
	u8         reserved_3[0x10];

	u8         register_data[0][0x20];
};

enum {
	MLX5_TEARDOWN_HCA_OUT_FORCE_STATE_SUCCESS = 0x0,
	MLX5_TEARDOWN_HCA_OUT_FORCE_STATE_FAIL = 0x1,
};

struct mlx5_ifc_teardown_hca_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x3f];

	u8	   state[0x1];
};

enum {
	MLX5_TEARDOWN_HCA_IN_PROFILE_GRACEFUL_CLOSE  = 0x0,
	MLX5_TEARDOWN_HCA_IN_PROFILE_FORCE_CLOSE     = 0x1,
	MLX5_TEARDOWN_HCA_IN_PROFILE_PREPARE_FAST_TEARDOWN = 0x2,
};

struct mlx5_ifc_teardown_hca_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x10];
	u8         profile[0x10];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_set_delay_drop_params_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x40];
};

struct mlx5_ifc_set_delay_drop_params_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x20];

	u8         reserved_at_60[0x10];
	u8         delay_drop_timeout[0x10];
};

struct mlx5_ifc_query_delay_drop_params_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x20];

	u8         reserved_at_60[0x10];
	u8         delay_drop_timeout[0x10];
};

struct mlx5_ifc_query_delay_drop_params_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x40];
};

struct mlx5_ifc_suspend_qp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_suspend_qp_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_sqerr2rts_qp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_sqerr2rts_qp_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];

	u8         opt_param_mask[0x20];

	u8         reserved_4[0x20];

	struct mlx5_ifc_qpc_bits qpc;

	u8         reserved_5[0x80];
};

struct mlx5_ifc_sqd2rts_qp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_sqd2rts_qp_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];

	u8         opt_param_mask[0x20];

	u8         reserved_4[0x20];

	struct mlx5_ifc_qpc_bits qpc;

	u8         reserved_5[0x80];
};

struct mlx5_ifc_set_wol_rol_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_set_wol_rol_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         rol_mode_valid[0x1];
	u8         wol_mode_valid[0x1];
	u8         reserved_2[0xe];
	u8         rol_mode[0x8];
	u8         wol_mode[0x8];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_set_roce_address_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_set_roce_address_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         roce_address_index[0x10];
	u8         reserved_2[0x10];

	u8         reserved_3[0x20];

	struct mlx5_ifc_roce_addr_layout_bits roce_address;
};

struct mlx5_ifc_set_rdb_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_set_rdb_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x18];
	u8         rdb_list_size[0x8];

	struct mlx5_ifc_rdbc_bits rdb_context[0];
};

struct mlx5_ifc_set_mad_demux_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

enum {
	MLX5_SET_MAD_DEMUX_IN_DEMUX_MODE_PASS_ALL   = 0x0,
	MLX5_SET_MAD_DEMUX_IN_DEMUX_MODE_SELECTIVE  = 0x2,
};

struct mlx5_ifc_set_mad_demux_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x20];

	u8         reserved_3[0x6];
	u8         demux_mode[0x2];
	u8         reserved_4[0x18];
};

struct mlx5_ifc_set_l2_table_entry_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_set_l2_table_entry_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x60];

	u8         reserved_3[0x8];
	u8         table_index[0x18];

	u8         reserved_4[0x20];

	u8         reserved_5[0x13];
	u8         vlan_valid[0x1];
	u8         vlan[0xc];

	struct mlx5_ifc_mac_address_layout_bits mac_address;

	u8         reserved_6[0xc0];
};

struct mlx5_ifc_set_issi_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_set_issi_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x10];
	u8         current_issi[0x10];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_set_hca_cap_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_set_hca_cap_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];

	union mlx5_ifc_hca_cap_union_bits capability;
};

enum {
	MLX5_SET_FTE_MODIFY_ENABLE_MASK_ACTION			= 0x0,
	MLX5_SET_FTE_MODIFY_ENABLE_MASK_FLOW_TAG		= 0x1,
	MLX5_SET_FTE_MODIFY_ENABLE_MASK_DESTINATION_LIST	= 0x2,
	MLX5_SET_FTE_MODIFY_ENABLE_MASK_FLOW_COUNTERS		= 0x3
};

struct mlx5_ifc_set_flow_table_root_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_set_flow_table_root_in_bits {
        u8         opcode[0x10];
        u8         reserved_at_10[0x10];

        u8         reserved_at_20[0x10];
        u8         op_mod[0x10];

        u8         other_vport[0x1];
        u8         reserved_at_41[0xf];
        u8         vport_number[0x10];

        u8         reserved_at_60[0x20];

        u8         table_type[0x8];
        u8         reserved_at_88[0x7];
        u8         table_of_other_vport[0x1];
        u8         table_vport_number[0x10];

        u8         reserved_at_a0[0x8];
        u8         table_id[0x18];

        u8         reserved_at_c0[0x8];
        u8         underlay_qpn[0x18];
        u8         table_eswitch_owner_vhca_id_valid[0x1];
        u8         reserved_at_e1[0xf];
        u8         table_eswitch_owner_vhca_id[0x10];
        u8         reserved_at_100[0x100];
};

struct mlx5_ifc_set_fte_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_set_fte_in_bits {
        u8         opcode[0x10];
        u8         reserved_at_10[0x10];

        u8         reserved_at_20[0x10];
        u8         op_mod[0x10];

        u8         other_vport[0x1];
        u8         reserved_at_41[0xf];
        u8         vport_number[0x10];

        u8         reserved_at_60[0x20];

        u8         table_type[0x8];
        u8         reserved_at_88[0x18];

        u8         reserved_at_a0[0x8];
        u8         table_id[0x18];

        u8         ignore_flow_level[0x1];
        u8         reserved_at_c1[0x17];
        u8         modify_enable_mask[0x8];

        u8         reserved_at_e0[0x20];

        u8         flow_index[0x20];

        u8         reserved_at_120[0xe0];

        struct mlx5_ifc_flow_context_bits flow_context;
};

struct mlx5_ifc_set_driver_version_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_set_driver_version_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];

	u8         driver_version[64][0x8];
};

struct mlx5_ifc_set_dc_cnak_trace_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_set_dc_cnak_trace_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         enable[0x1];
	u8         reserved_2[0x1f];

	u8         reserved_3[0x160];

	struct mlx5_ifc_cmd_pas_bits pas;
};

struct mlx5_ifc_set_burst_size_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_set_burst_size_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x20];

	u8         reserved_3[0x9];
	u8         device_burst_size[0x17];
};

struct mlx5_ifc_rts2rts_qp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_rts2rts_qp_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];

	u8         opt_param_mask[0x20];

	u8         reserved_4[0x20];

	struct mlx5_ifc_qpc_bits qpc;

	u8         reserved_5[0x80];
};

struct mlx5_ifc_rtr2rts_qp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_rtr2rts_qp_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];

	u8         opt_param_mask[0x20];

	u8         reserved_4[0x20];

	struct mlx5_ifc_qpc_bits qpc;

	u8         reserved_5[0x80];
};

struct mlx5_ifc_rst2init_qp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_rst2init_qp_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];

	u8         opt_param_mask[0x20];

	u8         reserved_4[0x20];

	struct mlx5_ifc_qpc_bits qpc;

	u8         reserved_5[0x80];
};

struct mlx5_ifc_query_xrq_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x40];

	struct mlx5_ifc_xrqc_bits xrq_context;
};

struct mlx5_ifc_query_xrq_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x8];
	u8         xrqn[0x18];

	u8         reserved_at_60[0x20];
};

struct mlx5_ifc_resume_qp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_resume_qp_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_xrc_srq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_xrc_srqc_bits xrc_srq_context_entry;

	u8         reserved_2[0x600];

	u8         pas[0][0x40];
};

struct mlx5_ifc_query_xrc_srq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         xrc_srqn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_wol_rol_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x10];
	u8         rol_mode[0x8];
	u8         wol_mode[0x8];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_query_wol_rol_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

enum {
	MLX5_QUERY_VPORT_STATE_OUT_STATE_DOWN  = 0x0,
	MLX5_QUERY_VPORT_STATE_OUT_STATE_UP    = 0x1,
};

struct mlx5_ifc_query_vport_state_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x20];

	u8         reserved_2[0x18];
	u8         admin_state[0x4];
	u8         state[0x4];
};

enum {
	MLX5_QUERY_VPORT_STATE_IN_OP_MOD_VNIC_VPORT  = 0x0,
	MLX5_QUERY_VPORT_STATE_IN_OP_MOD_ESW_VPORT   = 0x1,
	MLX5_QUERY_VPORT_STATE_IN_OP_MOD_UPLINK      = 0x2,
};

enum {
        MLX5_FLOW_CONTEXT_ACTION_ALLOW     = 0x1,
        MLX5_FLOW_CONTEXT_ACTION_DROP      = 0x2,
        MLX5_FLOW_CONTEXT_ACTION_FWD_DEST  = 0x4,
        MLX5_FLOW_CONTEXT_ACTION_COUNT     = 0x8,
        MLX5_FLOW_CONTEXT_ACTION_PACKET_REFORMAT = 0x10,
        MLX5_FLOW_CONTEXT_ACTION_DECAP     = 0x20,
        MLX5_FLOW_CONTEXT_ACTION_MOD_HDR   = 0x40,
        MLX5_FLOW_CONTEXT_ACTION_VLAN_POP  = 0x80,
        MLX5_FLOW_CONTEXT_ACTION_VLAN_PUSH = 0x100,
        MLX5_FLOW_CONTEXT_ACTION_VLAN_POP_2  = 0x400,
        MLX5_FLOW_CONTEXT_ACTION_VLAN_PUSH_2 = 0x800,
        MLX5_FLOW_CONTEXT_ACTION_CRYPTO_DECRYPT = 0x1000,
        MLX5_FLOW_CONTEXT_ACTION_CRYPTO_ENCRYPT = 0x2000,
        MLX5_FLOW_CONTEXT_ACTION_EXECUTE_ASO = 0x4000,
};

struct mlx5_ifc_query_vport_state_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xf];
	u8         vport_number[0x10];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_vnic_env_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x40];

	struct mlx5_ifc_vnic_diagnostic_statistics_bits vport_env;
};

enum {
	MLX5_QUERY_VNIC_ENV_IN_OP_MOD_VPORT_DIAG_STATISTICS  = 0x0,
};

struct mlx5_ifc_query_vnic_env_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_at_41[0xf];
	u8         vport_number[0x10];

	u8         reserved_at_60[0x20];
};

struct mlx5_ifc_query_vport_counter_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_traffic_counter_bits received_errors;

	struct mlx5_ifc_traffic_counter_bits transmit_errors;

	struct mlx5_ifc_traffic_counter_bits received_ib_unicast;

	struct mlx5_ifc_traffic_counter_bits transmitted_ib_unicast;

	struct mlx5_ifc_traffic_counter_bits received_ib_multicast;

	struct mlx5_ifc_traffic_counter_bits transmitted_ib_multicast;

	struct mlx5_ifc_traffic_counter_bits received_eth_broadcast;

	struct mlx5_ifc_traffic_counter_bits transmitted_eth_broadcast;

	struct mlx5_ifc_traffic_counter_bits received_eth_unicast;

	struct mlx5_ifc_traffic_counter_bits transmitted_eth_unicast;

	struct mlx5_ifc_traffic_counter_bits received_eth_multicast;

	struct mlx5_ifc_traffic_counter_bits transmitted_eth_multicast;

	u8         reserved_2[0xa00];
};

enum {
	MLX5_QUERY_VPORT_COUNTER_IN_OP_MOD_VPORT_COUNTERS  = 0x0,
};

struct mlx5_ifc_query_vport_counter_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xb];
	u8         port_num[0x4];
	u8         vport_number[0x10];

	u8         reserved_3[0x60];

	u8         clear[0x1];
	u8         reserved_4[0x1f];

	u8         reserved_5[0x20];
};

struct mlx5_ifc_query_tis_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_tisc_bits tis_context;
};

struct mlx5_ifc_query_tis_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         tisn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_tir_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0xc0];

	struct mlx5_ifc_tirc_bits tir_context;
};

struct mlx5_ifc_query_tir_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         tirn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_srq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_srqc_bits srq_context_entry;

	u8         reserved_2[0x600];

	u8         pas[0][0x40];
};

struct mlx5_ifc_query_srq_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         srqn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_sq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0xc0];

	struct mlx5_ifc_sqc_bits sq_context;
};

struct mlx5_ifc_query_sq_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         sqn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_special_contexts_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8	   dump_fill_mkey[0x20];

	u8         resd_lkey[0x20];
};

struct mlx5_ifc_query_special_contexts_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_query_scheduling_element_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0xc0];

	struct mlx5_ifc_scheduling_context_bits scheduling_context;

	u8         reserved_at_300[0x100];
};

enum {
	MLX5_SCHEDULING_ELEMENT_IN_HIERARCHY_E_SWITCH = 0x2,
};

struct mlx5_ifc_query_scheduling_element_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         scheduling_hierarchy[0x8];
	u8         reserved_at_48[0x18];

	u8         scheduling_element_id[0x20];

	u8         reserved_at_80[0x180];
};

struct mlx5_ifc_query_rqt_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0xc0];

	struct mlx5_ifc_rqtc_bits rqt_context;
};

struct mlx5_ifc_query_rqt_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         rqtn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_rq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0xc0];

	struct mlx5_ifc_rqc_bits rq_context;
};

struct mlx5_ifc_query_rq_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         rqn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_roce_address_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_roce_addr_layout_bits roce_address;
};

struct mlx5_ifc_query_roce_address_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         roce_address_index[0x10];
	u8         reserved_2[0x10];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_rmp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0xc0];

	struct mlx5_ifc_rmpc_bits rmp_context;
};

struct mlx5_ifc_query_rmp_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         rmpn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_rdb_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x20];

	u8         reserved_2[0x18];
	u8         rdb_list_size[0x8];

	struct mlx5_ifc_rdbc_bits rdb_context[0];
};

struct mlx5_ifc_query_rdb_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_qp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	u8         opt_param_mask[0x20];

	u8         reserved_2[0x20];

	struct mlx5_ifc_qpc_bits qpc;

	u8         reserved_3[0x80];

	u8         pas[0][0x40];
};

struct mlx5_ifc_query_qp_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_q_counter_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	u8         rx_write_requests[0x20];

	u8         reserved_2[0x20];

	u8         rx_read_requests[0x20];

	u8         reserved_3[0x20];

	u8         rx_atomic_requests[0x20];

	u8         reserved_4[0x20];

	u8         rx_dct_connect[0x20];

	u8         reserved_5[0x20];

	u8         out_of_buffer[0x20];

	u8         reserved_7[0x20];

	u8         out_of_sequence[0x20];

	u8         reserved_8[0x20];

	u8         duplicate_request[0x20];

	u8         reserved_9[0x20];

	u8         rnr_nak_retry_err[0x20];

	u8         reserved_10[0x20];

	u8         packet_seq_err[0x20];

	u8         reserved_11[0x20];

	u8         implied_nak_seq_err[0x20];

	u8         reserved_12[0x20];

	u8         local_ack_timeout_err[0x20];

	u8         reserved_13[0x20];

	u8         resp_rnr_nak[0x20];

	u8         reserved_14[0x20];

	u8         req_rnr_retries_exceeded[0x20];

	u8         reserved_15[0x460];
};

struct mlx5_ifc_query_q_counter_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x80];

	u8         clear[0x1];
	u8         reserved_3[0x1f];

	u8         reserved_4[0x18];
	u8         counter_set_id[0x8];
};

struct mlx5_ifc_query_pages_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x10];
	u8         function_id[0x10];

	u8         num_pages[0x20];
};

enum {
	MLX5_QUERY_PAGES_IN_OP_MOD_BOOT_PAGES	  = 0x1,
	MLX5_QUERY_PAGES_IN_OP_MOD_INIT_PAGES	  = 0x2,
	MLX5_QUERY_PAGES_IN_OP_MOD_REGULAR_PAGES  = 0x3,
};

struct mlx5_ifc_query_pages_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x10];
	u8         function_id[0x10];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_nic_vport_context_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_nic_vport_context_bits nic_vport_context;
};

struct mlx5_ifc_query_nic_vport_context_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xf];
	u8         vport_number[0x10];

	u8         reserved_3[0x5];
	u8         allowed_list_type[0x3];
	u8         reserved_4[0x18];
};

struct mlx5_ifc_query_mkey_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_mkc_bits memory_key_mkey_entry;

	u8         reserved_2[0x600];

	u8         bsf0_klm0_pas_mtt0_1[16][0x8];

	u8         bsf1_klm1_pas_mtt2_3[16][0x8];
};

struct mlx5_ifc_query_mkey_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         mkey_index[0x18];

	u8         pg_access[0x1];
	u8         reserved_3[0x1f];
};

struct mlx5_ifc_query_mad_demux_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	u8         mad_dumux_parameters_block[0x20];
};

struct mlx5_ifc_query_mad_demux_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_query_l2_table_entry_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0xa0];

	u8         reserved_2[0x13];
	u8         vlan_valid[0x1];
	u8         vlan[0xc];

	struct mlx5_ifc_mac_address_layout_bits mac_address;

	u8         reserved_3[0xc0];
};

struct mlx5_ifc_query_l2_table_entry_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x60];

	u8         reserved_3[0x8];
	u8         table_index[0x18];

	u8         reserved_4[0x140];
};

struct mlx5_ifc_query_issi_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x10];
	u8         current_issi[0x10];

	u8         reserved_2[0xa0];

	u8         supported_issi_reserved[76][0x8];
	u8         supported_issi_dw0[0x20];
};

struct mlx5_ifc_query_issi_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_query_hca_vport_pkey_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_pkey_bits pkey[0];
};

struct mlx5_ifc_query_hca_vport_pkey_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xb];
	u8         port_num[0x4];
	u8         vport_number[0x10];

	u8         reserved_3[0x10];
	u8         pkey_index[0x10];
};

struct mlx5_ifc_query_hca_vport_gid_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x20];

	u8         gids_num[0x10];
	u8         reserved_2[0x10];

	struct mlx5_ifc_array128_auto_bits gid[0];
};

struct mlx5_ifc_query_hca_vport_gid_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xb];
	u8         port_num[0x4];
	u8         vport_number[0x10];

	u8         reserved_3[0x10];
	u8         gid_index[0x10];
};

struct mlx5_ifc_query_hca_vport_context_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_hca_vport_context_bits hca_vport_context;
};

struct mlx5_ifc_query_hca_vport_context_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xb];
	u8         port_num[0x4];
	u8         vport_number[0x10];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_hca_cap_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	union mlx5_ifc_hca_cap_union_bits capability;
};

struct mlx5_ifc_query_hca_cap_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_query_flow_table_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x80];

	struct mlx5_ifc_flow_table_context_bits flow_table_context;
};

struct mlx5_ifc_query_flow_table_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xf];
	u8         vport_number[0x10];

	u8         reserved_3[0x20];

	u8         table_type[0x8];
	u8         reserved_4[0x18];

	u8         reserved_5[0x8];
	u8         table_id[0x18];

	u8         reserved_6[0x140];
};

struct mlx5_ifc_query_fte_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x1c0];

	struct mlx5_ifc_flow_context_bits flow_context;
};

struct mlx5_ifc_query_fte_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xf];
	u8         vport_number[0x10];

	u8         reserved_3[0x20];

	u8         table_type[0x8];
	u8         reserved_4[0x18];

	u8         reserved_5[0x8];
	u8         table_id[0x18];

	u8         reserved_6[0x40];

	u8         flow_index[0x20];

	u8         reserved_7[0xe0];
};

enum {
	MLX5_QUERY_FLOW_GROUP_OUT_MATCH_CRITERIA_ENABLE_OUTER_HEADERS    = 0x0,
	MLX5_QUERY_FLOW_GROUP_OUT_MATCH_CRITERIA_ENABLE_MISC_PARAMETERS  = 0x1,
	MLX5_QUERY_FLOW_GROUP_OUT_MATCH_CRITERIA_ENABLE_INNER_HEADERS    = 0x2,
};

struct mlx5_ifc_query_flow_group_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0xa0];

	u8         start_flow_index[0x20];

	u8         reserved_2[0x20];

	u8         end_flow_index[0x20];

	u8         reserved_3[0xa0];

	u8         reserved_4[0x18];
	u8         match_criteria_enable[0x8];

	struct mlx5_ifc_fte_match_param_bits match_criteria;

	u8         reserved_5[0xe00];
};

struct mlx5_ifc_query_flow_group_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xf];
	u8         vport_number[0x10];

	u8         reserved_3[0x20];

	u8         table_type[0x8];
	u8         reserved_4[0x18];

	u8         reserved_5[0x8];
	u8         table_id[0x18];

	u8         group_id[0x20];

	u8         reserved_6[0x120];
};

struct mlx5_ifc_query_flow_counter_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x40];

	struct mlx5_ifc_traffic_counter_bits flow_statistics[0];
};

struct mlx5_ifc_query_flow_counter_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x80];

	u8         clear[0x1];
	u8         reserved_at_c1[0xf];
	u8         num_of_counters[0x10];

	u8         reserved_at_e0[0x10];
	u8         flow_counter_id[0x10];
};

struct mlx5_ifc_query_esw_vport_context_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_esw_vport_context_bits esw_vport_context;
};

struct mlx5_ifc_query_esw_vport_context_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xf];
	u8         vport_number[0x10];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_eq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_eqc_bits eq_context_entry;

	u8         reserved_2[0x40];

	u8         event_bitmask[0x40];

	u8         reserved_3[0x580];

	u8         pas[0][0x40];
};

struct mlx5_ifc_query_eq_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x18];
	u8         eq_number[0x8];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_set_action_in_bits {
        u8         action_type[0x4];
        u8         field[0xc];
        u8         reserved_at_10[0x3];
        u8         offset[0x5];
        u8         reserved_at_18[0x3];
        u8         length[0x5];

        u8         data[0x20];
};

struct mlx5_ifc_add_action_in_bits {
        u8         action_type[0x4];
        u8         field[0xc];
        u8         reserved_at_10[0x10];

        u8         data[0x20];
};

struct mlx5_ifc_copy_action_in_bits {
        u8         action_type[0x4];
        u8         src_field[0xc];
        u8         reserved_at_10[0x3];
        u8         src_offset[0x5];
        u8         reserved_at_18[0x3];
        u8         length[0x5];

        u8         reserved_at_20[0x4];
        u8         dst_field[0xc];
        u8         reserved_at_30[0x3];
        u8         dst_offset[0x5];
        u8         reserved_at_38[0x8];
};

union mlx5_ifc_set_add_copy_action_in_auto_bits {
        struct mlx5_ifc_set_action_in_bits  set_action_in;
        struct mlx5_ifc_add_action_in_bits  add_action_in;
        struct mlx5_ifc_copy_action_in_bits copy_action_in;
        u8         reserved_at_0[0x40];
};

enum {
        MLX5_ACTION_TYPE_SET   = 0x1,
        MLX5_ACTION_TYPE_ADD   = 0x2,
        MLX5_ACTION_TYPE_COPY  = 0x3,
};

enum {
        MLX5_ACTION_IN_FIELD_OUT_SMAC_47_16    = 0x1,
        MLX5_ACTION_IN_FIELD_OUT_SMAC_15_0     = 0x2,
        MLX5_ACTION_IN_FIELD_OUT_ETHERTYPE     = 0x3,
        MLX5_ACTION_IN_FIELD_OUT_DMAC_47_16    = 0x4,
        MLX5_ACTION_IN_FIELD_OUT_DMAC_15_0     = 0x5,
        MLX5_ACTION_IN_FIELD_OUT_IP_DSCP       = 0x6,
        MLX5_ACTION_IN_FIELD_OUT_TCP_FLAGS     = 0x7,
        MLX5_ACTION_IN_FIELD_OUT_TCP_SPORT     = 0x8,
        MLX5_ACTION_IN_FIELD_OUT_TCP_DPORT     = 0x9,
        MLX5_ACTION_IN_FIELD_OUT_IP_TTL        = 0xa,
        MLX5_ACTION_IN_FIELD_OUT_UDP_SPORT     = 0xb,
        MLX5_ACTION_IN_FIELD_OUT_UDP_DPORT     = 0xc,
        MLX5_ACTION_IN_FIELD_OUT_SIPV6_127_96  = 0xd,
        MLX5_ACTION_IN_FIELD_OUT_SIPV6_95_64   = 0xe,
        MLX5_ACTION_IN_FIELD_OUT_SIPV6_63_32   = 0xf,
        MLX5_ACTION_IN_FIELD_OUT_SIPV6_31_0    = 0x10,
        MLX5_ACTION_IN_FIELD_OUT_DIPV6_127_96  = 0x11,
        MLX5_ACTION_IN_FIELD_OUT_DIPV6_95_64   = 0x12,
        MLX5_ACTION_IN_FIELD_OUT_DIPV6_63_32   = 0x13,
        MLX5_ACTION_IN_FIELD_OUT_DIPV6_31_0    = 0x14,
        MLX5_ACTION_IN_FIELD_OUT_SIPV4         = 0x15,
        MLX5_ACTION_IN_FIELD_OUT_DIPV4         = 0x16,
        MLX5_ACTION_IN_FIELD_OUT_FIRST_VID     = 0x17,
        MLX5_ACTION_IN_FIELD_OUT_IPV6_HOPLIMIT = 0x47,
        MLX5_ACTION_IN_FIELD_METADATA_REG_A    = 0x49,
        MLX5_ACTION_IN_FIELD_METADATA_REG_B    = 0x50,
        MLX5_ACTION_IN_FIELD_METADATA_REG_C_0  = 0x51,
        MLX5_ACTION_IN_FIELD_METADATA_REG_C_1  = 0x52,
        MLX5_ACTION_IN_FIELD_METADATA_REG_C_2  = 0x53,
        MLX5_ACTION_IN_FIELD_METADATA_REG_C_3  = 0x54,
        MLX5_ACTION_IN_FIELD_METADATA_REG_C_4  = 0x55,
        MLX5_ACTION_IN_FIELD_METADATA_REG_C_5  = 0x56,
        MLX5_ACTION_IN_FIELD_METADATA_REG_C_6  = 0x57,
        MLX5_ACTION_IN_FIELD_METADATA_REG_C_7  = 0x58,
        MLX5_ACTION_IN_FIELD_OUT_TCP_SEQ_NUM   = 0x59,
        MLX5_ACTION_IN_FIELD_OUT_TCP_ACK_NUM   = 0x5B,
        MLX5_ACTION_IN_FIELD_IPSEC_SYNDROME    = 0x5D,
        MLX5_ACTION_IN_FIELD_OUT_EMD_47_32     = 0x6F,
        MLX5_ACTION_IN_FIELD_OUT_EMD_31_0      = 0x70,
};

struct mlx5_ifc_alloc_modify_header_context_out_bits {
        u8         status[0x8];
        u8         reserved_at_8[0x18];

        u8         syndrome[0x20];

        u8         modify_header_id[0x20];

        u8         reserved_at_60[0x20];
};

struct mlx5_ifc_alloc_modify_header_context_in_bits {
        u8         opcode[0x10];
        u8         reserved_at_10[0x10];

        u8         reserved_at_20[0x10];
        u8         op_mod[0x10];

        u8         reserved_at_40[0x20];

        u8         table_type[0x8];
        u8         reserved_at_68[0x10];
        u8         num_of_actions[0x8];

        union mlx5_ifc_set_add_copy_action_in_auto_bits actions[];
};

struct mlx5_ifc_dealloc_modify_header_context_out_bits {
        u8         status[0x8];
        u8         reserved_at_8[0x18];

        u8         syndrome[0x20];

        u8         reserved_at_40[0x40];
};

struct mlx5_ifc_dealloc_modify_header_context_in_bits {
        u8         opcode[0x10];
        u8         reserved_at_10[0x10];

        u8         reserved_at_20[0x10];
        u8         op_mod[0x10];

        u8         modify_header_id[0x20];

        u8         reserved_at_60[0x20];
};

struct mlx5_ifc_query_modify_header_context_in_bits {
        u8         opcode[0x10];
        u8         uid[0x10];

        u8         reserved_at_20[0x10];
        u8         op_mod[0x10];

        u8         modify_header_id[0x20];

        u8         reserved_at_60[0xa0];
};

struct mlx5_ifc_query_dct_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_dctc_bits dct_context_entry;

	u8         reserved_2[0x180];
};

struct mlx5_ifc_query_dct_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         dctn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_dc_cnak_trace_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         enable[0x1];
	u8         reserved_1[0x1f];

	u8         reserved_2[0x160];

	struct mlx5_ifc_cmd_pas_bits pas;
};

struct mlx5_ifc_query_dc_cnak_trace_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_packet_reformat_context_in_bits {
        u8         reformat_type[0x8];
        u8         reserved_at_8[0x4];
        u8         reformat_param_0[0x4];
        u8         reserved_at_10[0x6];
        u8         reformat_data_size[0xa];

        u8         reformat_param_1[0x8];
        u8         reserved_at_28[0x8];
        u8         reformat_data[2][0x8];

        u8         more_reformat_data[][0x8];
};

struct mlx5_ifc_query_packet_reformat_context_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0xa0];

	struct mlx5_ifc_packet_reformat_context_in_bits packet_reformat_context[0];
};

struct mlx5_ifc_query_packet_reformat_context_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         packet_reformat_id[0x20];

	u8         reserved_at_60[0xa0];
};

struct mlx5_ifc_alloc_packet_reformat_context_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         packet_reformat_id[0x20];

	u8         reserved_at_60[0x20];
};

enum mlx5_reformat_ctx_type {
	MLX5_REFORMAT_TYPE_L2_TO_VXLAN = 0x0,
	MLX5_REFORMAT_TYPE_L2_TO_NVGRE = 0x1,
	MLX5_REFORMAT_TYPE_L2_TO_L2_TUNNEL = 0x2,
	MLX5_REFORMAT_TYPE_L3_TUNNEL_TO_L2 = 0x3,
	MLX5_REFORMAT_TYPE_L2_TO_L3_TUNNEL = 0x4,
	MLX5_REFORMAT_TYPE_ADD_ESP_TRANSPORT_OVER_IPV4 = 0x5,
	MLX5_REFORMAT_TYPE_ADD_ESP_TRANSPORT_OVER_UDPV4 = 0x7,
	MLX5_REFORMAT_TYPE_DEL_ESP_TRANSPORT = 0x8,
	MLX5_REFORMAT_TYPE_DEL_ESP_TRANSPORT_OVER_UDP = 0xa,
	MLX5_REFORMAT_TYPE_ADD_ESP_TRANSPORT_OVER_IPV6 = 0xb,
	MLX5_REFORMAT_TYPE_ADD_ESP_TRANSPORT_OVER_UDPV6 = 0xc,
};

struct mlx5_ifc_alloc_packet_reformat_context_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0xa0];

	struct mlx5_ifc_packet_reformat_context_in_bits packet_reformat_context;
};

struct mlx5_ifc_dealloc_packet_reformat_context_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x40];
};

struct mlx5_ifc_dealloc_packet_reformat_context_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_20[0x10];
	u8         op_mod[0x10];

	u8         packet_reformat_id[0x20];

	u8         reserved_60[0x20];
};

struct mlx5_ifc_diagnostic_cntr_struct_bits {
	u8         counter_id[0x10];
	u8         sample_id[0x10];

	u8         time_stamp_31_0[0x20];

	u8         counter_value_h[0x20];

	u8         counter_value_l[0x20];
};

enum {
	MLX5_DIAGNOSTIC_PARAMS_CONTEXT_ENABLE_ENABLE   = 0x1,
	MLX5_DIAGNOSTIC_PARAMS_CONTEXT_ENABLE_DISABLE  = 0x0,
};

struct mlx5_ifc_query_cq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_cqc_bits cq_context;

	u8         reserved_2[0x600];

	u8         pas[0][0x40];
};

struct mlx5_ifc_query_cq_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         cqn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_cong_status_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x20];

	u8         enable[0x1];
	u8         tag_enable[0x1];
	u8         reserved_2[0x1e];
};

struct mlx5_ifc_query_cong_status_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x18];
	u8         priority[0x4];
	u8         cong_protocol[0x4];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_cong_statistics_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	u8         rp_cur_flows[0x20];

	u8         sum_flows[0x20];

	u8         rp_cnp_ignored_high[0x20];

	u8         rp_cnp_ignored_low[0x20];

	u8         rp_cnp_handled_high[0x20];

	u8         rp_cnp_handled_low[0x20];

	u8         reserved_2[0x100];

	u8         time_stamp_high[0x20];

	u8         time_stamp_low[0x20];

	u8         accumulators_period[0x20];

	u8         np_ecn_marked_roce_packets_high[0x20];

	u8         np_ecn_marked_roce_packets_low[0x20];

	u8         np_cnp_sent_high[0x20];

	u8         np_cnp_sent_low[0x20];

	u8         reserved_3[0x560];
};

struct mlx5_ifc_query_cong_statistics_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         clear[0x1];
	u8         reserved_2[0x1f];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_cong_params_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	union mlx5_ifc_cong_control_roce_ecn_auto_bits congestion_parameters;
};

struct mlx5_ifc_query_cong_params_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x1c];
	u8         cong_protocol[0x4];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_query_burst_size_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x20];

	u8         reserved_2[0x9];
	u8         device_burst_size[0x17];
};

struct mlx5_ifc_query_burst_size_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_query_adapter_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_query_adapter_param_block_bits query_adapter_struct;
};

struct mlx5_ifc_query_adapter_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_qp_2rst_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_qp_2rst_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_qp_2err_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_qp_2err_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_para_vport_element_bits {
	u8         reserved_at_0[0xc];
	u8         traffic_class[0x4];
	u8         qos_para_vport_number[0x10];
};

struct mlx5_ifc_page_fault_resume_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_page_fault_resume_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         error[0x1];
	u8         reserved_2[0x4];
	u8         rdma[0x1];
	u8         read_write[0x1];
	u8         req_res[0x1];
	u8         qpn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_nop_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_nop_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_modify_vport_state_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

enum {
	MLX5_MODIFY_VPORT_STATE_IN_OP_MOD_NIC_VPORT  = 0x0,
	MLX5_MODIFY_VPORT_STATE_IN_OP_MOD_ESW_VPORT  = 0x1,
	MLX5_MODIFY_VPORT_STATE_IN_OP_MOD_UPLINK     = 0x2,
};

enum {
	MLX5_MODIFY_VPORT_STATE_IN_ADMIN_STATE_DOWN    = 0x0,
	MLX5_MODIFY_VPORT_STATE_IN_ADMIN_STATE_UP      = 0x1,
	MLX5_MODIFY_VPORT_STATE_IN_ADMIN_STATE_FOLLOW  = 0x2,
};

struct mlx5_ifc_modify_vport_state_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xf];
	u8         vport_number[0x10];

	u8         reserved_3[0x18];
	u8         admin_state[0x4];
	u8         reserved_4[0x4];
};

struct mlx5_ifc_modify_tis_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_modify_tis_bitmask_bits {
	u8         reserved_at_0[0x20];

	u8         reserved_at_20[0x1d];
	u8         lag_tx_port_affinity[0x1];
	u8         strict_lag_tx_port_affinity[0x1];
	u8         prio[0x1];
};

struct mlx5_ifc_modify_tis_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         tisn[0x18];

	u8         reserved_3[0x20];

	struct mlx5_ifc_modify_tis_bitmask_bits bitmask;

	u8         reserved_4[0x40];

	struct mlx5_ifc_tisc_bits ctx;
};

struct mlx5_ifc_modify_tir_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

enum
{
	MLX5_MODIFY_SQ_BITMASK_PACKET_PACING_RATE_LIMIT_INDEX = 0x1 << 0,
	MLX5_MODIFY_SQ_BITMASK_QOS_PARA_VPORT_NUMBER =		0x1 << 1
};

struct mlx5_ifc_modify_tir_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         tirn[0x18];

	u8         reserved_3[0x20];

	u8         modify_bitmask[0x40];

	u8         reserved_4[0x40];

	struct mlx5_ifc_tirc_bits tir_context;
};

struct mlx5_ifc_modify_sq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_modify_sq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         sq_state[0x4];
	u8         reserved_2[0x4];
	u8         sqn[0x18];

	u8         reserved_3[0x20];

	u8         modify_bitmask[0x40];

	u8         reserved_4[0x40];

	struct mlx5_ifc_sqc_bits ctx;
};

struct mlx5_ifc_modify_scheduling_element_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x1c0];
};

enum {
	MLX5_MODIFY_SCHEDULING_ELEMENT_IN_SCHEDULING_HIERARCHY_E_SWITCH  = 0x2,
};

enum {
	MLX5_MODIFY_SCHEDULING_ELEMENT_BITMASK_BW_SHARE        = 0x1,
	MLX5_MODIFY_SCHEDULING_ELEMENT_BITMASK_MAX_AVERAGE_BW  = 0x2,
};

struct mlx5_ifc_modify_scheduling_element_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         scheduling_hierarchy[0x8];
	u8         reserved_at_48[0x18];

	u8         scheduling_element_id[0x20];

	u8         reserved_at_80[0x20];

	u8         modify_bitmask[0x20];

	u8         reserved_at_c0[0x40];

	struct mlx5_ifc_scheduling_context_bits scheduling_context;

	u8         reserved_at_300[0x100];
};

struct mlx5_ifc_modify_rqt_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_rqt_bitmask_bits {
	u8         reserved_at_0[0x20];

	u8         reserved_at_20[0x1f];
	u8         rqn_list[0x1];
};


struct mlx5_ifc_modify_rqt_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         rqtn[0x18];

	u8         reserved_3[0x20];

	struct mlx5_ifc_rqt_bitmask_bits bitmask;

	u8         reserved_4[0x40];

	struct mlx5_ifc_rqtc_bits ctx;
};

struct mlx5_ifc_modify_rq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

enum {
	MLX5_MODIFY_RQ_IN_MODIFY_BITMASK_VSD = 1ULL << 1,
	MLX5_MODIFY_RQ_IN_MODIFY_BITMASK_MODIFY_RQ_COUNTER_SET_ID = 1ULL << 3,
};

struct mlx5_ifc_modify_rq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         rq_state[0x4];
	u8         reserved_2[0x4];
	u8         rqn[0x18];

	u8         reserved_3[0x20];

	u8         modify_bitmask[0x40];

	u8         reserved_4[0x40];

	struct mlx5_ifc_rqc_bits ctx;
};

struct mlx5_ifc_modify_rmp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_rmp_bitmask_bits {
	u8	   reserved[0x20];

	u8         reserved1[0x1f];
	u8         lwm[0x1];
};

struct mlx5_ifc_modify_rmp_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         rmp_state[0x4];
	u8         reserved_2[0x4];
	u8         rmpn[0x18];

	u8         reserved_3[0x20];

	struct mlx5_ifc_rmp_bitmask_bits bitmask;

	u8         reserved_4[0x40];

	struct mlx5_ifc_rmpc_bits ctx;
};

struct mlx5_ifc_modify_nic_vport_context_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_modify_nic_vport_field_select_bits {
	u8         reserved_0[0x14];
	u8         disable_uc_local_lb[0x1];
	u8         disable_mc_local_lb[0x1];
	u8         node_guid[0x1];
	u8         port_guid[0x1];
	u8         min_wqe_inline_mode[0x1];
	u8         mtu[0x1];
	u8         change_event[0x1];
	u8         promisc[0x1];
	u8         permanent_address[0x1];
	u8         addresses_list[0x1];
	u8         roce_en[0x1];
	u8         reserved_1[0x1];
};

struct mlx5_ifc_modify_nic_vport_context_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xf];
	u8         vport_number[0x10];

	struct mlx5_ifc_modify_nic_vport_field_select_bits field_select;

	u8         reserved_3[0x780];

	struct mlx5_ifc_nic_vport_context_bits nic_vport_context;
};

struct mlx5_ifc_modify_hca_vport_context_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_grh_bits {
	u8	ip_version[4];
	u8	traffic_class[8];
	u8	flow_label[20];
	u8	payload_length[16];
	u8	next_header[8];
	u8	hop_limit[8];
	u8	sgid[128];
	u8	dgid[128];
};

struct mlx5_ifc_bth_bits {
	u8	opcode[8];
	u8	se[1];
	u8	migreq[1];
	u8	pad_count[2];
	u8	tver[4];
	u8	p_key[16];
	u8	reserved8[8];
	u8	dest_qp[24];
	u8	ack_req[1];
	u8	reserved7[7];
	u8	psn[24];
};

struct mlx5_ifc_aeth_bits {
	u8	syndrome[8];
	u8	msn[24];
};

struct mlx5_ifc_dceth_bits {
	u8	reserved0[8];
	u8	session_id[24];
	u8	reserved1[8];
	u8	dci_dct[24];
};

struct mlx5_ifc_modify_hca_vport_context_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xb];
	u8         port_num[0x4];
	u8         vport_number[0x10];

	u8         reserved_3[0x20];

	struct mlx5_ifc_hca_vport_context_bits hca_vport_context;
};

enum {
        MLX5_MODIFY_FLOW_TABLE_MISS_TABLE_ID     = (1UL << 0),
        MLX5_MODIFY_FLOW_TABLE_LAG_NEXT_TABLE_ID = (1UL << 15),
};

struct mlx5_ifc_modify_flow_table_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x40];
};

enum {
	MLX5_MODIFY_FLOW_TABLE_SELECT_MISS_ACTION_AND_ID = 0x1,
	MLX5_MODIFY_FLOW_TABLE_SELECT_LAG_MASTER_NEXT_TABLE_ID = 0x8000,
};

struct mlx5_ifc_modify_flow_table_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_at_41[0xf];
	u8         vport_number[0x10];

	u8         reserved_at_60[0x10];
	u8         modify_field_select[0x10];

	u8         table_type[0x8];
	u8         reserved_at_88[0x18];

	u8         reserved_at_a0[0x8];
	u8         table_id[0x18];

	struct mlx5_ifc_flow_table_context_bits flow_table_context;
};

struct mlx5_ifc_modify_esw_vport_context_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_esw_vport_context_fields_select_bits {
	u8         reserved[0x1c];
	u8         vport_cvlan_insert[0x1];
	u8         vport_svlan_insert[0x1];
	u8         vport_cvlan_strip[0x1];
	u8         vport_svlan_strip[0x1];
};

struct mlx5_ifc_modify_esw_vport_context_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xf];
	u8         vport_number[0x10];

	struct mlx5_ifc_esw_vport_context_fields_select_bits field_select;

	struct mlx5_ifc_esw_vport_context_bits esw_vport_context;
};

struct mlx5_ifc_modify_cq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

enum {
	MLX5_MODIFY_CQ_IN_OP_MOD_MODIFY_CQ  = 0x0,
	MLX5_MODIFY_CQ_IN_OP_MOD_RESIZE_CQ  = 0x1,
};

struct mlx5_ifc_modify_cq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         cqn[0x18];

	union mlx5_ifc_modify_field_select_resize_field_select_auto_bits modify_field_select_resize_field_select;

	struct mlx5_ifc_cqc_bits cq_context;

	u8         reserved_at_280[0x60];

	u8         cq_umem_valid[0x1];
	u8         reserved_at_2e1[0x1f];

	u8         reserved_at_300[0x580];

	u8         pas[0][0x40];
};

struct mlx5_ifc_modify_cong_status_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_modify_cong_status_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x18];
	u8         priority[0x4];
	u8         cong_protocol[0x4];

	u8         enable[0x1];
	u8         tag_enable[0x1];
	u8         reserved_3[0x1e];
};

struct mlx5_ifc_modify_cong_params_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_modify_cong_params_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x1c];
	u8         cong_protocol[0x4];

	union mlx5_ifc_field_select_802_1_r_roce_auto_bits field_select;

	u8         reserved_3[0x80];

	union mlx5_ifc_cong_control_roce_ecn_auto_bits congestion_parameters;
};

struct mlx5_ifc_manage_pages_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         output_num_entries[0x20];

	u8         reserved_1[0x20];

	u8         pas[0][0x40];
};

enum {
	MLX5_PAGES_CANT_GIVE                            = 0x0,
	MLX5_PAGES_GIVE                                 = 0x1,
	MLX5_PAGES_TAKE                                 = 0x2,
};

struct mlx5_ifc_manage_pages_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x10];
	u8         function_id[0x10];

	u8         input_num_entries[0x20];

	u8         pas[0][0x40];
};

struct mlx5_ifc_mad_ifc_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	u8         response_mad_packet[256][0x8];
};

struct mlx5_ifc_mad_ifc_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         remote_lid[0x10];
	u8         reserved_2[0x8];
	u8         port[0x8];

	u8         reserved_3[0x20];

	u8         mad[256][0x8];
};

struct mlx5_ifc_init_hca_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

enum {
	MLX5_INIT_HCA_IN_OP_MOD_INIT      = 0x0,
	MLX5_INIT_HCA_IN_OP_MOD_PRE_INIT  = 0x1,
};

struct mlx5_ifc_init_hca_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_init2rtr_qp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_init2rtr_qp_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];

	u8         opt_param_mask[0x20];

	u8         reserved_4[0x20];

	struct mlx5_ifc_qpc_bits qpc;

	u8         reserved_5[0x80];
};

struct mlx5_ifc_init2init_qp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_init2init_qp_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];

	u8         opt_param_mask[0x20];

	u8         reserved_4[0x20];

	struct mlx5_ifc_qpc_bits qpc;

	u8         reserved_5[0x80];
};

struct mlx5_ifc_get_dropped_packet_log_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	u8         packet_headers_log[128][0x8];

	u8         packet_syndrome[64][0x8];
};

struct mlx5_ifc_get_dropped_packet_log_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_encryption_key_obj_bits {
	u8         modify_field_select[0x40];

	u8         reserved_at_40[0x14];
	u8         key_size[0x4];
	u8         reserved_at_58[0x4];
	u8         key_type[0x4];

	u8         reserved_at_60[0x8];
	u8         pd[0x18];

	u8         reserved_at_80[0x180];

	u8         key[8][0x20];

	u8         reserved_at_300[0x500];
};

struct mlx5_ifc_gen_eqe_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x18];
	u8         eq_number[0x8];

	u8         reserved_3[0x20];

	u8         eqe[64][0x8];
};

struct mlx5_ifc_gen_eq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_enable_hca_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x20];
};

struct mlx5_ifc_enable_hca_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x10];
	u8         function_id[0x10];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_drain_dct_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_drain_dct_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         dctn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_disable_hca_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x20];
};

struct mlx5_ifc_disable_hca_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x10];
	u8         function_id[0x10];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_detach_from_mcg_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_detach_from_mcg_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];

	u8         multicast_gid[16][0x8];
};

struct mlx5_ifc_destroy_xrc_srq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_xrc_srq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         xrc_srqn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_destroy_tis_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_tis_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         tisn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_destroy_tir_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_tir_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         tirn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_destroy_srq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_srq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         srqn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_destroy_sq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_sq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         sqn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_destroy_scheduling_element_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x1c0];
};

enum {
	MLX5_DESTROY_SCHEDULING_ELEMENT_IN_SCHEDULING_HIERARCHY_E_SWITCH  = 0x2,
};

struct mlx5_ifc_destroy_scheduling_element_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         scheduling_hierarchy[0x8];
	u8         reserved_at_48[0x18];

	u8         scheduling_element_id[0x20];

	u8         reserved_at_80[0x180];
};

struct mlx5_ifc_destroy_rqt_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_rqt_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         rqtn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_destroy_rq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_rq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         rqn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_destroy_rmp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_rmp_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         rmpn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_destroy_qp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_qp_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_destroy_qos_para_vport_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x1c0];
};

struct mlx5_ifc_destroy_qos_para_vport_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x20];

	u8         reserved_at_60[0x10];
	u8         qos_para_vport_number[0x10];

	u8         reserved_at_80[0x180];
};

struct mlx5_ifc_destroy_psv_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_psv_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         psvn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_destroy_mkey_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_mkey_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         mkey_index[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_destroy_flow_table_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_flow_table_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xf];
	u8         vport_number[0x10];

	u8         reserved_3[0x20];

	u8         table_type[0x8];
	u8         reserved_4[0x18];

	u8         reserved_5[0x8];
	u8         table_id[0x18];

	u8         reserved_6[0x140];
};

struct mlx5_ifc_destroy_flow_group_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_flow_group_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xf];
	u8         vport_number[0x10];

	u8         reserved_3[0x20];

	u8         table_type[0x8];
	u8         reserved_4[0x18];

	u8         reserved_5[0x8];
	u8         table_id[0x18];

	u8         group_id[0x20];

	u8         reserved_6[0x120];
};

struct mlx5_ifc_destroy_encryption_key_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x40];
};

struct mlx5_ifc_destroy_encryption_key_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         obj_type[0x10];

	u8         obj_id[0x20];

	u8         reserved_at_60[0x20];
};

struct mlx5_ifc_destroy_eq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_eq_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x18];
	u8         eq_number[0x8];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_destroy_dct_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_dct_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         dctn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_destroy_cq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_destroy_cq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         cqn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_delete_vxlan_udp_dport_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_delete_vxlan_udp_dport_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x20];

	u8         reserved_3[0x10];
	u8         vxlan_udp_port[0x10];
};

struct mlx5_ifc_delete_l2_table_entry_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_delete_l2_table_entry_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x60];

	u8         reserved_3[0x8];
	u8         table_index[0x18];

	u8         reserved_4[0x140];
};

struct mlx5_ifc_delete_fte_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_delete_fte_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         other_vport[0x1];
	u8         reserved_2[0xf];
	u8         vport_number[0x10];

	u8         reserved_3[0x20];

	u8         table_type[0x8];
	u8         reserved_4[0x18];

	u8         reserved_5[0x8];
	u8         table_id[0x18];

	u8         reserved_6[0x40];

	u8         flow_index[0x20];

	u8         reserved_7[0xe0];
};

struct mlx5_ifc_dealloc_xrcd_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_dealloc_xrcd_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         xrcd[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_dealloc_uar_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_dealloc_uar_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         uar[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_dealloc_transport_domain_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_dealloc_transport_domain_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         transport_domain[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_dealloc_q_counter_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_counter_id_bits {
	u8         reserved[0x10];
	u8         counter_id[0x10];
};

struct mlx5_ifc_diagnostic_params_context_bits {
	u8         num_of_counters[0x10];
	u8         reserved_2[0x8];
	u8         log_num_of_samples[0x8];

	u8         single[0x1];
	u8         repetitive[0x1];
	u8         sync[0x1];
	u8         clear[0x1];
	u8         on_demand[0x1];
	u8         enable[0x1];
	u8         reserved_3[0x12];
	u8         log_sample_period[0x8];

	u8         reserved_4[0x80];

	struct mlx5_ifc_counter_id_bits counter_id[0];
};

struct mlx5_ifc_query_diagnostic_params_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x40];
};

struct mlx5_ifc_query_diagnostic_params_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	struct mlx5_ifc_diagnostic_params_context_bits diagnostic_params_ctx;
};

struct mlx5_ifc_set_diagnostic_params_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	struct mlx5_ifc_diagnostic_params_context_bits diagnostic_params_ctx;
};

struct mlx5_ifc_set_diagnostic_params_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_query_diagnostic_counters_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         num_of_samples[0x10];
	u8         sample_index[0x10];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_diagnostic_counter_bits {
	u8         counter_id[0x10];
	u8         sample_id[0x10];

	u8         time_stamp_31_0[0x20];

	u8         counter_value_h[0x20];

	u8         counter_value_l[0x20];
};

struct mlx5_ifc_query_diagnostic_counters_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	struct mlx5_ifc_diagnostic_counter_bits diag_counter[0];
};

struct mlx5_ifc_dealloc_q_counter_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x18];
	u8         counter_set_id[0x8];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_dealloc_pd_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_dealloc_pd_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         pd[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_dealloc_flow_counter_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_dealloc_flow_counter_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         flow_counter_id[0x20];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_create_xrq_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x8];
	u8         xrqn[0x18];

	u8         reserved_at_60[0x20];
};

struct mlx5_ifc_create_xrq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x40];

	struct mlx5_ifc_xrqc_bits xrq_context;
};

struct mlx5_ifc_deactivate_tracer_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_deactivate_tracer_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         mkey[0x20];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_xrc_srq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         xrc_srqn[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_xrc_srq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];

	struct mlx5_ifc_xrc_srqc_bits xrc_srq_context_entry;

	u8         reserved_at_280[0x60];

	u8         xrc_srq_umem_valid[0x1];
	u8         reserved_at_2e1[0x1f];

	u8         reserved_at_300[0x580];

	u8         pas[0][0x40];
};

struct mlx5_ifc_create_tis_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         tisn[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_tis_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0xc0];

	struct mlx5_ifc_tisc_bits ctx;
};

struct mlx5_ifc_create_tir_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         tirn[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_tir_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0xc0];

	struct mlx5_ifc_tirc_bits tir_context;
};

struct mlx5_ifc_create_srq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         srqn[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_srq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];

	struct mlx5_ifc_srqc_bits srq_context_entry;

	u8         reserved_3[0x600];

	u8         pas[0][0x40];
};

struct mlx5_ifc_create_sq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         sqn[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_sq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0xc0];

	struct mlx5_ifc_sqc_bits ctx;
};

struct mlx5_ifc_create_scheduling_element_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x40];

	u8         scheduling_element_id[0x20];

	u8         reserved_at_a0[0x160];
};

enum {
	MLX5_CREATE_SCHEDULING_ELEMENT_IN_SCHEDULING_HIERARCHY_E_SWITCH  = 0x2,
};

struct mlx5_ifc_create_scheduling_element_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         scheduling_hierarchy[0x8];
	u8         reserved_at_48[0x18];

	u8         reserved_at_60[0xa0];

	struct mlx5_ifc_scheduling_context_bits scheduling_context;

	u8         reserved_at_300[0x100];
};

struct mlx5_ifc_create_rqt_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         rqtn[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_rqt_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0xc0];

	struct mlx5_ifc_rqtc_bits rqt_context;
};

struct mlx5_ifc_create_rq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         rqn[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_rq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0xc0];

	struct mlx5_ifc_rqc_bits ctx;
};

struct mlx5_ifc_create_rmp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         rmpn[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_rmp_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0xc0];

	struct mlx5_ifc_rmpc_bits ctx;
};

struct mlx5_ifc_create_qp_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         qpn[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_qp_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         input_qpn[0x18];

	u8         reserved_3[0x20];

	u8         opt_param_mask[0x20];

	u8         reserved_4[0x20];

	struct mlx5_ifc_qpc_bits qpc;

	u8         reserved_at_800[0x60];

	u8         wq_umem_valid[0x1];
	u8         reserved_at_861[0x1f];

	u8         pas[0][0x40];
};

struct mlx5_ifc_create_qos_para_vport_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x20];

	u8         reserved_at_60[0x10];
	u8         qos_para_vport_number[0x10];

	u8         reserved_at_80[0x180];
};

struct mlx5_ifc_create_qos_para_vport_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x1c0];
};

struct mlx5_ifc_create_psv_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	u8         reserved_2[0x8];
	u8         psv0_index[0x18];

	u8         reserved_3[0x8];
	u8         psv1_index[0x18];

	u8         reserved_4[0x8];
	u8         psv2_index[0x18];

	u8         reserved_5[0x8];
	u8         psv3_index[0x18];
};

struct mlx5_ifc_create_psv_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         num_psv[0x4];
	u8         reserved_2[0x4];
	u8         pd[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_create_mkey_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         mkey_index[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_mkey_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x20];

	u8         pg_access[0x1];
	u8         mkey_umem_valid[0x1];
	u8         reserved_at_62[0x1e];

	struct mlx5_ifc_mkc_bits memory_key_mkey_entry;

	u8         reserved_4[0x80];

	u8         translations_octword_actual_size[0x20];

	u8         reserved_5[0x560];

	u8         klm_pas_mtt[0][0x20];
};

struct mlx5_ifc_create_flow_table_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         table_id[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_flow_table_in_bits {
        u8         opcode[0x10];
        u8         uid[0x10];

        u8         reserved_at_20[0x10];
        u8         op_mod[0x10];

        u8         other_vport[0x1];
        u8         reserved_at_41[0xf];
        u8         vport_number[0x10];

        u8         reserved_at_60[0x20];

        u8         table_type[0x8];
        u8         reserved_at_88[0x18];

        u8         reserved_at_a0[0x20];

        struct mlx5_ifc_flow_table_context_bits flow_table_context;
};

struct mlx5_ifc_create_flow_group_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         group_id[0x18];

	u8         reserved_2[0x20];
};

enum {
	MLX5_CREATE_FLOW_GROUP_IN_MATCH_CRITERIA_ENABLE_OUTER_HEADERS     = 0x0,
	MLX5_CREATE_FLOW_GROUP_IN_MATCH_CRITERIA_ENABLE_MISC_PARAMETERS   = 0x1,
	MLX5_CREATE_FLOW_GROUP_IN_MATCH_CRITERIA_ENABLE_INNER_HEADERS     = 0x2,
	MLX5_CREATE_FLOW_GROUP_IN_MATCH_CRITERIA_ENABLE_MISC_PARAMETERS_2 = 0x3,
};

struct mlx5_ifc_create_flow_group_in_bits {
        u8         opcode[0x10];
        u8         reserved_at_10[0x10];

        u8         reserved_at_20[0x10];
        u8         op_mod[0x10];

        u8         other_vport[0x1];
        u8         reserved_at_41[0xf];
        u8         vport_number[0x10];

        u8         reserved_at_60[0x20];

        u8         table_type[0x8];
        u8         reserved_at_88[0x4];
        u8         group_type[0x4];
        u8         reserved_at_90[0x10];

        u8         reserved_at_a0[0x8];
        u8         table_id[0x18];

        u8         source_eswitch_owner_vhca_id_valid[0x1];

        u8         reserved_at_c1[0x1f];

        u8         start_flow_index[0x20];

        u8         reserved_at_100[0x20];

        u8         end_flow_index[0x20];

        u8         reserved_at_140[0x10];
        u8         match_definer_id[0x10];

        u8         reserved_at_160[0x80];

        u8         reserved_at_1e0[0x18];
        u8         match_criteria_enable[0x8];

        struct mlx5_ifc_fte_match_param_bits match_criteria;

        u8         reserved_at_1200[0xe00];
};

struct mlx5_ifc_create_encryption_key_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         obj_id[0x20];

	u8         reserved_at_60[0x20];
};

struct mlx5_ifc_create_encryption_key_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         obj_type[0x10];

	u8         reserved_at_40[0x40];

	struct mlx5_ifc_encryption_key_obj_bits encryption_key_object;
};

struct mlx5_ifc_create_eq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x18];
	u8         eq_number[0x8];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_eq_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];

	struct mlx5_ifc_eqc_bits eq_context_entry;

	u8         reserved_3[0x40];

	u8         event_bitmask[0x40];

	u8         reserved_4[0x580];

	u8         pas[0][0x40];
};

struct mlx5_ifc_create_dct_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         dctn[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_dct_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];

	struct mlx5_ifc_dctc_bits dct_context_entry;

	u8         reserved_3[0x180];
};

struct mlx5_ifc_create_cq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         cqn[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_create_cq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];

	struct mlx5_ifc_cqc_bits cq_context;

	u8         reserved_at_280[0x60];

	u8         cq_umem_valid[0x1];
	u8         reserved_at_2e1[0x59f];

	u8         pas[0][0x40];
};

struct mlx5_ifc_config_int_moderation_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x4];
	u8         min_delay[0xc];
	u8         int_vector[0x10];

	u8         reserved_2[0x20];
};

enum {
	MLX5_CONFIG_INT_MODERATION_IN_OP_MOD_WRITE  = 0x0,
	MLX5_CONFIG_INT_MODERATION_IN_OP_MOD_READ   = 0x1,
};

struct mlx5_ifc_config_int_moderation_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x4];
	u8         min_delay[0xc];
	u8         int_vector[0x10];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_attach_to_mcg_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_attach_to_mcg_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         qpn[0x18];

	u8         reserved_3[0x20];

	u8         multicast_gid[16][0x8];
};

struct mlx5_ifc_arm_xrq_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x40];
};

struct mlx5_ifc_arm_xrq_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x8];
	u8         xrqn[0x18];

	u8         reserved_at_60[0x10];
	u8         lwm[0x10];
};

struct mlx5_ifc_arm_xrc_srq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

enum {
	MLX5_ARM_XRC_SRQ_IN_OP_MOD_XRC_SRQ  = 0x1,
};

struct mlx5_ifc_arm_xrc_srq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         xrc_srqn[0x18];

	u8         reserved_3[0x10];
	u8         lwm[0x10];
};

struct mlx5_ifc_arm_rq_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

enum {
	MLX5_ARM_RQ_IN_OP_MOD_SRQ  = 0x1,
};

struct mlx5_ifc_arm_rq_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         srq_number[0x18];

	u8         reserved_3[0x10];
	u8         lwm[0x10];
};

struct mlx5_ifc_arm_dct_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_arm_dct_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x8];
	u8         dctn[0x18];

	u8         reserved_3[0x20];
};

struct mlx5_ifc_alloc_xrcd_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         xrcd[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_alloc_xrcd_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_alloc_uar_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         uar[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_alloc_uar_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_alloc_transport_domain_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         transport_domain[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_alloc_transport_domain_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_alloc_q_counter_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x18];
	u8         counter_set_id[0x8];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_alloc_q_counter_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_alloc_pd_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x8];
	u8         pd[0x18];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_alloc_pd_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_alloc_flow_counter_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         flow_counter_id[0x20];

	u8         reserved_at_60[0x20];
};

struct mlx5_ifc_alloc_flow_counter_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x38];
	u8         flow_counter_bulk[0x8];
};

struct mlx5_ifc_add_vxlan_udp_dport_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_add_vxlan_udp_dport_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x20];

	u8         reserved_3[0x10];
	u8         vxlan_udp_port[0x10];
};

struct mlx5_ifc_activate_tracer_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];
};

struct mlx5_ifc_activate_tracer_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         mkey[0x20];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_set_rate_limit_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x40];
};

struct mlx5_ifc_set_rate_limit_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x10];
	u8         rate_limit_index[0x10];

	u8         reserved_at_60[0x20];

	u8         rate_limit[0x20];

	u8         burst_upper_bound[0x20];

	u8         reserved_at_c0[0x10];
	u8         typical_packet_size[0x10];

	u8         reserved_at_e0[0x120];
};

struct mlx5_ifc_access_register_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         reserved_1[0x40];

	u8         register_data[0][0x20];
};

enum {
	MLX5_ACCESS_REGISTER_IN_OP_MOD_WRITE  = 0x0,
	MLX5_ACCESS_REGISTER_IN_OP_MOD_READ   = 0x1,
};

struct mlx5_ifc_access_register_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         reserved_2[0x10];
	u8         register_id[0x10];

	u8         argument[0x20];

	u8         register_data[0][0x20];
};

struct mlx5_ifc_sltp_reg_bits {
	u8         status[0x4];
	u8         version[0x4];
	u8         local_port[0x8];
	u8         pnat[0x2];
	u8         reserved_0[0x2];
	u8         lane[0x4];
	u8         reserved_1[0x8];

	u8         reserved_2[0x20];

	u8         reserved_3[0x7];
	u8         polarity[0x1];
	u8         ob_tap0[0x8];
	u8         ob_tap1[0x8];
	u8         ob_tap2[0x8];

	u8         reserved_4[0xc];
	u8         ob_preemp_mode[0x4];
	u8         ob_reg[0x8];
	u8         ob_bias[0x8];

	u8         reserved_5[0x20];
};

struct mlx5_ifc_slrp_reg_bits {
	u8         status[0x4];
	u8         version[0x4];
	u8         local_port[0x8];
	u8         pnat[0x2];
	u8         reserved_0[0x2];
	u8         lane[0x4];
	u8         reserved_1[0x8];

	u8         ib_sel[0x2];
	u8         reserved_2[0x11];
	u8         dp_sel[0x1];
	u8         dp90sel[0x4];
	u8         mix90phase[0x8];

	u8         ffe_tap0[0x8];
	u8         ffe_tap1[0x8];
	u8         ffe_tap2[0x8];
	u8         ffe_tap3[0x8];

	u8         ffe_tap4[0x8];
	u8         ffe_tap5[0x8];
	u8         ffe_tap6[0x8];
	u8         ffe_tap7[0x8];

	u8         ffe_tap8[0x8];
	u8         mixerbias_tap_amp[0x8];
	u8         reserved_3[0x7];
	u8         ffe_tap_en[0x9];

	u8         ffe_tap_offset0[0x8];
	u8         ffe_tap_offset1[0x8];
	u8         slicer_offset0[0x10];

	u8         mixer_offset0[0x10];
	u8         mixer_offset1[0x10];

	u8         mixerbgn_inp[0x8];
	u8         mixerbgn_inn[0x8];
	u8         mixerbgn_refp[0x8];
	u8         mixerbgn_refn[0x8];

	u8         sel_slicer_lctrl_h[0x1];
	u8         sel_slicer_lctrl_l[0x1];
	u8         reserved_4[0x1];
	u8         ref_mixer_vreg[0x5];
	u8         slicer_gctrl[0x8];
	u8         lctrl_input[0x8];
	u8         mixer_offset_cm1[0x8];

	u8         common_mode[0x6];
	u8         reserved_5[0x1];
	u8         mixer_offset_cm0[0x9];
	u8         reserved_6[0x7];
	u8         slicer_offset_cm[0x9];
};

struct mlx5_ifc_slrg_reg_bits {
	u8         status[0x4];
	u8         version[0x4];
	u8         local_port[0x8];
	u8         pnat[0x2];
	u8         reserved_0[0x2];
	u8         lane[0x4];
	u8         reserved_1[0x8];

	u8         time_to_link_up[0x10];
	u8         reserved_2[0xc];
	u8         grade_lane_speed[0x4];

	u8         grade_version[0x8];
	u8         grade[0x18];

	u8         reserved_3[0x4];
	u8         height_grade_type[0x4];
	u8         height_grade[0x18];

	u8         height_dz[0x10];
	u8         height_dv[0x10];

	u8         reserved_4[0x10];
	u8         height_sigma[0x10];

	u8         reserved_5[0x20];

	u8         reserved_6[0x4];
	u8         phase_grade_type[0x4];
	u8         phase_grade[0x18];

	u8         reserved_7[0x8];
	u8         phase_eo_pos[0x8];
	u8         reserved_8[0x8];
	u8         phase_eo_neg[0x8];

	u8         ffe_set_tested[0x10];
	u8         test_errors_per_lane[0x10];
};

struct mlx5_ifc_pvlc_reg_bits {
	u8         reserved_0[0x8];
	u8         local_port[0x8];
	u8         reserved_1[0x10];

	u8         reserved_2[0x1c];
	u8         vl_hw_cap[0x4];

	u8         reserved_3[0x1c];
	u8         vl_admin[0x4];

	u8         reserved_4[0x1c];
	u8         vl_operational[0x4];
};

struct mlx5_ifc_pude_reg_bits {
	u8         swid[0x8];
	u8         local_port[0x8];
	u8         reserved_0[0x4];
	u8         admin_status[0x4];
	u8         reserved_1[0x4];
	u8         oper_status[0x4];

	u8         reserved_2[0x60];
};

enum {
	MLX5_PTYS_REG_PROTO_MASK_INFINIBAND  = 0x1,
	MLX5_PTYS_REG_PROTO_MASK_ETHERNET    = 0x4,
};

struct mlx5_ifc_ptys_reg_bits {
	u8         reserved_0[0x1];
	u8         an_disable_admin[0x1];
	u8         an_disable_cap[0x1];
	u8         reserved_1[0x4];
	u8         force_tx_aba_param[0x1];
	u8         local_port[0x8];
	u8         reserved_2[0xd];
	u8         proto_mask[0x3];

	u8         an_status[0x4];
	u8         reserved_3[0xc];
	u8         data_rate_oper[0x10];

	u8         ext_eth_proto_capability[0x20];

	u8         eth_proto_capability[0x20];

	u8         ib_link_width_capability[0x10];
	u8         ib_proto_capability[0x10];

	u8         ext_eth_proto_admin[0x20];

	u8         eth_proto_admin[0x20];

	u8         ib_link_width_admin[0x10];
	u8         ib_proto_admin[0x10];

	u8         ext_eth_proto_oper[0x20];

	u8         eth_proto_oper[0x20];

	u8         ib_link_width_oper[0x10];
	u8         ib_proto_oper[0x10];

	u8         reserved_4[0x1c];
	u8         connector_type[0x4];

	u8         eth_proto_lp_advertise[0x20];

	u8         reserved_5[0x60];
};

struct mlx5_ifc_ptas_reg_bits {
	u8         reserved_0[0x20];

	u8         algorithm_options[0x10];
	u8         reserved_1[0x4];
	u8         repetitions_mode[0x4];
	u8         num_of_repetitions[0x8];

	u8         grade_version[0x8];
	u8         height_grade_type[0x4];
	u8         phase_grade_type[0x4];
	u8         height_grade_weight[0x8];
	u8         phase_grade_weight[0x8];

	u8         gisim_measure_bits[0x10];
	u8         adaptive_tap_measure_bits[0x10];

	u8         ber_bath_high_error_threshold[0x10];
	u8         ber_bath_mid_error_threshold[0x10];

	u8         ber_bath_low_error_threshold[0x10];
	u8         one_ratio_high_threshold[0x10];

	u8         one_ratio_high_mid_threshold[0x10];
	u8         one_ratio_low_mid_threshold[0x10];

	u8         one_ratio_low_threshold[0x10];
	u8         ndeo_error_threshold[0x10];

	u8         mixer_offset_step_size[0x10];
	u8         reserved_2[0x8];
	u8         mix90_phase_for_voltage_bath[0x8];

	u8         mixer_offset_start[0x10];
	u8         mixer_offset_end[0x10];

	u8         reserved_3[0x15];
	u8         ber_test_time[0xb];
};

struct mlx5_ifc_pspa_reg_bits {
	u8         swid[0x8];
	u8         local_port[0x8];
	u8         sub_port[0x8];
	u8         reserved_0[0x8];

	u8         reserved_1[0x20];
};

struct mlx5_ifc_ppsc_reg_bits {
	u8         reserved_0[0x8];
	u8         local_port[0x8];
	u8         reserved_1[0x10];

	u8         reserved_2[0x60];

	u8         reserved_3[0x1c];
	u8         wrps_admin[0x4];

	u8         reserved_4[0x1c];
	u8         wrps_status[0x4];

	u8         up_th_vld[0x1];
	u8         down_th_vld[0x1];
	u8         reserved_5[0x6];
	u8         up_threshold[0x8];
	u8         reserved_6[0x8];
	u8         down_threshold[0x8];

	u8         reserved_7[0x20];

	u8         reserved_8[0x1c];
	u8         srps_admin[0x4];

	u8         reserved_9[0x60];
};

struct mlx5_ifc_pplr_reg_bits {
	u8         reserved_0[0x8];
	u8         local_port[0x8];
	u8         reserved_1[0x10];

	u8         reserved_2[0x8];
	u8         lb_cap[0x8];
	u8         reserved_3[0x8];
	u8         lb_en[0x8];
};

struct mlx5_ifc_pplm_reg_bits {
	u8         reserved_at_0[0x8];
	u8	   local_port[0x8];
	u8	   reserved_at_10[0x10];

	u8	   reserved_at_20[0x20];

	u8	   port_profile_mode[0x8];
	u8	   static_port_profile[0x8];
	u8	   active_port_profile[0x8];
	u8	   reserved_at_58[0x8];

	u8	   retransmission_active[0x8];
	u8	   fec_mode_active[0x18];

	u8	   rs_fec_correction_bypass_cap[0x4];
	u8	   reserved_at_84[0x8];
	u8	   fec_override_cap_56g[0x4];
	u8	   fec_override_cap_100g[0x4];
	u8	   fec_override_cap_50g[0x4];
	u8	   fec_override_cap_25g[0x4];
	u8	   fec_override_cap_10g_40g[0x4];

	u8	   rs_fec_correction_bypass_admin[0x4];
	u8	   reserved_at_a4[0x8];
	u8	   fec_override_admin_56g[0x4];
	u8	   fec_override_admin_100g[0x4];
	u8	   fec_override_admin_50g[0x4];
	u8	   fec_override_admin_25g[0x4];
	u8	   fec_override_admin_10g_40g[0x4];

	u8	   fec_override_cap_400g_8x[0x10];
	u8	   fec_override_cap_200g_4x[0x10];
	u8	   fec_override_cap_100g_2x[0x10];
	u8	   fec_override_cap_50g_1x[0x10];

	u8	   fec_override_admin_400g_8x[0x10];
	u8	   fec_override_admin_200g_4x[0x10];
	u8	   fec_override_admin_100g_2x[0x10];
	u8	   fec_override_admin_50g_1x[0x10];

	u8	   reserved_at_140[0x140];
};

struct mlx5_ifc_ppll_reg_bits {
	u8         num_pll_groups[0x8];
	u8         pll_group[0x8];
	u8         reserved_0[0x4];
	u8         num_plls[0x4];
	u8         reserved_1[0x8];

	u8         reserved_2[0x1f];
	u8         ae[0x1];

	u8         pll_status[4][0x40];
};

struct mlx5_ifc_ppad_reg_bits {
	u8         reserved_0[0x3];
	u8         single_mac[0x1];
	u8         reserved_1[0x4];
	u8         local_port[0x8];
	u8         mac_47_32[0x10];

	u8         mac_31_0[0x20];

	u8         reserved_2[0x40];
};

struct mlx5_ifc_pmtu_reg_bits {
	u8         reserved_0[0x8];
	u8         local_port[0x8];
	u8         reserved_1[0x10];

	u8         max_mtu[0x10];
	u8         reserved_2[0x10];

	u8         admin_mtu[0x10];
	u8         reserved_3[0x10];

	u8         oper_mtu[0x10];
	u8         reserved_4[0x10];
};

struct mlx5_ifc_pmpr_reg_bits {
	u8         reserved_0[0x8];
	u8         module[0x8];
	u8         reserved_1[0x10];

	u8         reserved_2[0x18];
	u8         attenuation_5g[0x8];

	u8         reserved_3[0x18];
	u8         attenuation_7g[0x8];

	u8         reserved_4[0x18];
	u8         attenuation_12g[0x8];
};

struct mlx5_ifc_pmpe_reg_bits {
	u8         reserved_0[0x8];
	u8         module[0x8];
	u8         reserved_1[0xc];
	u8         module_status[0x4];

	u8         reserved_2[0x14];
	u8         error_type[0x4];
	u8         reserved_3[0x8];

	u8         reserved_4[0x40];
};

struct mlx5_ifc_pmpc_reg_bits {
	u8         module_state_updated[32][0x8];
};

struct mlx5_ifc_pmlpn_reg_bits {
	u8         reserved_0[0x4];
	u8         mlpn_status[0x4];
	u8         local_port[0x8];
	u8         reserved_1[0x10];

	u8         e[0x1];
	u8         reserved_2[0x1f];
};

struct mlx5_ifc_pmlp_reg_bits {
	u8         rxtx[0x1];
	u8         reserved_0[0x7];
	u8         local_port[0x8];
	u8         reserved_1[0x8];
	u8         width[0x8];

	u8         lane0_module_mapping[0x20];

	u8         lane1_module_mapping[0x20];

	u8         lane2_module_mapping[0x20];

	u8         lane3_module_mapping[0x20];

	u8         reserved_2[0x160];
};

struct mlx5_ifc_pmaos_reg_bits {
	u8         reserved_0[0x8];
	u8         module[0x8];
	u8         reserved_1[0x4];
	u8         admin_status[0x4];
	u8         reserved_2[0x4];
	u8         oper_status[0x4];

	u8         ase[0x1];
	u8         ee[0x1];
	u8         reserved_3[0x12];
	u8         error_type[0x4];
	u8         reserved_4[0x6];
	u8         e[0x2];

	u8         reserved_5[0x40];
};

struct mlx5_ifc_plpc_reg_bits {
	u8         reserved_0[0x4];
	u8         profile_id[0xc];
	u8         reserved_1[0x4];
	u8         proto_mask[0x4];
	u8         reserved_2[0x8];

	u8         reserved_3[0x10];
	u8         lane_speed[0x10];

	u8         reserved_4[0x17];
	u8         lpbf[0x1];
	u8         fec_mode_policy[0x8];

	u8         retransmission_capability[0x8];
	u8         fec_mode_capability[0x18];

	u8         retransmission_support_admin[0x8];
	u8         fec_mode_support_admin[0x18];

	u8         retransmission_request_admin[0x8];
	u8         fec_mode_request_admin[0x18];

	u8         reserved_5[0x80];
};

struct mlx5_ifc_pll_status_data_bits {
	u8         reserved_0[0x1];
	u8         lock_cal[0x1];
	u8         lock_status[0x2];
	u8         reserved_1[0x2];
	u8         algo_f_ctrl[0xa];
	u8         analog_algo_num_var[0x6];
	u8         f_ctrl_measure[0xa];

	u8         reserved_2[0x2];
	u8         analog_var[0x6];
	u8         reserved_3[0x2];
	u8         high_var[0x6];
	u8         reserved_4[0x2];
	u8         low_var[0x6];
	u8         reserved_5[0x2];
	u8         mid_val[0x6];
};

struct mlx5_ifc_plib_reg_bits {
	u8         reserved_0[0x8];
	u8         local_port[0x8];
	u8         reserved_1[0x8];
	u8         ib_port[0x8];

	u8         reserved_2[0x60];
};

struct mlx5_ifc_plbf_reg_bits {
	u8         reserved_0[0x8];
	u8         local_port[0x8];
	u8         reserved_1[0xd];
	u8         lbf_mode[0x3];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_pipg_reg_bits {
	u8         reserved_0[0x8];
	u8         local_port[0x8];
	u8         reserved_1[0x10];

	u8         dic[0x1];
	u8         reserved_2[0x19];
	u8         ipg[0x4];
	u8         reserved_3[0x2];
};

struct mlx5_ifc_pifr_reg_bits {
	u8         reserved_0[0x8];
	u8         local_port[0x8];
	u8         reserved_1[0x10];

	u8         reserved_2[0xe0];

	u8         port_filter[8][0x20];

	u8         port_filter_update_en[8][0x20];
};

struct mlx5_ifc_phys_layer_cntrs_bits {
	u8         time_since_last_clear_high[0x20];

	u8         time_since_last_clear_low[0x20];

	u8         symbol_errors_high[0x20];

	u8         symbol_errors_low[0x20];

	u8         sync_headers_errors_high[0x20];

	u8         sync_headers_errors_low[0x20];

	u8         edpl_bip_errors_lane0_high[0x20];

	u8         edpl_bip_errors_lane0_low[0x20];

	u8         edpl_bip_errors_lane1_high[0x20];

	u8         edpl_bip_errors_lane1_low[0x20];

	u8         edpl_bip_errors_lane2_high[0x20];

	u8         edpl_bip_errors_lane2_low[0x20];

	u8         edpl_bip_errors_lane3_high[0x20];

	u8         edpl_bip_errors_lane3_low[0x20];

	u8         fc_fec_corrected_blocks_lane0_high[0x20];

	u8         fc_fec_corrected_blocks_lane0_low[0x20];

	u8         fc_fec_corrected_blocks_lane1_high[0x20];

	u8         fc_fec_corrected_blocks_lane1_low[0x20];

	u8         fc_fec_corrected_blocks_lane2_high[0x20];

	u8         fc_fec_corrected_blocks_lane2_low[0x20];

	u8         fc_fec_corrected_blocks_lane3_high[0x20];

	u8         fc_fec_corrected_blocks_lane3_low[0x20];

	u8         fc_fec_uncorrectable_blocks_lane0_high[0x20];

	u8         fc_fec_uncorrectable_blocks_lane0_low[0x20];

	u8         fc_fec_uncorrectable_blocks_lane1_high[0x20];

	u8         fc_fec_uncorrectable_blocks_lane1_low[0x20];

	u8         fc_fec_uncorrectable_blocks_lane2_high[0x20];

	u8         fc_fec_uncorrectable_blocks_lane2_low[0x20];

	u8         fc_fec_uncorrectable_blocks_lane3_high[0x20];

	u8         fc_fec_uncorrectable_blocks_lane3_low[0x20];

	u8         rs_fec_corrected_blocks_high[0x20];

	u8         rs_fec_corrected_blocks_low[0x20];

	u8         rs_fec_uncorrectable_blocks_high[0x20];

	u8         rs_fec_uncorrectable_blocks_low[0x20];

	u8         rs_fec_no_errors_blocks_high[0x20];

	u8         rs_fec_no_errors_blocks_low[0x20];

	u8         rs_fec_single_error_blocks_high[0x20];

	u8         rs_fec_single_error_blocks_low[0x20];

	u8         rs_fec_corrected_symbols_total_high[0x20];

	u8         rs_fec_corrected_symbols_total_low[0x20];

	u8         rs_fec_corrected_symbols_lane0_high[0x20];

	u8         rs_fec_corrected_symbols_lane0_low[0x20];

	u8         rs_fec_corrected_symbols_lane1_high[0x20];

	u8         rs_fec_corrected_symbols_lane1_low[0x20];

	u8         rs_fec_corrected_symbols_lane2_high[0x20];

	u8         rs_fec_corrected_symbols_lane2_low[0x20];

	u8         rs_fec_corrected_symbols_lane3_high[0x20];

	u8         rs_fec_corrected_symbols_lane3_low[0x20];

	u8         link_down_events[0x20];

	u8         successful_recovery_events[0x20];

	u8         reserved_0[0x180];
};

struct mlx5_ifc_ib_port_cntrs_grp_data_layout_bits {
	u8	   symbol_error_counter[0x10];

	u8         link_error_recovery_counter[0x8];

	u8         link_downed_counter[0x8];

	u8         port_rcv_errors[0x10];

	u8         port_rcv_remote_physical_errors[0x10];

	u8         port_rcv_switch_relay_errors[0x10];

	u8         port_xmit_discards[0x10];

	u8         port_xmit_constraint_errors[0x8];

	u8         port_rcv_constraint_errors[0x8];

	u8         reserved_at_70[0x8];

	u8         link_overrun_errors[0x8];

	u8	   reserved_at_80[0x10];

	u8         vl_15_dropped[0x10];

	u8	   reserved_at_a0[0xa0];
};

struct mlx5_ifc_phys_layer_statistical_cntrs_bits {
	u8         time_since_last_clear_high[0x20];

	u8         time_since_last_clear_low[0x20];

	u8         phy_received_bits_high[0x20];

	u8         phy_received_bits_low[0x20];

	u8         phy_symbol_errors_high[0x20];

	u8         phy_symbol_errors_low[0x20];

	u8         phy_corrected_bits_high[0x20];

	u8         phy_corrected_bits_low[0x20];

	u8         phy_corrected_bits_lane0_high[0x20];

	u8         phy_corrected_bits_lane0_low[0x20];

	u8         phy_corrected_bits_lane1_high[0x20];

	u8         phy_corrected_bits_lane1_low[0x20];

	u8         phy_corrected_bits_lane2_high[0x20];

	u8         phy_corrected_bits_lane2_low[0x20];

	u8         phy_corrected_bits_lane3_high[0x20];

	u8         phy_corrected_bits_lane3_low[0x20];

	u8         reserved_at_200[0x5c0];
};

struct mlx5_ifc_infiniband_port_cntrs_bits {
	u8         symbol_error_counter[0x10];
	u8         link_error_recovery_counter[0x8];
	u8         link_downed_counter[0x8];

	u8         port_rcv_errors[0x10];
	u8         port_rcv_remote_physical_errors[0x10];

	u8         port_rcv_switch_relay_errors[0x10];
	u8         port_xmit_discards[0x10];

	u8         port_xmit_constraint_errors[0x8];
	u8         port_rcv_constraint_errors[0x8];
	u8         reserved_0[0x8];
	u8         local_link_integrity_errors[0x4];
	u8         excessive_buffer_overrun_errors[0x4];

	u8         reserved_1[0x10];
	u8         vl_15_dropped[0x10];

	u8         port_xmit_data[0x20];

	u8         port_rcv_data[0x20];

	u8         port_xmit_pkts[0x20];

	u8         port_rcv_pkts[0x20];

	u8         port_xmit_wait[0x20];

	u8         reserved_2[0x680];
};

struct mlx5_ifc_phrr_reg_bits {
	u8         clr[0x1];
	u8         reserved_0[0x7];
	u8         local_port[0x8];
	u8         reserved_1[0x10];

	u8         hist_group[0x8];
	u8         reserved_2[0x10];
	u8         hist_id[0x8];

	u8         reserved_3[0x40];

	u8         time_since_last_clear_high[0x20];

	u8         time_since_last_clear_low[0x20];

	u8         bin[10][0x20];
};

struct mlx5_ifc_phbr_for_prio_reg_bits {
	u8         reserved_0[0x18];
	u8         prio[0x8];
};

struct mlx5_ifc_phbr_for_port_tclass_reg_bits {
	u8         reserved_0[0x18];
	u8         tclass[0x8];
};

struct mlx5_ifc_phbr_binding_reg_bits {
	u8         opcode[0x4];
	u8         reserved_0[0x4];
	u8         local_port[0x8];
	u8         pnat[0x2];
	u8         reserved_1[0xe];

	u8         hist_group[0x8];
	u8         reserved_2[0x10];
	u8         hist_id[0x8];

	u8         reserved_3[0x10];
	u8         hist_type[0x10];

	u8         hist_parameters[0x20];

	u8         hist_min_value[0x20];

	u8         hist_max_value[0x20];

	u8         sample_time[0x20];
};

enum {
	MLX5_PFCC_REG_PPAN_DISABLED  = 0x0,
	MLX5_PFCC_REG_PPAN_ENABLED   = 0x1,
};

struct mlx5_ifc_pfcc_reg_bits {
	u8         dcbx_operation_type[0x2];
	u8         cap_local_admin[0x1];
	u8         cap_remote_admin[0x1];
	u8         reserved_0[0x4];
	u8         local_port[0x8];
	u8         pnat[0x2];
	u8         reserved_1[0xc];
	u8         shl_cap[0x1];
	u8         shl_opr[0x1];

	u8         ppan[0x4];
	u8         reserved_2[0x4];
	u8         prio_mask_tx[0x8];
	u8         reserved_3[0x8];
	u8         prio_mask_rx[0x8];

	u8         pptx[0x1];
	u8         aptx[0x1];
	u8         reserved_4[0x6];
	u8         pfctx[0x8];
	u8         reserved_5[0x8];
	u8         cbftx[0x8];

	u8         pprx[0x1];
	u8         aprx[0x1];
	u8         reserved_6[0x6];
	u8         pfcrx[0x8];
	u8         reserved_7[0x8];
	u8         cbfrx[0x8];

	u8         device_stall_minor_watermark[0x10];
	u8         device_stall_critical_watermark[0x10];

	u8         reserved_8[0x60];
};

struct mlx5_ifc_pelc_reg_bits {
	u8         op[0x4];
	u8         reserved_0[0x4];
	u8         local_port[0x8];
	u8         reserved_1[0x10];

	u8         op_admin[0x8];
	u8         op_capability[0x8];
	u8         op_request[0x8];
	u8         op_active[0x8];

	u8         admin[0x40];

	u8         capability[0x40];

	u8         request[0x40];

	u8         active[0x40];

	u8         reserved_2[0x80];
};

struct mlx5_ifc_peir_reg_bits {
	u8         reserved_0[0x8];
	u8         local_port[0x8];
	u8         reserved_1[0x10];

	u8         reserved_2[0xc];
	u8         error_count[0x4];
	u8         reserved_3[0x10];

	u8         reserved_4[0xc];
	u8         lane[0x4];
	u8         reserved_5[0x8];
	u8         error_type[0x8];
};

struct mlx5_ifc_qcam_access_reg_cap_mask {
	u8         qcam_access_reg_cap_mask_127_to_20[0x6C];
	u8         qpdpm[0x1];
	u8         qcam_access_reg_cap_mask_18_to_4[0x0F];
	u8         qdpm[0x1];
	u8         qpts[0x1];
	u8         qcap[0x1];
	u8         qcam_access_reg_cap_mask_0[0x1];
};

struct mlx5_ifc_qcam_qos_feature_cap_mask {
	u8         qcam_qos_feature_cap_mask_127_to_1[0x7F];
	u8         qpts_trust_both[0x1];
};

struct mlx5_ifc_qcam_reg_bits {
	u8         reserved_at_0[0x8];
	u8         feature_group[0x8];
	u8         reserved_at_10[0x8];
	u8         access_reg_group[0x8];
	u8         reserved_at_20[0x20];

	union {
		struct mlx5_ifc_qcam_access_reg_cap_mask reg_cap;
		u8  reserved_at_0[0x80];
	} qos_access_reg_cap_mask;

	u8         reserved_at_c0[0x80];

	union {
		struct mlx5_ifc_qcam_qos_feature_cap_mask feature_cap;
		u8  reserved_at_0[0x80];
	} qos_feature_cap_mask;

	u8         reserved_at_1c0[0x80];
};

struct mlx5_ifc_pcam_enhanced_features_bits {
	u8         reserved_at_0[0x6d];
	u8         rx_icrc_encapsulated_counter[0x1];
	u8	   reserved_at_6e[0x4];
	u8         ptys_extended_ethernet[0x1];
	u8	   reserved_at_73[0x3];
	u8         pfcc_mask[0x1];
	u8         reserved_at_77[0x3];
	u8         per_lane_error_counters[0x1];
	u8         rx_buffer_fullness_counters[0x1];
	u8         ptys_connector_type[0x1];
	u8         reserved_at_7d[0x1];
	u8         ppcnt_discard_group[0x1];
	u8         ppcnt_statistical_group[0x1];
};

struct mlx5_ifc_pcam_regs_5000_to_507f_bits {
	u8         port_access_reg_cap_mask_127_to_96[0x20];
	u8         port_access_reg_cap_mask_95_to_64[0x20];

	u8         reserved_at_40[0xe];
	u8         pddr[0x1];
	u8         reserved_at_4f[0xd];

	u8         pplm[0x1];
	u8         port_access_reg_cap_mask_34_to_32[0x3];

	u8         port_access_reg_cap_mask_31_to_13[0x13];
	u8         pbmc[0x1];
	u8         pptb[0x1];
	u8         port_access_reg_cap_mask_10_to_09[0x2];
	u8         ppcnt[0x1];
	u8         port_access_reg_cap_mask_07_to_00[0x8];
};

struct mlx5_ifc_pcam_reg_bits {
	u8         reserved_at_0[0x8];
	u8         feature_group[0x8];
	u8         reserved_at_10[0x8];
	u8         access_reg_group[0x8];

	u8         reserved_at_20[0x20];

	union {
		struct mlx5_ifc_pcam_regs_5000_to_507f_bits regs_5000_to_507f;
		u8         reserved_at_0[0x80];
	} port_access_reg_cap_mask;

	u8         reserved_at_c0[0x80];

	union {
		struct mlx5_ifc_pcam_enhanced_features_bits enhanced_features;
		u8         reserved_at_0[0x80];
	} feature_cap_mask;

	u8         reserved_at_1c0[0xc0];
};

struct mlx5_ifc_mcam_enhanced_features_bits {
	u8         reserved_at_0[0x6e];
	u8         pcie_status_and_power[0x1];
	u8         reserved_at_111[0x10];
	u8         pcie_performance_group[0x1];
};

struct mlx5_ifc_mcam_access_reg_bits {
	u8         reserved_at_0[0x1c];
	u8         mcda[0x1];
	u8         mcc[0x1];
	u8         mcqi[0x1];
	u8         reserved_at_1f[0x1];

	u8         regs_95_to_64[0x20];
	u8         regs_63_to_32[0x20];
	u8         regs_31_to_0[0x20];
};

struct mlx5_ifc_mcam_reg_bits {
	u8         reserved_at_0[0x8];
	u8         feature_group[0x8];
	u8         reserved_at_10[0x8];
	u8         access_reg_group[0x8];

	u8         reserved_at_20[0x20];

	union {
		struct mlx5_ifc_mcam_access_reg_bits access_regs;
		u8         reserved_at_0[0x80];
	} mng_access_reg_cap_mask;

	u8         reserved_at_c0[0x80];

	union {
		struct mlx5_ifc_mcam_enhanced_features_bits enhanced_features;
		u8         reserved_at_0[0x80];
	} mng_feature_cap_mask;

	u8         reserved_at_1c0[0x80];
};

struct mlx5_ifc_pcap_reg_bits {
	u8         reserved_0[0x8];
	u8         local_port[0x8];
	u8         reserved_1[0x10];

	u8         port_capability_mask[4][0x20];
};

struct mlx5_ifc_pbmc_reg_bits {
	u8         reserved_at_0[0x8];
	u8         local_port[0x8];
	u8         reserved_at_10[0x10];

	u8         xoff_timer_value[0x10];
	u8         xoff_refresh[0x10];

	u8         reserved_at_40[0x9];
	u8         fullness_threshold[0x7];
	u8         port_buffer_size[0x10];

	struct mlx5_ifc_bufferx_reg_bits buffer[10];

	u8         reserved_at_2e0[0x80];
};

struct mlx5_ifc_paos_reg_bits {
	u8         swid[0x8];
	u8         local_port[0x8];
	u8         reserved_0[0x4];
	u8         admin_status[0x4];
	u8         reserved_1[0x4];
	u8         oper_status[0x4];

	u8         ase[0x1];
	u8         ee[0x1];
	u8         reserved_2[0x1c];
	u8         e[0x2];

	u8         reserved_3[0x40];
};

struct mlx5_ifc_pamp_reg_bits {
	u8         reserved_0[0x8];
	u8         opamp_group[0x8];
	u8         reserved_1[0xc];
	u8         opamp_group_type[0x4];

	u8         start_index[0x10];
	u8         reserved_2[0x4];
	u8         num_of_indices[0xc];

	u8         index_data[18][0x10];
};

struct mlx5_ifc_link_level_retrans_cntr_grp_date_bits {
	u8         llr_rx_cells_high[0x20];

	u8         llr_rx_cells_low[0x20];

	u8         llr_rx_error_high[0x20];

	u8         llr_rx_error_low[0x20];

	u8         llr_rx_crc_error_high[0x20];

	u8         llr_rx_crc_error_low[0x20];

	u8         llr_tx_cells_high[0x20];

	u8         llr_tx_cells_low[0x20];

	u8         llr_tx_ret_cells_high[0x20];

	u8         llr_tx_ret_cells_low[0x20];

	u8         llr_tx_ret_events_high[0x20];

	u8         llr_tx_ret_events_low[0x20];

	u8         reserved_0[0x640];
};

struct mlx5_ifc_mtmp_reg_bits {
	u8         i[0x1];
	u8         reserved_at_1[0x18];
	u8         sensor_index[0x7];

	u8         reserved_at_20[0x10];
	u8         temperature[0x10];

	u8         mte[0x1];
	u8         mtr[0x1];
	u8         reserved_at_42[0x0e];
	u8         max_temperature[0x10];

	u8         tee[0x2];
	u8         reserved_at_62[0x0e];
	u8         temperature_threshold_hi[0x10];

	u8         reserved_at_80[0x10];
	u8         temperature_threshold_lo[0x10];

	u8         reserved_at_100[0x20];

	u8         sensor_name[0x40];
};

struct mlx5_ifc_lane_2_module_mapping_bits {
	u8         reserved_0[0x6];
	u8         rx_lane[0x2];
	u8         reserved_1[0x6];
	u8         tx_lane[0x2];
	u8         reserved_2[0x8];
	u8         module[0x8];
};

struct mlx5_ifc_eth_per_traffic_class_layout_bits {
	u8         transmit_queue_high[0x20];

	u8         transmit_queue_low[0x20];

	u8         reserved_0[0x780];
};

struct mlx5_ifc_eth_per_traffic_class_cong_layout_bits {
	u8         no_buffer_discard_uc_high[0x20];

	u8         no_buffer_discard_uc_low[0x20];

	u8         wred_discard_high[0x20];

	u8         wred_discard_low[0x20];

	u8         reserved_0[0x740];
};

struct mlx5_ifc_eth_per_prio_grp_data_layout_bits {
	u8         rx_octets_high[0x20];

	u8         rx_octets_low[0x20];

	u8         reserved_0[0xc0];

	u8         rx_frames_high[0x20];

	u8         rx_frames_low[0x20];

	u8         tx_octets_high[0x20];

	u8         tx_octets_low[0x20];

	u8         reserved_1[0xc0];

	u8         tx_frames_high[0x20];

	u8         tx_frames_low[0x20];

	u8         rx_pause_high[0x20];

	u8         rx_pause_low[0x20];

	u8         rx_pause_duration_high[0x20];

	u8         rx_pause_duration_low[0x20];

	u8         tx_pause_high[0x20];

	u8         tx_pause_low[0x20];

	u8         tx_pause_duration_high[0x20];

	u8         tx_pause_duration_low[0x20];

	u8         rx_pause_transition_high[0x20];

	u8         rx_pause_transition_low[0x20];

	u8         rx_discards_high[0x20];

	u8         rx_discards_low[0x20];

	u8         device_stall_minor_watermark_cnt_high[0x20];

	u8         device_stall_minor_watermark_cnt_low[0x20];

	u8         device_stall_critical_watermark_cnt_high[0x20];

	u8         device_stall_critical_watermark_cnt_low[0x20];

	u8         reserved_2[0x340];
};

struct mlx5_ifc_eth_extended_cntrs_grp_data_layout_bits {
	u8         port_transmit_wait_high[0x20];

	u8         port_transmit_wait_low[0x20];

	u8         ecn_marked_high[0x20];

	u8         ecn_marked_low[0x20];

	u8         no_buffer_discard_mc_high[0x20];

	u8         no_buffer_discard_mc_low[0x20];

	u8         rx_ebp_high[0x20];

	u8         rx_ebp_low[0x20];

	u8         tx_ebp_high[0x20];

	u8         tx_ebp_low[0x20];

        u8         rx_buffer_almost_full_high[0x20];

        u8         rx_buffer_almost_full_low[0x20];

        u8         rx_buffer_full_high[0x20];

        u8         rx_buffer_full_low[0x20];

        u8         rx_icrc_encapsulated_high[0x20];

        u8         rx_icrc_encapsulated_low[0x20];

	u8         reserved_0[0x80];

        u8         tx_stats_pkts64octets_high[0x20];

        u8         tx_stats_pkts64octets_low[0x20];

        u8         tx_stats_pkts65to127octets_high[0x20];

        u8         tx_stats_pkts65to127octets_low[0x20];

        u8         tx_stats_pkts128to255octets_high[0x20];

        u8         tx_stats_pkts128to255octets_low[0x20];

        u8         tx_stats_pkts256to511octets_high[0x20];

        u8         tx_stats_pkts256to511octets_low[0x20];

        u8         tx_stats_pkts512to1023octets_high[0x20];

        u8         tx_stats_pkts512to1023octets_low[0x20];

        u8         tx_stats_pkts1024to1518octets_high[0x20];

        u8         tx_stats_pkts1024to1518octets_low[0x20];

        u8         tx_stats_pkts1519to2047octets_high[0x20];

        u8         tx_stats_pkts1519to2047octets_low[0x20];

        u8         tx_stats_pkts2048to4095octets_high[0x20];

        u8         tx_stats_pkts2048to4095octets_low[0x20];

        u8         tx_stats_pkts4096to8191octets_high[0x20];

        u8         tx_stats_pkts4096to8191octets_low[0x20];

        u8         tx_stats_pkts8192to10239octets_high[0x20];

        u8         tx_stats_pkts8192to10239octets_low[0x20];

	u8         reserved_1[0x2C0];
};

struct mlx5_ifc_eth_802_3_cntrs_grp_data_layout_bits {
	u8         a_frames_transmitted_ok_high[0x20];

	u8         a_frames_transmitted_ok_low[0x20];

	u8         a_frames_received_ok_high[0x20];

	u8         a_frames_received_ok_low[0x20];

	u8         a_frame_check_sequence_errors_high[0x20];

	u8         a_frame_check_sequence_errors_low[0x20];

	u8         a_alignment_errors_high[0x20];

	u8         a_alignment_errors_low[0x20];

	u8         a_octets_transmitted_ok_high[0x20];

	u8         a_octets_transmitted_ok_low[0x20];

	u8         a_octets_received_ok_high[0x20];

	u8         a_octets_received_ok_low[0x20];

	u8         a_multicast_frames_xmitted_ok_high[0x20];

	u8         a_multicast_frames_xmitted_ok_low[0x20];

	u8         a_broadcast_frames_xmitted_ok_high[0x20];

	u8         a_broadcast_frames_xmitted_ok_low[0x20];

	u8         a_multicast_frames_received_ok_high[0x20];

	u8         a_multicast_frames_received_ok_low[0x20];

	u8         a_broadcast_frames_recieved_ok_high[0x20];

	u8         a_broadcast_frames_recieved_ok_low[0x20];

	u8         a_in_range_length_errors_high[0x20];

	u8         a_in_range_length_errors_low[0x20];

	u8         a_out_of_range_length_field_high[0x20];

	u8         a_out_of_range_length_field_low[0x20];

	u8         a_frame_too_long_errors_high[0x20];

	u8         a_frame_too_long_errors_low[0x20];

	u8         a_symbol_error_during_carrier_high[0x20];

	u8         a_symbol_error_during_carrier_low[0x20];

	u8         a_mac_control_frames_transmitted_high[0x20];

	u8         a_mac_control_frames_transmitted_low[0x20];

	u8         a_mac_control_frames_received_high[0x20];

	u8         a_mac_control_frames_received_low[0x20];

	u8         a_unsupported_opcodes_received_high[0x20];

	u8         a_unsupported_opcodes_received_low[0x20];

	u8         a_pause_mac_ctrl_frames_received_high[0x20];

	u8         a_pause_mac_ctrl_frames_received_low[0x20];

	u8         a_pause_mac_ctrl_frames_transmitted_high[0x20];

	u8         a_pause_mac_ctrl_frames_transmitted_low[0x20];

	u8         reserved_0[0x300];
};

struct mlx5_ifc_eth_3635_cntrs_grp_data_layout_bits {
	u8         dot3stats_alignment_errors_high[0x20];

	u8         dot3stats_alignment_errors_low[0x20];

	u8         dot3stats_fcs_errors_high[0x20];

	u8         dot3stats_fcs_errors_low[0x20];

	u8         dot3stats_single_collision_frames_high[0x20];

	u8         dot3stats_single_collision_frames_low[0x20];

	u8         dot3stats_multiple_collision_frames_high[0x20];

	u8         dot3stats_multiple_collision_frames_low[0x20];

	u8         dot3stats_sqe_test_errors_high[0x20];

	u8         dot3stats_sqe_test_errors_low[0x20];

	u8         dot3stats_deferred_transmissions_high[0x20];

	u8         dot3stats_deferred_transmissions_low[0x20];

	u8         dot3stats_late_collisions_high[0x20];

	u8         dot3stats_late_collisions_low[0x20];

	u8         dot3stats_excessive_collisions_high[0x20];

	u8         dot3stats_excessive_collisions_low[0x20];

	u8         dot3stats_internal_mac_transmit_errors_high[0x20];

	u8         dot3stats_internal_mac_transmit_errors_low[0x20];

	u8         dot3stats_carrier_sense_errors_high[0x20];

	u8         dot3stats_carrier_sense_errors_low[0x20];

	u8         dot3stats_frame_too_longs_high[0x20];

	u8         dot3stats_frame_too_longs_low[0x20];

	u8         dot3stats_internal_mac_receive_errors_high[0x20];

	u8         dot3stats_internal_mac_receive_errors_low[0x20];

	u8         dot3stats_symbol_errors_high[0x20];

	u8         dot3stats_symbol_errors_low[0x20];

	u8         dot3control_in_unknown_opcodes_high[0x20];

	u8         dot3control_in_unknown_opcodes_low[0x20];

	u8         dot3in_pause_frames_high[0x20];

	u8         dot3in_pause_frames_low[0x20];

	u8         dot3out_pause_frames_high[0x20];

	u8         dot3out_pause_frames_low[0x20];

	u8         reserved_0[0x3c0];
};

struct mlx5_ifc_eth_2863_cntrs_grp_data_layout_bits {
	u8         if_in_octets_high[0x20];

	u8         if_in_octets_low[0x20];

	u8         if_in_ucast_pkts_high[0x20];

	u8         if_in_ucast_pkts_low[0x20];

	u8         if_in_discards_high[0x20];

	u8         if_in_discards_low[0x20];

	u8         if_in_errors_high[0x20];

	u8         if_in_errors_low[0x20];

	u8         if_in_unknown_protos_high[0x20];

	u8         if_in_unknown_protos_low[0x20];

	u8         if_out_octets_high[0x20];

	u8         if_out_octets_low[0x20];

	u8         if_out_ucast_pkts_high[0x20];

	u8         if_out_ucast_pkts_low[0x20];

	u8         if_out_discards_high[0x20];

	u8         if_out_discards_low[0x20];

	u8         if_out_errors_high[0x20];

	u8         if_out_errors_low[0x20];

	u8         if_in_multicast_pkts_high[0x20];

	u8         if_in_multicast_pkts_low[0x20];

	u8         if_in_broadcast_pkts_high[0x20];

	u8         if_in_broadcast_pkts_low[0x20];

	u8         if_out_multicast_pkts_high[0x20];

	u8         if_out_multicast_pkts_low[0x20];

	u8         if_out_broadcast_pkts_high[0x20];

	u8         if_out_broadcast_pkts_low[0x20];

	u8         reserved_0[0x480];
};

struct mlx5_ifc_eth_2819_cntrs_grp_data_layout_bits {
	u8         ether_stats_drop_events_high[0x20];

	u8         ether_stats_drop_events_low[0x20];

	u8         ether_stats_octets_high[0x20];

	u8         ether_stats_octets_low[0x20];

	u8         ether_stats_pkts_high[0x20];

	u8         ether_stats_pkts_low[0x20];

	u8         ether_stats_broadcast_pkts_high[0x20];

	u8         ether_stats_broadcast_pkts_low[0x20];

	u8         ether_stats_multicast_pkts_high[0x20];

	u8         ether_stats_multicast_pkts_low[0x20];

	u8         ether_stats_crc_align_errors_high[0x20];

	u8         ether_stats_crc_align_errors_low[0x20];

	u8         ether_stats_undersize_pkts_high[0x20];

	u8         ether_stats_undersize_pkts_low[0x20];

	u8         ether_stats_oversize_pkts_high[0x20];

	u8         ether_stats_oversize_pkts_low[0x20];

	u8         ether_stats_fragments_high[0x20];

	u8         ether_stats_fragments_low[0x20];

	u8         ether_stats_jabbers_high[0x20];

	u8         ether_stats_jabbers_low[0x20];

	u8         ether_stats_collisions_high[0x20];

	u8         ether_stats_collisions_low[0x20];

	u8         ether_stats_pkts64octets_high[0x20];

	u8         ether_stats_pkts64octets_low[0x20];

	u8         ether_stats_pkts65to127octets_high[0x20];

	u8         ether_stats_pkts65to127octets_low[0x20];

	u8         ether_stats_pkts128to255octets_high[0x20];

	u8         ether_stats_pkts128to255octets_low[0x20];

	u8         ether_stats_pkts256to511octets_high[0x20];

	u8         ether_stats_pkts256to511octets_low[0x20];

	u8         ether_stats_pkts512to1023octets_high[0x20];

	u8         ether_stats_pkts512to1023octets_low[0x20];

	u8         ether_stats_pkts1024to1518octets_high[0x20];

	u8         ether_stats_pkts1024to1518octets_low[0x20];

	u8         ether_stats_pkts1519to2047octets_high[0x20];

	u8         ether_stats_pkts1519to2047octets_low[0x20];

	u8         ether_stats_pkts2048to4095octets_high[0x20];

	u8         ether_stats_pkts2048to4095octets_low[0x20];

	u8         ether_stats_pkts4096to8191octets_high[0x20];

	u8         ether_stats_pkts4096to8191octets_low[0x20];

	u8         ether_stats_pkts8192to10239octets_high[0x20];

	u8         ether_stats_pkts8192to10239octets_low[0x20];

	u8         reserved_0[0x280];
};

struct mlx5_ifc_ib_portcntrs_attribute_grp_data_bits {
	u8         symbol_error_counter[0x10];
	u8         link_error_recovery_counter[0x8];
	u8         link_downed_counter[0x8];

	u8         port_rcv_errors[0x10];
	u8         port_rcv_remote_physical_errors[0x10];

	u8         port_rcv_switch_relay_errors[0x10];
	u8         port_xmit_discards[0x10];

	u8         port_xmit_constraint_errors[0x8];
	u8         port_rcv_constraint_errors[0x8];
	u8         reserved_0[0x8];
	u8         local_link_integrity_errors[0x4];
	u8         excessive_buffer_overrun_errors[0x4];

	u8         reserved_1[0x10];
	u8         vl_15_dropped[0x10];

	u8         port_xmit_data[0x20];

	u8         port_rcv_data[0x20];

	u8         port_xmit_pkts[0x20];

	u8         port_rcv_pkts[0x20];

	u8         port_xmit_wait[0x20];

	u8         reserved_2[0x680];
};

struct mlx5_ifc_trc_tlb_reg_bits {
	u8         reserved_0[0x80];

	u8         tlb_addr[0][0x40];
};

struct mlx5_ifc_trc_read_fifo_reg_bits {
	u8         reserved_0[0x10];
	u8         requested_event_num[0x10];

	u8         reserved_1[0x20];

	u8         reserved_2[0x10];
	u8         acual_event_num[0x10];

	u8         reserved_3[0x20];

	u8         event[0][0x40];
};

struct mlx5_ifc_trc_lock_reg_bits {
	u8         reserved_0[0x1f];
	u8         lock[0x1];

	u8         reserved_1[0x60];
};

struct mlx5_ifc_trc_filter_reg_bits {
	u8         status[0x1];
	u8         reserved_0[0xf];
	u8         filter_index[0x10];

	u8         reserved_1[0x20];

	u8         filter_val[0x20];

	u8         reserved_2[0x1a0];
};

struct mlx5_ifc_trc_event_reg_bits {
	u8         status[0x1];
	u8         reserved_0[0xf];
	u8         event_index[0x10];

	u8         reserved_1[0x20];

	u8         event_id[0x20];

	u8         event_selector_val[0x10];
	u8         event_selector_size[0x10];

	u8         reserved_2[0x180];
};

struct mlx5_ifc_trc_conf_reg_bits {
	u8         limit_en[0x1];
	u8         reserved_0[0x3];
	u8         dump_mode[0x4];
	u8         reserved_1[0x15];
	u8         state[0x3];

	u8         reserved_2[0x20];

	u8         limit_event_index[0x20];

	u8         mkey[0x20];

	u8         fifo_ready_ev_num[0x20];

	u8         reserved_3[0x160];
};

struct mlx5_ifc_trc_cap_reg_bits {
	u8         reserved_0[0x18];
	u8         dump_mode[0x8];

	u8         reserved_1[0x20];

	u8         num_of_events[0x10];
	u8         num_of_filters[0x10];

	u8         fifo_size[0x20];

	u8         tlb_size[0x10];
	u8         event_size[0x10];

	u8         reserved_2[0x160];
};

struct mlx5_ifc_set_node_in_bits {
	u8         node_description[64][0x8];
};

struct mlx5_ifc_register_power_settings_bits {
	u8         reserved_0[0x18];
	u8         power_settings_level[0x8];

	u8         reserved_1[0x60];
};

struct mlx5_ifc_register_host_endianess_bits {
	u8         he[0x1];
	u8         reserved_0[0x1f];

	u8         reserved_1[0x60];
};

struct mlx5_ifc_register_diag_buffer_ctrl_bits {
	u8         physical_address[0x40];
};

struct mlx5_ifc_qtct_reg_bits {
	u8         operation_type[0x2];
	u8         cap_local_admin[0x1];
	u8         cap_remote_admin[0x1];
	u8         reserved_0[0x4];
	u8         port_number[0x8];
	u8         reserved_1[0xd];
	u8         prio[0x3];

	u8         reserved_2[0x1d];
	u8         tclass[0x3];
};

struct mlx5_ifc_qpdp_reg_bits {
	u8         reserved_0[0x8];
	u8         port_number[0x8];
	u8         reserved_1[0x10];

	u8         reserved_2[0x1d];
	u8         pprio[0x3];
};

struct mlx5_ifc_port_info_ro_fields_param_bits {
	u8         reserved_0[0x8];
	u8         port[0x8];
	u8         max_gid[0x10];

	u8         reserved_1[0x20];

	u8         port_guid[0x40];
};

struct mlx5_ifc_nvqc_reg_bits {
	u8         type[0x20];

	u8         reserved_0[0x18];
	u8         version[0x4];
	u8         reserved_1[0x2];
	u8         support_wr[0x1];
	u8         support_rd[0x1];
};

struct mlx5_ifc_nvia_reg_bits {
	u8         reserved_0[0x1d];
	u8         target[0x3];

	u8         reserved_1[0x20];
};

struct mlx5_ifc_nvdi_reg_bits {
	struct mlx5_ifc_config_item_bits configuration_item_header;
};

struct mlx5_ifc_nvda_reg_bits {
	struct mlx5_ifc_config_item_bits configuration_item_header;

	u8         configuration_item_data[0x20];
};

struct mlx5_ifc_node_info_ro_fields_param_bits {
	u8         system_image_guid[0x40];

	u8         reserved_0[0x40];

	u8         node_guid[0x40];

	u8         reserved_1[0x10];
	u8         max_pkey[0x10];

	u8         reserved_2[0x20];
};

struct mlx5_ifc_ets_tcn_config_reg_bits {
	u8         g[0x1];
	u8         b[0x1];
	u8         r[0x1];
	u8         reserved_0[0x9];
	u8         group[0x4];
	u8         reserved_1[0x9];
	u8         bw_allocation[0x7];

	u8         reserved_2[0xc];
	u8         max_bw_units[0x4];
	u8         reserved_3[0x8];
	u8         max_bw_value[0x8];
};

struct mlx5_ifc_ets_global_config_reg_bits {
	u8         reserved_0[0x2];
	u8         r[0x1];
	u8         reserved_1[0x1d];

	u8         reserved_2[0xc];
	u8         max_bw_units[0x4];
	u8         reserved_3[0x8];
	u8         max_bw_value[0x8];
};

struct mlx5_ifc_qetc_reg_bits {
	u8                                         reserved_at_0[0x8];
	u8                                         port_number[0x8];
	u8                                         reserved_at_10[0x30];

	struct mlx5_ifc_ets_tcn_config_reg_bits    tc_configuration[0x8];
	struct mlx5_ifc_ets_global_config_reg_bits global_configuration;
};

struct mlx5_ifc_nodnic_mac_filters_bits {
	struct mlx5_ifc_mac_address_layout_bits mac_filter0;

	struct mlx5_ifc_mac_address_layout_bits mac_filter1;

	struct mlx5_ifc_mac_address_layout_bits mac_filter2;

	struct mlx5_ifc_mac_address_layout_bits mac_filter3;

	struct mlx5_ifc_mac_address_layout_bits mac_filter4;

	u8         reserved_0[0xc0];
};

struct mlx5_ifc_nodnic_gid_filters_bits {
	u8         mgid_filter0[16][0x8];

	u8         mgid_filter1[16][0x8];

	u8         mgid_filter2[16][0x8];

	u8         mgid_filter3[16][0x8];
};

enum {
	MLX5_NODNIC_CONFIG_REG_NUM_PORTS_SINGLE_PORT  = 0x0,
	MLX5_NODNIC_CONFIG_REG_NUM_PORTS_DUAL_PORT    = 0x1,
};

enum {
	MLX5_NODNIC_CONFIG_REG_CQE_FORMAT_LEGACY_CQE  = 0x0,
	MLX5_NODNIC_CONFIG_REG_CQE_FORMAT_NEW_CQE     = 0x1,
};

struct mlx5_ifc_nodnic_config_reg_bits {
	u8         no_dram_nic_revision[0x8];
	u8         hardware_format[0x8];
	u8         support_receive_filter[0x1];
	u8         support_promisc_filter[0x1];
	u8         support_promisc_multicast_filter[0x1];
	u8         reserved_0[0x2];
	u8         log_working_buffer_size[0x3];
	u8         log_pkey_table_size[0x4];
	u8         reserved_1[0x3];
	u8         num_ports[0x1];

	u8         reserved_2[0x2];
	u8         log_max_ring_size[0x6];
	u8         reserved_3[0x18];

	u8         lkey[0x20];

	u8         cqe_format[0x4];
	u8         reserved_4[0x1c];

	u8         node_guid[0x40];

	u8         reserved_5[0x740];

	struct mlx5_ifc_nodnic_port_config_reg_bits port1_settings;

	struct mlx5_ifc_nodnic_port_config_reg_bits port2_settings;
};

struct mlx5_ifc_vlan_layout_bits {
	u8         reserved_0[0x14];
	u8         vlan[0xc];

	u8         reserved_1[0x20];
};

struct mlx5_ifc_umr_pointer_desc_argument_bits {
	u8         reserved_0[0x20];

	u8         mkey[0x20];

	u8         addressh_63_32[0x20];

	u8         addressl_31_0[0x20];
};

struct mlx5_ifc_ud_adrs_vector_bits {
	u8         dc_key[0x40];

	u8         ext[0x1];
	u8         reserved_0[0x7];
	u8         destination_qp_dct[0x18];

	u8         static_rate[0x4];
	u8         sl_eth_prio[0x4];
	u8         fl[0x1];
	u8         mlid[0x7];
	u8         rlid_udp_sport[0x10];

	u8         reserved_1[0x20];

	u8         rmac_47_16[0x20];

	u8         rmac_15_0[0x10];
	u8         tclass[0x8];
	u8         hop_limit[0x8];

	u8         reserved_2[0x1];
	u8         grh[0x1];
	u8         reserved_3[0x2];
	u8         src_addr_index[0x8];
	u8         flow_label[0x14];

	u8         rgid_rip[16][0x8];
};

struct mlx5_ifc_port_module_event_bits {
	u8         reserved_0[0x8];
	u8         module[0x8];
	u8         reserved_1[0xc];
	u8         module_status[0x4];

	u8         reserved_2[0x14];
	u8         error_type[0x4];
	u8         reserved_3[0x8];

	u8         reserved_4[0xa0];
};

struct mlx5_ifc_icmd_control_bits {
	u8         opcode[0x10];
	u8         status[0x8];
	u8         reserved_0[0x7];
	u8         busy[0x1];
};

struct mlx5_ifc_eqe_bits {
	u8         reserved_0[0x8];
	u8         event_type[0x8];
	u8         reserved_1[0x8];
	u8         event_sub_type[0x8];

	u8         reserved_2[0xe0];

	union mlx5_ifc_event_auto_bits event_data;

	u8         reserved_3[0x10];
	u8         signature[0x8];
	u8         reserved_4[0x7];
	u8         owner[0x1];
};

enum {
	MLX5_CMD_QUEUE_ENTRY_TYPE_PCIE_CMD_IF_TRANSPORT  = 0x7,
};

struct mlx5_ifc_cmd_queue_entry_bits {
	u8         type[0x8];
	u8         reserved_0[0x18];

	u8         input_length[0x20];

	u8         input_mailbox_pointer_63_32[0x20];

	u8         input_mailbox_pointer_31_9[0x17];
	u8         reserved_1[0x9];

	u8         command_input_inline_data[16][0x8];

	u8         command_output_inline_data[16][0x8];

	u8         output_mailbox_pointer_63_32[0x20];

	u8         output_mailbox_pointer_31_9[0x17];
	u8         reserved_2[0x9];

	u8         output_length[0x20];

	u8         token[0x8];
	u8         signature[0x8];
	u8         reserved_3[0x8];
	u8         status[0x7];
	u8         ownership[0x1];
};

struct mlx5_ifc_cmd_out_bits {
	u8         status[0x8];
	u8         reserved_0[0x18];

	u8         syndrome[0x20];

	u8         command_output[0x20];
};

struct mlx5_ifc_cmd_in_bits {
	u8         opcode[0x10];
	u8         reserved_0[0x10];

	u8         reserved_1[0x10];
	u8         op_mod[0x10];

	u8         command[0][0x20];
};

struct mlx5_ifc_cmd_if_box_bits {
	u8         mailbox_data[512][0x8];

	u8         reserved_0[0x180];

	u8         next_pointer_63_32[0x20];

	u8         next_pointer_31_10[0x16];
	u8         reserved_1[0xa];

	u8         block_number[0x20];

	u8         reserved_2[0x8];
	u8         token[0x8];
	u8         ctrl_signature[0x8];
	u8         signature[0x8];
};

struct mlx5_ifc_mtt_bits {
	u8         ptag_63_32[0x20];

	u8         ptag_31_8[0x18];
	u8         reserved_0[0x6];
	u8         wr_en[0x1];
	u8         rd_en[0x1];
};

struct mlx5_ifc_tls_progress_params_bits {
	u8         valid[0x1];
	u8         reserved_at_1[0x7];
	u8         pd[0x18];

	u8         next_record_tcp_sn[0x20];

	u8         hw_resync_tcp_sn[0x20];

	u8         record_tracker_state[0x2];
	u8         auth_state[0x2];
	u8         reserved_at_64[0x4];
	u8         hw_offset_record_number[0x18];
};

struct mlx5_ifc_tls_static_params_bits {
	u8         const_2[0x2];
	u8         tls_version[0x4];
	u8         const_1[0x2];
	u8         reserved_at_8[0x14];
	u8         encryption_standard[0x4];

	u8         reserved_at_20[0x20];

	u8         initial_record_number[0x40];

	u8         resync_tcp_sn[0x20];

	u8         gcm_iv[0x20];

	u8         implicit_iv[0x40];

	u8         reserved_at_100[0x8];
	u8         dek_index[0x18];

	u8         reserved_at_120[0xe0];
};

/* Vendor Specific Capabilities, VSC */
enum {
	MLX5_VSC_DOMAIN_ICMD			= 0x1,
	MLX5_VSC_DOMAIN_PROTECTED_CRSPACE	= 0x6,
	MLX5_VSC_DOMAIN_SCAN_CRSPACE		= 0x7,
	MLX5_VSC_DOMAIN_SEMAPHORES		= 0xA,
};

struct mlx5_ifc_vendor_specific_cap_bits {
	u8         type[0x8];
	u8         length[0x8];
	u8         next_pointer[0x8];
	u8         capability_id[0x8];

	u8         status[0x3];
	u8         reserved_0[0xd];
	u8         space[0x10];

	u8         counter[0x20];

	u8         semaphore[0x20];

	u8         flag[0x1];
	u8         reserved_1[0x1];
	u8         address[0x1e];

	u8         data[0x20];
};

struct mlx5_ifc_vsc_space_bits {
	u8 status[0x3];
	u8 reserved0[0xd];
	u8 space[0x10];
};

struct mlx5_ifc_vsc_addr_bits {
	u8 flag[0x1];
	u8 reserved0[0x1];
	u8 address[0x1e];
};

enum {
	MLX5_INITIAL_SEG_NIC_INTERFACE_FULL_DRIVER  = 0x0,
	MLX5_INITIAL_SEG_NIC_INTERFACE_DISABLED     = 0x1,
	MLX5_INITIAL_SEG_NIC_INTERFACE_NO_DRAM_NIC  = 0x2,
};

enum {
	MLX5_INITIAL_SEG_NIC_INTERFACE_SUPPORTED_FULL_DRIVER  = 0x0,
	MLX5_INITIAL_SEG_NIC_INTERFACE_SUPPORTED_DISABLED     = 0x1,
	MLX5_INITIAL_SEG_NIC_INTERFACE_SUPPORTED_NO_DRAM_NIC  = 0x2,
};

enum {
	MLX5_HEALTH_SYNDR_FW_ERR                                      = 0x1,
	MLX5_HEALTH_SYNDR_IRISC_ERR                                   = 0x7,
	MLX5_HEALTH_SYNDR_HW_UNRECOVERABLE_ERR                        = 0x8,
	MLX5_HEALTH_SYNDR_CRC_ERR                                     = 0x9,
	MLX5_HEALTH_SYNDR_FETCH_PCI_ERR                               = 0xa,
	MLX5_HEALTH_SYNDR_HW_FTL_ERR                                  = 0xb,
	MLX5_HEALTH_SYNDR_ASYNC_EQ_OVERRUN_ERR                        = 0xc,
	MLX5_HEALTH_SYNDR_EQ_ERR                                      = 0xd,
	MLX5_HEALTH_SYNDR_EQ_INV                                      = 0xe,
	MLX5_HEALTH_SYNDR_FFSER_ERR                                   = 0xf,
	MLX5_HEALTH_SYNDR_HIGH_TEMP                                   = 0x10,
};

struct mlx5_ifc_initial_seg_bits {
	u8         fw_rev_minor[0x10];
	u8         fw_rev_major[0x10];

	u8         cmd_interface_rev[0x10];
	u8         fw_rev_subminor[0x10];

	u8         reserved_0[0x40];

	u8         cmdq_phy_addr_63_32[0x20];

	u8         cmdq_phy_addr_31_12[0x14];
	u8         reserved_1[0x2];
	u8         nic_interface[0x2];
	u8         log_cmdq_size[0x4];
	u8         log_cmdq_stride[0x4];

	u8         command_doorbell_vector[0x20];

	u8         reserved_2[0xf00];

	u8         initializing[0x1];
	u8         reserved_3[0x4];
	u8         nic_interface_supported[0x3];
	u8         reserved_4[0x18];

	struct mlx5_ifc_health_buffer_bits health_buffer;

	u8         no_dram_nic_offset[0x20];

	u8         reserved_5[0x6de0];

	u8         internal_timer_h[0x20];

	u8         internal_timer_l[0x20];

	u8         reserved_6[0x20];

	u8         reserved_7[0x1f];
	u8         clear_int[0x1];

	u8         health_syndrome[0x8];
	u8         health_counter[0x18];

	u8         reserved_8[0x17fc0];
};

union mlx5_ifc_icmd_interface_document_bits {
	struct mlx5_ifc_fw_version_bits fw_version;
	struct mlx5_ifc_icmd_access_reg_in_bits icmd_access_reg_in;
	struct mlx5_ifc_icmd_access_reg_out_bits icmd_access_reg_out;
	struct mlx5_ifc_icmd_init_ocsd_in_bits icmd_init_ocsd_in;
	struct mlx5_ifc_icmd_ocbb_init_in_bits icmd_ocbb_init_in;
	struct mlx5_ifc_icmd_ocbb_query_etoc_stats_out_bits icmd_ocbb_query_etoc_stats_out;
	struct mlx5_ifc_icmd_ocbb_query_header_stats_out_bits icmd_ocbb_query_header_stats_out;
	struct mlx5_ifc_icmd_query_cap_general_bits icmd_query_cap_general;
	struct mlx5_ifc_icmd_query_cap_in_bits icmd_query_cap_in;
	struct mlx5_ifc_icmd_query_fw_info_out_bits icmd_query_fw_info_out;
	struct mlx5_ifc_icmd_query_virtual_mac_out_bits icmd_query_virtual_mac_out;
	struct mlx5_ifc_icmd_set_virtual_mac_in_bits icmd_set_virtual_mac_in;
	struct mlx5_ifc_icmd_set_wol_rol_in_bits icmd_set_wol_rol_in;
	struct mlx5_ifc_icmd_set_wol_rol_out_bits icmd_set_wol_rol_out;
	u8         reserved_0[0x42c0];
};

union mlx5_ifc_eth_cntrs_grp_data_layout_auto_bits {
	struct mlx5_ifc_eth_802_3_cntrs_grp_data_layout_bits eth_802_3_cntrs_grp_data_layout;
	struct mlx5_ifc_eth_2863_cntrs_grp_data_layout_bits eth_2863_cntrs_grp_data_layout;
	struct mlx5_ifc_eth_2819_cntrs_grp_data_layout_bits eth_2819_cntrs_grp_data_layout;
	struct mlx5_ifc_eth_3635_cntrs_grp_data_layout_bits eth_3635_cntrs_grp_data_layout;
	struct mlx5_ifc_eth_extended_cntrs_grp_data_layout_bits eth_extended_cntrs_grp_data_layout;
	struct mlx5_ifc_eth_discard_cntrs_grp_bits eth_discard_cntrs_grp;
	struct mlx5_ifc_eth_per_prio_grp_data_layout_bits eth_per_prio_grp_data_layout;
	struct mlx5_ifc_phys_layer_cntrs_bits phys_layer_cntrs;
	struct mlx5_ifc_phys_layer_statistical_cntrs_bits phys_layer_statistical_cntrs;
	struct mlx5_ifc_infiniband_port_cntrs_bits infiniband_port_cntrs;
	u8         reserved_0[0x7c0];
};

struct mlx5_ifc_ppcnt_reg_bits {
	u8         swid[0x8];
	u8         local_port[0x8];
	u8         pnat[0x2];
	u8         reserved_0[0x8];
	u8         grp[0x6];

	u8         clr[0x1];
	u8         reserved_1[0x1c];
	u8         prio_tc[0x3];

	union mlx5_ifc_eth_cntrs_grp_data_layout_auto_bits counter_set;
};

struct mlx5_ifc_pcie_lanes_counters_bits {
	u8         life_time_counter_high[0x20];

	u8         life_time_counter_low[0x20];

	u8         error_counter_lane0[0x20];

	u8         error_counter_lane1[0x20];

	u8         error_counter_lane2[0x20];

	u8         error_counter_lane3[0x20];

	u8         error_counter_lane4[0x20];

	u8         error_counter_lane5[0x20];

	u8         error_counter_lane6[0x20];

	u8         error_counter_lane7[0x20];

	u8         error_counter_lane8[0x20];

	u8         error_counter_lane9[0x20];

	u8         error_counter_lane10[0x20];

	u8         error_counter_lane11[0x20];

	u8         error_counter_lane12[0x20];

	u8         error_counter_lane13[0x20];

	u8         error_counter_lane14[0x20];

	u8         error_counter_lane15[0x20];

	u8         reserved_at_240[0x580];
};

struct mlx5_ifc_pcie_lanes_counters_ext_bits {
	u8         reserved_at_0[0x40];

	u8         error_counter_lane0[0x20];

	u8         error_counter_lane1[0x20];

	u8         error_counter_lane2[0x20];

	u8         error_counter_lane3[0x20];

	u8         error_counter_lane4[0x20];

	u8         error_counter_lane5[0x20];

	u8         error_counter_lane6[0x20];

	u8         error_counter_lane7[0x20];

	u8         error_counter_lane8[0x20];

	u8         error_counter_lane9[0x20];

	u8         error_counter_lane10[0x20];

	u8         error_counter_lane11[0x20];

	u8         error_counter_lane12[0x20];

	u8         error_counter_lane13[0x20];

	u8         error_counter_lane14[0x20];

	u8         error_counter_lane15[0x20];

	u8         reserved_at_240[0x580];
};

struct mlx5_ifc_pcie_perf_counters_bits {
	u8         life_time_counter_high[0x20];

	u8         life_time_counter_low[0x20];

	u8         rx_errors[0x20];

	u8         tx_errors[0x20];

	u8         l0_to_recovery_eieos[0x20];

	u8         l0_to_recovery_ts[0x20];

	u8         l0_to_recovery_framing[0x20];

	u8         l0_to_recovery_retrain[0x20];

	u8         crc_error_dllp[0x20];

	u8         crc_error_tlp[0x20];

	u8         tx_overflow_buffer_pkt[0x40];

	u8         outbound_stalled_reads[0x20];

	u8         outbound_stalled_writes[0x20];

	u8         outbound_stalled_reads_events[0x20];

	u8         outbound_stalled_writes_events[0x20];

	u8         tx_overflow_buffer_marked_pkt[0x40];

	u8         reserved_at_240[0x580];
};

struct mlx5_ifc_pcie_perf_counters_ext_bits {
	u8         reserved_at_0[0x40];

	u8         rx_errors[0x20];

	u8         tx_errors[0x20];

	u8         reserved_at_80[0xc0];

	u8         tx_overflow_buffer_pkt[0x40];

	u8         outbound_stalled_reads[0x20];

	u8         outbound_stalled_writes[0x20];

	u8         outbound_stalled_reads_events[0x20];

	u8         outbound_stalled_writes_events[0x20];

	u8         tx_overflow_buffer_marked_pkt[0x40];

	u8         reserved_at_240[0x580];
};

struct mlx5_ifc_pcie_timers_states_bits {
	u8         life_time_counter_high[0x20];

	u8         life_time_counter_low[0x20];

	u8         time_to_boot_image_start[0x20];

	u8         time_to_link_image[0x20];

	u8         calibration_time[0x20];

	u8         time_to_first_perst[0x20];

	u8         time_to_detect_state[0x20];

	u8         time_to_l0[0x20];

	u8         time_to_crs_en[0x20];

	u8         time_to_plastic_image_start[0x20];

	u8         time_to_iron_image_start[0x20];

	u8         perst_handler[0x20];

	u8         times_in_l1[0x20];

	u8         times_in_l23[0x20];

	u8         dl_down[0x20];

	u8         config_cycle1usec[0x20];

	u8         config_cycle2to7usec[0x20];

	u8         config_cycle8to15usec[0x20];

	u8         config_cycle16to63usec[0x20];

	u8         config_cycle64usec[0x20];

	u8         correctable_err_msg_sent[0x20];

	u8         non_fatal_err_msg_sent[0x20];

	u8         fatal_err_msg_sent[0x20];

	u8         reserved_at_2e0[0x4e0];
};

struct mlx5_ifc_pcie_timers_states_ext_bits {
	u8         reserved_at_0[0x40];

	u8         time_to_boot_image_start[0x20];

	u8         time_to_link_image[0x20];

	u8         calibration_time[0x20];

	u8         time_to_first_perst[0x20];

	u8         time_to_detect_state[0x20];

	u8         time_to_l0[0x20];

	u8         time_to_crs_en[0x20];

	u8         time_to_plastic_image_start[0x20];

	u8         time_to_iron_image_start[0x20];

	u8         perst_handler[0x20];

	u8         times_in_l1[0x20];

	u8         times_in_l23[0x20];

	u8         dl_down[0x20];

	u8         config_cycle1usec[0x20];

	u8         config_cycle2to7usec[0x20];

	u8         config_cycle8to15usec[0x20];

	u8         config_cycle16to63usec[0x20];

	u8         config_cycle64usec[0x20];

	u8         correctable_err_msg_sent[0x20];

	u8         non_fatal_err_msg_sent[0x20];

	u8         fatal_err_msg_sent[0x20];

	u8         reserved_at_2e0[0x4e0];
};

union mlx5_ifc_mpcnt_reg_counter_set_auto_bits {
	struct mlx5_ifc_pcie_perf_counters_bits pcie_perf_counters;
	struct mlx5_ifc_pcie_lanes_counters_bits pcie_lanes_counters;
	struct mlx5_ifc_pcie_timers_states_bits pcie_timers_states;
	u8         reserved_at_0[0x7c0];
};

union mlx5_ifc_mpcnt_reg_counter_set_auto_ext_bits {
	struct mlx5_ifc_pcie_perf_counters_ext_bits pcie_perf_counters_ext;
	struct mlx5_ifc_pcie_lanes_counters_ext_bits pcie_lanes_counters_ext;
	struct mlx5_ifc_pcie_timers_states_ext_bits pcie_timers_states_ext;
	u8         reserved_at_0[0x7c0];
};

struct mlx5_ifc_mpcnt_reg_bits {
	u8         reserved_at_0[0x2];
	u8         depth[0x6];
	u8         pcie_index[0x8];
	u8         node[0x8];
	u8         reserved_at_18[0x2];
	u8         grp[0x6];

	u8         clr[0x1];
	u8         reserved_at_21[0x1f];

	union mlx5_ifc_mpcnt_reg_counter_set_auto_bits counter_set;
};

struct mlx5_ifc_mpcnt_reg_ext_bits {
	u8         reserved_at_0[0x2];
	u8         depth[0x6];
	u8         pcie_index[0x8];
	u8         node[0x8];
	u8         reserved_at_18[0x2];
	u8         grp[0x6];

	u8         clr[0x1];
	u8         reserved_at_21[0x1f];

	union mlx5_ifc_mpcnt_reg_counter_set_auto_ext_bits counter_set;
};

struct mlx5_ifc_monitor_opcodes_layout_bits {
	u8         reserved_at_0[0x10];
	u8         monitor_opcode[0x10];
};

union mlx5_ifc_pddr_status_opcode_bits {
	struct mlx5_ifc_monitor_opcodes_layout_bits monitor_opcodes;
	u8         reserved_at_0[0x20];
};

struct mlx5_ifc_troubleshooting_info_page_layout_bits {
	u8         reserved_at_0[0x10];
	u8         group_opcode[0x10];

	union mlx5_ifc_pddr_status_opcode_bits status_opcode;

	u8         user_feedback_data[0x10];
	u8         user_feedback_index[0x10];

	u8         status_message[0x760];
};

union mlx5_ifc_pddr_page_data_bits {
	struct mlx5_ifc_troubleshooting_info_page_layout_bits troubleshooting_info_page;
	struct mlx5_ifc_pddr_module_info_bits pddr_module_info;
	u8         reserved_at_0[0x7c0];
};

struct mlx5_ifc_pddr_reg_bits {
	u8         reserved_at_0[0x8];
	u8         local_port[0x8];
	u8         pnat[0x2];
	u8         reserved_at_12[0xe];

	u8         reserved_at_20[0x18];
	u8         page_select[0x8];

	union mlx5_ifc_pddr_page_data_bits page_data;
};

enum {
	MLX5_ACCESS_REG_SUMMARY_CTRL_ID_MPEIN = 0x9050,
	MLX5_MPEIN_PWR_STATUS_INVALID = 0,
	MLX5_MPEIN_PWR_STATUS_SUFFICIENT = 1,
	MLX5_MPEIN_PWR_STATUS_INSUFFICIENT = 2,
};

struct mlx5_ifc_mpein_reg_bits {
	u8         reserved_at_0[0x2];
	u8         depth[0x6];
	u8         pcie_index[0x8];
	u8         node[0x8];
	u8         reserved_at_18[0x8];

	u8         capability_mask[0x20];

	u8         reserved_at_40[0x8];
	u8         link_width_enabled[0x8];
	u8         link_speed_enabled[0x10];

	u8         lane0_physical_position[0x8];
	u8         link_width_active[0x8];
	u8         link_speed_active[0x10];

	u8         num_of_pfs[0x10];
	u8         num_of_vfs[0x10];

	u8         bdf0[0x10];
	u8         reserved_at_b0[0x10];

	u8         max_read_request_size[0x4];
	u8         max_payload_size[0x4];
	u8         reserved_at_c8[0x5];
	u8         pwr_status[0x3];
	u8         port_type[0x4];
	u8         reserved_at_d4[0xb];
	u8         lane_reversal[0x1];

	u8         reserved_at_e0[0x14];
	u8         pci_power[0xc];

	u8         reserved_at_100[0x20];

	u8         device_status[0x10];
	u8         port_state[0x8];
	u8         reserved_at_138[0x8];

	u8         reserved_at_140[0x10];
	u8         receiver_detect_result[0x10];

	u8         reserved_at_160[0x20];
};

struct mlx5_ifc_mpein_reg_ext_bits {
	u8         reserved_at_0[0x2];
	u8         depth[0x6];
	u8         pcie_index[0x8];
	u8         node[0x8];
	u8         reserved_at_18[0x8];

	u8         reserved_at_20[0x20];

	u8         reserved_at_40[0x8];
	u8         link_width_enabled[0x8];
	u8         link_speed_enabled[0x10];

	u8         lane0_physical_position[0x8];
	u8         link_width_active[0x8];
	u8         link_speed_active[0x10];

	u8         num_of_pfs[0x10];
	u8         num_of_vfs[0x10];

	u8         bdf0[0x10];
	u8         reserved_at_b0[0x10];

	u8         max_read_request_size[0x4];
	u8         max_payload_size[0x4];
	u8         reserved_at_c8[0x5];
	u8         pwr_status[0x3];
	u8         port_type[0x4];
	u8         reserved_at_d4[0xb];
	u8         lane_reversal[0x1];
};

struct mlx5_ifc_mcqi_cap_bits {
	u8         supported_info_bitmask[0x20];

	u8         component_size[0x20];

	u8         max_component_size[0x20];

	u8         log_mcda_word_size[0x4];
	u8         reserved_at_64[0xc];
	u8         mcda_max_write_size[0x10];

	u8         rd_en[0x1];
	u8         reserved_at_81[0x1];
	u8         match_chip_id[0x1];
	u8         match_psid[0x1];
	u8         check_user_timestamp[0x1];
	u8         match_base_guid_mac[0x1];
	u8         reserved_at_86[0x1a];
};

struct mlx5_ifc_mcqi_reg_bits {
	u8         read_pending_component[0x1];
	u8         reserved_at_1[0xf];
	u8         component_index[0x10];

	u8         reserved_at_20[0x20];

	u8         reserved_at_40[0x1b];
	u8         info_type[0x5];

	u8         info_size[0x20];

	u8         offset[0x20];

	u8         reserved_at_a0[0x10];
	u8         data_size[0x10];

	u8         data[0][0x20];
};

struct mlx5_ifc_mcc_reg_bits {
	u8         reserved_at_0[0x4];
	u8         time_elapsed_since_last_cmd[0xc];
	u8         reserved_at_10[0x8];
	u8         instruction[0x8];

	u8         reserved_at_20[0x10];
	u8         component_index[0x10];

	u8         reserved_at_40[0x8];
	u8         update_handle[0x18];

	u8         handle_owner_type[0x4];
	u8         handle_owner_host_id[0x4];
	u8         reserved_at_68[0x1];
	u8         control_progress[0x7];
	u8         error_code[0x8];
	u8         reserved_at_78[0x4];
	u8         control_state[0x4];

	u8         component_size[0x20];

	u8         reserved_at_a0[0x60];
};

struct mlx5_ifc_mcda_reg_bits {
	u8         reserved_at_0[0x8];
	u8         update_handle[0x18];

	u8         offset[0x20];

	u8         reserved_at_40[0x10];
	u8         size[0x10];

	u8         reserved_at_60[0x20];

	u8         data[0][0x20];
};

union mlx5_ifc_ports_control_registers_document_bits {
	struct mlx5_ifc_ib_portcntrs_attribute_grp_data_bits ib_portcntrs_attribute_grp_data;
	struct mlx5_ifc_bufferx_reg_bits bufferx_reg;
	struct mlx5_ifc_eth_2819_cntrs_grp_data_layout_bits eth_2819_cntrs_grp_data_layout;
	struct mlx5_ifc_eth_2863_cntrs_grp_data_layout_bits eth_2863_cntrs_grp_data_layout;
	struct mlx5_ifc_eth_3635_cntrs_grp_data_layout_bits eth_3635_cntrs_grp_data_layout;
	struct mlx5_ifc_eth_802_3_cntrs_grp_data_layout_bits eth_802_3_cntrs_grp_data_layout;
	struct mlx5_ifc_eth_discard_cntrs_grp_bits eth_discard_cntrs_grp;
	struct mlx5_ifc_eth_extended_cntrs_grp_data_layout_bits eth_extended_cntrs_grp_data_layout;
	struct mlx5_ifc_eth_per_prio_grp_data_layout_bits eth_per_prio_grp_data_layout;
	struct mlx5_ifc_eth_per_traffic_class_cong_layout_bits eth_per_traffic_class_cong_layout;
	struct mlx5_ifc_eth_per_traffic_class_layout_bits eth_per_traffic_class_layout;
	struct mlx5_ifc_lane_2_module_mapping_bits lane_2_module_mapping;
	struct mlx5_ifc_link_level_retrans_cntr_grp_date_bits link_level_retrans_cntr_grp_date;
	struct mlx5_ifc_pamp_reg_bits pamp_reg;
	struct mlx5_ifc_paos_reg_bits paos_reg;
	struct mlx5_ifc_pbmc_reg_bits pbmc_reg;
	struct mlx5_ifc_pcap_reg_bits pcap_reg;
	struct mlx5_ifc_peir_reg_bits peir_reg;
	struct mlx5_ifc_pelc_reg_bits pelc_reg;
	struct mlx5_ifc_pfcc_reg_bits pfcc_reg;
	struct mlx5_ifc_phbr_binding_reg_bits phbr_binding_reg;
	struct mlx5_ifc_phbr_for_port_tclass_reg_bits phbr_for_port_tclass_reg;
	struct mlx5_ifc_phbr_for_prio_reg_bits phbr_for_prio_reg;
	struct mlx5_ifc_phrr_reg_bits phrr_reg;
	struct mlx5_ifc_phys_layer_cntrs_bits phys_layer_cntrs;
	struct mlx5_ifc_pifr_reg_bits pifr_reg;
	struct mlx5_ifc_pipg_reg_bits pipg_reg;
	struct mlx5_ifc_plbf_reg_bits plbf_reg;
	struct mlx5_ifc_plib_reg_bits plib_reg;
	struct mlx5_ifc_pll_status_data_bits pll_status_data;
	struct mlx5_ifc_plpc_reg_bits plpc_reg;
	struct mlx5_ifc_pmaos_reg_bits pmaos_reg;
	struct mlx5_ifc_pmlp_reg_bits pmlp_reg;
	struct mlx5_ifc_pmlpn_reg_bits pmlpn_reg;
	struct mlx5_ifc_pmpc_reg_bits pmpc_reg;
	struct mlx5_ifc_pmpe_reg_bits pmpe_reg;
	struct mlx5_ifc_pmpr_reg_bits pmpr_reg;
	struct mlx5_ifc_pmtu_reg_bits pmtu_reg;
	struct mlx5_ifc_ppad_reg_bits ppad_reg;
	struct mlx5_ifc_ppcnt_reg_bits ppcnt_reg;
	struct mlx5_ifc_ppll_reg_bits ppll_reg;
	struct mlx5_ifc_pplm_reg_bits pplm_reg;
	struct mlx5_ifc_pplr_reg_bits pplr_reg;
	struct mlx5_ifc_ppsc_reg_bits ppsc_reg;
	struct mlx5_ifc_pspa_reg_bits pspa_reg;
	struct mlx5_ifc_ptas_reg_bits ptas_reg;
	struct mlx5_ifc_ptys_reg_bits ptys_reg;
	struct mlx5_ifc_pude_reg_bits pude_reg;
	struct mlx5_ifc_pvlc_reg_bits pvlc_reg;
	struct mlx5_ifc_slrg_reg_bits slrg_reg;
	struct mlx5_ifc_slrp_reg_bits slrp_reg;
	struct mlx5_ifc_sltp_reg_bits sltp_reg;
	u8         reserved_0[0x7880];
};

union mlx5_ifc_debug_enhancements_document_bits {
	struct mlx5_ifc_health_buffer_bits health_buffer;
	u8         reserved_0[0x200];
};

union mlx5_ifc_no_dram_nic_document_bits {
	struct mlx5_ifc_nodnic_config_reg_bits nodnic_config_reg;
	struct mlx5_ifc_nodnic_cq_arming_word_bits nodnic_cq_arming_word;
	struct mlx5_ifc_nodnic_event_word_bits nodnic_event_word;
	struct mlx5_ifc_nodnic_gid_filters_bits nodnic_gid_filters;
	struct mlx5_ifc_nodnic_mac_filters_bits nodnic_mac_filters;
	struct mlx5_ifc_nodnic_port_config_reg_bits nodnic_port_config_reg;
	struct mlx5_ifc_nodnic_ring_config_reg_bits nodnic_ring_config_reg;
	struct mlx5_ifc_nodnic_ring_doorbell_bits nodnic_ring_doorbell;
	u8         reserved_0[0x3160];
};

union mlx5_ifc_uplink_pci_interface_document_bits {
	struct mlx5_ifc_initial_seg_bits initial_seg;
	struct mlx5_ifc_vendor_specific_cap_bits vendor_specific_cap;
	u8         reserved_0[0x20120];
};

struct mlx5_ifc_qpdpm_dscp_reg_bits {
	u8         e[0x1];
	u8         reserved_at_01[0x0b];
	u8         prio[0x04];
};

struct mlx5_ifc_qpdpm_reg_bits {
	u8                                     reserved_at_0[0x8];
	u8                                     local_port[0x8];
	u8                                     reserved_at_10[0x10];
	struct mlx5_ifc_qpdpm_dscp_reg_bits    dscp[64];
};

struct mlx5_ifc_qpts_reg_bits {
	u8         reserved_at_0[0x8];
	u8         local_port[0x8];
	u8         reserved_at_10[0x2d];
	u8         trust_state[0x3];
};

struct mlx5_ifc_mfrl_reg_bits {
	u8         reserved_at_0[0x38];
	u8         reset_level[0x8];
};

enum {
      MLX5_ACCESS_REG_SUMMARY_CTRL_ID_MTCAP	= 0x9009,
      MLX5_ACCESS_REG_SUMMARY_CTRL_ID_MTECR	= 0x9109,
      MLX5_ACCESS_REG_SUMMARY_CTRL_ID_MTMP	= 0x900a,
      MLX5_ACCESS_REG_SUMMARY_CTRL_ID_MTWE	= 0x900b,
      MLX5_ACCESS_REG_SUMMARY_CTRL_ID_MTBR	= 0x900f,
      MLX5_ACCESS_REG_SUMMARY_CTRL_ID_MTEWE	= 0x910b,
      MLX5_MAX_TEMPERATURE = 16,
};

struct mlx5_ifc_mtbr_temp_record_bits {
	u8         max_temperature[0x10];
	u8         temperature[0x10];
};

struct mlx5_ifc_mtbr_reg_bits {
	u8         reserved_at_0[0x14];
	u8         base_sensor_index[0xc];

	u8         reserved_at_20[0x18];
	u8         num_rec[0x8];

	u8         reserved_at_40[0x40];

	struct mlx5_ifc_mtbr_temp_record_bits temperature_record[MLX5_MAX_TEMPERATURE];
};

struct mlx5_ifc_mtbr_reg_ext_bits {
	u8         reserved_at_0[0x14];
	u8         base_sensor_index[0xc];

	u8         reserved_at_20[0x18];
	u8         num_rec[0x8];

	u8         reserved_at_40[0x40];

    struct mlx5_ifc_mtbr_temp_record_bits temperature_record[MLX5_MAX_TEMPERATURE];
};

struct mlx5_ifc_mtcap_bits {
	u8         reserved_at_0[0x19];
	u8         sensor_count[0x7];

	u8         reserved_at_20[0x19];
	u8         internal_sensor_count[0x7];

	u8         sensor_map[0x40];
};

struct mlx5_ifc_mtcap_ext_bits {
	u8         reserved_at_0[0x19];
	u8         sensor_count[0x7];

	u8         reserved_at_20[0x20];

	u8         sensor_map[0x40];
};

struct mlx5_ifc_mtecr_bits {
	u8         reserved_at_0[0x4];
	u8         last_sensor[0xc];
	u8         reserved_at_10[0x4];
	u8         sensor_count[0xc];

	u8         reserved_at_20[0x19];
	u8         internal_sensor_count[0x7];

	u8         sensor_map_0[0x20];

	u8         reserved_at_60[0x2a0];
};

struct mlx5_ifc_mtecr_ext_bits {
	u8         reserved_at_0[0x4];
	u8         last_sensor[0xc];
	u8         reserved_at_10[0x4];
	u8         sensor_count[0xc];

	u8         reserved_at_20[0x20];

	u8         sensor_map_0[0x20];

	u8         reserved_at_60[0x2a0];
};

struct mlx5_ifc_mtewe_bits {
	u8         reserved_at_0[0x4];
	u8         last_sensor[0xc];
	u8         reserved_at_10[0x4];
	u8         sensor_count[0xc];

	u8         sensor_warning_0[0x20];

	u8         reserved_at_40[0x2a0];
};

struct mlx5_ifc_mtewe_ext_bits {
	u8         reserved_at_0[0x4];
	u8         last_sensor[0xc];
	u8         reserved_at_10[0x4];
	u8         sensor_count[0xc];

	u8         sensor_warning_0[0x20];

	u8         reserved_at_40[0x2a0];
};

struct mlx5_ifc_mtmp_bits {
	u8         reserved_at_0[0x14];
	u8         sensor_index[0xc];

	u8         reserved_at_20[0x10];
	u8         temperature[0x10];

	u8         mte[0x1];
	u8         mtr[0x1];
	u8         reserved_at_42[0xe];
	u8         max_temperature[0x10];

	u8         tee[0x2];
	u8         reserved_at_62[0xe];
	u8         temperature_threshold_hi[0x10];

	u8         reserved_at_80[0x10];
	u8         temperature_threshold_lo[0x10];

	u8         reserved_at_a0[0x20];

	u8         sensor_name_hi[0x20];

	u8         sensor_name_lo[0x20];
};

struct mlx5_ifc_mtmp_ext_bits {
	u8         reserved_at_0[0x14];
	u8         sensor_index[0xc];

	u8         reserved_at_20[0x10];
	u8         temperature[0x10];

	u8         mte[0x1];
	u8         mtr[0x1];
	u8         reserved_at_42[0xe];
	u8         max_temperature[0x10];

	u8         tee[0x2];
	u8         reserved_at_62[0xe];
	u8         temperature_threshold_hi[0x10];

	u8         reserved_at_80[0x10];
	u8         temperature_threshold_lo[0x10];

	u8         reserved_at_a0[0x20];

	u8         sensor_name_hi[0x20];

	u8         sensor_name_lo[0x20];
};

struct mlx5_ifc_general_obj_in_cmd_hdr_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         vhca_tunnel_id[0x10];
	u8         obj_type[0x10];

	u8         obj_id[0x20];

	u8         reserved_at_60[0x20];
};

struct mlx5_ifc_general_obj_out_cmd_hdr_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         obj_id[0x20];

	u8         reserved_at_60[0x20];
};

struct mlx5_ifc_umem_bits {
	u8         reserved_at_0[0x80];

	u8         reserved_at_80[0x1b];
	u8         log_page_size[0x5];

	u8         page_offset[0x20];

	u8         num_of_mtt[0x40];

	struct mlx5_ifc_mtt_bits  mtt[0];
};

struct mlx5_ifc_uctx_bits {
	u8         cap[0x20];

	u8         reserved_at_20[0x160];
};

struct mlx5_ifc_create_umem_in_bits {
	u8         opcode[0x10];
	u8         uid[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x40];

	struct mlx5_ifc_umem_bits  umem;
};

struct mlx5_ifc_create_uctx_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x40];

	struct mlx5_ifc_uctx_bits  uctx;
};

struct mlx5_ifc_destroy_uctx_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x10];
	u8         uid[0x10];

	u8         reserved_at_60[0x20];
};

struct mlx5_ifc_mtrc_string_db_param_bits {
	u8         string_db_base_address[0x20];

	u8         reserved_at_20[0x8];
	u8         string_db_size[0x18];
};

struct mlx5_ifc_mtrc_cap_bits {
	u8         trace_owner[0x1];
	u8         trace_to_memory[0x1];
	u8         reserved_at_2[0x4];
	u8         trc_ver[0x2];
	u8         reserved_at_8[0x14];
	u8         num_string_db[0x4];

	u8         first_string_trace[0x8];
	u8         num_string_trace[0x8];
	u8         reserved_at_30[0x28];

	u8         log_max_trace_buffer_size[0x8];

	u8         reserved_at_60[0x20];

	struct mlx5_ifc_mtrc_string_db_param_bits string_db_param[8];

	u8         reserved_at_280[0x180];
};

struct mlx5_ifc_mtrc_conf_bits {
	u8         reserved_at_0[0x1c];
	u8         trace_mode[0x4];
	u8         reserved_at_20[0x18];
	u8         log_trace_buffer_size[0x8];
	u8         trace_mkey[0x20];
	u8         reserved_at_60[0x3a0];
};

struct mlx5_ifc_mtrc_stdb_bits {
	u8         string_db_index[0x4];
	u8         reserved_at_4[0x4];
	u8         read_size[0x18];
	u8         start_offset[0x20];
	u8         string_db_data[0];
};

struct mlx5_ifc_mtrc_ctrl_bits {
	u8         trace_status[0x2];
	u8         reserved_at_2[0x2];
	u8         arm_event[0x1];
	u8         reserved_at_5[0xb];
	u8         modify_field_select[0x10];
	u8         reserved_at_20[0x2b];
	u8         current_timestamp52_32[0x15];
	u8         current_timestamp31_0[0x20];
	u8         reserved_at_80[0x180];
};

struct mlx5_ifc_affiliated_event_header_bits {
	u8         reserved_at_0[0x10];
	u8         obj_type[0x10];

	u8         obj_id[0x20];
};

#define MLX5_FC_BULK_SIZE_FACTOR 128

enum mlx5_fc_bulk_alloc_bitmask {
	MLX5_FC_BULK_128   = (1 << 0),
	MLX5_FC_BULK_256   = (1 << 1),
	MLX5_FC_BULK_512   = (1 << 2),
	MLX5_FC_BULK_1024  = (1 << 3),
	MLX5_FC_BULK_2048  = (1 << 4),
	MLX5_FC_BULK_4096  = (1 << 5),
	MLX5_FC_BULK_8192  = (1 << 6),
	MLX5_FC_BULK_16384 = (1 << 7),
};

#define MLX5_FC_BULK_NUM_FCS(fc_enum) (MLX5_FC_BULK_SIZE_FACTOR * (fc_enum))

struct mlx5_ifc_ipsec_cap_bits {
	u8         ipsec_full_offload[0x1];
	u8         ipsec_crypto_offload[0x1];
	u8         ipsec_esn[0x1];
	u8         ipsec_crypto_esp_aes_gcm_256_encrypt[0x1];
	u8         ipsec_crypto_esp_aes_gcm_128_encrypt[0x1];
	u8         ipsec_crypto_esp_aes_gcm_256_decrypt[0x1];
	u8         ipsec_crypto_esp_aes_gcm_128_decrypt[0x1];
	u8         reserved_at_7[0x4];
	u8         log_max_ipsec_offload[0x5];
	u8         reserved_at_10[0x10];

	u8         min_log_ipsec_full_replay_window[0x8];
	u8         max_log_ipsec_full_replay_window[0x8];
	u8         reserved_at_30[0x7d0];
};

enum {
	MLX5_IPSEC_OBJECT_ICV_LEN_16B,
};

enum {
	MLX5_IPSEC_ASO_REG_C_0_1 = 0x0,
	MLX5_IPSEC_ASO_REG_C_2_3 = 0x1,
	MLX5_IPSEC_ASO_REG_C_4_5 = 0x2,
	MLX5_IPSEC_ASO_REG_C_6_7 = 0x3,
};

enum {
	MLX5_IPSEC_ASO_MODE              = 0x0,
	MLX5_IPSEC_ASO_REPLAY_PROTECTION = 0x1,
	MLX5_IPSEC_ASO_INC_SN            = 0x2,
};

enum {
	MLX5_IPSEC_ASO_REPLAY_WIN_32BIT  = 0x0,
	MLX5_IPSEC_ASO_REPLAY_WIN_64BIT  = 0x1,
	MLX5_IPSEC_ASO_REPLAY_WIN_128BIT = 0x2,
	MLX5_IPSEC_ASO_REPLAY_WIN_256BIT = 0x3,
};

struct mlx5_ifc_ipsec_aso_bits {
	u8         valid[0x1];
	u8         reserved_at_201[0x1];
	u8         mode[0x2];
	u8         window_sz[0x2];
	u8         soft_lft_arm[0x1];
	u8         hard_lft_arm[0x1];
	u8         remove_flow_enable[0x1];
	u8         esn_event_arm[0x1];
	u8         reserved_at_20a[0x16];

	u8         remove_flow_pkt_cnt[0x20];

	u8         remove_flow_soft_lft[0x20];

	u8         reserved_at_260[0x80];

	u8         mode_parameter[0x20];

	u8         replay_protection_window[0x100];
};

struct mlx5_ifc_ipsec_obj_bits {
	u8         modify_field_select[0x40];
	u8         full_offload[0x1];
	u8         reserved_at_41[0x1];
	u8         esn_en[0x1];
	u8         esn_overlap[0x1];
	u8         reserved_at_44[0x2];
	u8         icv_length[0x2];
	u8         reserved_at_48[0x4];
	u8         aso_return_reg[0x4];
	u8         reserved_at_50[0x10];

	u8         esn_msb[0x20];

	u8         reserved_at_80[0x8];
	u8         dekn[0x18];

	u8         salt[0x20];

	u8         implicit_iv[0x40];

	u8         reserved_at_100[0x8];
	u8         ipsec_aso_access_pd[0x18];
	u8         reserved_at_120[0xe0];

	struct mlx5_ifc_ipsec_aso_bits ipsec_aso;
};

struct mlx5_ifc_create_ipsec_obj_in_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits general_obj_in_cmd_hdr;
	struct mlx5_ifc_ipsec_obj_bits ipsec_object;
};

enum {
	MLX5_MODIFY_IPSEC_BITMASK_ESN_OVERLAP = 1 << 0,
	MLX5_MODIFY_IPSEC_BITMASK_ESN_MSB = 1 << 1,
};

struct mlx5_ifc_query_ipsec_obj_out_bits {
	struct mlx5_ifc_general_obj_out_cmd_hdr_bits general_obj_out_cmd_hdr;
	struct mlx5_ifc_ipsec_obj_bits ipsec_object;
};

struct mlx5_ifc_modify_ipsec_obj_in_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits general_obj_in_cmd_hdr;
	struct mlx5_ifc_ipsec_obj_bits ipsec_object;
};

enum {
	MLX5_GENERAL_OBJECT_TYPE_ENCRYPTION_KEY_PURPOSE_TLS = 0x1,
	MLX5_GENERAL_OBJECT_TYPE_ENCRYPTION_KEY_PURPOSE_IPSEC = 0x2,
	MLX5_GENERAL_OBJECT_TYPE_ENCRYPTION_KEY_PURPOSE_MACSEC = 0x4,
};
#endif /* MLX5_IFC_H */
