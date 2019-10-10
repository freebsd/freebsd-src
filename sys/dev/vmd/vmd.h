/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 * $FreeBSD$
 *
 */

#ifndef __VMD_PRIVATE_H__
#define	__VMD_PRIVATE_H__

struct vmd_irq_handler {
	TAILQ_ENTRY(vmd_irq_handler)	vmd_link;
	device_t			vmd_child;
	driver_intr_t			*vmd_intr;
	void				*vmd_arg;
	int				vmd_rid;
};

struct vmd_irq {
	struct resource			*vmd_res;
	int				vmd_rid;
	void 				*vmd_handle;
	struct vmd_softc		*vmd_sc;
	int				vmd_instance;
	TAILQ_HEAD(,vmd_irq_handler)	vmd_list;
};

/*
 * VMD specific data.
 */
struct vmd_softc
{
	device_t		vmd_dev;
	device_t		vmd_child;
	uint32_t		vmd_flags;	/* flags */
#define	PCIB_SUBTRACTIVE	0x1
#define	PCIB_DISABLE_MSI	0x2
#define	PCIB_DISABLE_MSIX	0x4
#define	PCIB_ENABLE_ARI		0x8
#define	PCIB_HOTPLUG		0x10
#define	PCIB_HOTPLUG_CMD_PENDING 0x20
#define	PCIB_DETACH_PENDING	0x40
#define	PCIB_DETACHING		0x80
	u_int			vmd_domain;	/* domain number */
	struct pcib_secbus	vmd_bus;	/* secondary bus numbers */

#define VMD_MAX_BAR         3
	struct resource         *vmd_regs_resource[VMD_MAX_BAR];
	int                     vmd_regs_rid[VMD_MAX_BAR];
	bus_space_handle_t      vmd_bhandle;
	bus_space_tag_t         vmd_btag;
	int                     vmd_io_rid;
	struct resource         *vmd_io_resource;
	void                    *vmd_intr;
	struct vmd_irq          *vmd_irq;
	int			vmd_msix_count;
#ifdef TASK_QUEUE_INTR
	struct taskqueue	*vmd_irq_tq;
	struct task		vmd_irq_task;
#else
	struct mtx		vmd_irq_lock;
#endif
};

#endif
