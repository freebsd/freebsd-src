/*-
 * Copyright (c) 1998 Doug Rabson
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

#ifndef _MACHINE_SWIZ_H_
#define	_MACHINE_SWIZ_H_

/*
 * Macros for accessing device ports or memory in a sparse address space.
 */

#define SPARSE_READ(o)			(*(u_int32_t*) (o))
#define SPARSE_WRITE(o, d)		(*(u_int32_t*) (o) = (d))

#define SPARSE_BYTE_OFFSET(o)		(((o) << 5) | (0 << 3))
#define SPARSE_WORD_OFFSET(o)		(((o) << 5) | (1 << 3))
#define SPARSE_LONG_OFFSET(o)		(((o) << 5) | (3 << 3))

#define SPARSE_BYTE_ADDRESS(base, o)	((base) + SPARSE_BYTE_OFFSET(o))
#define SPARSE_WORD_ADDRESS(base, o)	((base) + SPARSE_WORD_OFFSET(o))
#define SPARSE_LONG_ADDRESS(base, o)	((base) + SPARSE_LONG_OFFSET(o))

#define SPARSE_BYTE_EXTRACT(o, d)	((d) >> (8*((o) & 3)))
#define SPARSE_WORD_EXTRACT(o, d)	((d) >> (8*((o) & 2)))
#define SPARSE_LONG_EXTRACT(o, d)	(d)

#define SPARSE_BYTE_INSERT(o, d)	((d) << (8*((o) & 3)))
#define SPARSE_WORD_INSERT(o, d)	((d) << (8*((o) & 2)))
#define SPARSE_LONG_INSERT(o, d)	(d)

#define SPARSE_READ_BYTE(base, o)	\
	SPARSE_BYTE_EXTRACT(o, SPARSE_READ(base + SPARSE_BYTE_OFFSET(o)))

#define SPARSE_READ_WORD(base, o)	\
	SPARSE_WORD_EXTRACT(o, SPARSE_READ(base + SPARSE_WORD_OFFSET(o)))

#define SPARSE_READ_LONG(base, o)	\
	SPARSE_READ(base + SPARSE_LONG_OFFSET(o))

#define SPARSE_WRITE_BYTE(base, o, d)	\
	SPARSE_WRITE(base + SPARSE_BYTE_OFFSET(o), SPARSE_BYTE_INSERT(o, d))

#define SPARSE_WRITE_WORD(base, o, d)	\
	SPARSE_WRITE(base + SPARSE_WORD_OFFSET(o), SPARSE_WORD_INSERT(o, d))

#define SPARSE_WRITE_LONG(base, o, d)	\
	SPARSE_WRITE(base + SPARSE_LONG_OFFSET(o), d)

#endif /* !_MACHINE_SWIZ_H_ */
