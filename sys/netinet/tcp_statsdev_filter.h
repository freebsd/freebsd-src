/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * This software was developed as part of the tcpstats kernel module.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _NETINET_TCP_STATSDEV_FILTER_H_
#define _NETINET_TCP_STATSDEV_FILTER_H_

#ifdef _KERNEL
#include <sys/param.h>
#include <netinet/in.h>
#else
#include <sys/types.h>
#include <netinet/in.h>
#endif

#include <netinet/tcp_statsdev.h>

/* --- Field group bitmasks --- */
#define	TSR_FIELDS_IDENTITY	0x001
#define	TSR_FIELDS_STATE	0x002
#define	TSR_FIELDS_CONGESTION	0x004
#define	TSR_FIELDS_RTT		0x008
#define	TSR_FIELDS_SEQUENCES	0x010
#define	TSR_FIELDS_COUNTERS	0x020
#define	TSR_FIELDS_TIMERS	0x040
#define	TSR_FIELDS_BUFFERS	0x080
#define	TSR_FIELDS_ECN		0x100
#define	TSR_FIELDS_NAMES	0x200
#define	TSR_FIELDS_ALL		0x3FF
#define	TSR_FIELDS_DEFAULT	0x08F

/* --- Parser API --- */
#define	TSF_PARSE_MAXLEN	512
#define	TSF_PARSE_MAXDIRECTIVES	16
#define	TSF_ERRBUF_SIZE		128

int	tsf_parse_filter_string(const char *input, size_t len,
	    struct tcpstats_filter *out, char *errbuf, size_t errbuflen);

#endif /* _NETINET_TCP_STATSDEV_FILTER_H_ */
