/* $FreeBSD$ */
/*
 * Copyright (c) 2001 by Matthew Jacob
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Alternatively, this software may be distributed under the terms of the
 * the GNU Public License ("GPL", Library, Version 2).
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Matthew Jacob <mjacob@feral.com)
 *
 */
/*
 * ioctl definitions for Qlogic FC/SCSI HBA driver
 */
#define	ISP_IOC		(021)	/* 'Ctrl-Q' */

/*
 * This ioctl sets/retrieves the debugging level for this hba instance.
 * Note that this is not a simple integer level- see ispvar.h for definitions.
 *
 * The arguments is a pointer to an integer with the new debugging level.
 * The old value is written into this argument.
 */

#define	ISP_SDBLEV	_IOWR(ISP_IOC, 0, int)

/*
 * This ioctl resets the HBA. Use with caution.
 */
#define	ISP_RESETHBA	_IO(ISP_IOC, 1)

/*
 * This ioctl performs a fibre chanel rescan.
 */
#define	ISP_FC_RESCAN	_IO(ISP_IOC, 2)

/*
 * Initiate a LIP
 */
#define	ISP_FC_LIP	_IO(ISP_IOC, 3)

/*
 * Return the Port Database structure for the named device, or ENODEV if none.
 * Caller fills in virtual loopid (0..255), aka 'target'. The driver returns
 * ENODEV (if nothing valid there) or the actual loopid (for local loop devices
 * only), 24 bit Port ID and Node and Port WWNs.
 */
struct isp_fc_device {
	u_int32_t	loopid;	/* 0..255 */
	u_int32_t	portid;	/* 24 bit Port ID */
	u_int64_t	node_wwn;
	u_int64_t	port_wwn;
};

#define	ISP_FC_GETDINFO	_IOWR(ISP_IOC, 4, struct isp_fc_device)
