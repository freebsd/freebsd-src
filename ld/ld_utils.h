/*-
 * Copyright (c) 2012,2013 Kai Wang
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
 * $Id: ld_utils.h 2908 2013-02-03 06:06:01Z kaiwang27 $
 */

#define	READ_16(P,V)					\
	do {						\
		if (lo->lo_endian == ELFDATA2MSB)	\
			READ_16BE(P, V);		\
		else					\
			READ_16LE(P, V);		\
	} while (0)

#define	READ_32(P,V)					\
	do {						\
		if (lo->lo_endian == ELFDATA2MSB)	\
			READ_32BE(P, V);		\
		else					\
			READ_32LE(P, V);		\
	} while (0)

#define	READ_64(P,V)					\
	do {						\
		if (lo->lo_endian == ELFDATA2MSB)	\
			READ_64BE(P, V);		\
		else					\
			READ_64LE(P, V);		\
	} while (0)

#define READ_16BE(P,V)					\
	do {						\
		(V) = ((P)[0] << 8) | (P)[1];		\
	} while (0)

#define READ_32BE(P,V)							\
	do {								\
		(V) = ((unsigned)(P)[0] << 24) | ((P)[1] << 16) |	\
		    ((P)[2] << 8) | (P)[3];				\
	} while (0)

#define	READ_64BE(P,V)					\
	do {						\
		(V) = ((uint64_t)(P)[0] << 56) |	\
		    ((uint64_t)(P)[1] << 48) |		\
		    ((uint64_t)(P)[2] << 40) |		\
		    ((uint64_t)(P)[3] << 32) | 		\
		    ((uint64_t)(P)[4] << 24) |		\
		    ((uint64_t)(P)[5] << 16) |		\
		    ((uint64_t)(P)[6] << 8) | (P)[7];	\
	} while (0)

#define READ_16LE(P,V)					\
	do {						\
		(V) = ((P)[1] << 8) | (P)[0];		\
	} while (0)

#define READ_32LE(P,V)							\
	do {								\
		(V) = ((unsigned)(P)[3] << 24) | ((P)[2] << 16) |	\
		    ((P)[1] << 8) | (P)[0];				\
	} while (0)

#define	READ_64LE(P,V)							\
	do {								\
		(V) = ((uint64_t)(P)[7] << 56) |	\
		    ((uint64_t)(P)[6] << 48) |		\
		    ((uint64_t)(P)[5] << 40) |		\
		    ((uint64_t)(P)[4] << 32) | 		\
		    ((uint64_t)(P)[3] << 24) |		\
		    ((uint64_t)(P)[2] << 16) |		\
		    ((uint64_t)(P)[1] << 8) | (P)[0];	\
	} while (0)

#define	WRITE_8(P,V)					\
	do {						\
		*(P) = (V) & 0xff;			\
	} while (0)

#define	WRITE_16(P,V)					\
	do {						\
		if (lo->lo_endian == ELFDATA2MSB)	\
			WRITE_16BE(P, V);		\
		else					\
			WRITE_16LE(P, V);		\
	} while (0)

#define	WRITE_32(P,V)					\
	do {						\
		if (lo->lo_endian == ELFDATA2MSB)	\
			WRITE_32BE(P, V);		\
		else					\
			WRITE_32LE(P, V);		\
	} while (0)

#define	WRITE_64(P,V)					\
	do {						\
		if (lo->lo_endian == ELFDATA2MSB)	\
			WRITE_64BE(P, V);		\
		else					\
			WRITE_64LE(P, V);		\
	} while (0)

#define	WRITE_16BE(P,V)					\
	do {						\
		(P)[0] = ((V) >> 8) & 0xff;             \
		(P)[1] = (V) & 0xff;                    \
	} while (0)

#define	WRITE_32BE(P,V)					\
	do {						\
		(P)[0] = ((V) >> 24) & 0xff;            \
		(P)[1] = ((V) >> 16) & 0xff;            \
		(P)[2] = ((V) >> 8) & 0xff;             \
		(P)[3] = (V) & 0xff;                    \
	} while (0)

#define	WRITE_64BE(P,V)					\
	do {						\
		WRITE_32BE((P),(V) >> 32);		\
		WRITE_32BE((P) + 4, (V) & 0xffffffffU);	\
	} while (0)

#define	WRITE_16LE(P,V)					\
	do {						\
		(P)[0] = (V) & 0xff;			\
		(P)[1] = ((V) >> 8) & 0xff;		\
	} while (0)

#define	WRITE_32LE(P,V)					\
	do {						\
		(P)[0] = (V) & 0xff;			\
		(P)[1] = ((V) >> 8) & 0xff;		\
		(P)[2] = ((V) >> 16) & 0xff;		\
		(P)[3] = ((V) >> 24) & 0xff;		\
	} while (0)

#define	WRITE_64LE(P,V)					\
	do {						\
		WRITE_32LE((P), (V) & 0xffffffffU);	\
		WRITE_32LE((P) + 4, (V) >> 32);		\
	} while (0)
