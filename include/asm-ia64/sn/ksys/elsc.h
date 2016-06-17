/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997, 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 */
#ifndef _ASM_SN_KSYS_ELSC_H
#define _ASM_SN_KSYS_ELSC_H

#include <linux/config.h>
#include <asm/sn/ksys/l1.h>

/*
 * Error codes
 *
 *   The possible ELSC error codes are a superset of the I2C error codes,
 *   so ELSC error codes begin at -100.
 */

#define ELSC_ERROR_NONE			0

#define ELSC_ERROR_CMD_SEND	       (-100)	/* Error sending command    */
#define ELSC_ERROR_CMD_CHECKSUM	       (-101)	/* Command checksum bad     */
#define ELSC_ERROR_CMD_UNKNOWN	       (-102)	/* Unknown command          */
#define ELSC_ERROR_CMD_ARGS	       (-103)	/* Invalid argument(s)      */
#define ELSC_ERROR_CMD_PERM	       (-104)	/* Permission denied	    */
#define ELSC_ERROR_CMD_STATE	       (-105)	/* not allowed in this state*/

#define ELSC_ERROR_RESP_TIMEOUT	       (-110)	/* ELSC response timeout    */
#define ELSC_ERROR_RESP_CHECKSUM       (-111)	/* Response checksum bad    */
#define ELSC_ERROR_RESP_FORMAT	       (-112)	/* Response format error    */
#define ELSC_ERROR_RESP_DIR	       (-113)	/* Response direction error */

#define ELSC_ERROR_MSG_LOST	       (-120)	/* Queue full; msg. lost    */
#define ELSC_ERROR_LOCK_TIMEOUT	       (-121)	/* ELSC response timeout    */
#define ELSC_ERROR_DATA_SEND	       (-122)	/* Error sending data       */
#define ELSC_ERROR_NIC		       (-123)	/* NIC processing error     */
#define ELSC_ERROR_NVMAGIC	       (-124)	/* Bad magic no. in NVRAM   */
#define ELSC_ERROR_MODULE	       (-125)	/* Moduleid processing err  */

#endif /* _ASM_SN_KSYS_ELSC_H */
