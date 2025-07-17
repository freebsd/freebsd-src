/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VHPET_H_
#define	_VHPET_H_

#define	VHPET_BASE	0xfed00000
#define	VHPET_SIZE	1024

#ifdef _KERNEL
struct vm_snapshot_meta;

struct vhpet *vhpet_init(struct vm *vm);
void 	vhpet_cleanup(struct vhpet *vhpet);
int	vhpet_mmio_write(struct vcpu *vcpu, uint64_t gpa, uint64_t val,
	    int size, void *arg);
int	vhpet_mmio_read(struct vcpu *vcpu, uint64_t gpa, uint64_t *val,
	    int size, void *arg);
int	vhpet_getcap(struct vm_hpet_cap *cap);
#ifdef BHYVE_SNAPSHOT
int	vhpet_snapshot(struct vhpet *vhpet, struct vm_snapshot_meta *meta);
int	vhpet_restore_time(struct vhpet *vhpet);
#endif

#endif /* _KERNEL */

#endif	/* _VHPET_H_ */
