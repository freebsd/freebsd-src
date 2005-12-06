/*-
 * Copyright (c) 2002 - 2003 NetGroup, Politecnico di Torino (Italy)
 * Copyright (c) 2005 Jung-uk Kim <jkim@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Politecnico di Torino nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS intERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _BPF_JIT_MACHDEP_H_
#define _BPF_JIT_MACHDEP_H_

/*
 * Registers
 */
#define EAX	0
#define ECX	1
#define EDX	2
#define EBX	3
#define ESP	4
#define EBP	5
#define ESI	6
#define EDI	7

#define AX	0
#define CX	1
#define DX	2
#define BX	3
#define SP	4
#define BP	5
#define SI	6
#define DI	7

#define AL	0
#define CL	1
#define DL	2
#define BL	3

/* A stream of native binary code.*/
typedef struct bpf_bin_stream {
	/* Current native instruction pointer. */
	int		cur_ip;

	/*
	 * Current BPF instruction pointer, i.e. position in
	 * the BPF program reached by the jitter.
	 */
	int		bpf_pc;

	/* Instruction buffer, contains the generated native code. */
	char		*ibuf;

	/* Jumps reference table. */
	u_int		*refs;
} bpf_bin_stream;

/*
 * Prototype of the emit functions.
 *
 * Different emit functions are used to create the reference table and
 * to generate the actual filtering code. This allows to have simpler
 * instruction macros.
 * The first parameter is the stream that will receive the data.
 * The second one is a variable containing the data.
 * The third one is the length, that can be 1, 2, or 4 since it is possible
 * to emit a byte, a short, or a word at a time.
 */
typedef void (*emit_func)(bpf_bin_stream *stream, u_int value, u_int n);

/*
 * native Instruction Macros
 */

/* mov r32,i32 */
#define MOVid(r32, i32) do {						\
	emitm(&stream, (11 << 4) | (1 << 3) | (r32 & 0x7), 1);		\
	emitm(&stream, i32, 4);						\
} while (0)

/* mov dr32,sr32 */
#define MOVrd(dr32, sr32) do {						\
	emitm(&stream, (8 << 4) | 3 | (1 << 3), 1);			\
	emitm(&stream,							\
	    (3 << 6) | ((dr32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
} while (0)

/* mov dr32,sr32[off] */
#define MOVodd(dr32, sr32, off) do {					\
	emitm(&stream, (8 << 4) | 3 | (1 << 3), 1);			\
	emitm(&stream,							\
	    (1 << 6) | ((dr32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
	emitm(&stream, off, 1);						\
} while (0)

/* mov dr32,sr32[or32] */
#define MOVobd(dr32, sr32, or32) do {					\
	emitm(&stream, (8 << 4) | 3 | (1 << 3), 1);			\
	emitm(&stream, ((dr32 & 0x7) << 3) | 4, 1);			\
	emitm(&stream, ((or32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
} while (0)

/* mov dr16,sr32[or32] */
#define MOVobw(dr32, sr32, or32) do {					\
	emitm(&stream, 0x66, 1);					\
	emitm(&stream, (8 << 4) | 3 | (1 << 3), 1);			\
	emitm(&stream, ((dr32 & 0x7) << 3) | 4, 1);			\
	emitm(&stream, ((or32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
} while (0)

/* mov dr8,sr32[or32] */
#define MOVobb(dr8, sr32, or32) do {					\
	emitm(&stream, 0x8a, 1);					\
	emitm(&stream, ((dr8 & 0x7) << 3) | 4, 1);			\
	emitm(&stream, ((or32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
} while (0)

/* mov [dr32][or32],sr32 */
#define MOVomd(dr32, or32, sr32) do {					\
	emitm(&stream, 0x89, 1);					\
	emitm(&stream, ((sr32 & 0x7) << 3) | 4, 1);			\
	emitm(&stream, ((or32 & 0x7) << 3) | (dr32 & 0x7), 1);		\
} while (0)

/* bswap dr32 */
#define BSWAP(dr32) do {						\
	emitm(&stream, 0xf, 1);						\
	emitm(&stream, (0x19 << 3) | dr32, 1);				\
} while (0)

/* xchg al,ah */
#define SWAP_AX() do {							\
	emitm(&stream, 0x86, 1);					\
	emitm(&stream, 0xc4, 1);					\
} while (0)

/* push r32 */
#define PUSH(r32) do {							\
	emitm(&stream, (5 << 4) | (0 << 3) | (r32 & 0x7), 1);		\
} while (0)

/* pop r32 */
#define POP(r32) do {							\
	emitm(&stream, (5 << 4) | (1 << 3) | (r32 & 0x7), 1);		\
} while (0)

/* leave/ret */
#define LEAVE_RET() do {						\
	emitm(&stream, 0xc9, 1);					\
	emitm(&stream, 0xc3, 1);					\
} while (0)

/* add dr32,sr32 */
#define ADDrd(dr32, sr32) do {						\
	emitm(&stream, 0x03, 1);					\
	emitm(&stream,							\
	    (3 << 6) | ((dr32 & 0x7) << 3) | (sr32 & 0x7), 1);	\
} while (0)

/* add eax,i32 */
#define ADD_EAXi(i32) do {						\
	emitm(&stream, 0x05, 1);					\
	emitm(&stream, i32, 4);						\
} while (0)

/* add r32,i32 */
#define ADDid(r32, i32) do {						\
	emitm(&stream, 0x81, 1);					\
	emitm(&stream, (24 << 3) | r32, 1);				\
	emitm(&stream, i32, 4);						\
} while (0)

/* add r32,i8 */
#define ADDib(r32, i8) do {						\
	emitm(&stream, 0x83, 1);					\
	emitm(&stream, (24 << 3) | r32, 1);				\
	emitm(&stream, i8, 1);						\
} while (0)

/* sub dr32,sr32 */
#define SUBrd(dr32, sr32) do {						\
	emitm(&stream, 0x2b, 1);					\
	emitm(&stream,							\
	    (3 << 6) | ((dr32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
} while (0)

/* sub eax,i32 */
#define SUB_EAXi(i32) do {						\
	emitm(&stream, 0x2d, 1);					\
	emitm(&stream, i32, 4);						\
} while (0)

/* mul r32 */
#define MULrd(r32) do {							\
	emitm(&stream, 0xf7, 1);					\
	emitm(&stream, (7 << 5) | (r32 & 0x7), 1);			\
} while (0)

/* div r32 */
#define DIVrd(r32) do {							\
	emitm(&stream, 0xf7, 1);					\
	emitm(&stream, (15 << 4) | (r32 & 0x7), 1);			\
} while (0)

/* and r8,i8 */
#define ANDib(r8, i8) do {						\
	emitm(&stream, 0x80, 1);					\
	emitm(&stream, (7 << 5) | r8, 1);				\
	emitm(&stream, i8, 1);						\
} while (0)

/* and r32,i32 */
#define ANDid(r32, i32) do {						\
	if (r32 == EAX) {						\
		emitm(&stream, 0x25, 1);				\
		emitm(&stream, i32, 4);					\
	} else {							\
		emitm(&stream, 0x81, 1);				\
		emitm(&stream, (7 << 5) | r32, 1);			\
		emitm(&stream, i32, 4);					\
	}								\
} while (0)

/* and dr32,sr32 */
#define ANDrd(dr32, sr32) do {						\
	emitm(&stream, 0x23, 1);					\
	emitm(&stream,							\
	    (3 << 6) | ((dr32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
} while (0)

/* or dr32,sr32 */
#define ORrd(dr32, sr32) do {						\
	emitm(&stream, 0x0b, 1);					\
	emitm(&stream,							\
	    (3 << 6) | ((dr32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
} while (0)

/* or r32,i32 */
#define ORid(r32, i32) do {						\
	if (r32 == EAX) {						\
		emitm(&stream, 0x0d, 1);				\
		emitm(&stream, i32, 4);					\
	} else {							\
		emitm(&stream, 0x81, 1);				\
		emitm(&stream, (25 << 3) | r32, 1);			\
		emitm(&stream, i32, 4);					\
	}								\
} while (0)

/* shl r32,i8 */
#define SHLib(r32, i8) do {						\
	emitm(&stream, 0xc1, 1);					\
	emitm(&stream, (7 << 5) | (r32 & 0x7), 1);			\
	emitm(&stream, i8, 1);						\
} while (0)

/* shl dr32,cl */
#define SHL_CLrb(dr32) do {						\
	emitm(&stream, 0xd3, 1);					\
	emitm(&stream, (7 << 5) | (dr32 & 0x7), 1);			\
} while (0)

/* shr r32,i8 */
#define SHRib(r32, i8) do {						\
	emitm(&stream, 0xc1, 1);					\
	emitm(&stream, (29 << 3) | (r32 & 0x7), 1);			\
	emitm(&stream, i8, 1);						\
} while (0)

/* shr dr32,cl */
#define SHR_CLrb(dr32) do {						\
	emitm(&stream, 0xd3, 1);					\
	emitm(&stream, (29 << 3) | (dr32 & 0x7), 1);			\
} while (0)

/* neg r32 */
#define NEGd(r32) do {							\
	emitm(&stream, 0xf7, 1);					\
	emitm(&stream, (27 << 3) | (r32 & 0x7), 1);			\
} while (0)

/* cmp dr32,sr32[off] */
#define CMPodd(dr32, sr32, off) do {					\
	emitm(&stream, (3 << 4) | 3 | (1 << 3), 1);			\
	emitm(&stream,							\
	    (1 << 6) | ((dr32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
	emitm(&stream, off, 1);						\
} while (0)

/* cmp dr32,sr32 */
#define CMPrd(dr32, sr32) do {						\
	emitm(&stream, 0x3b, 1);					\
	emitm(&stream,							\
	    (3 << 6) | ((dr32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
} while (0)

/* cmp dr32,i32 */
#define CMPid(dr32, i32) do {						\
	if (dr32 == EAX){						\
		emitm(&stream, 0x3d, 1);				\
		emitm(&stream, i32, 4);					\
	} else {							\
		emitm(&stream, 0x81, 1);				\
		emitm(&stream, (0x1f << 3) | (dr32 & 0x7), 1);		\
		emitm(&stream, i32, 4);					\
	}								\
} while (0)

/* jne off32 */
#define JNEb(off8) do {							\
	emitm(&stream, 0x75, 1);					\
	emitm(&stream, off8, 1);					\
} while (0)

/* je off32 */
#define JE(off32) do {							\
	emitm(&stream, 0x0f, 1);					\
	emitm(&stream, 0x84, 1);					\
	emitm(&stream, off32, 4);					\
} while (0)

/* jle off32 */
#define JLE(off32) do {							\
	emitm(&stream, 0x0f, 1);					\
	emitm(&stream, 0x8e, 1);					\
	emitm(&stream, off32, 4);					\
} while (0)

/* jle off8 */
#define JLEb(off8) do {							\
	emitm(&stream, 0x7e, 1);					\
	emitm(&stream, off8, 1);					\
} while (0)

/* ja off32 */
#define JA(off32) do {							\
	emitm(&stream, 0x0f, 1);					\
	emitm(&stream, 0x87, 1);					\
	emitm(&stream, off32, 4);					\
} while (0)

/* jae off32 */
#define JAE(off32) do {							\
	emitm(&stream, 0x0f, 1);					\
	emitm(&stream, 0x83, 1);					\
	emitm(&stream, off32, 4);					\
} while (0)

/* jg off32 */
#define JG(off32) do {							\
	emitm(&stream, 0x0f, 1);					\
	emitm(&stream, 0x8f, 1);					\
	emitm(&stream, off32, 4);					\
} while (0)

/* jge off32 */
#define JGE(off32) do {							\
	emitm(&stream, 0x0f, 1);					\
	emitm(&stream, 0x8d, 1);					\
	emitm(&stream, off32, 4);					\
} while (0)

/* jmp off32 */
#define JMP(off32) do {							\
	emitm(&stream, 0xe9, 1);					\
	emitm(&stream, off32, 4);					\
} while (0)

/* xor eax,eax */
#define ZERO_EAX() do {							\
	emitm(&stream, 0x31, 1);					\
	emitm(&stream, 0xc0, 1);					\
} while (0)

#endif	/* _BPF_JIT_MACHDEP_H_ */
