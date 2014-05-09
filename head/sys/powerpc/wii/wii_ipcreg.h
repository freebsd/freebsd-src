/*-
 * Copyright (C) 2012 Margarida Gouveia
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_POWERPC_WII_WII_IPCREG_H
#define	_POWERPC_WII_WII_IPCREG_H

#define	WIIIPC_REG_ADDR		0x0d000000
#define	WIIIPC_REG_LEN		0x40
#define	WIIIPC_IOH_ADDR		0x133e0000
#define	WIIIPC_IOH_LEN		0xc20000

#define	WIIIPC_TXBUF		0x00
#define	WIIIPC_CSR		0x04
#define	WIIIPC_CSR_TXSTART	0x01
#define	WIIIPC_CSR_TBEI		0x02
#define	WIIIPC_CSR_RBFI		0x04
#define	WIIIPC_CSR_RXREADY	0x08
#define	WIIIPC_CSR_RBFIMASK	0x10
#define	WIIIPC_CSR_TBEIMASK	0x20
#define	WIIIPC_RXBUF		0x08
#define	WIIIPC_ISR		0x30
#define	WIIIPC_ISR_MAGIC 	0x40000000

enum wiiipc_cmd {
	WIIIPC_CMD_OPEN		= 1,
	WIIIPC_CMD_CLOSE	= 2,
	WIIIPC_CMD_READ		= 3,
	WIIIPC_CMD_WRITE	= 4,
	WIIIPC_CMD_SEEK		= 5,
	WIIIPC_CMD_IOCTL	= 6,
	WIIIPC_CMD_IOCTLV	= 7,
	WIIIPC_CMD_ASYNCRESP	= 8
};

struct wiiipc_ipc_msg {
	uint32_t	ipc_cmd;
	int32_t		ipc_result;
	int32_t	 	ipc_fd;	/* WIIIPC_CMD_ASYNCRESP - the original cmd */
	union {
		struct {
			intptr_t  pathname;
			uint32_t  mode;
		} _ipc_open;
		struct {
			intptr_t  data;
			uint32_t  len;
		} _ipc_read, _ipc_write;
		struct {
			int32_t   offset;
			int32_t   whence;
		} _ipc_seek;
		struct {
			uint32_t  request;
			intptr_t  ibuf;
			uint32_t  ilen;
			intptr_t  obuf;
			uint32_t  olen;
		} _ipc_ioctl;
		struct {
			uint32_t  request;
			uint32_t  argin;
			uint32_t  argout;
			intptr_t  iovec;
		} _ipc_ioctlv;
		uint32_t _ipc_argv[5];
	} args;
} __attribute__((packed));

CTASSERT(sizeof(struct wiiipc_ipc_msg) == 32);

#define	ipc_open 	args._ipc_open
#define	ipc_read	args._ipc_read
#define	ipc_write	args._ipc_write
#define	ipc_ioctl 	args._ipc_ioctl
#define	ipc_ioctlv	args._ipc_ioctlv

#endif	/* _POWERPC_WII_WII_IPCREG_H */
