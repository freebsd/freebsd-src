/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Arm Ltd
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * Portions of this software were developed by Andrew Turner under sponsorship
 * from the FreeBSD Foundation.
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

#ifndef _ARM64_ARM_SPE_DEV_H_
#define _ARM64_ARM_SPE_DEV_H_

#include <sys/mutex.h>
#include <sys/taskqueue.h>

#include <vm/vm.h>

#include <arm64/spe/arm_spe.h>

#include <dev/hwt/hwt_context.h>

#define        ARM_SPE_DEBUG
#undef ARM_SPE_DEBUG

#ifdef ARM_SPE_DEBUG
#define		dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define		dprintf(fmt, ...)
#endif

DECLARE_CLASS(arm_spe_driver);

struct cdev;
struct resource;

extern bool arm64_pid_in_contextidr;

int spe_register(device_t dev);
void arm_spe_disable(void *arg __unused);
int spe_backend_disable_smp(struct hwt_context *ctx);
void arm_spe_send_buffer(void *arg, int pending __unused);

/*
  PSB CSYNC is a Profiling Synchronization Barrier encoded in the hint space
 * so it is a NOP on earlier architecture.
 */
#define		psb_csync()	__asm __volatile("hint #17" ::: "memory");

struct arm_spe_softc {
	device_t                dev;

	struct resource         *sc_irq_res;
	void                    *sc_irq_cookie;
	struct cdev             *sc_cdev;
	struct mtx              sc_lock;
	struct task             task;

	int64_t                sc_pmsidr;
	int                     kqueue_fd;
	struct thread           *hwt_td;
	struct arm_spe_info     *spe_info;
	struct hwt_context      *ctx;
	STAILQ_HEAD(, arm_spe_queue) pending;
	uint64_t                npending;

	uint64_t                pmbidr;
	uint64_t                pmsidr;

	uint16_t                kva_align;
};

struct arm_spe_buf_info {
	struct arm_spe_info     *info;
	uint64_t                pmbptr;
	uint8_t                 buf_idx : 1;
	bool                    buf_svc : 1;
	bool                    buf_wait : 1;
	bool                    partial_rec : 1;
};

struct arm_spe_info {
	int                     ident; /* tid or cpu_id */
	struct mtx              lock;
	struct arm_spe_softc    *sc;
	struct task             task[2];
	bool                    enabled : 1;

	/* buffer is split in half as a ping-pong buffer */
	vm_object_t             bufobj;
	vm_offset_t             kvaddr;
	size_t                  buf_size;
	uint8_t                 buf_idx : 1; /* 0 = first half of buf, 1 = 2nd half */
	struct arm_spe_buf_info buf_info[2];

	/* config */
	enum arm_spe_profiling_level level;
	enum arm_spe_ctx_field  ctx_field;
	/* filters */
	uint64_t                pmsfcr;
	uint64_t                pmsevfr;
	uint64_t                pmslatfr;
	/* interval */
	uint64_t                pmsirr;
	uint64_t                pmsicr;
	/* control */
	uint64_t                pmscr;
};

struct arm_spe_queue {
	int             ident;
	u_int           buf_idx  : 1;
	bool            partial_rec : 1;
	bool            final_buf : 1;
	vm_offset_t     offset;
	STAILQ_ENTRY(arm_spe_queue) next;
};

static inline vm_offset_t buf_start_addr(u_int buf_idx, struct arm_spe_info *info)
{
	vm_offset_t addr;
	if (buf_idx == 0)
		addr = info->kvaddr;
	if (buf_idx == 1)
	addr = info->kvaddr + (info->buf_size/2);

	return (addr);
}

static inline vm_offset_t buf_end_addr(u_int buf_idx, struct arm_spe_info *info)
{
	vm_offset_t addr;
	if (buf_idx == 0)
		addr = info->kvaddr + (info->buf_size/2);
	if (buf_idx == 1)
		addr = info->kvaddr + info->buf_size;

	return (addr);
}

#endif /* _ARM64_ARM_SPE_DEV_H_ */
