/*-
 * Copyright (c) 2006 Li, Xiao <intron@intron.ac>.  All rights reserved.
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

/*
 * Linux Kernel Implementation of Asynchronous I/O
 */

#ifndef	_LINUX_AIO_H_
#define	_LINUX_AIO_H_

typedef unsigned long linux_aio_context_t;

enum {
	LINUX_IOCB_CMD_PREAD = 0,
	LINUX_IOCB_CMD_PWRITE = 1,
	LINUX_IOCB_CMD_FSYNC = 2,
	LINUX_IOCB_CMD_FDSYNC = 3,
#if 0
	LINUX_IOCB_CMD_PREADX = 4,
	LINUX_IOCB_CMD_POLL = 5,
#endif
	LINUX_IOCB_CMD_NOOP = 6,
};

struct linux_io_event {
	uint64_t	data;
	uint64_t	obj;
	int64_t		res;
	int64_t		res2;
};

#if _BYTE_ORDER == _LITTLE_ENDIAN
#define LINUX_AIO_PADDED(x,y)	x,y
#elif _BYTE_ORDER == _BIG_ENDIAN
#define LINUX_AIO_PADDED(x,y)	y,x
#else
#error Unidentified byte order !!!
#endif

struct linux_iocb {
	uint64_t	aio_data;
	uint32_t	LINUX_AIO_PADDED(aio_key, aio_reserved1);

	uint16_t	aio_lio_opcode;
	int16_t		aio_reqprio;
	uint32_t	aio_fildes;

	uint64_t	aio_buf;
	uint64_t	aio_nbytes;
	int64_t		aio_offset;

	uint64_t	aio_reserved2;	/* TODO: use this for a (struct sigevent *) */
	uint64_t	aio_reserved3;

};

/* User space context information structure */
struct linux_aio_ring {
	l_uint	ring_id;
	l_uint	ring_nr;
	l_uint	ring_head;
	l_uint	ring_tail;
#define LINUX_AIO_RING_MAGIC			0xa10a10a1
	l_uint	ring_magic;
#define LINUX_AIO_RING_COMPAT_FEATURES		1
	l_uint	ring_compat_features;
#define LINUX_AIO_RING_INCOMPAT_FEATURES	0
	l_uint	ring_incompat_features;
	l_uint	ring_header_length; /* Size of this structure */

	struct linux_io_event	ring_io_events[0];
};

#endif /* !_LINUX_AIO_H_ */
