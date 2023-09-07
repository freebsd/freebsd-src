/*
 * Copyright (c) 2013-2022, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* ZynqMP power management enums and defines */

#ifndef PM_DEFS_H
#define PM_DEFS_H

/*********************************************************************
 * Macro definitions
 ********************************************************************/

/*
 * Version number is a 32bit value, like:
 * (PM_VERSION_MAJOR << 16) | PM_VERSION_MINOR
 */
#define PM_VERSION_MAJOR	1U
#define PM_VERSION_MINOR	1U

#define PM_VERSION	((PM_VERSION_MAJOR << 16U) | PM_VERSION_MINOR)

/**
 * PM API versions
 */
/* Expected version of firmware APIs */
#define FW_API_BASE_VERSION		(1U)
/* Expected version of firmware API for feature check */
#define FW_API_VERSION_2		(2U)
/* Version of APIs implemented in ATF */
#define ATF_API_BASE_VERSION		(1U)

/* Capabilities for RAM */
#define PM_CAP_ACCESS	0x1U
#define PM_CAP_CONTEXT	0x2U

#define MAX_LATENCY	(~0U)
#define MAX_QOS		100U

/* State arguments of the self suspend */
#define PM_STATE_CPU_IDLE		0x0U
#define PM_STATE_SUSPEND_TO_RAM		0xFU

/* APU processor states */
#define PM_PROC_STATE_FORCEDOFF		0U
#define PM_PROC_STATE_ACTIVE		1U
#define PM_PROC_STATE_SLEEP		2U
#define PM_PROC_STATE_SUSPENDING	3U

#define EM_FUNID_NUM_MASK    0xF0000U

#define PM_GET_CALLBACK_DATA		0xa01
#define PM_SET_SUSPEND_MODE		0xa02
#define PM_GET_TRUSTZONE_VERSION	0xa03

/*********************************************************************
 * Enum definitions
 ********************************************************************/

enum pm_api_id {
	/* Miscellaneous API functions: */
	PM_GET_API_VERSION = 1, /* Do not change or move */
	PM_SET_CONFIGURATION,
	PM_GET_NODE_STATUS,
	PM_GET_OP_CHARACTERISTIC,
	PM_REGISTER_NOTIFIER,
	/* API for suspending of PUs: */
	PM_REQ_SUSPEND,
	PM_SELF_SUSPEND,
	PM_FORCE_POWERDOWN,
	PM_ABORT_SUSPEND,
	PM_REQ_WAKEUP,
	PM_SET_WAKEUP_SOURCE,
	PM_SYSTEM_SHUTDOWN,
	/* API for managing PM slaves: */
	PM_REQ_NODE,
	PM_RELEASE_NODE,
	PM_SET_REQUIREMENT,
	PM_SET_MAX_LATENCY,
	/* Direct control API functions: */
	PM_RESET_ASSERT,
	PM_RESET_GET_STATUS,
	PM_MMIO_WRITE,
	PM_MMIO_READ,
	PM_INIT_FINALIZE,
	PM_FPGA_LOAD,
	PM_FPGA_GET_STATUS,
	PM_GET_CHIPID,
	PM_SECURE_RSA_AES,
	PM_SECURE_SHA,
	PM_SECURE_RSA,
	PM_PINCTRL_REQUEST,
	PM_PINCTRL_RELEASE,
	PM_PINCTRL_GET_FUNCTION,
	PM_PINCTRL_SET_FUNCTION,
	PM_PINCTRL_CONFIG_PARAM_GET,
	PM_PINCTRL_CONFIG_PARAM_SET,
	PM_IOCTL,
	/* API to query information from firmware */
	PM_QUERY_DATA,
	/* Clock control API functions */
	PM_CLOCK_ENABLE,
	PM_CLOCK_DISABLE,
	PM_CLOCK_GETSTATE,
	PM_CLOCK_SETDIVIDER,
	PM_CLOCK_GETDIVIDER,
	PM_CLOCK_SETRATE,
	PM_CLOCK_GETRATE,
	PM_CLOCK_SETPARENT,
	PM_CLOCK_GETPARENT,
	PM_SECURE_IMAGE,
	/* FPGA PL Readback */
	PM_FPGA_READ,
	PM_SECURE_AES,
	/* PLL control API functions */
	PM_PLL_SET_PARAMETER,
	PM_PLL_GET_PARAMETER,
	PM_PLL_SET_MODE,
	PM_PLL_GET_MODE,
	/* PM Register Access API */
	PM_REGISTER_ACCESS,
	PM_EFUSE_ACCESS,
	PM_FPGA_GET_VERSION,
	PM_FPGA_GET_FEATURE_LIST,
	PM_FEATURE_CHECK = 63,
	PM_API_MAX
};

enum pm_query_id {
	PM_QID_INVALID = 0,
	PM_QID_CLOCK_GET_NAME,
	PM_QID_CLOCK_GET_TOPOLOGY,
	PM_QID_CLOCK_GET_FIXEDFACTOR_PARAMS,
	PM_QID_CLOCK_GET_PARENTS,
	PM_QID_CLOCK_GET_ATTRIBUTES,
	PM_QID_PINCTRL_GET_NUM_PINS,
	PM_QID_PINCTRL_GET_NUM_FUNCTIONS,
	PM_QID_PINCTRL_GET_NUM_FUNCTION_GROUPS,
	PM_QID_PINCTRL_GET_FUNCTION_NAME,
	PM_QID_PINCTRL_GET_FUNCTION_GROUPS,
	PM_QID_PINCTRL_GET_PIN_GROUPS,
	PM_QID_CLOCK_GET_NUM_CLOCKS,
	PM_QID_CLOCK_GET_MAX_DIVISOR,
};

enum pm_node_id {
	NODE_UNKNOWN = 0,
	NODE_APU,
	NODE_APU_0,
	NODE_APU_1,
	NODE_APU_2,
	NODE_APU_3,
	NODE_RPU,
	NODE_RPU_0,
	NODE_RPU_1,
	NODE_PLD,
	NODE_FPD,
	NODE_OCM_BANK_0,
	NODE_OCM_BANK_1,
	NODE_OCM_BANK_2,
	NODE_OCM_BANK_3,
	NODE_TCM_0_A,
	NODE_TCM_0_B,
	NODE_TCM_1_A,
	NODE_TCM_1_B,
	NODE_L2,
	NODE_GPU_PP_0,
	NODE_GPU_PP_1,
	NODE_USB_0,
	NODE_USB_1,
	NODE_TTC_0,
	NODE_TTC_1,
	NODE_TTC_2,
	NODE_TTC_3,
	NODE_SATA,
	NODE_ETH_0,
	NODE_ETH_1,
	NODE_ETH_2,
	NODE_ETH_3,
	NODE_UART_0,
	NODE_UART_1,
	NODE_SPI_0,
	NODE_SPI_1,
	NODE_I2C_0,
	NODE_I2C_1,
	NODE_SD_0,
	NODE_SD_1,
	NODE_DP,
	NODE_GDMA,
	NODE_ADMA,
	NODE_NAND,
	NODE_QSPI,
	NODE_GPIO,
	NODE_CAN_0,
	NODE_CAN_1,
	NODE_EXTERN,
	NODE_APLL,
	NODE_VPLL,
	NODE_DPLL,
	NODE_RPLL,
	NODE_IOPLL,
	NODE_DDR,
	NODE_IPI_APU,
	NODE_IPI_RPU_0,
	NODE_GPU,
	NODE_PCIE,
	NODE_PCAP,
	NODE_RTC,
	NODE_LPD,
	NODE_VCU,
	NODE_IPI_RPU_1,
	NODE_IPI_PL_0,
	NODE_IPI_PL_1,
	NODE_IPI_PL_2,
	NODE_IPI_PL_3,
	NODE_PL,
	NODE_GEM_TSU,
	NODE_SWDT_0,
	NODE_SWDT_1,
	NODE_CSU,
	NODE_PJTAG,
	NODE_TRACE,
	NODE_TESTSCAN,
	NODE_PMU,
	NODE_MAX,
};

enum pm_request_ack {
	REQ_ACK_NO = 1,
	REQ_ACK_BLOCKING,
	REQ_ACK_NON_BLOCKING,
};

enum pm_abort_reason {
	ABORT_REASON_WKUP_EVENT = 100,
	ABORT_REASON_PU_BUSY,
	ABORT_REASON_NO_PWRDN,
	ABORT_REASON_UNKNOWN,
};

enum pm_suspend_reason {
	SUSPEND_REASON_PU_REQ = 201,
	SUSPEND_REASON_ALERT,
	SUSPEND_REASON_SYS_SHUTDOWN,
};

enum pm_ram_state {
	PM_RAM_STATE_OFF = 1,
	PM_RAM_STATE_RETENTION,
	PM_RAM_STATE_ON,
};

enum pm_opchar_type {
	PM_OPCHAR_TYPE_POWER = 1,
	PM_OPCHAR_TYPE_TEMP,
	PM_OPCHAR_TYPE_LATENCY,
};

/**
 * @PM_RET_SUCCESS:		success
 * @PM_RET_ERROR_ARGS:		illegal arguments provided (deprecated)
 * @PM_RET_ERROR_NOTSUPPORTED:	feature not supported  (deprecated)
 * @PM_RET_ERROR_NOT_ENABLED:	feature is not enabled
 * @PM_RET_ERROR_INTERNAL:	internal error
 * @PM_RET_ERROR_CONFLICT:	conflict
 * @PM_RET_ERROR_ACCESS:	access rights violation
 * @PM_RET_ERROR_INVALID_NODE:	invalid node
 * @PM_RET_ERROR_DOUBLE_REQ:	duplicate request for same node
 * @PM_RET_ERROR_ABORT_SUSPEND:	suspend procedure has been aborted
 * @PM_RET_ERROR_TIMEOUT:	timeout in communication with PMU
 * @PM_RET_ERROR_NODE_USED:	node is already in use
 */
enum pm_ret_status {
	PM_RET_SUCCESS = (0U),
	PM_RET_ERROR_ARGS = (1U),
	PM_RET_ERROR_NOTSUPPORTED = (4U),
	PM_RET_ERROR_NOT_ENABLED = (29U),
	PM_RET_ERROR_INTERNAL = (2000U),
	PM_RET_ERROR_CONFLICT = (2001U),
	PM_RET_ERROR_ACCESS = (2002U),
	PM_RET_ERROR_INVALID_NODE = (2003U),
	PM_RET_ERROR_DOUBLE_REQ = (2004U),
	PM_RET_ERROR_ABORT_SUSPEND = (2005U),
	PM_RET_ERROR_TIMEOUT = (2006U),
	PM_RET_ERROR_NODE_USED = (2007U),
	PM_RET_ERROR_NO_FEATURE = (2008U)
};

/**
 * @PM_INITIAL_BOOT:	boot is a fresh system startup
 * @PM_RESUME:		boot is a resume
 * @PM_BOOT_ERROR:	error, boot cause cannot be identified
 */
enum pm_boot_status {
	PM_INITIAL_BOOT,
	PM_RESUME,
	PM_BOOT_ERROR,
};

/**
 * @PMF_SHUTDOWN_TYPE_SHUTDOWN:		shutdown
 * @PMF_SHUTDOWN_TYPE_RESET:		reset/reboot
 * @PMF_SHUTDOWN_TYPE_SETSCOPE_ONLY:	set the shutdown/reboot scope
 */
enum pm_shutdown_type {
	PMF_SHUTDOWN_TYPE_SHUTDOWN,
	PMF_SHUTDOWN_TYPE_RESET,
	PMF_SHUTDOWN_TYPE_SETSCOPE_ONLY,
};

/**
 * @PMF_SHUTDOWN_SUBTYPE_SUBSYSTEM:	shutdown/reboot APU subsystem only
 * @PMF_SHUTDOWN_SUBTYPE_PS_ONLY:	shutdown/reboot entire PS (but not PL)
 * @PMF_SHUTDOWN_SUBTYPE_SYSTEM:	shutdown/reboot entire system
 */
enum pm_shutdown_subtype {
	PMF_SHUTDOWN_SUBTYPE_SUBSYSTEM,
	PMF_SHUTDOWN_SUBTYPE_PS_ONLY,
	PMF_SHUTDOWN_SUBTYPE_SYSTEM,
};

/**
 * @PM_PLL_PARAM_DIV2:		Enable for divide by 2 function inside the PLL
 * @PM_PLL_PARAM_FBDIV:		Feedback divisor integer portion for the PLL
 * @PM_PLL_PARAM_DATA:		Feedback divisor fractional portion for the PLL
 * @PM_PLL_PARAM_PRE_SRC:	Clock source for PLL input
 * @PM_PLL_PARAM_POST_SRC:	Clock source for PLL Bypass mode
 * @PM_PLL_PARAM_LOCK_DLY:	Lock circuit config settings for lock windowsize
 * @PM_PLL_PARAM_LOCK_CNT:	Lock circuit counter setting
 * @PM_PLL_PARAM_LFHF:		PLL loop filter high frequency capacitor control
 * @PM_PLL_PARAM_CP:		PLL charge pump control
 * @PM_PLL_PARAM_RES:		PLL loop filter resistor control
 */
enum pm_pll_param {
	PM_PLL_PARAM_DIV2,
	PM_PLL_PARAM_FBDIV,
	PM_PLL_PARAM_DATA,
	PM_PLL_PARAM_PRE_SRC,
	PM_PLL_PARAM_POST_SRC,
	PM_PLL_PARAM_LOCK_DLY,
	PM_PLL_PARAM_LOCK_CNT,
	PM_PLL_PARAM_LFHF,
	PM_PLL_PARAM_CP,
	PM_PLL_PARAM_RES,
	PM_PLL_PARAM_MAX,
};

/**
 * @PM_PLL_MODE_RESET:		PLL is in reset (not locked)
 * @PM_PLL_MODE_INTEGER:	PLL is locked in integer mode
 * @PM_PLL_MODE_FRACTIONAL:	PLL is locked in fractional mode
 */
enum pm_pll_mode {
	PM_PLL_MODE_RESET,
	PM_PLL_MODE_INTEGER,
	PM_PLL_MODE_FRACTIONAL,
	PM_PLL_MODE_MAX,
};

/**
 * @PM_CLOCK_DIV0_ID:		Clock divider 0
 * @PM_CLOCK_DIV1_ID:		Clock divider 1
 */
enum pm_clock_div_id {
	PM_CLOCK_DIV0_ID,
	PM_CLOCK_DIV1_ID,
};

/**
 * EM API IDs
 */
enum em_api_id {
	EM_SET_ACTION = 1,
	EM_REMOVE_ACTION,
	EM_SEND_ERRORS,
};

#endif /* PM_DEFS_H */
