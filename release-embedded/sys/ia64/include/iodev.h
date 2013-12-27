/*-
 * Copyright (c) 2010 Marcel Moolenaar
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
 */

#ifndef _MACHINE_IODEV_H_
#define	_MACHINE_IODEV_H_

#include <sys/uuid.h>

#ifdef _KERNEL
#include <machine/bus.h>
#endif

#define	IODEV_EFIVAR_GETVAR	0
#define	IODEV_EFIVAR_NEXTNAME	1
#define	IODEV_EFIVAR_SETVAR	2

struct iodev_efivar_req {
	u_int	access;
	u_int	result;			/* errno value */
	size_t	namesize;
	u_short	*name;			/* UCS-2 */
	struct uuid vendor;
	uint32_t attrib;
	size_t	datasize;
	void	*data;
};

#define	IODEV_EFIVAR	_IOWR('I', 1, struct iodev_efivar_req)

#ifdef _KERNEL
#define	iodev_read_1	bus_space_read_io_1
#define	iodev_read_2	bus_space_read_io_2
#define	iodev_read_4	bus_space_read_io_4
#define	iodev_write_1	bus_space_write_io_1
#define	iodev_write_2	bus_space_write_io_2
#define	iodev_write_4	bus_space_write_io_4

int	 iodev_open(struct thread *td);
int	 iodev_close(struct thread *td);
int	 iodev_ioctl(u_long, caddr_t data);

#endif /* _KERNEL */
#endif /* _MACHINE_IODEV_H_ */
