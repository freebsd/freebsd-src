/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 The FreeBSD Foundation
 *
 * This software was developed by Ka Ho Ng
 * under sponsorship from the FreeBSD Foundation.
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

#ifndef _SYS_SNDSTAT_H_
#define _SYS_SNDSTAT_H_

#include <sys/types.h>
#ifndef _IOWR
#include <sys/ioccom.h>
#endif  /* !_IOWR */

struct sndstat_nvlbuf_arg {
	size_t nbytes;	/* [IN/OUT] buffer size/number of bytes filled */
	void *buf;	/* [OUT] buffer holding a packed nvlist */
};

/*
 * Common labels
 */
#define SNDSTAT_LABEL_DSPS	"dsps"
#define SNDSTAT_LABEL_FROM_USER	"from_user"
#define SNDSTAT_LABEL_PCHAN	"pchan"
#define SNDSTAT_LABEL_RCHAN	"rchan"
#define SNDSTAT_LABEL_PMINRATE	"pminrate"
#define SNDSTAT_LABEL_PMAXRATE	"pmaxrate"
#define SNDSTAT_LABEL_RMINRATE	"rminrate"
#define SNDSTAT_LABEL_RMAXRATE	"rmaxrate"
#define SNDSTAT_LABEL_PFMTS	"pfmts"
#define SNDSTAT_LABEL_RFMTS	"rfmts"
#define SNDSTAT_LABEL_NAMEUNIT	"nameunit"
#define SNDSTAT_LABEL_DEVNODE	"devnode"
#define SNDSTAT_LABEL_DESC	"desc"
#define SNDSTAT_LABEL_PROVIDER	"provider"
#define SNDSTAT_LABEL_PROVIDER_INFO	"provider_info"

/*
 * sound(4)-specific labels
 */
#define SNDSTAT_LABEL_SOUND4_PROVIDER	"sound(4)"
#define SNDSTAT_LABEL_SOUND4_UNIT	"unit"
#define SNDSTAT_LABEL_SOUND4_BITPERFECT	"bitperfect"
#define SNDSTAT_LABEL_SOUND4_PVCHAN	"pvchan"
#define SNDSTAT_LABEL_SOUND4_RVCHAN	"rvchan"

#define SNDSTAT_REFRESH_DEVS	_IO('D', 100)
#define SNDSTAT_GET_DEVS	_IOWR('D', 101, struct sndstat_nvlbuf_arg)
#define SNDSTAT_ADD_USER_DEVS	_IOWR('D', 102, struct sndstat_nvlbuf_arg)
#define SNDSTAT_FLUSH_USER_DEVS	_IO('D', 103)

#ifdef _KERNEL
#ifdef COMPAT_FREEBSD32

struct sndstat_nvlbuf_arg32 {
	uint32_t nbytes;
	uint32_t buf;
};

#define SNDSTAT_GET_DEVS32 \
	_IOC_NEWTYPE(SNDSTAT_GET_DEVS, struct sndstat_nvlbuf_arg32)
#define SNDSTAT_ADD_USER_DEVS32 \
	_IOC_NEWTYPE(SNDSTAT_ADD_USER_DEVS, struct sndstat_nvlbuf_arg32)

#endif
#endif

#endif /* !_SYS_SNDSTAT_H_ */
