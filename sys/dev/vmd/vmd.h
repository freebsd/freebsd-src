/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Alexander Motin <mav@FreeBSD.org>
 * Copyright 2019 Cisco Systems, Inc.
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
 */

#ifndef __VMD_PRIVATE_H__
#define	__VMD_PRIVATE_H__

#include <dev/pci/pcib_private.h>

struct vmd_irq_user {
	LIST_ENTRY(vmd_irq_user)	viu_link;
	device_t			viu_child;
	int				viu_vector;
};

struct vmd_irq {
	struct resource			*vi_res;
	int				vi_rid;
	int				vi_irq;
	void				*vi_handle;
	int				vi_nusers;
};

struct vmd_softc {
	struct pcib_softc		psc;

#define VMD_MAX_BAR		3
	int				vmd_regs_rid[VMD_MAX_BAR];
	struct resource			*vmd_regs_res[VMD_MAX_BAR];
	struct vmd_irq			*vmd_irq;
	LIST_HEAD(,vmd_irq_user)	vmd_users;
	int				vmd_fist_vector;
	int				vmd_msix_count;
	uint8_t				vmd_bus_start;
	uint8_t				vmd_bus_end;
	bus_dma_tag_t			vmd_dma_tag;
};

#endif
