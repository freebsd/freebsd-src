/*-
 * Copyright (c) 1995 Bruce D. Evans.
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
 * 3. Neither the name of the author nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	from: FreeBSD: src/sys/i386/include/md_var.h,v 1.40 2001/07/12
 * $FreeBSD: src/sys/sparc64/include/md_var.h,v 1.16.10.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef	_MACHINE_MD_VAR_H_
#define	_MACHINE_MD_VAR_H_

typedef void cpu_block_copy_t(const void *src, void *dst, size_t len);
typedef void cpu_block_zero_t(void *dst, size_t len);

extern	char	tl0_base[];
extern	char	_end[];

extern	long	Maxmem;

extern	vm_offset_t kstack0;
extern	vm_paddr_t kstack0_phys;

struct	pcpu;
struct	md_utrap;

void	cpu_identify(u_long vers, u_int clock, u_int id);
void	cpu_setregs(struct pcpu *pc);
int	is_physical_memory(vm_paddr_t addr);
struct md_utrap *utrap_alloc(void);
void	utrap_free(struct md_utrap *ut);
struct md_utrap *utrap_hold(struct md_utrap *ut);

cpu_block_copy_t spitfire_block_copy;
cpu_block_zero_t spitfire_block_zero;

extern	cpu_block_copy_t *cpu_block_copy;
extern	cpu_block_zero_t *cpu_block_zero;

/*
 * Given that the Sun disk label only uses 16-bit fields for cylinders,
 * heads and sectors we might need to adjust the geometry of large IDE
 * disks.
 * We have to have a knowledge that a device_t is a struct device * here
 * to avoid including too many things from this file.
 */
struct disk;
struct device;
void sparc64_ad_firmware_geom_adjust(struct device *dev, struct disk *disk);
#define	ad_firmware_geom_adjust(dev, dsk)				\
	sparc64_ad_firmware_geom_adjust(dev, dsk)

#endif /* !_MACHINE_MD_VAR_H_ */
