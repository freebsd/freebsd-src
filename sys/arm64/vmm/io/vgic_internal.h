/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2018 Alexandru Elisei <alexandru.elisei@gmail.com>
 * Copyright (C) 2020-2022 Andrew Turner
 * Copyright (C) 2023 Arm Ltd
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VGIC_INTERNAL_H_
#define	_VGIC_INTERNAL_H_

struct vgic_v3_irq {
	/* List of IRQs that are active or pending */
	TAILQ_ENTRY(vgic_v3_irq) act_pend_list;
	struct mtx irq_spinmtx;
	uint64_t mpidr;
	int target_vcpu;
	uint32_t irq;
	bool active;
	bool pending;
	bool enabled;
	bool level;
	bool on_aplist;
	uint8_t priority;
	uint8_t config;
#define	VGIC_CONFIG_MASK	0x2
#define	VGIC_CONFIG_LEVEL	0x0
#define	VGIC_CONFIG_EDGE	0x2
};

/* Global data not needed by EL2 */
struct vgic_v3 {
	struct mtx 	dist_mtx;
	uint64_t 	dist_start;
	size_t   	dist_end;

	uint64_t 	redist_start;
	size_t 		redist_end;

	uint32_t 	gicd_ctlr;	/* Distributor Control Register */

	struct vgic_v3_irq *irqs;
};

/* Per-CPU data not needed by EL2 */
struct vgic_v3_cpu {
	/*
	 * We need a mutex for accessing the list registers because they are
	 * modified asynchronously by the virtual timer.
	 *
	 * Note that the mutex *MUST* be a spin mutex because an interrupt can
	 * be injected by a callout callback function, thereby modifying the
	 * list registers from a context where sleeping is forbidden.
	 */
	struct mtx	lr_mtx;

	struct vgic_v3_irq *private_irqs;
	TAILQ_HEAD(, vgic_v3_irq) irq_act_pend;
	u_int		ich_lr_used;
};

typedef void (register_read)(struct hypctx *, u_int, uint64_t *, void *);
typedef void (register_write)(struct hypctx *, u_int, u_int, u_int,
    uint64_t, void *);

register_read vgic_zero_read;
register_write vgic_ignore_write;

#define	VGIC_8_BIT	(1 << 0)
/* (1 << 1) is reserved for 16 bit accesses */
#define	VGIC_32_BIT	(1 << 2)
#define	VGIC_64_BIT	(1 << 3)

struct vgic_register {
	u_int start;	/* Start within a memory region */
	u_int end;
	u_int size;
	u_int flags;
	register_read *read;
	register_write *write;
};

#define	VGIC_REGISTER_RANGE(reg_start, reg_end, reg_size, reg_flags, readf, \
    writef)								\
{									\
	.start = (reg_start),						\
	.end = (reg_end),						\
	.size = (reg_size),						\
	.flags = (reg_flags),						\
	.read = (readf),						\
	.write = (writef),						\
}

#define	VGIC_REGISTER_RANGE_RAZ_WI(reg_start, reg_end, reg_size, reg_flags) \
	VGIC_REGISTER_RANGE(reg_start, reg_end, reg_size, reg_flags,	\
	    vgic_zero_read, vgic_ignore_write)

#define	VGIC_REGISTER(start_addr, reg_size, reg_flags, readf, writef)	\
	VGIC_REGISTER_RANGE(start_addr, (start_addr) + (reg_size),	\
	    reg_size, reg_flags, readf, writef)

#define	VGIC_REGISTER_RAZ_WI(start_addr, reg_size, reg_flags)		\
	VGIC_REGISTER_RANGE_RAZ_WI(start_addr,				\
	    (start_addr) + (reg_size), reg_size, reg_flags)

bool vgic_register_read(struct hypctx *, struct vgic_register *, u_int, u_int,
    u_int, uint64_t *, void *);
bool vgic_register_write(struct hypctx *, struct vgic_register *, u_int, u_int,
    u_int, uint64_t, void *);

#endif /* _VGIC_INTERNAL_H_ */
