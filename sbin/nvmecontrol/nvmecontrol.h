/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 2012-2013 Intel Corporation
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

#ifndef __NVMECONTROL_H__
#define __NVMECONTROL_H__

#include <dev/nvme/nvme.h>

typedef void (*nvme_fn_t)(int argc, char *argv[]);

struct nvme_function {
	const char	*name;
	nvme_fn_t	fn;
	const char	*usage;
};

#define NVME_CTRLR_PREFIX	"nvme"
#define NVME_NS_PREFIX		"ns"

#define DEVLIST_USAGE							       \
"       nvmecontrol devlist\n"

#define IDENTIFY_USAGE							       \
"       nvmecontrol identify [-x [-v]] <controller id|namespace id>\n"

#define PERFTEST_USAGE							       \
"       nvmecontrol perftest <-n num_threads> <-o read|write>\n"	       \
"                            <-s size_in_bytes> <-t time_in_seconds>\n"	       \
"                            <-i intr|wait> [-f refthread] [-p]\n"	       \
"                            <namespace id>\n"

#define RESET_USAGE							       \
"       nvmecontrol reset <controller id>\n"

#define LOGPAGE_USAGE							       \
"       nvmecontrol logpage <-p page_id> [-b] [-v vendor] [-x] <controller id|namespace id>\n"  \

#define FIRMWARE_USAGE							       \
"       nvmecontrol firmware [-s slot] [-f path_to_firmware] [-a] <controller id>\n"

#define FORMAT_USAGE							       \
"       nvmecontrol format [-f fmt] [-m mset] [-p pi] [-l pil] [-E] [-C] <controller id|namespace id>\n"

#define POWER_USAGE							       \
"       nvmecontrol power [-l] [-p new-state [-w workload-hint]] <controller id>\n"

#define WDC_USAGE							       \
"       nvmecontrol wdc (cap-diag|drive-log|get-crash-dump|purge|purge-montior)\n"

void devlist(int argc, char *argv[]);
void identify(int argc, char *argv[]);
void perftest(int argc, char *argv[]);
void reset(int argc, char *argv[]);
void logpage(int argc, char *argv[]);
void firmware(int argc, char *argv[]);
void format(int argc, char *argv[]);
void power(int argc, char *argv[]);
void wdc(int argc, char *argv[]);

int open_dev(const char *str, int *fd, int show_error, int exit_on_error);
void parse_ns_str(const char *ns_str, char *ctrlr_str, uint32_t *nsid);
void read_controller_data(int fd, struct nvme_controller_data *cdata);
void read_namespace_data(int fd, uint32_t nsid, struct nvme_namespace_data *nsdata);
void print_hex(void *data, uint32_t length);
void read_logpage(int fd, uint8_t log_page, uint32_t nsid, void *payload,
    uint32_t payload_size);
void gen_usage(struct nvme_function *);
void dispatch(int argc, char *argv[], struct nvme_function *f);

/* Utility Routines */
/*
 * 128-bit integer augments to standard values. On i386 this
 * doesn't exist, so we use 64-bit values. So, on 32-bit i386,
 * you'll get truncated values until someone implement 128bit
 * ints in sofware.
 */
#define UINT128_DIG	39
#ifdef __i386__
typedef uint64_t uint128_t;
#else
typedef __uint128_t uint128_t;
#endif

static __inline uint128_t
to128(void *p)
{
	return *(uint128_t *)p;
}

uint64_t le48dec(const void *pp);
char * uint128_to_str(uint128_t u, char *buf, size_t buflen);

#endif
