/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Scott Long
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

#ifndef _TB_IOCTL_H
#define _TB_IOCTL_H

struct tbt_ioc {
	void	*data;	/* user-supplied buffer for the nvlist */
	size_t	size;	/* size of the user-supplied buffer */
	size_t	len;	/* amount of data in the nvlist */
};

#define TBT_NAMLEN	16
#define TBT_DEVICE_NAME "tbtctl"
#define TBT_IOCMAXLEN	4096

#define TBT_DISCOVER	_IOWR('h', 1, struct tbt_ioc)
#define TBT_DISCOVER_TYPE	"type"
#define TBT_DISCOVER_IFACE	"iface"
#define TBT_DISCOVER_DOMAIN	"domain"
#define TBT_DISCOVER_ROUTER	"router"

#define TBT_REQUEST	_IOWR('h', 2, struct tbt_ioc)

#endif /* _TB_IOCTL_H */
