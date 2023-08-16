/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_C4XXX_RESET_H_
#define ADF_C4XXX_RESET_H_

#include <adf_accel_devices.h>
#include <adf_common_drv.h>
#include "adf_c4xxx_hw_data.h"

/* IA2IOSFSB register definitions */
#define ADF_C4XXX_IA2IOSFSB_PORTCMD (0x60000 + 0x1320)
#define ADF_C4XXX_IA2IOSFSB_LOADD (0x60000 + 0x1324)
#define ADF_C4XXX_IA2IOSFSB_HIADD (0x60000 + 0x1328)
#define ADF_C4XXX_IA2IOSFSB_DATA(index) ((index)*0x4 + 0x60000 + 0x132C)
#define ADF_C4XXX_IA2IOSFSB_KHOLE (0x60000 + 0x136C)
#define ADF_C4XXX_IA2IOSFSB_STATUS (0x60000 + 0x1370)

/* IOSF-SB Port command definitions */
/* Ethernet controller Port ID */
#define ADF_C4XXX_ETH_PORT_ID 0x61
/* Byte enable */
#define ADF_C4XXX_PORTD_CMD_BE 0xFF
/* Non posted; Only non-posted commands are used */
#define ADF_C4XXX_PORTD_CMD_NP 0x1
/* Number of DWORDs to transfer */
#define ADF_C4XXX_PORTD_CMD_LENDW 0x2
/* Extended header always used */
#define ADF_C4XXX_PORTD_CMD_EH 0x1
/* Address length */
#define ADF_C4XXX_PORTD_CMD_ALEN 0x0
/* Message opcode: Private Register Write Non-Posted or Posted Message*/
#define ADF_C4XXX_MOPCODE 0x07

/* Compute port command based on port ID */
#define ADF_C4XXX_GET_PORT_CMD(port_id)                                        \
	((((port_id)&0xFF) << 24) | (ADF_C4XXX_PORTD_CMD_BE << 16) |           \
	 (ADF_C4XXX_PORTD_CMD_NP << 15) | (ADF_C4XXX_PORTD_CMD_LENDW << 10) |  \
	 (ADF_C4XXX_PORTD_CMD_EH << 9) | (ADF_C4XXX_PORTD_CMD_ALEN << 8) |     \
	 (ADF_C4XXX_MOPCODE))

/* Pending reset event/ack message over IOSF-SB */
#define ADF_C4XXX_IOSFSB_RESET_EVENT BIT(0)
#define ADF_C4XXX_IOSFSB_RESET_ACK BIT(7)

/* Upon an FLR, the PCI_EXP_AERUCS register must be read and we must make sure
 * that not other bit is set excepted the:
 * - UR (Unsupported request) bit<20>
 * - IEUNC (Uncorrectable Internal Error) bit <22>
 */
#define PCIE_C4XXX_VALID_ERR_MASK (~BIT(20) ^ BIT(22))

/* Trigger: trigger an IOSF SB message */
#define ADF_C4XXX_IOSFSB_TRIGGER BIT(0)

/* IOSF-SB status definitions */
/* Response status bits<1:0> definitions
 * 00 = Successful
 * 01 = Unsuccessful
 * 10 = Powered down
 * 11 = Multicast
 */
#define ADF_C4XXX_IA2IOSFSB_STATUS_RTS (BIT(0) | BIT(1))
#define ADF_C4XXX_IA2IOSFSB_STATUS_PEND BIT(6)
/* Allow 100ms polling interval */
#define ADF_C4XXX_IA2IOSFSB_POLLING_INTERVAL 100
/* Allow a maximum of 500ms before timing out */
#define ADF_C4XXX_IA2IOSFSB_POLLING_COUNT 5

/* Ethernet notification polling interval */
#define ADF_C4XXX_MAX_ETH_ACK_ATTEMPT 100
#define ADF_C4XXX_ETH_ACK_POLLING_INTERVAL 10

void adf_c4xxx_dev_restore(struct adf_accel_dev *accel_dev);
void notify_and_wait_ethernet(struct adf_accel_dev *accel_dev);
#endif
