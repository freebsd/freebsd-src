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
#define RAX	0
#define RCX	1
#define RDX	2
#define RBX	3
#define RSP	4
#define RBP	5
#define RSI	6
#define RDI	7

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

/* movl i32,r32 */
#define MOVid(i32, r32) do {						\
	emitm(&stream, (11 << 4) | (1 << 3) | (r32 & 0x7), 1);		\
	emitm(&stream, i32, 4);						\
} while (0)

/* movq i64,r64 */
#define MOViq(i64, r64) do {						\
	emitm(&stream, 0x48, 1);					\
	emitm(&stream, (11 << 4) | (1 << 3) | (r64 & 0x7), 1);		\
	emitm(&stream, i64, 4);						\
	emitm(&stream, (i64 >> 32), 4);					\
} while (0)

/* movl sr32,dr32 */
#define MOVrd(sr32, dr32) do {						\
	emitm(&stream, (8 << 4) | 3 | (1 << 3), 1);			\
	emitm(&stream,							\
	    (3 << 6) | ((dr32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
} while (0)

/* movq sr64,dr64 */
#define MOVrq(sr64, dr64) do {						\
	emitm(&stream, 0x48, 1);					\
	emitm(&stream, (8 << 4) | 3 | (1 << 3), 1);			\
	emitm(&stream,							\
	    (3 << 6) | ((dr64 & 0x7) << 3) | (sr64 & 0x7), 1);		\
} while (0)

/* movl off(sr64),dr32 */
#define MOVoqd(off, sr64, dr32) do {					\
	emitm(&stream, (8 << 4) | 3 | (1 << 3), 1);			\
	emitm(&stream,							\
	    (1 << 6) | ((dr32 & 0x7) << 3) | (sr64 & 0x7), 1);		\
	emitm(&stream, off, 1);						\
} while (0)

/* movl sr32,off(dr64) */
#define MOVdoq(sr32, off, dr64) do {					\
	emitm(&stream, (8 << 4) | 1 | (1 << 3), 1);			\
	emitm(&stream,							\
	    (1 << 6) | ((sr32 & 0x7) << 3) | (dr64 & 0x7), 1);		\
	emitm(&stream, off, 1);						\
} while (0)

/* movl (sr64,or64,1),dr32 */
#define MOVobd(sr64, or64, dr32) do {					\
	emitm(&stream, (8 << 4) | 3 | (1 << 3), 1);			\
	emitm(&stream, ((dr32 & 0x7) << 3) | 4, 1);			\
	emitm(&stream, ((or64 & 0x7) << 3) | (sr64 & 0x7), 1);		\
} while (0)

/* movw (sr64,or64,1),dr16 */
#define MOVobw(sr64, or64, dr16) do {					\
	emitm(&stream, 0x66, 1);					\
	emitm(&stream, (8 << 4) | 3 | (1 << 3), 1);			\
	emitm(&stream, ((dr16 & 0x7) << 3) | 4, 1);			\
	emitm(&stream, ((or64 & 0x7) << 3) | (sr64 & 0x7), 1);		\
} while (0)

/* movb (sr64,or64,1),dr8 */
#define MOVobb(sr64, or64, dr8) do {					\
	emitm(&stream, 0x8a, 1);					\
	emitm(&stream, ((dr8 & 0x7) << 3) | 4, 1);			\
	emitm(&stream, ((or64 & 0x7) << 3) | (sr64 & 0x7), 1);		\
} while (0)

/* movl sr32,(dr64,or64,1) */
#define MOVomd(sr32, dr64, or64) do {					\
	emitm(&stream, 0x89, 1);					\
	emitm(&stream, ((sr32 & 0x7) << 3) | 4, 1);			\
	emitm(&stream, ((or64 & 0x7) << 3) | (dr64 & 0x7), 1);		\
} while (0)

/* bswapl dr32 */
#define BSWAP(dr32) do {						\
	emitm(&stream, 0xf, 1);						\
	emitm(&stream, (0x19 << 3) | dr32, 1);				\
} while (0)

/* xchgb %al,%ah */
#define SWAP_AX() do {							\
	emitm(&stream, 0x86, 1);					\
	emitm(&stream, 0xc4, 1);					\
} while (0)

/* pushq r64 */
#define PUSH(r64) do {							\
	emitm(&stream, (5 << 4) | (0 << 3) | (r64 & 0x7), 1);		\
} while (0)

/* popq r64 */
#define POP(r64) do {							\
	emitm(&stream, (5 << 4) | (1 << 3) | (r64 & 0x7), 1);		\
} while (0)

/* leaveq/retq */
#define LEAVE_RET() do {						\
	emitm(&stream, 0xc9, 1);					\
	emitm(&stream, 0xc3, 1);					\
} while (0)

/* addl sr32,dr32 */
#define ADDrd(sr32, dr32) do {						\
	emitm(&stream, 0x03, 1);					\
	emitm(&stream,							\
	    (3 << 6) | ((dr32 & 0x7) << 3) | (sr32 & 0x7), 1);	\
} while (0)

/* addl i32,%eax */
#define ADD_EAXi(i32) do {						\
	emitm(&stream, 0x05, 1);					\
	emitm(&stream, i32, 4);						\
} while (0)

/* addl i32,r32 */
#define ADDid(i32, r32) do {						\
	emitm(&stream, 0x81, 1);					\
	emitm(&stream, (24 << 3) | r32, 1);				\
	emitm(&stream, i32, 4);						\
} while (0)

/* addl i8,r32 */
#define ADDib(i8, r32) do {						\
	emitm(&stream, 0x83, 1);					\
	emitm(&stream, (24 << 3) | r32, 1);				\
	emitm(&stream, i8, 1);						\
} while (0)

/* subl sr32,dr32 */
#define SUBrd(sr32, dr32) do {						\
	emitm(&stream, 0x2b, 1);					\
	emitm(&stream,							\
	    (3 << 6) | ((dr32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
} while (0)

/* subl i32,%eax */
#define SUB_EAXi(i32) do {						\
	emitm(&stream, 0x2d, 1);					\
	emitm(&stream, i32, 4);						\
} while (0)

/* mull r32 */
#define MULrd(r32) do {							\
	emitm(&stream, 0xf7, 1);					\
	emitm(&stream, (7 << 5) | (r32 & 0x7), 1);			\
} while (0)

/* divl r32 */
#define DIVrd(r32) do {							\
	emitm(&stream, 0xf7, 1);					\
	emitm(&stream, (15 << 4) | (r32 & 0x7), 1);			\
} while (0)

/* andb i8,r8 */
#define ANDib(i8, r8) do {						\
	emitm(&stream, 0x80, 1);					\
	emitm(&stream, (7 << 5) | r8, 1);				\
	emitm(&stream, i8, 1);						\
} while (0)

/* andl i32,r32 */
#define ANDid(i32, r32) do {						\
	if (r32 == EAX) {						\
		emitm(&stream, 0x25, 1);				\
		emitm(&stream, i32, 4);					\
	} else {							\
		emitm(&stream, 0x81, 1);				\
		emitm(&stream, (7 << 5) | r32, 1);			\
		emitm(&stream, i32, 4);					\
	}								\
} while (0)

/* andl sr32,dr32 */
#define ANDrd(sr32, dr32) do {						\
	emitm(&stream, 0x23, 1);					\
	emitm(&stream,							\
	    (3 << 6) | ((dr32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
} while (0)

/* orl sr32,dr32 */
#define ORrd(sr32, dr32) do {						\
	emitm(&stream, 0x0b, 1);					\
	emitm(&stream,							\
	    (3 << 6) | ((dr32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
} while (0)

/* orl i32,r32 */
#define ORid(i32, r32) do {						\
	if (r32 == EAX) {						\
		emitm(&stream, 0x0d, 1);				\
		emitm(&stream, i32, 4);					\
	} else {							\
		emitm(&stream, 0x81, 1);				\
		emitm(&stream, (25 << 3) | r32, 1);			\
		emitm(&stream, i32, 4);					\
	}								\
} while (0)

/* shll i8,r32 */
#define SHLib(i8, r32) do {						\
	emitm(&stream, 0xc1, 1);					\
	emitm(&stream, (7 << 5) | (r32 & 0x7), 1);			\
	emitm(&stream, i8, 1);						\
} while (0)

/* shll %cl,dr32 */
#define SHL_CLrb(dr32) do {						\
	emitm(&stream, 0xd3, 1);					\
	emitm(&stream, (7 << 5) | (dr32 & 0x7), 1);			\
} while (0)

/* shrl i8,r32 */
#define SHRib(i8, r32) do {						\
	emitm(&stream, 0xc1, 1);					\
	emitm(&stream, (29 << 3) | (r32 & 0x7), 1);			\
	emitm(&stream, i8, 1);						\
} while (0)

/* shrl %cl,dr32 */
#define SHR_CLrb(dr32) do {						\
	emitm(&stream, 0xd3, 1);					\
	emitm(&stream, (29 << 3) | (dr32 & 0x7), 1);			\
} while (0)

/* negl r32 */
#define NEGd(r32) do {							\
	emitm(&stream, 0xf7, 1);					\
	emitm(&stream, (27 << 3) | (r32 & 0x7), 1);			\
} while (0)

/* cmpl off(sr64),dr32 */
#define CMPoqd(off, sr64, dr32) do {					\
	emitm(&stream, (3 << 4) | 3 | (1 << 3), 1);			\
	emitm(&stream,							\
	    (1 << 6) | ((dr32 & 0x7) << 3) | (sr64 & 0x7), 1);		\
	emitm(&stream, off, 1);						\
} while (0)

/* cmpl sr32,dr32 */
#define CMPrd(sr32, dr32) do {						\
	emitm(&stream, 0x3b, 1);					\
	emitm(&stream,							\
	    (3 << 6) | ((dr32 & 0x7) << 3) | (sr32 & 0x7), 1);		\
} while (0)

/* cmpl i32,dr32 */
#define CMPid(i32, dr32) do {						\
	if (dr32 == EAX){						\
		emitm(&stream, 0x3d, 1);				\
		emitm(&stream, i32, 4);					\
	} else {							\
		emitm(&stream, 0x81, 1);				\
		emitm(&stream, (0x1f << 3) | (dr32 & 0x7), 1);		\
		emitm(&stream, i32, 4);					\
	}								\
} while (0)

/* jne off8 */
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

/* xorl %eax,%eax */
#define ZERO_EAX() do {							\
	emitm(&stream, 0x31, 1);					\
	emitm(&stream, 0xc0, 1);					\
} while (0)

/* xorl %edx,%edx */
#define ZERO_EDX() do {							\
	emitm(&stream, 0x31, 1);					\
	emitm(&stream, 0xd2, 1);					\
} while (0)

#endif	/* _BPF_JIT_MACHDEP_H_ */
