/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 */

#ifndef __NVMECONTROL_H__
#define __NVMECONTROL_H__

#include <dev/nvme/nvme.h>
#include "comnd.h"

typedef void (*print_fn_t)(const struct nvme_controller_data *cdata, void *buf, uint32_t size);

struct logpage_function {
        SLIST_ENTRY(logpage_function)   link;
	uint8_t		log_page;
	const char     *vendor;
	const char     *name;
	print_fn_t	print_fn;
	size_t		size;
};

#define NVME_LOGPAGE(unique, lp, vend, nam, fn, sz)			\
	static struct logpage_function unique ## _lpf = {		\
		.log_page = lp,						\
		.vendor = vend,						\
		.name = nam,						\
		.print_fn = fn, 					\
		.size = sz,						\
	} ;								\
        static void logpage_reg_##unique(void) __attribute__((constructor)); \
        static void logpage_reg_##unique(void) { logpage_register(&unique##_lpf); }

#define DEFAULT_SIZE	(4096)
struct kv_name {
	uint32_t key;
	const char *name;
};

/*
 * Generically convert little endian to host endian, based on the type of the thing
 * being converted. Use the proposed name for future changes to endian.h.
 */
#define letoh(x)							\
	_Generic(x,							\
	uint8_t: (x),							\
	uint16_t: le16toh(x),						\
	uint32_t: le32toh(x),						\
	uint64_t: le64toh(x))

const char *kv_lookup(const struct kv_name *kv, size_t kv_count, uint32_t key);

void logpage_register(struct logpage_function *p);
#define NVME_CTRLR_PREFIX	"nvme"
#define NVME_NS_PREFIX		"ns"

int open_dev(const char *str, int *fd, int write, int exit_on_error);
void get_nsid(int fd, char **ctrlr_str, uint32_t *nsid);
int read_controller_data(int fd, struct nvme_controller_data *cdata);
int read_namespace_data(int fd, uint32_t nsid, struct nvme_namespace_data *nsdata);
int read_active_namespaces(int fd, uint32_t nsid, struct nvme_ns_list *nslist);
void print_hex(void *data, uint32_t length);
void print_namespace(struct nvme_namespace_data *nsdata);
void read_logpage(int fd, uint8_t log_page, uint32_t nsid, uint8_t lsp,
    uint16_t lsi, uint8_t rae, uint64_t lpo, uint8_t csi, uint8_t ot,
    uint16_t uuid_index, void *payload, uint32_t payload_size);
void print_temp_C(uint16_t t);
void print_temp_K(uint16_t t);
void print_intel_add_smart(const struct nvme_controller_data *cdata __unused, void *buf, uint32_t size __unused);

/* Utility Routines */
/*
 * C23 supports 128-bit integers via _BitInt(128). clang 16 and gcc 13 support
 * this. Older compilers will support 128-bit ints on 64-bit
 * platforms. Otherwise we truncate this to 64-bit on 32-bit systems with older
 * compilers. We also check for > C18 instead of >= C23 because clang 17 was
 * released before the __STDC_VERSION__ was defined.
 */
#define UINT128_DIG	39
#if __STDC_VERSION__ >= 202311L
typedef unsigned _BitInt(128) uint128_t;
#elif defined(__SIZEOF_INT128__)
typedef __uint128_t uint128_t;
#else
typedef uint64_t uint128_t;
#endif

static __inline uint128_t
to128(void *p)
{
#if __STDC_VERSION__ >= 202311L || defined(__SIZEOF_INT128__)
	uint64_t lo, hi;

	lo = le64dec(p);
	hi = le64dec((const char *)p + 8);
	return ((uint128_t)hi << 64 | lo);
#else
	return (le64dec(p));
#endif
}

uint64_t le48dec(const void *pp);
char * uint128_to_str(uint128_t u, char *buf, size_t buflen);
#endif
