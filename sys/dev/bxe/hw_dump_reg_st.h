/*-
 * Copyright (c) 2007-2011 Broadcom Corporation. All rights reserved.
 *
 *    Gary Zambrano <zambrano@broadcom.com>
 *    David Christensen <davidch@broadcom.com>
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
 * 3. Neither the name of Broadcom Corporation nor the name of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written consent.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

 /*$FreeBSD$*/

#ifndef _HW_DUMP_REG_ST_H
#define	_HW_DUMP_REG_ST_H

#define	BXE_GRCDUMP_BUF_SIZE 	0xE0000

#define	XSTORM_WAITP_ADDRESS    0x2b8a80
#define	TSTORM_WAITP_ADDRESS    0x2b8a80
#define	USTORM_WAITP_ADDRESS    0x2b8a80
#define	CSTORM_WAITP_ADDRESS    0x2b8a80
#define	TSTORM_CAM_MODE         0x1B1440

/* Register address structure. */
typedef struct reg_addr {
	uint32_t	addr;
	uint32_t	size;
}*preg_addr;

/* Wide register address structure. */
typedef struct wreg_addr {
	uint32_t	addr;
	uint32_t	size;
	uint32_t	const_regs_count;
	uint32_t	* const_regs;
}*pwreg_addr;

/* Dump header parameters. */
struct hd_param {
	uint32_t	time_stamp;
	uint32_t	diag_ver;
	uint32_t	grc_dump_ver;
};

/* Global parameters. */
extern struct	wreg_addr wreg_addrs_e1[];
extern struct	reg_addr reg_addrs_e1[];
extern uint32_t	regs_count_e1;
extern uint32_t	wregs_count_e1;
extern struct	hd_param hd_param_e1;

extern struct	wreg_addr wreg_addrs_e1h[];
extern struct	reg_addr reg_addrs_e1h[];
extern uint32_t	regs_count_e1h;
extern uint32_t	wregs_count_e1h;
extern struct	hd_param hd_param_e1h;

#endif //_HW_DUMP_REG_ST_H
