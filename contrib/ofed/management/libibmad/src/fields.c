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

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <mad.h>
#include <infiniband/common.h>

/*
 * BITSOFFS and BE_OFFS are required due the fact that the bit offsets are inconsistently
 * encoded in the IB spec - IB headers are encoded such that the bit offsets
 * are in big endian convention (BE_OFFS), while the SMI/GSI queries data fields bit
 * offsets are specified using real bit offset (?!)
 * The following macros normalize everything to big endian offsets.
 */
#define BITSOFFS(o, w)	(((o) & ~31) | ((32 - ((o) & 31) - (w)))), (w)
#define BE_OFFS(o, w)	(o), (w)
#define BE_TO_BITSOFFS(o, w)	(((o) & ~31) | ((32 - ((o) & 31) - (w))))

ib_field_t ib_mad_f [] = {
	[0]	{0, 0},		/* IB_NO_FIELD - reserved as invalid */

	[IB_GID_PREFIX_F]		{0, 64, "GidPrefix", mad_dump_rhex},
	[IB_GID_GUID_F]			{64, 64, "GidGuid", mad_dump_rhex},

	/*
	 * MAD: common MAD fields (IB spec 13.4.2)
	 * SMP: Subnet Management packets - lid routed (IB spec 14.2.1.1)
	 * DSMP: Subnet Management packets - direct route (IB spec 14.2.1.2)
	 * SA: Subnet Administration packets (IB spec 15.2.1.1)
	 */

	/* first MAD word (0-3 bytes) */
	[IB_MAD_METHOD_F]		{BE_OFFS(0, 7), "MadMethod", mad_dump_hex}, /* TODO: add dumper */
	[IB_MAD_RESPONSE_F] 		{BE_OFFS(7, 1), "MadIsResponse", mad_dump_uint}, /* TODO: add dumper */
	[IB_MAD_CLASSVER_F] 		{BE_OFFS(8, 8), "MadClassVersion", mad_dump_uint},
	[IB_MAD_MGMTCLASS_F] 		{BE_OFFS(16, 8), "MadMgmtClass", mad_dump_uint},  /* TODO: add dumper */
	[IB_MAD_BASEVER_F] 		{BE_OFFS(24, 8), "MadBaseVersion", mad_dump_uint},

	/* second MAD word (4-7 bytes) */
	[IB_MAD_STATUS_F] 		{BE_OFFS(48, 16), "MadStatus", mad_dump_hex}, /* TODO: add dumper */

	/* DR SMP only */
	[IB_DRSMP_HOPCNT_F] 		{BE_OFFS(32, 8), "DrSmpHopCnt", mad_dump_uint},
	[IB_DRSMP_HOPPTR_F] 		{BE_OFFS(40, 8), "DrSmpHopPtr", mad_dump_uint},
	[IB_DRSMP_STATUS_F] 		{BE_OFFS(48, 15), "DrSmpStatus", mad_dump_hex}, /* TODO: add dumper */
	[IB_DRSMP_DIRECTION_F] 		{BE_OFFS(63, 1), "DrSmpDirection", mad_dump_uint}, /* TODO: add dumper */

	/* words 3,4,5,6 (8-23 bytes) */
	[IB_MAD_TRID_F] 		{64, 64, "MadTRID", mad_dump_hex},
	[IB_MAD_ATTRID_F] 		{BE_OFFS(144, 16), "MadAttr", mad_dump_hex}, /* TODO: add dumper */
	[IB_MAD_ATTRMOD_F] 		{160, 32, "MadModifier", mad_dump_hex}, /* TODO: add dumper */

	/* word 7,8 (24-31 bytes) */
	[IB_MAD_MKEY_F] 		{196, 64, "MadMkey", mad_dump_hex},

	/* word 9 (32-37 bytes) */
	[IB_DRSMP_DRDLID_F] 		{BE_OFFS(256, 16), "DrSmpDLID", mad_dump_hex},
	[IB_DRSMP_DRSLID_F] 		{BE_OFFS(272, 16), "DrSmpSLID", mad_dump_hex},

	/* word 12 (44-47 bytes) */
	[IB_SA_ATTROFFS_F] 		{BE_OFFS(46*8, 16), "SaAttrOffs", mad_dump_uint},

	/* word 13,14 (48-55 bytes) */
	[IB_SA_COMPMASK_F] 		{48*8, 64, "SaCompMask", mad_dump_hex},

	/* word 13,14 (56-255 bytes) */
	[IB_SA_DATA_F] 			{56*8, (256-56)*8, "SaData", mad_dump_hex},

	[IB_DRSMP_PATH_F] 		{1024, 512, "DrSmpPath", mad_dump_hex},
	[IB_DRSMP_RPATH_F] 		{1536, 512, "DrSmpRetPath", mad_dump_hex},

	[IB_GS_DATA_F] 			{64*8, (256-64) * 8, "GsData", mad_dump_hex},

	/*
	 * PortInfo fields:
	 */
	[IB_PORT_MKEY_F]		{0, 64, "Mkey", mad_dump_hex},
	[IB_PORT_GID_PREFIX_F]		{64, 64, "GidPrefix", mad_dump_hex},
	[IB_PORT_LID_F]			{BITSOFFS(128, 16), "Lid", mad_dump_hex},
	[IB_PORT_SMLID_F]		{BITSOFFS(144, 16), "SMLid", mad_dump_hex},
	[IB_PORT_CAPMASK_F]		{160, 32, "CapMask", mad_dump_portcapmask},
	[IB_PORT_DIAG_F]		{BITSOFFS(192, 16), "DiagCode", mad_dump_hex},
	[IB_PORT_MKEY_LEASE_F]		{BITSOFFS(208, 16), "MkeyLeasePeriod", mad_dump_uint},
	[IB_PORT_LOCAL_PORT_F]		{BITSOFFS(224, 8), "LocalPort", mad_dump_uint},
	[IB_PORT_LINK_WIDTH_ENABLED_F]	{BITSOFFS(232, 8), "LinkWidthEnabled", mad_dump_linkwidthen},
	[IB_PORT_LINK_WIDTH_SUPPORTED_F]	{BITSOFFS(240, 8), "LinkWidthSupported", mad_dump_linkwidthsup},
	[IB_PORT_LINK_WIDTH_ACTIVE_F]	{BITSOFFS(248, 8), "LinkWidthActive", mad_dump_linkwidth},
	[IB_PORT_LINK_SPEED_SUPPORTED_F]	{BITSOFFS(256, 4), "LinkSpeedSupported", mad_dump_linkspeedsup},
	[IB_PORT_STATE_F]		{BITSOFFS(260, 4), "LinkState", mad_dump_portstate},
	[IB_PORT_PHYS_STATE_F]		{BITSOFFS(264, 4), "PhysLinkState", mad_dump_physportstate},
	[IB_PORT_LINK_DOWN_DEF_F]	{BITSOFFS(268, 4), "LinkDownDefState", mad_dump_linkdowndefstate},
	[IB_PORT_MKEY_PROT_BITS_F]	{BITSOFFS(272, 2), "ProtectBits", mad_dump_uint},
	[IB_PORT_LMC_F]			{BITSOFFS(277, 3), "LMC", mad_dump_uint},
	[IB_PORT_LINK_SPEED_ACTIVE_F]	{BITSOFFS(280, 4), "LinkSpeedActive", mad_dump_linkspeed},
	[IB_PORT_LINK_SPEED_ENABLED_F]	{BITSOFFS(284, 4), "LinkSpeedEnabled", mad_dump_linkspeeden},
	[IB_PORT_NEIGHBOR_MTU_F]	{BITSOFFS(288, 4), "NeighborMTU", mad_dump_mtu},
	[IB_PORT_SMSL_F]		{BITSOFFS(292, 4), "SMSL", mad_dump_uint},
	[IB_PORT_VL_CAP_F]		{BITSOFFS(296, 4), "VLCap", mad_dump_vlcap},
	[IB_PORT_INIT_TYPE_F]		{BITSOFFS(300, 4), "InitType", mad_dump_hex},
	[IB_PORT_VL_HIGH_LIMIT_F] 	{BITSOFFS(304, 8), "VLHighLimit", mad_dump_uint},
	[IB_PORT_VL_ARBITRATION_HIGH_CAP_F]	{BITSOFFS(312, 8), "VLArbHighCap", mad_dump_uint},
	[IB_PORT_VL_ARBITRATION_LOW_CAP_F]	{BITSOFFS(320, 8), "VLArbLowCap", mad_dump_uint},

	[IB_PORT_INIT_TYPE_REPLY_F]	{BITSOFFS(328, 4), "InitReply", mad_dump_hex},
	[IB_PORT_MTU_CAP_F]		{BITSOFFS(332, 4), "MtuCap", mad_dump_mtu},
	[IB_PORT_VL_STALL_COUNT_F]	{BITSOFFS(336, 3), "VLStallCount", mad_dump_uint},
	[IB_PORT_HOQ_LIFE_F]		{BITSOFFS(339, 5), "HoqLife", mad_dump_uint},
	[IB_PORT_OPER_VLS_F]		{BITSOFFS(344, 4), "OperVLs", mad_dump_opervls},
	[IB_PORT_PART_EN_INB_F]		{BITSOFFS(348, 1), "PartEnforceInb", mad_dump_uint},
	[IB_PORT_PART_EN_OUTB_F]	{BITSOFFS(349, 1), "PartEnforceOutb", mad_dump_uint},
	[IB_PORT_FILTER_RAW_INB_F]	{BITSOFFS(350, 1), "FilterRawInb", mad_dump_uint},
	[IB_PORT_FILTER_RAW_OUTB_F]	{BITSOFFS(351, 1), "FilterRawOutb", mad_dump_uint},
	[IB_PORT_MKEY_VIOL_F]		{BITSOFFS(352, 16), "MkeyViolations", mad_dump_uint},
	[IB_PORT_PKEY_VIOL_F]		{BITSOFFS(368, 16), "PkeyViolations", mad_dump_uint},
	[IB_PORT_QKEY_VIOL_F]		{BITSOFFS(384, 16), "QkeyViolations", mad_dump_uint},
	[IB_PORT_GUID_CAP_F]		{BITSOFFS(400, 8), "GuidCap", mad_dump_uint},
	[IB_PORT_CLIENT_REREG_F]	{BITSOFFS(408, 1), "ClientReregister", mad_dump_uint},
	[IB_PORT_SUBN_TIMEOUT_F]	{BITSOFFS(411, 5), "SubnetTimeout", mad_dump_uint},
	[IB_PORT_RESP_TIME_VAL_F]	{BITSOFFS(419, 5), "RespTimeVal", mad_dump_uint},
	[IB_PORT_LOCAL_PHYS_ERR_F]	{BITSOFFS(424, 4), "LocalPhysErr", mad_dump_uint},
	[IB_PORT_OVERRUN_ERR_F]		{BITSOFFS(428, 4), "OverrunErr", mad_dump_uint},
	[IB_PORT_MAX_CREDIT_HINT_F]	{BITSOFFS(432, 16), "MaxCreditHint", mad_dump_uint},
	[IB_PORT_LINK_ROUND_TRIP_F]	{BITSOFFS(456, 24), "RoundTrip", mad_dump_uint},

	/*
	 * NodeInfo fields:
	 */
	[IB_NODE_BASE_VERS_F]		{BITSOFFS(0,8), "BaseVers", mad_dump_uint},
	[IB_NODE_CLASS_VERS_F]		{BITSOFFS(8,8), "ClassVers", mad_dump_uint},
	[IB_NODE_TYPE_F]		{BITSOFFS(16,8), "NodeType", mad_dump_node_type},
	[IB_NODE_NPORTS_F]		{BITSOFFS(24,8), "NumPorts", mad_dump_uint},
	[IB_NODE_SYSTEM_GUID_F]		{32, 64, "SystemGuid", mad_dump_hex},
	[IB_NODE_GUID_F]		{96, 64, "Guid", mad_dump_hex},
	[IB_NODE_PORT_GUID_F]		{160, 64, "PortGuid", mad_dump_hex},
	[IB_NODE_PARTITION_CAP_F]	{BITSOFFS(224,16), "PartCap", mad_dump_uint},
	[IB_NODE_DEVID_F]		{BITSOFFS(240,16), "DevId", mad_dump_hex},
	[IB_NODE_REVISION_F]		{256, 32, "Revision", mad_dump_hex},
	[IB_NODE_LOCAL_PORT_F]		{BITSOFFS(288,8), "LocalPort", mad_dump_uint},
	[IB_NODE_VENDORID_F]		{BITSOFFS(296,24), "VendorId", mad_dump_hex},

	/*
	 * SwitchInfo fields:
	 */
	[IB_SW_LINEAR_FDB_CAP_F]	{BITSOFFS(0, 16), "LinearFdbCap", mad_dump_uint},
	[IB_SW_RANDOM_FDB_CAP_F]	{BITSOFFS(16, 16), "RandomFdbCap", mad_dump_uint},
	[IB_SW_MCAST_FDB_CAP_F]		{BITSOFFS(32, 16), "McastFdbCap", mad_dump_uint},
	[IB_SW_LINEAR_FDB_TOP_F]	{BITSOFFS(48, 16), "LinearFdbTop", mad_dump_uint},
	[IB_SW_DEF_PORT_F]		{BITSOFFS(64, 8), "DefPort", mad_dump_uint},
	[IB_SW_DEF_MCAST_PRIM_F]	{BITSOFFS(72, 8), "DefMcastPrimPort", mad_dump_uint},
	[IB_SW_DEF_MCAST_NOT_PRIM_F]	{BITSOFFS(80, 8), "DefMcastNotPrimPort", mad_dump_uint},
	[IB_SW_LIFE_TIME_F]		{BITSOFFS(88, 5), "LifeTime", mad_dump_uint},
	[IB_SW_STATE_CHANGE_F]		{BITSOFFS(93, 1), "StateChange", mad_dump_uint},
	[IB_SW_LIDS_PER_PORT_F]		{BITSOFFS(96,16), "LidsPerPort", mad_dump_uint},
	[IB_SW_PARTITION_ENFORCE_CAP_F]	{BITSOFFS(112, 16), "PartEnforceCap", mad_dump_uint},
	[IB_SW_PARTITION_ENF_INB_F]	{BITSOFFS(128, 1), "InboundPartEnf", mad_dump_uint},
	[IB_SW_PARTITION_ENF_OUTB_F]	{BITSOFFS(129, 1), "OutboundPartEnf", mad_dump_uint},
	[IB_SW_FILTER_RAW_INB_F]	{BITSOFFS(130, 1), "FilterRawInbound", mad_dump_uint},
	[IB_SW_FILTER_RAW_OUTB_F]	{BITSOFFS(131, 1), "FilterRawOutbound", mad_dump_uint},
	[IB_SW_ENHANCED_PORT0_F]	{BITSOFFS(132, 1), "EnhancedPort0", mad_dump_uint},

	/*
	 * SwitchLinearForwardingTable fields:
	 */
	[IB_LINEAR_FORW_TBL_F]		{0, 512, "LinearForwTbl", mad_dump_array},

	/*
	 * SwitchMulticastForwardingTable fields:
	 */
	[IB_MULTICAST_FORW_TBL_F]	{0, 512, "MulticastForwTbl", mad_dump_array},

	/*
	 * Notice/Trap fields
	 */
	[IB_NOTICE_IS_GENERIC_F]  	{BITSOFFS(0, 1), "NoticeIsGeneric", mad_dump_uint},
	[IB_NOTICE_TYPE_F]        	{BITSOFFS(1, 7), "NoticeType", mad_dump_uint},
	[IB_NOTICE_PRODUCER_F]    	{BITSOFFS(8, 24), "NoticeProducerType", mad_dump_node_type},
	[IB_NOTICE_TRAP_NUMBER_F] 	{BITSOFFS(32, 16), "NoticeTrapNumber", mad_dump_uint},
	[IB_NOTICE_ISSUER_LID_F]  	{BITSOFFS(48, 16), "NoticeIssuerLID", mad_dump_uint},
	[IB_NOTICE_TOGGLE_F]      	{BITSOFFS(64, 1), "NoticeToggle", mad_dump_uint},
	[IB_NOTICE_COUNT_F]       	{BITSOFFS(65, 15), "NoticeCount", mad_dump_uint},
	[IB_NOTICE_DATA_DETAILS_F]    	{80, 432, "NoticeDataDetails", mad_dump_array},
	[IB_NOTICE_DATA_LID_F]    	{BITSOFFS(80, 16), "NoticeDataLID", mad_dump_uint},
	[IB_NOTICE_DATA_144_LID_F]    	{BITSOFFS(96, 16), "NoticeDataTrap144LID", mad_dump_uint},
	[IB_NOTICE_DATA_144_CAPMASK_F]  {BITSOFFS(128, 32), "NoticeDataTrap144CapMask", mad_dump_uint},

	/*
	 * NodeDescription fields:
	 */
	[IB_NODE_DESC_F]		{0, 64*8, "NodeDesc", mad_dump_string},

	/*
	 * Port counters
	 */
	[IB_PC_PORT_SELECT_F] 		{BITSOFFS(8, 8), "PortSelect", mad_dump_uint},
	[IB_PC_COUNTER_SELECT_F] 	{BITSOFFS(16, 16), "CounterSelect", mad_dump_hex},
	[IB_PC_ERR_SYM_F] 		{BITSOFFS(32, 16), "SymbolErrors", mad_dump_uint},
	[IB_PC_LINK_RECOVERS_F] 	{BITSOFFS(48, 8), "LinkRecovers", mad_dump_uint},
	[IB_PC_LINK_DOWNED_F] 		{BITSOFFS(56, 8), "LinkDowned", mad_dump_uint},
	[IB_PC_ERR_RCV_F] 		{BITSOFFS(64, 16), "RcvErrors", mad_dump_uint},
	[IB_PC_ERR_PHYSRCV_F] 		{BITSOFFS(80, 16), "RcvRemotePhysErrors", mad_dump_uint},
	[IB_PC_ERR_SWITCH_REL_F]	{BITSOFFS(96, 16), "RcvSwRelayErrors", mad_dump_uint},
	[IB_PC_XMT_DISCARDS_F] 		{BITSOFFS(112, 16), "XmtDiscards", mad_dump_uint},
	[IB_PC_ERR_XMTCONSTR_F] 	{BITSOFFS(128, 8), "XmtConstraintErrors", mad_dump_uint},
	[IB_PC_ERR_RCVCONSTR_F] 	{BITSOFFS(136, 8), "RcvConstraintErrors", mad_dump_uint},
	[IB_PC_ERR_LOCALINTEG_F] 	{BITSOFFS(152, 4), "LinkIntegrityErrors", mad_dump_uint},
	[IB_PC_ERR_EXCESS_OVR_F] 	{BITSOFFS(156, 4), "ExcBufOverrunErrors", mad_dump_uint},
	[IB_PC_VL15_DROPPED_F] 		{BITSOFFS(176, 16), "VL15Dropped", mad_dump_uint},
	[IB_PC_XMT_BYTES_F] 		{192, 32, "XmtData", mad_dump_uint},
	[IB_PC_RCV_BYTES_F] 		{224, 32, "RcvData", mad_dump_uint},
	[IB_PC_XMT_PKTS_F] 		{256, 32, "XmtPkts", mad_dump_uint},
	[IB_PC_RCV_PKTS_F] 		{288, 32, "RcvPkts", mad_dump_uint},

	/*
	 * SMInfo
	 */
	[IB_SMINFO_GUID_F]		{0, 64, "SmInfoGuid", mad_dump_hex},
	[IB_SMINFO_KEY_F]		{64, 64, "SmInfoKey", mad_dump_hex},
	[IB_SMINFO_ACT_F]		{128, 32, "SmActivity", mad_dump_uint},
	[IB_SMINFO_PRIO_F]		{BITSOFFS(160, 4), "SmPriority", mad_dump_uint},
	[IB_SMINFO_STATE_F]		{BITSOFFS(164, 4), "SmState", mad_dump_uint},

	/*
	 * SA RMPP
	 */
	[IB_SA_RMPP_VERS_F]		{BE_OFFS(24*8+24, 8), "RmppVers", mad_dump_uint},
	[IB_SA_RMPP_TYPE_F]		{BE_OFFS(24*8+16, 8), "RmppType", mad_dump_uint},
	[IB_SA_RMPP_RESP_F]		{BE_OFFS(24*8+11, 5), "RmppResp", mad_dump_uint},
	[IB_SA_RMPP_FLAGS_F]		{BE_OFFS(24*8+8, 3), "RmppFlags", mad_dump_hex},
	[IB_SA_RMPP_STATUS_F]		{BE_OFFS(24*8+0, 8), "RmppStatus", mad_dump_hex},

	/* data1 */
	[IB_SA_RMPP_D1_F]		{28*8, 32, "RmppData1", mad_dump_hex},
	[IB_SA_RMPP_SEGNUM_F]		{28*8, 32, "RmppSegNum", mad_dump_uint},
	/* data2 */
	[IB_SA_RMPP_D2_F]		{32*8, 32, "RmppData2", mad_dump_hex},
	[IB_SA_RMPP_LEN_F]		{32*8, 32, "RmppPayload", mad_dump_uint},
	[IB_SA_RMPP_NEWWIN_F]		{32*8, 32, "RmppNewWin", mad_dump_uint},

	/*
	 * SA Path rec
	 */
	[IB_SA_PR_DGID_F]		{64, 128, "PathRecDGid", mad_dump_array},
	[IB_SA_PR_SGID_F]		{192, 128, "PathRecSGid", mad_dump_array},
	[IB_SA_PR_DLID_F]		{BITSOFFS(320,16), "PathRecDLid", mad_dump_hex},
	[IB_SA_PR_SLID_F]		{BITSOFFS(336,16), "PathRecSLid", mad_dump_hex},
	[IB_SA_PR_NPATH_F]		{BITSOFFS(393,7), "PathRecNumPath", mad_dump_uint},

	/*
	 * SA Get Multi Path
	 */
	[IB_SA_MP_NPATH_F]		{BITSOFFS(41,7), "MultiPathNumPath", mad_dump_uint},
	[IB_SA_MP_NSRC_F]		{BITSOFFS(120,8), "MultiPathNumSrc", mad_dump_uint},
	[IB_SA_MP_NDEST_F]		{BITSOFFS(128,8), "MultiPathNumDest", mad_dump_uint},
	[IB_SA_MP_GID0_F]		{192, 128, "MultiPathGid", mad_dump_array},

	/*
	 * MC Member rec
	 */
	[IB_SA_MCM_MGID_F]		{0, 128, "McastMemMGid", mad_dump_array},
	[IB_SA_MCM_PORTGID_F]		{128, 128, "McastMemPortGid", mad_dump_array},
	[IB_SA_MCM_QKEY_F]		{256, 32, "McastMemQkey", mad_dump_hex},
	[IB_SA_MCM_MLID_F]		{BITSOFFS(288, 16), "McastMemMLid", mad_dump_hex},
	[IB_SA_MCM_MTU_F]		{BITSOFFS(306, 6), "McastMemMTU", mad_dump_uint},
	[IB_SA_MCM_TCLASS_F]		{BITSOFFS(312, 8), "McastMemTClass", mad_dump_uint},
	[IB_SA_MCM_PKEY_F]		{BITSOFFS(320, 16), "McastMemPkey", mad_dump_uint},
	[IB_SA_MCM_RATE_F]		{BITSOFFS(338, 6), "McastMemRate", mad_dump_uint},
	[IB_SA_MCM_SL_F]		{BITSOFFS(352, 4), "McastMemSL", mad_dump_uint},
	[IB_SA_MCM_FLOW_LABEL_F]	{BITSOFFS(356, 20), "McastMemFlowLbl", mad_dump_uint},
	[IB_SA_MCM_JOIN_STATE_F]	{BITSOFFS(388, 4), "McastMemJoinState", mad_dump_uint},
	[IB_SA_MCM_PROXY_JOIN_F]	{BITSOFFS(392, 1), "McastMemProxyJoin", mad_dump_uint},

	/*
	 * Service record
	 */
	[IB_SA_SR_ID_F]			{0, 64, "ServRecID", mad_dump_hex},
	[IB_SA_SR_GID_F]		{64, 128, "ServRecGid", mad_dump_array},
	[IB_SA_SR_PKEY_F]		{BITSOFFS(192, 16), "ServRecPkey", mad_dump_hex},
	[IB_SA_SR_LEASE_F]		{224, 32, "ServRecLease", mad_dump_hex},
	[IB_SA_SR_KEY_F]		{256, 128, "ServRecKey", mad_dump_hex},
	[IB_SA_SR_NAME_F]		{384, 512, "ServRecName", mad_dump_string},
	[IB_SA_SR_DATA_F]		{896, 512, "ServRecData", mad_dump_array},	/* ATS for example */

	/*
	 * ATS SM record - within SA_SR_DATA
	 */
	[IB_ATS_SM_NODE_ADDR_F]		{12*8, 32, "ATSNodeAddr", mad_dump_hex},
	[IB_ATS_SM_MAGIC_KEY_F]		{BITSOFFS(16*8, 16), "ATSMagicKey", mad_dump_hex},
	[IB_ATS_SM_NODE_TYPE_F]		{BITSOFFS(18*8, 16), "ATSNodeType", mad_dump_hex},
	[IB_ATS_SM_NODE_NAME_F]		{32*8, 32*8, "ATSNodeName", mad_dump_string},

	/*
	 * SLTOVL MAPPING TABLE
	 */
	[IB_SLTOVL_MAPPING_TABLE_F]	{0, 64, "SLToVLMap", mad_dump_hex},

	/*
	 * VL ARBITRATION TABLE
	 */
	[IB_VL_ARBITRATION_TABLE_F]	{0, 512, "VLArbTbl", mad_dump_array},

	/*
	 * IB vendor classes range 2
	 */
	[IB_VEND2_OUI_F]		{BE_OFFS(36*8, 24), "OUI", mad_dump_array},
	[IB_VEND2_DATA_F]		{40*8, (256-40)*8, "Vendor2Data", mad_dump_array},

	/*
	 * Extended port counters
	 */
	[IB_PC_EXT_PORT_SELECT_F]	{BITSOFFS(8, 8), "PortSelect", mad_dump_uint},
	[IB_PC_EXT_COUNTER_SELECT_F]	{BITSOFFS(16, 16), "CounterSelect", mad_dump_hex},
	[IB_PC_EXT_XMT_BYTES_F]		{64, 64, "PortXmitData", mad_dump_uint},
	[IB_PC_EXT_RCV_BYTES_F]		{128, 64, "PortRcvData", mad_dump_uint},
	[IB_PC_EXT_XMT_PKTS_F]		{192, 64, "PortXmitPkts", mad_dump_uint},
	[IB_PC_EXT_RCV_PKTS_F]		{256, 64, "PortRcvPkts", mad_dump_uint},
	[IB_PC_EXT_XMT_UPKTS_F]		{320, 64, "PortUnicastXmitPkts", mad_dump_uint},
	[IB_PC_EXT_RCV_UPKTS_F]		{384, 64, "PortUnicastRcvPkts", mad_dump_uint},
	[IB_PC_EXT_XMT_MPKTS_F]		{448, 64, "PortMulticastXmitPkts", mad_dump_uint},
	[IB_PC_EXT_RCV_MPKTS_F]		{512, 64, "PortMulticastRcvPkts", mad_dump_uint},

	/*
	 * GUIDInfo fields
	 */
	[IB_GUID_GUID0_F]		{0, 64, "GUID0", mad_dump_hex},

};

void
_set_field64(void *buf, int base_offs, ib_field_t *f, uint64_t val)
{
	uint64_t nval;

	nval = htonll(val);
	memcpy((char *)buf + base_offs + f->bitoffs / 8, &nval, sizeof(uint64_t));
}

uint64_t
_get_field64(void *buf, int base_offs, ib_field_t *f)
{
	uint64_t val;
	memcpy(&val, ((char *)buf + base_offs + f->bitoffs / 8), sizeof(uint64_t));
	return ntohll(val);
}

void
_set_field(void *buf, int base_offs, ib_field_t *f, uint32_t val)
{
	int prebits = (8 - (f->bitoffs & 7)) & 7;
	int postbits = (f->bitoffs + f->bitlen) & 7;
	int bytelen = f->bitlen / 8;
	unsigned idx = base_offs + f->bitoffs / 8;
	char *p = (char *)buf;

	if (!bytelen && (f->bitoffs & 7) + f->bitlen < 8) {
		p[3^idx] &= ~((((1 << f->bitlen) - 1)) << (f->bitoffs & 7));
		p[3^idx] |= (val & ((1 << f->bitlen) - 1)) << (f->bitoffs & 7);
		return;
	}

	if (prebits) {	/* val lsb in byte msb */
		p[3^idx] &= (1 << (8 - prebits)) - 1;
		p[3^idx++] |= (val & ((1 << prebits) - 1)) << (8 - prebits);
		val >>= prebits;
	}

	/* BIG endian byte order */
	for (; bytelen--; val >>= 8)
		p[3^idx++] = val & 0xff;

	if (postbits) {	/* val msb in byte lsb */
		p[3^idx] &= ~((1 << postbits) - 1);
		p[3^idx] |= val;
	}
}

uint32_t
_get_field(void *buf, int base_offs, ib_field_t *f)
{
	int prebits = (8 - (f->bitoffs & 7)) & 7;
	int postbits = (f->bitoffs + f->bitlen) & 7;
	int bytelen = f->bitlen / 8;
	unsigned idx = base_offs + f->bitoffs / 8;
	uint8_t *p = (uint8_t *)buf;
	uint32_t val = 0, v = 0, i;

	if (!bytelen && (f->bitoffs & 7) + f->bitlen < 8)
		return (p[3^idx] >> (f->bitoffs & 7)) & ((1 << f->bitlen) - 1);

	if (prebits)	/* val lsb from byte msb */
		v = p[3^idx++] >> (8 - prebits);

	if (postbits) {	/* val msb from byte lsb */
		i = base_offs + (f->bitoffs + f->bitlen) / 8;
		val = (p[3^i] & ((1 << postbits) - 1));
	}

	/* BIG endian byte order */
	for (idx += bytelen - 1; bytelen--; idx--)
		val = (val << 8) | p[3^idx];

	return (val << prebits) | v;
}

/* field must be byte aligned */
void
_set_array(void *buf, int base_offs, ib_field_t *f, void *val)
{
	int bitoffs = f->bitoffs;

	if (f->bitlen < 32)
		bitoffs = BE_TO_BITSOFFS(bitoffs, f->bitlen);

	memcpy((uint8_t *)buf + base_offs + bitoffs / 8, val, f->bitlen / 8);
}

void
_get_array(void *buf, int base_offs, ib_field_t *f, void *val)
{
	int bitoffs = f->bitoffs;

	if (f->bitlen < 32)
		bitoffs = BE_TO_BITSOFFS(bitoffs, f->bitlen);

	memcpy(val, (uint8_t *)buf + base_offs + bitoffs / 8, f->bitlen / 8);
}
