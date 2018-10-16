/*-
 * Copyright (c) 2017 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef __DEV_NVDIMM_VAR_H__
#define	__DEV_NVDIMM_VAR_H__

typedef uint32_t nfit_handle_t;

struct nvdimm_dev {
	device_t	nv_dev;
	nfit_handle_t	nv_handle;
	uint64_t	**nv_flush_addr;
	int		nv_flush_addr_cnt;
	int		nv_devs_idx;
};

enum SPA_mapping_type {
	SPA_TYPE_VOLATILE_MEMORY	= 0,
	SPA_TYPE_PERSISTENT_MEMORY	= 1,
	SPA_TYPE_CONTROL_REGION		= 2,
	SPA_TYPE_DATA_REGION		= 3,
	SPA_TYPE_VOLATILE_VIRTUAL_DISK	= 4,
	SPA_TYPE_VOLATILE_VIRTUAL_CD	= 5,
	SPA_TYPE_PERSISTENT_VIRTUAL_DISK= 6,
	SPA_TYPE_PERSISTENT_VIRTUAL_CD	= 7,
};

struct SPA_mapping {
	enum SPA_mapping_type	spa_type;
	int			spa_domain;
	int			spa_nfit_idx;
	uint64_t		spa_phys_base;
	uint64_t		spa_len;
	uint64_t		spa_efi_mem_flags;
	void			*spa_kva;
	struct cdev		*spa_dev;
	struct g_geom		*spa_g;
	struct g_provider	*spa_p;
	struct bio_queue_head	spa_g_queue;
	struct mtx		spa_g_mtx;
	struct mtx		spa_g_stat_mtx;
	struct devstat		*spa_g_devstat;
	struct proc		*spa_g_proc;
	struct vm_object	*spa_obj;
	bool			spa_g_proc_run;
	bool			spa_g_proc_exiting;
};

struct nvdimm_ns_walk_ctx {
	ACPI_STATUS	(*func)(ACPI_HANDLE, void *);
	void		*arg;
};

extern struct SPA_mapping *spa_mappings;
extern int spa_mappings_cnt;

MALLOC_DECLARE(M_NVDIMM);

struct nvdimm_dev *nvdimm_find_by_handle(nfit_handle_t nv_handle);
int nvdimm_iterate_nfit(ACPI_TABLE_NFIT *nfitbl, enum AcpiNfitType type,
    int (*cb)(void *, void *), void *arg);

#endif		/* __DEV_NVDIMM_VAR_H__ */
