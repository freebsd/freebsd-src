/*-
 * Copyright (c) 1999 Jonathan Lemon
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id$
 */

/*
 * #defines and software structures for the Compaq RAID card
 */

/*
 * board register offsets
 */
#define R_CMD_FIFO	0x04
#define R_DONE_FIFO	0x08
#define R_INT_MASK	0x0C
#define R_STATUS	0x10
#define R_INT_PENDING	0x14

/*
 * interrupt mask values 
 */
#define INT_DISABLE	0x00
#define INT_ENABLE	0x01

/*
 * return status codes
 */
#define SOFT_ERROR	0x02
#define HARD_ERROR	0x04
#define CMD_REJECTED	0x14

/*
 * command types 
 */
#define	CMD_GET_LOG_DRV_INFO	0x10
#define	CMD_GET_CTRL_INFO	0x11
#define	CMD_SENSE_DRV_STATUS	0x12
#define	CMD_START_RECOVERY	0x13
#define	CMD_GET_PHYS_DRV_INFO	0x15
#define	CMD_BLINK_DRV_LEDS	0x16
#define	CMD_SENSE_DRV_LEDS	0x17
#define	CMD_GET_LOG_DRV_EXT	0x18
#define	CMD_GET_CTRL_INFO	0x11
#define CMD_READ		0x20
#define CMD_WRITE		0x30
#define CMD_WRITE_MEDIA		0x31
#define CMD_GET_CONFIG		0x50
#define CMD_SET_CONFIG		0x51
#define CMD_FLUSH_CACHE		0xc2

/*
 * command structures
 */
struct ida_drive_info {
	u_int16_t	secsize 	__attribute__ ((packed));
	u_int32_t	secperunit 	__attribute__ ((packed));
	u_int16_t	ncylinders 	__attribute__ ((packed));
	u_int8_t	nheads	 	__attribute__ ((packed));
	u_int8_t	signature 	__attribute__ ((packed));
	u_int8_t	psectors 	__attribute__ ((packed));
	u_int16_t	wprecomp 	__attribute__ ((packed));
	u_int8_t	max_acc 	__attribute__ ((packed));
	u_int8_t	control 	__attribute__ ((packed));
	u_int16_t	pcylinders 	__attribute__ ((packed));
	u_int8_t	ptracks 	__attribute__ ((packed));
	u_int16_t	landing_zone 	__attribute__ ((packed));
	u_int8_t	nsectors 	__attribute__ ((packed));
	u_int8_t	checksum 	__attribute__ ((packed));
	u_int8_t	mirror 		__attribute__ ((packed));
};

struct ida_controller_info {
	u_int8_t	num_drvs 	__attribute__ ((packed));
	u_int32_t	signature 	__attribute__ ((packed));
	u_int8_t	firm_rev[4] 	__attribute__ ((packed));
};
