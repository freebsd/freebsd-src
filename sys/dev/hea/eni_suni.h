/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: src/sys/dev/hea/eni_suni.h,v 1.2 1999/08/28 00:41:46 peter Exp $
 *
 */

/*
 * Efficient ENI Adapter Support
 * -----------------------------
 *
 * Defines for SUNI chip
 *
 */

#ifndef	_ENI_ENI_SUNI_H
#define	_ENI_ENI_SUNI_H

/*
 * Interrupt bits in SUNI Master Interrupt Status Reg
 */
#define	SUNI_RSOPI		0x01
#define	SUNI_RLOPI		0x02
#define	SUNI_RPOPI		0x04
#define	SUNI_RACPI		0x08
#define	SUNI_TACPI		0x10
#define	SUNI_RDOOLI		0x20
#define	SUNI_LCDI		0x40
#define	SUNI_TROOLI		0x80

/*
 * SUNI Register numbers
 */
#define	SUNI_MASTER_REG		0x00		/* Master reset and ID */
#define	SUNI_IS_REG		0x02		/* Master Interrupt Status */
#define	SUNI_CLOCK_REG		0x06		/* Clock synth/control/status */
#define	SUNI_RSOP_REG		0x10		/* RSOP control/Interrupt Status */
#define	SUNI_SECT_BIP_REG	0x12
#define	SUNI_RLOP_REG		0x18		/* RLOP control/Interrupt Status */
#define	SUNI_LINE_BIP_REG	0x1A
#define	SUNI_LINE_FEBE_REG	0x1D
#define	SUNI_RPOP_IS_REG	0x31		/* RPOP Interrupt Status */
#define	SUNI_PATH_BIP_REG	0x38
#define	SUNI_PATH_FEBE_REG	0x3A
#define	SUNI_RACP_REG		0x50		/* RACP control/status */
#define	SUNI_HECS_REG		0x54
#define	SUNI_UHECS_REG		0x55
#define	SUNI_TACP_REG		0x60		/* TACP control/status */

/*
 * Delay timer to allow SUNI statistic registers to load
 */
#define	SUNI_DELAY		10

#endif	/* _ENI_ENI_SUNI_H */

