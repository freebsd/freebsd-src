/*-
 * Copyright (c) 2000,2001 Jonathan Chen.
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
 * $FreeBSD$
 */

/*
 * Cardbus CIS definitions
 */

struct cis_tupleinfo;

int	cardbus_do_cis(device_t, device_t);

#define	MAXTUPLESIZE		0x400

/* BAR */
#define	TPL_BAR_REG_ASI_MASK			0x07
#define	TPL_BAR_REG_AS				0x10
#define	TPL_BAR_REG_PREFETCHABLE_ONLY		0x20
#define	TPL_BAR_REG_PREFETCHABLE_CACHEABLE	0x40
#define	TPL_BAR_REG_PREFETCHABLE		0x60
#define	TPL_BAR_REG_BELOW1MB			0x80

/* CISTPL_FUNC */
#define	TPL_FUNC_MF		0	/* multi function tuple */
#define	TPL_FUNC_MEM		1	/* memory */
#define	TPL_FUNC_SERIAL		2	/* serial, including modem and fax */
#define	TPL_FUNC_PARALLEL	3	/* parallel, including printer and SCSI */
#define	TPL_FUNC_DISK		4	/* Disk */
#define	TPL_FUNC_VIDEO		5	/* Video Adaptor */
#define	TPL_FUNC_LAN		6	/* LAN Adaptor */
#define	TPL_FUNC_AIMS		7	/* Auto Inclement Mass Strages */

/* TPL_FUNC_LAN */
#define	TPL_FUNCE_LAN_TECH	1	/* technology */
#define	TPL_FUNCE_LAN_SPEED	2	/* speed */
#define	TPL_FUNCE_LAN_MEDIA	3	/* which media do you use? */
#define	TPL_FUNCE_LAN_NID	4	/* node id (address) */
#define	TPL_FUNCE_LAN_CONN	5	/* connector type (shape) */

/* TPL_FUNC_SERIAL */
#define	TPL_FUNCE_SER_UART	0	/* UART type */
