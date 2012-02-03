/*-
 * Copyright (c) 2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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

#ifndef _OPENSOLARIS_SYS_KSTAT_H_
#define	_OPENSOLARIS_SYS_KSTAT_H_

#include <sys/sysctl.h>

#define	KSTAT_TYPE_NAMED	1

#define	KSTAT_FLAG_VIRTUAL	0x01

typedef struct kstat {
	void	*ks_data;
	u_int	 ks_ndata;
#ifdef _KERNEL
	struct sysctl_ctx_list ks_sysctl_ctx;
	struct sysctl_oid *ks_sysctl_root;
#endif
} kstat_t;

typedef struct kstat_named {
#define	KSTAT_STRLEN	31
	char	name[KSTAT_STRLEN];
#define	KSTAT_DATA_CHAR		0
#define	KSTAT_DATA_INT32	1
#define	KSTAT_DATA_UINT32	2
#define	KSTAT_DATA_INT64	3
#define	KSTAT_DATA_UINT64	4
	uchar_t	data_type;
	union {
		uint64_t	ui64;
	} value;
} kstat_named_t;

kstat_t *kstat_create(char *module, int instance, char *name, char *cls,
    uchar_t type, ulong_t ndata, uchar_t flags);
void kstat_install(kstat_t *ksp);
void kstat_delete(kstat_t *ksp);

#endif	/* _OPENSOLARIS_SYS_KSTAT_H_ */
