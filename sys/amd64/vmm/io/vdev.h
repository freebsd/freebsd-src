/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 *
 * $FreeBSD$
 */

#ifndef _VDEV_H_
#define	_VDEV_H_

typedef enum {
	BYTE	= 1,
	WORD	= 2,
	DWORD	= 4,
	QWORD	= 8,
} opsize_t;

typedef enum {
	MMIO_READ = 1,
	MMIO_WRITE = 2,
} region_attr_t;

struct io_region {
	uint64_t	base;
	uint64_t	len;
	region_attr_t	attr;
	int		vcpu;
};

typedef int (*vdev_init_t)(void* dev);
typedef int (*vdev_reset_t)(void* dev);
typedef int (*vdev_halt_t)(void* dev);
typedef int (*vdev_memread_t)(void* dev, uint64_t gpa, opsize_t size, uint64_t *data);
typedef int (*vdev_memwrite_t)(void* dev, uint64_t gpa, opsize_t size, uint64_t data);


struct vdev_ops {
	const char	*name;
	vdev_init_t	init;
	vdev_reset_t	reset;
	vdev_halt_t	halt;
	vdev_memread_t	memread;
	vdev_memwrite_t	memwrite;
};


void vdev_vm_init(void);
void vdev_vm_cleanup(void);

int  vdev_register(struct vdev_ops *ops, void *dev);
void vdev_unregister(void *dev);

int  vdev_register_region(struct vdev_ops *ops, void *dev, struct io_region *io);
void vdev_unregister_region(void *dev, struct io_region *io);

int vdev_init(void);
int vdev_reset(void);
int vdev_halt(void);
int vdev_memread(uint64_t gpa, opsize_t size, uint64_t *data);
int vdev_memwrite(uint64_t gpa, opsize_t size, uint64_t data);

#endif	/* _VDEV_H_ */

