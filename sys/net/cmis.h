/*-
 * Copyright (c) 2026 Netflix Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * The following set of constants are from the OIF Common Management
 * Interface Specification (CMIS) revision 5.3, September 2024.
 *
 * CMIS defines a 256-byte addressable memory with lower (0-127) and
 * upper (128-255) regions.  Lower memory is always accessible.
 * Upper memory is paged via byte 127 (page select) and byte 126
 * (bank select).
 *
 * All values are read across an I2C bus at address 0xA0.
 */

#ifndef	_NET_CMIS_H_
#define	_NET_CMIS_H_

#define	CMIS_BASE	0xA0	/* Base I2C address for all requests */

/* CMIS Module Types (SFF-8024 Identifier, byte 0) */
#define	CMIS_ID_QSFP_DD	0x18	/* QSFP-DD */
#define	CMIS_ID_QSFP8X		0x19	/* QSFP 8X (OSFP) */
#define	CMIS_ID_SFP_DD		0x1A	/* SFP-DD */
#define	CMIS_ID_DSFP		0x1B	/* DSFP */
#define	CMIS_ID_QSFP_CMIS	0x1E	/* QSFP+ with CMIS */

/* Table 8-4: Lower Memory Map (bytes 0x00-0x7F) */
enum {
	/* Table 8-5: Management Characteristics (bytes 0-2) */
	CMIS_ID			= 0,	/* SFF-8024 Identifier */
	CMIS_REV		= 1,	/* CMIS revision (major.minor) */
	CMIS_MODULE_TYPE	= 2,	/* Memory model, config options */

	/* Table 8-6: Global Status (byte 3) */
	CMIS_MODULE_STATE	= 3,	/* Module state, interrupt status */

	/* Table 8-8: Flags Summary (bytes 4-7) */
	CMIS_FLAGS_BANK0	= 4,	/* Flags summary, bank 0 */
	CMIS_FLAGS_BANK1	= 5,	/* Flags summary, bank 1 */
	CMIS_FLAGS_BANK2	= 6,	/* Flags summary, bank 2 */
	CMIS_FLAGS_BANK3	= 7,	/* Flags summary, bank 3 */

	/* Table 8-9: Module-Level Flags (bytes 8-13) */
	CMIS_MOD_FLAGS_START	= 8,	/* Module firmware/state flags */
	CMIS_MOD_FLAGS_TEMP_VCC	= 9,	/* Temp/VCC alarm/warning flags */
	CMIS_MOD_FLAGS_AUX	= 10,	/* Aux monitor alarm/warning flags */
	CMIS_MOD_FLAGS_CUSTOM	= 11,	/* Custom/Aux3 monitor flags */
	CMIS_MOD_FLAGS_RSVD	= 12,	/* Reserved */
	CMIS_MOD_FLAGS_VENDOR	= 13,	/* Custom module-level flags */

	/* Table 8-10: Module-Level Monitor Values (bytes 14-25) */
	CMIS_TEMP		= 14,	/* S16 Temperature (1/256 deg C) */
	CMIS_VCC		= 16,	/* U16 Supply Voltage (100 uV) */
	CMIS_AUX1		= 18,	/* S16 Aux1 Monitor */
	CMIS_AUX2		= 20,	/* S16 Aux2 Monitor */
	CMIS_AUX3		= 22,	/* S16 Aux3 Monitor */
	CMIS_CUSTOM_MON		= 24,	/* S16/U16 Custom Monitor */

	/* Table 8-11: Module Global Controls (bytes 26-30) */
	CMIS_MOD_CTRL		= 26,	/* Global control bits */
	CMIS_MOD_CTRL2		= 27,	/* Global control bits (cont.) */
	CMIS_MOD_CTRL3		= 28,	/* Global control bits (cont.) */
	CMIS_MOD_CTRL4		= 29,	/* Global control bits (cont.) */
	CMIS_MOD_CTRL5		= 30,	/* Global control bits (cont.) */

	/* Table 8-12: Module Level Masks (bytes 31-36) */
	CMIS_MOD_MASKS_START	= 31,	/* Module-level masks start */
	CMIS_MOD_MASKS_END	= 36,	/* Module-level masks end */

	/* Table 8-13: CDB Command Status (bytes 37-38) */
	CMIS_CDB_STATUS1	= 37,	/* CDB instance 1 status */
	CMIS_CDB_STATUS2	= 38,	/* CDB instance 2 status */

	/* Table 8-15: Module Active Firmware Version (bytes 39-40) */
	CMIS_FW_VER_MAJOR	= 39,	/* Active firmware major version */
	CMIS_FW_VER_MINOR	= 40,	/* Active firmware minor version */

	/* Table 8-16: Fault Information (byte 41) */
	CMIS_FAULT_CAUSE	= 41,	/* Fault cause for ModuleFault */

	/* Table 8-17: Miscellaneous Status (bytes 42-45) */
	CMIS_MISC_STATUS_START	= 42,	/* Password status, etc. */
	CMIS_MISC_STATUS_END	= 45,

	/* Table 8-18: Extended Module Information (bytes 56-63) */
	CMIS_EXT_MOD_INFO_START	= 56,
	CMIS_EXT_MOD_INFO_END	= 63,

	/* Table 8-21: Media Type (byte 85) */
	CMIS_MEDIA_TYPE		= 85,	/* MediaType encoding */

	/* Table 8-23: Application Descriptors (bytes 86-117) */
	CMIS_APP_DESC_START	= 86,	/* First Application Descriptor */
	CMIS_APP_DESC1		= 86,	/* AppDescriptor 1 (AppSel 1) */
	CMIS_APP_DESC2		= 90,	/* AppDescriptor 2 (AppSel 2) */
	CMIS_APP_DESC3		= 94,	/* AppDescriptor 3 (AppSel 3) */
	CMIS_APP_DESC4		= 98,	/* AppDescriptor 4 (AppSel 4) */
	CMIS_APP_DESC5		= 102,	/* AppDescriptor 5 (AppSel 5) */
	CMIS_APP_DESC6		= 106,	/* AppDescriptor 6 (AppSel 6) */
	CMIS_APP_DESC7		= 110,	/* AppDescriptor 7 (AppSel 7) */
	CMIS_APP_DESC8		= 114,	/* AppDescriptor 8 (AppSel 8) */

	/* Table 8-24: Password (bytes 118-125) */
	CMIS_PASSWORD_CHANGE	= 118,	/* Password change entry (4 bytes) */
	CMIS_PASSWORD_ENTRY	= 122,	/* Password entry area (4 bytes) */

	/* Table 8-25: Page Mapping (bytes 126-127) */
	CMIS_BANK_SEL		= 126,	/* Bank select */
	CMIS_PAGE_SEL		= 127,	/* Page select */
};

/*
 * Byte 2 (CMIS_MODULE_TYPE) bit definitions (Table 8-5)
 */
#define	CMIS_MODULE_TYPE_FLAT	(1 << 7) /* MemoryModel: 1=flat, 0=paged */
#define	CMIS_MODULE_TYPE_STEPPED (1 << 6) /* SteppedConfigOnly */
#define	CMIS_MODULE_TYPE_MCISPEED_MASK	0x3C /* MciMaxSpeed, bits 5:2 */
#define	CMIS_MODULE_TYPE_MCISPEED_SHIFT	2
#define	CMIS_MODULE_TYPE_AUTOCOM_MASK	0x03 /* AutoCommissioning, bits 1:0 */

/* MciMaxSpeed values (I2CMCI) */
#define	CMIS_MCISPEED_400KHZ	0	/* Up to 400 kHz */
#define	CMIS_MCISPEED_1MHZ	1	/* Up to 1 MHz */
#define	CMIS_MCISPEED_3_4MHZ	2	/* Up to 3.4 MHz */

/* AutoCommissioning values (when SteppedConfigOnly=1) */
#define	CMIS_AUTOCOM_NONE	0x00	/* Neither regular nor hot */
#define	CMIS_AUTOCOM_REGULAR	0x01	/* Only regular (ApplyDPInit) */
#define	CMIS_AUTOCOM_HOT	0x02	/* Only hot (ApplyImmediate) */

/*
 * Byte 3 (CMIS_MODULE_STATE) bit definitions (Table 8-6)
 */
#define	CMIS_MODULE_STATE_MASK	0x0E	/* ModuleState, bits 3:1 */
#define	CMIS_MODULE_STATE_SHIFT	1
#define	CMIS_MODULE_STATE_INTL	0x01	/* InterruptDeasserted (bit 0) */

/* Table 8-7: Module State Encodings (bits 3:1 of byte 3) */
#define	CMIS_STATE_LOWPWR	1	/* ModuleLowPwr */
#define	CMIS_STATE_PWRUP	2	/* ModulePwrUp */
#define	CMIS_STATE_READY	3	/* ModuleReady */
#define	CMIS_STATE_PWRDN	4	/* ModulePwrDn */
#define	CMIS_STATE_FAULT	5	/* ModuleFault */

/*
 * Bytes 4-7 (CMIS_FLAGS_BANKn) bit definitions (Table 8-8)
 * Same layout for all 4 bank bytes.
 */
#define	CMIS_FLAGS_PAGE2CH	(1 << 3) /* Flags on Page 2Ch */
#define	CMIS_FLAGS_PAGE14H	(1 << 2) /* Flags on Page 14h */
#define	CMIS_FLAGS_PAGE12H	(1 << 1) /* Flags on Page 12h */
#define	CMIS_FLAGS_PAGE11H	(1 << 0) /* Flags on Page 11h */

/*
 * Byte 8 (CMIS_MOD_FLAGS_START) bit definitions (Table 8-9)
 */
#define	CMIS_FLAG_CDB_COMPLETE2	(1 << 7) /* CdbCmdCompleteFlag2 */
#define	CMIS_FLAG_CDB_COMPLETE1	(1 << 6) /* CdbCmdCompleteFlag1 */
#define	CMIS_FLAG_DP_FW_ERROR	(1 << 2) /* DataPathFirmwareErrorFlag */
#define	CMIS_FLAG_MOD_FW_ERROR	(1 << 1) /* ModuleFirmwareErrorFlag */
#define	CMIS_FLAG_STATE_CHANGED	(1 << 0) /* ModuleStateChangedFlag */

/*
 * Byte 9 (CMIS_MOD_FLAGS_TEMP_VCC) bit definitions (Table 8-9)
 */
#define	CMIS_FLAG_VCC_LOW_WARN		(1 << 7) /* VccMonLowWarningFlag */
#define	CMIS_FLAG_VCC_HIGH_WARN		(1 << 6) /* VccMonHighWarningFlag */
#define	CMIS_FLAG_VCC_LOW_ALM		(1 << 5) /* VccMonLowAlarmFlag */
#define	CMIS_FLAG_VCC_HIGH_ALM		(1 << 4) /* VccMonHighAlarmFlag */
#define	CMIS_FLAG_TEMP_LOW_WARN		(1 << 3) /* TempMonLowWarningFlag */
#define	CMIS_FLAG_TEMP_HIGH_WARN 	(1 << 2) /* TempMonHighWarningFlag */
#define	CMIS_FLAG_TEMP_LOW_ALM		(1 << 1) /* TempMonLowAlarmFlag */
#define	CMIS_FLAG_TEMP_HIGH_ALM		(1 << 0) /* TempMonHighAlarmFlag */

/*
 * Byte 10 (CMIS_MOD_FLAGS_AUX) bit definitions (Table 8-9)
 */
#define	CMIS_FLAG_AUX2_LOW_WARN		(1 << 7)
#define	CMIS_FLAG_AUX2_HIGH_WARN	(1 << 6)
#define	CMIS_FLAG_AUX2_LOW_ALM		(1 << 5)
#define	CMIS_FLAG_AUX2_HIGH_ALM		(1 << 4)
#define	CMIS_FLAG_AUX1_LOW_WARN		(1 << 3)
#define		CMIS_FLAG_AUX1_HIGH_WARN (1 << 2)
#define	CMIS_FLAG_AUX1_LOW_ALM		(1 << 1)
#define	CMIS_FLAG_AUX1_HIGH_ALM		(1 << 0)

/*
 * Byte 11 (CMIS_MOD_FLAGS_CUSTOM) bit definitions (Table 8-9)
 */
#define	CMIS_FLAG_CUST_LOW_WARN		(1 << 7)
#define	CMIS_FLAG_CUST_HIGH_WARN	(1 << 6)
#define	CMIS_FLAG_CUST_LOW_ALM		(1 << 5)
#define	CMIS_FLAG_CUST_HIGH_ALM		(1 << 4)
#define	CMIS_FLAG_AUX3_LOW_WARN		(1 << 3)
#define	CMIS_FLAG_AUX3_HIGH_WARN	(1 << 2)
#define	CMIS_FLAG_AUX3_LOW_ALM		(1 << 1)
#define	CMIS_FLAG_AUX3_HIGH_ALM		(1 << 0)

/*
 * Byte 26 (CMIS_MOD_CTRL) bit definitions (Table 8-11)
 */
#define	CMIS_CTRL_BANK_BCAST		(1 << 7) /* BankBroadcastEnable */
#define	CMIS_CTRL_LOWPWR_HW		(1 << 6) /* LowPwrAllowRequestHW */
#define	CMIS_CTRL_SQUELCH_METHOD 	(1 << 5) /* SquelchMethodSelect */
#define	CMIS_CTRL_LOWPWR_SW		(1 << 4) /* LowPwrRequestSW */
#define	CMIS_CTRL_SW_RESET		(1 << 3) /* SoftwareReset */

/*
 * Byte 27 (CMIS_MOD_CTRL2) bit definitions (Table 8-11)
 */
#define	CMIS_CTRL2_MCISPEED_MASK 0x0F	/* MciSpeedConfiguration, bits 3:0 */

/*
 * Bytes 31-36 mask bits mirror bytes 8-13 flag bits (Table 8-12)
 * Use the same bit positions as CMIS_FLAG_* above.
 */

/*
 * Bytes 37-38 (CDB Status) bit definitions (Table 8-14)
 */
#define	CMIS_CDB_BUSY		(1 << 7) /* CdbIsBusy */
#define	CMIS_CDB_FAILED		(1 << 6) /* CdbHasFailed */
#define	CMIS_CDB_RESULT_MASK	0x3F	/* CdbCommandResult, bits 5:0 */

/* Table 8-20: Media Type Encodings */
#define	CMIS_MEDIA_TYPE_UNDEF	0x00	/* Undefined */
#define	CMIS_MEDIA_TYPE_MMF	0x01	/* Optical: MMF */
#define	CMIS_MEDIA_TYPE_SMF	0x02	/* Optical: SMF */
#define	CMIS_MEDIA_TYPE_COPPER	0x03	/* Passive/Active Copper */
#define	CMIS_MEDIA_TYPE_ACTIVE	0x04	/* Active Cable */
#define	CMIS_MEDIA_TYPE_BASET	0x05	/* BASE-T */

/* Application Descriptor constants */
#define	CMIS_APP_DESC_SIZE	4	/* Bytes per descriptor */
#define	CMIS_MAX_APP_DESC	8	/* Max descriptors in lower memory */

/* Table 8-22: Offsets within an Application Descriptor */
#define	CMIS_APP_HOST_IF_ID		0	/* HostInterfaceID */
#define	CMIS_APP_MEDIA_IF_ID		1	/* MediaInterfaceID */
#define	CMIS_APP_LANE_COUNT		2	/* Host[7:4], Media[3:0] */
#define	CMIS_APP_HOST_ASSIGN		3	/* HostLaneAssignment */
#define	CMIS_APP_HOST_LANES_MASK	0xF0	/* HostLaneCount, bits 7:4 */
#define	CMIS_APP_HOST_LANES_SHIFT	4
#define	CMIS_APP_MEDIA_LANES_MASK	0x0F	/* MediaLaneCount, bits 3:0 */

/*
 * Table 8-26: Page 00h - Administrative Information
 * Accessed with page=0x00, bank=0.
 */
enum {
	CMIS_P0_ID		= 128,	/* SFF-8024 Identifier copy */
	CMIS_P0_VENDOR_NAME	= 129,	/* Vendor name (16 bytes, ASCII) */
	CMIS_P0_VENDOR_OUI	= 145,	/* Vendor IEEE OUI (3 bytes) */
	CMIS_P0_VENDOR_PN	= 148,	/* Part number (16 bytes, ASCII) */
	CMIS_P0_VENDOR_REV	= 164,	/* Vendor revision (2 bytes) */
	CMIS_P0_VENDOR_SN	= 166,	/* Serial number (16 bytes, ASCII) */
	CMIS_P0_DATE_CODE	= 182,	/* Date code (8 bytes: YYMMDDLL) */
	CMIS_P0_CLEI		= 190,	/* CLEI code (10 bytes, ASCII) */
	CMIS_P0_MOD_POWER	= 200,	/* Module power class */
	CMIS_P0_MAX_POWER	= 201,	/* Max power (multiples of 0.25W) */
	CMIS_P0_CABLE_LEN	= 202,	/* Cable assembly link length */
	CMIS_P0_CONNECTOR	= 203,	/* Connector type (SFF-8024) */
	CMIS_P0_COPPER_ATTEN	= 204,	/* Copper cable attenuation (6 bytes) */
	CMIS_P0_MEDIA_LANE_INFO	= 210,	/* Supported near end media lanes */
	CMIS_P0_CABLE_ASM_INFO	= 211,	/* Far end breakout info */
	CMIS_P0_MEDIA_TECH	= 212,	/* Media interface technology */
	CMIS_P0_MCI_ADVERT	= 213,	/* MCI advertisement (2 bytes) */
	CMIS_P0_PAGE_CKSUM	= 222,	/* Page checksum (bytes 128-221) */
	CMIS_P0_CUSTOM		= 223,	/* Custom (33 bytes) */
};

/*
 * Table 8-82: Page 11h - Lane Status and Data Path Status
 * Accessed with page=0x11, bank=0 (lanes 1-8).
 */
enum {
	/* Table 8-83: Data Path States (bytes 128-131) */
	CMIS_P11_DPSTATE_12		= 128,	/* DPState for host lanes 1-2 */
	CMIS_P11_DPSTATE_34		= 129,	/* DPState for host lanes 3-4 */
	CMIS_P11_DPSTATE_56		= 130,	/* DPState for host lanes 5-6 */
	CMIS_P11_DPSTATE_78		= 131,	/* DPState for host lanes 7-8 */

	/* Table 8-85: Lane Output Status (bytes 132-133) */
	CMIS_P11_OUTPUT_RX		= 132,	/* OutputStatusRx per lane */
	CMIS_P11_OUTPUT_TX		= 133,	/* OutputStatusTx per lane */

	/* Table 8-86: State Changed Flags (bytes 134-135) */
	CMIS_P11_DPSTATE_CHGD		= 134,	/* DPStateChanged flags */
	CMIS_P11_OUTPUT_CHGD_TX		= 135,	/* OutputStatusChangedTx flags*/

	/* Table 8-87: Lane-Specific Tx Flags (bytes 136-141) */
	CMIS_P11_TX_FAULT		= 136,	/* TxFault per lane */
	CMIS_P11_TX_LOS			= 137,	/* TxLOS per lane */
	CMIS_P11_TX_CDR_LOL		= 138,	/* TxCDRLOL per lane */
	CMIS_P11_TX_ADPT_EQ_FAIL 	= 139,	/* TxAdaptEqFail per lane */
	CMIS_P11_TX_PWR_HIGH_ALM 	= 140,	/* TxPowerHighAlarm per lane */
	CMIS_P11_TX_PWR_LOW_ALM		= 141,	/* TxPowerLowAlarm per lane */
	CMIS_P11_TX_BIAS_HIGH_ALM 	= 142,	/* TxBiasHighAlarm per lane */
	CMIS_P11_TX_BIAS_LOW_ALM 	= 143,	/* TxBiasLowAlarm per lane */
	CMIS_P11_TX_PWR_HIGH_WARN 	= 144,	/* TxPowerHighWarning per lane*/
	CMIS_P11_TX_PWR_LOW_WARN 	= 145,	/* TxPowerLowWarning per lane */
	CMIS_P11_TX_BIAS_HIGH_WARN 	= 146,	/* TxBiasHighWarning per lane */
	CMIS_P11_TX_BIAS_LOW_WARN 	= 147,	/* TxBiasLowWarning per lane */

	/* Table 8-88: Rx Flags (bytes 148-153) */
	CMIS_P11_RX_LOS			= 148,	/* RxLOS per lane */
	CMIS_P11_RX_CDR_LOL		= 149,	/* RxCDRLOL per lane */
	CMIS_P11_RX_PWR_HIGH_ALM 	= 150,	/* RxPowerHighAlarm per lane */
	CMIS_P11_RX_PWR_LOW_ALM		= 151,	/* RxPowerLowAlarm per lane */
	CMIS_P11_RX_PWR_HIGH_WARN 	= 152,	/* RxPowerHighWarning per lane*/
	CMIS_P11_RX_PWR_LOW_WARN 	= 153,	/* RxPowerLowWarning per lane */

	/* Table 8-89: Lane-Specific Monitors (bytes 154-201) */
	CMIS_P11_TX_PWR_1		= 154,	/* U16 Tx optical pwr, lane 1 */
	CMIS_P11_TX_PWR_2		= 156,	/* (0.1 uW increments) */
	CMIS_P11_TX_PWR_3		= 158,
	CMIS_P11_TX_PWR_4		= 160,
	CMIS_P11_TX_PWR_5		= 162,
	CMIS_P11_TX_PWR_6		= 164,
	CMIS_P11_TX_PWR_7		= 166,
	CMIS_P11_TX_PWR_8		= 168,
	CMIS_P11_TX_BIAS_1		= 170,	/* U16 Tx bias current, lane 1*/
	CMIS_P11_TX_BIAS_2		= 172,	/* (2 uA increments) */
	CMIS_P11_TX_BIAS_3		= 174,
	CMIS_P11_TX_BIAS_4		= 176,
	CMIS_P11_TX_BIAS_5		= 178,
	CMIS_P11_TX_BIAS_6		= 180,
	CMIS_P11_TX_BIAS_7		= 182,
	CMIS_P11_TX_BIAS_8		= 184,
	CMIS_P11_RX_PWR_1		= 186,	/* U16 Rx input power, lane 1 */
	CMIS_P11_RX_PWR_2		= 188,	/* (0.1 uW increments) */
	CMIS_P11_RX_PWR_3		= 190,
	CMIS_P11_RX_PWR_4		= 192,
	CMIS_P11_RX_PWR_5		= 194,
	CMIS_P11_RX_PWR_6		= 196,
	CMIS_P11_RX_PWR_7		= 198,
	CMIS_P11_RX_PWR_8		= 200,

	/* Table 8-90: Config Command Status (bytes 202-205) */
	CMIS_P11_CONFIG_STAT_12		= 202,	/* ConfigStatus lanes 1-2 */
	CMIS_P11_CONFIG_STAT_34		= 203,	/* ConfigStatus lanes 3-4 */
	CMIS_P11_CONFIG_STAT_56		= 204,	/* ConfigStatus lanes 5-6 */
	CMIS_P11_CONFIG_STAT_78		= 205,	/* ConfigStatus lanes 7-8 */

	/* Table 8-93: Active Control Set (bytes 206-234) */
	CMIS_P11_ACS_DPCONFIG1		= 206,	/* DPConfigLane1 (AppSel[7:4])*/
	CMIS_P11_ACS_DPCONFIG2		= 207,	/* DPConfigLane2 */
	CMIS_P11_ACS_DPCONFIG3		= 208,	/* DPConfigLane3 */
	CMIS_P11_ACS_DPCONFIG4		= 209,	/* DPConfigLane4 */
	CMIS_P11_ACS_DPCONFIG5		= 210,	/* DPConfigLane5 */
	CMIS_P11_ACS_DPCONFIG6		= 211,	/* DPConfigLane6 */
	CMIS_P11_ACS_DPCONFIG7		= 212,	/* DPConfigLane7 */
	CMIS_P11_ACS_DPCONFIG8		= 213,	/* DPConfigLane8 */
	CMIS_P11_ACS_TX_START		= 214,	/* Provisioned Tx Controls */
	CMIS_P11_ACS_TX_END		= 225,
	CMIS_P11_ACS_RX_START		= 226,	/* Provisioned Rx Controls */
	CMIS_P11_ACS_RX_END		= 234,

	/* Table 8-96: Data Path Conditions (bytes 235-239) */
	CMIS_P11_DP_COND_START		= 235,
	CMIS_P11_DP_COND_END		= 239,

	/* Table 8-97: Media Lane Mapping (bytes 240-255) */
	CMIS_P11_MEDIA_MAP_START 	= 240,
	CMIS_P11_MEDIA_MAP_END		= 255,
};

/*
 * Per-lane bit positions for Page 11h flag/status registers.
 * Bytes 132-153 use bit N for lane N+1 (bit 7 = lane 8, bit 0 = lane 1).
 */
#define	CMIS_LANE8			(1 << 7)
#define	CMIS_LANE7			(1 << 6)
#define	CMIS_LANE6			(1 << 5)
#define	CMIS_LANE5			(1 << 4)
#define	CMIS_LANE4			(1 << 3)
#define	CMIS_LANE3			(1 << 2)
#define	CMIS_LANE2			(1 << 1)
#define	CMIS_LANE1			(1 << 0)

/*
 * DPState encoding within bytes 128-131 (Table 8-83).
 * Each byte holds two 4-bit DPState fields:
 * bits 7:4 = even lane, bits 3:0 = odd lane.
 */
#define	CMIS_DPSTATE_HI_MASK		0xF0	/* Upper nibble (even lane) */
#define	CMIS_DPSTATE_HI_SHIFT		4
#define	CMIS_DPSTATE_LO_MASK		0x0F	/* Lower nibble (odd lane) */

/* Table 8-84: Data Path State Encoding */
#define	CMIS_DPSTATE_DEACTIVATED	1	/* DPDeactivated (or unused) */
#define	CMIS_DPSTATE_INIT		2	/* DPInit */
#define	CMIS_DPSTATE_DEINIT		3	/* DPDeinit */
#define	CMIS_DPSTATE_ACTIVATED		4	/* DPActivated */
#define	CMIS_DPSTATE_TXTURNON		5	/* DPTxTurnOn */
#define	CMIS_DPSTATE_TXTURNOFF		6	/* DPTxTurnOff */
#define	CMIS_DPSTATE_INITIALIZED 	7	/* DPInitialized */

/*
 * ConfigStatus encoding within bytes 202-205 (Table 8-90/91).
 * Each byte holds two 4-bit status fields, same nibble layout as DPState.
 */
#define	CMIS_CFGSTAT_UNDEFINED		0x0	/* Undefined */
#define	CMIS_CFGSTAT_SUCCESS		0x1	/* ConfigSuccess */
#define	CMIS_CFGSTAT_REJECTED		0x2	/* ConfigRejected */
#define	CMIS_CFGSTAT_REJECTEDINV 	0x3	/* ConfigRejectedInvalidAppSel*/
#define	CMIS_CFGSTAT_INPROGRESS		0x4	/* ConfigInProgress */
#define	CMIS_CFGSTAT_REJECTEDLANE 	0x5	/* ConfigRejectedInvalidLane */
#define	CMIS_CFGSTAT_REJECTEDEQ		0x6	/* ConfigRejectedInvalidEq */

/* DPConfigLane (CMIS_P11_ACS_DPCONFIGn) bit definitions (Table 8-92/93) */
#define	CMIS_ACS_APPSEL_MASK		0xF0	/* AppSel code, bits 7:4 */
#define	CMIS_ACS_APPSEL_SHIFT		4
#define	CMIS_ACS_DATAPATH_MASK		0x0F	/* DataPathID, bits 3:0 */

/*
 * Page 00h bit definitions
 */

/* Byte 200 (CMIS_P0_MOD_POWER) bit definitions (Table 8-31) */
#define	CMIS_POWER_CLASS_MASK		0xE0	/* ModulePowerClass, bits 7:5 */
#define	CMIS_POWER_CLASS_SHIFT		5
#define	CMIS_POWER_CLASS_1		0	/* <=1.5W */
#define	CMIS_POWER_CLASS_2		1	/* <=3.5W */
#define	CMIS_POWER_CLASS_3		2	/* <=7.0W */
#define	CMIS_POWER_CLASS_4		3	/* <=8.0W */
#define	CMIS_POWER_CLASS_5		4	/* <=10.0W */
#define	CMIS_POWER_CLASS_6		5	/* <=12.0W */
#define	CMIS_POWER_CLASS_7		6	/* <=14.0W */
#define	CMIS_POWER_CLASS_8		7	/* >14.0W, see MaxPower byte */
#define	CMIS_POWER_MAX_IN_BYTE		(1 << 4) /* MaxPowerOverride: byte 201*/

/* Byte 202 (CMIS_P0_CABLE_LEN) bit definitions (Table 8-32) */
#define	CMIS_CABLE_LEN_MULT_MASK	0xC0	/* LengthMultiplier, bits 7:6 */
#define	CMIS_CABLE_LEN_MULT_SHIFT	6
#define	CMIS_CABLE_LEN_VAL_MASK		0x3F	/* Length value, bits 5:0 */
#define	CMIS_CABLE_LEN_MULT_01		0	/* x 0.1m */
#define	CMIS_CABLE_LEN_MULT_1		1	/* x 1m */
#define	CMIS_CABLE_LEN_MULT_10		2	/* x 10m */
#define	CMIS_CABLE_LEN_MULT_100		3	/* x 100m */

/* Lane monitor stride (each monitor is U16 = 2 bytes per lane) */
#define	CMIS_LANE_MON_SIZE		2
#define	CMIS_MAX_LANES			8

#endif /* !_NET_CMIS_H_ */
