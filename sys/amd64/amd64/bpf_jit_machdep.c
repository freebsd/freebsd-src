/*-
 * Copyright (C) 2002-2003 NetGroup, Politecnico di Torino (Italy)
 * Copyright (C) 2005-2008 Jung-uk Kim <jkim@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bpf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/bpf.h>
#include <net/bpf_jitter.h>

#include <amd64/amd64/bpf_jit_machdep.h>

bpf_filter_func	bpf_jit_compile(struct bpf_insn *, u_int, int *);

/*
 * emit routine to update the jump table
 */
static void
emit_length(bpf_bin_stream *stream, u_int value, u_int len)
{

	(stream->refs)[stream->bpf_pc] += len;
	stream->cur_ip += len;
}

/*
 * emit routine to output the actual binary code
 */
static void
emit_code(bpf_bin_stream *stream, u_int value, u_int len)
{

	switch (len) {
	case 1:
		stream->ibuf[stream->cur_ip] = (u_char)value;
		stream->cur_ip++;
		break;

	case 2:
		*((u_short *)(stream->ibuf + stream->cur_ip)) = (u_short)value;
		stream->cur_ip += 2;
		break;

	case 4:
		*((u_int *)(stream->ibuf + stream->cur_ip)) = value;
		stream->cur_ip += 4;
		break;
	}

	return;
}

/*
 * Function that does the real stuff
 */
bpf_filter_func
bpf_jit_compile(struct bpf_insn *prog, u_int nins, int *mem)
{
	struct bpf_insn *ins;
	u_int i, pass;
	bpf_bin_stream stream;

	/*
	 * NOTE: do not modify the name of this variable, as it's used by
	 * the macros to emit code.
	 */
	emit_func emitm;

	/* Do not compile an empty filter. */
	if (nins == 0)
		return (NULL);

	/* Allocate the reference table for the jumps */
	stream.refs = (u_int *)malloc((nins + 1) * sizeof(u_int),
	    M_BPFJIT, M_NOWAIT);
	if (stream.refs == NULL)
		return (NULL);

	/* Reset the reference table */
	for (i = 0; i < nins + 1; i++)
		stream.refs[i] = 0;

	stream.cur_ip = 0;
	stream.bpf_pc = 0;

	/*
	 * the first pass will emit the lengths of the instructions
	 * to create the reference table
	 */
	emitm = emit_length;

	pass = 0;
	for (;;) {
		ins = prog;

		/* create the procedure header */
		MOVrq2(RBX, R8);
		MOVrq(RDI, RBX);
		MOVrd2(ESI, R9D);
		MOVrd(EDX, EDI);

		for (i = 0; i < nins; i++) {
			stream.bpf_pc++;

			switch (ins->code) {
			default:
				return (NULL);

			case BPF_RET|BPF_K:
				MOVid(ins->k, EAX);
				MOVrq3(R8, RBX);
				RET();
				break;

			case BPF_RET|BPF_A:
				MOVrq3(R8, RBX);
				RET();
				break;

			case BPF_LD|BPF_W|BPF_ABS:
				MOVid(ins->k, ECX);
				MOVrd(ECX, ESI);
				ADDib(sizeof(int32_t), ECX);
				CMPrd(EDI, ECX);
				JBEb(6);
				ZEROrd(EAX);
				MOVrq3(R8, RBX);
				RET();
				MOVobd(RBX, RSI, EAX);
				BSWAP(EAX);
				break;

			case BPF_LD|BPF_H|BPF_ABS:
				ZEROrd(EAX);
				MOVid(ins->k, ECX);
				MOVrd(ECX, ESI);
				ADDib(sizeof(int16_t), ECX);
				CMPrd(EDI, ECX);
				JBEb(4);
				MOVrq3(R8, RBX);
				RET();
				MOVobw(RBX, RSI, AX);
				SWAP_AX();
				break;

			case BPF_LD|BPF_B|BPF_ABS:
				ZEROrd(EAX);
				MOVid(ins->k, ECX);
				CMPrd(EDI, ECX);
				JBEb(4);
				MOVrq3(R8, RBX);
				RET();
				MOVobb(RBX, RCX, AL);
				break;

			case BPF_LD|BPF_W|BPF_LEN:
				MOVrd3(R9D, EAX);
				break;

			case BPF_LDX|BPF_W|BPF_LEN:
				MOVrd3(R9D, EDX);
				break;

			case BPF_LD|BPF_W|BPF_IND:
				MOVid(ins->k, ECX);
				ADDrd(EDX, ECX);
				MOVrd(ECX, ESI);
				ADDib(sizeof(int32_t), ECX);
				CMPrd(EDI, ECX);
				JBEb(6);
				ZEROrd(EAX);
				MOVrq3(R8, RBX);
				RET();
				MOVobd(RBX, RSI, EAX);
				BSWAP(EAX);
				break;

			case BPF_LD|BPF_H|BPF_IND:
				ZEROrd(EAX);
				MOVid(ins->k, ECX);
				ADDrd(EDX, ECX);
				MOVrd(ECX, ESI);
				ADDib(sizeof(int16_t), ECX);
				CMPrd(EDI, ECX);
				JBEb(4);
				MOVrq3(R8, RBX);
				RET();
				MOVobw(RBX, RSI, AX);
				SWAP_AX();
				break;

			case BPF_LD|BPF_B|BPF_IND:
				ZEROrd(EAX);
				MOVid(ins->k, ECX);
				ADDrd(EDX, ECX);
				CMPrd(EDI, ECX);
				JBEb(4);
				MOVrq3(R8, RBX);
				RET();
				MOVobb(RBX, RCX, AL);
				break;

			case BPF_LDX|BPF_MSH|BPF_B:
				MOVid(ins->k, ECX);
				CMPrd(EDI, ECX);
				JBEb(6);
				ZEROrd(EAX);
				MOVrq3(R8, RBX);
				RET();
				ZEROrd(EDX);
				MOVobb(RBX, RCX, DL);
				ANDib(0x0f, DL);
				SHLib(2, EDX);
				break;

			case BPF_LD|BPF_IMM:
				MOVid(ins->k, EAX);
				break;

			case BPF_LDX|BPF_IMM:
				MOVid(ins->k, EDX);
				break;

			case BPF_LD|BPF_MEM:
				MOViq((uintptr_t)mem, RCX);
				MOVid(ins->k * 4, ESI);
				MOVobd(RCX, RSI, EAX);
				break;

			case BPF_LDX|BPF_MEM:
				MOViq((uintptr_t)mem, RCX);
				MOVid(ins->k * 4, ESI);
				MOVobd(RCX, RSI, EDX);
				break;

			case BPF_ST:
				/*
				 * XXX this command and the following could
				 * be optimized if the previous instruction
				 * was already of this type
				 */
				MOViq((uintptr_t)mem, RCX);
				MOVid(ins->k * 4, ESI);
				MOVomd(EAX, RCX, RSI);
				break;

			case BPF_STX:
				MOViq((uintptr_t)mem, RCX);
				MOVid(ins->k * 4, ESI);
				MOVomd(EDX, RCX, RSI);
				break;

			case BPF_JMP|BPF_JA:
				JMP(stream.refs[stream.bpf_pc + ins->k] -
				    stream.refs[stream.bpf_pc]);
				break;

			case BPF_JMP|BPF_JGT|BPF_K:
				if (ins->jt == 0 && ins->jf == 0)
					break;
				CMPid(ins->k, EAX);
				JCC(JA, JBE);
				break;

			case BPF_JMP|BPF_JGE|BPF_K:
				if (ins->jt == 0 && ins->jf == 0)
					break;
				CMPid(ins->k, EAX);
				JCC(JAE, JB);
				break;

			case BPF_JMP|BPF_JEQ|BPF_K:
				if (ins->jt == 0 && ins->jf == 0)
					break;
				CMPid(ins->k, EAX);
				JCC(JE, JNE);
				break;

			case BPF_JMP|BPF_JSET|BPF_K:
				if (ins->jt == 0 && ins->jf == 0)
					break;
				TESTid(ins->k, EAX);
				JCC(JNE, JE);
				break;

			case BPF_JMP|BPF_JGT|BPF_X:
				if (ins->jt == 0 && ins->jf == 0)
					break;
				CMPrd(EDX, EAX);
				JCC(JA, JBE);
				break;

			case BPF_JMP|BPF_JGE|BPF_X:
				if (ins->jt == 0 && ins->jf == 0)
					break;
				CMPrd(EDX, EAX);
				JCC(JAE, JB);
				break;

			case BPF_JMP|BPF_JEQ|BPF_X:
				if (ins->jt == 0 && ins->jf == 0)
					break;
				CMPrd(EDX, EAX);
				JCC(JE, JNE);
				break;

			case BPF_JMP|BPF_JSET|BPF_X:
				if (ins->jt == 0 && ins->jf == 0)
					break;
				TESTrd(EDX, EAX);
				JCC(JNE, JE);
				break;

			case BPF_ALU|BPF_ADD|BPF_X:
				ADDrd(EDX, EAX);
				break;

			case BPF_ALU|BPF_SUB|BPF_X:
				SUBrd(EDX, EAX);
				break;

			case BPF_ALU|BPF_MUL|BPF_X:
				MOVrd(EDX, ECX);
				MULrd(EDX);
				MOVrd(ECX, EDX);
				break;

			case BPF_ALU|BPF_DIV|BPF_X:
				TESTrd(EDX, EDX);
				JNEb(6);
				ZEROrd(EAX);
				MOVrq3(R8, RBX);
				RET();
				MOVrd(EDX, ECX);
				ZEROrd(EDX);
				DIVrd(ECX);
				MOVrd(ECX, EDX);
				break;

			case BPF_ALU|BPF_AND|BPF_X:
				ANDrd(EDX, EAX);
				break;

			case BPF_ALU|BPF_OR|BPF_X:
				ORrd(EDX, EAX);
				break;

			case BPF_ALU|BPF_LSH|BPF_X:
				MOVrd(EDX, ECX);
				SHL_CLrb(EAX);
				break;

			case BPF_ALU|BPF_RSH|BPF_X:
				MOVrd(EDX, ECX);
				SHR_CLrb(EAX);
				break;

			case BPF_ALU|BPF_ADD|BPF_K:
				ADD_EAXi(ins->k);
				break;

			case BPF_ALU|BPF_SUB|BPF_K:
				SUB_EAXi(ins->k);
				break;

			case BPF_ALU|BPF_MUL|BPF_K:
				MOVrd(EDX, ECX);
				MOVid(ins->k, EDX);
				MULrd(EDX);
				MOVrd(ECX, EDX);
				break;

			case BPF_ALU|BPF_DIV|BPF_K:
				MOVrd(EDX, ECX);
				ZEROrd(EDX);
				MOVid(ins->k, ESI);
				DIVrd(ESI);
				MOVrd(ECX, EDX);
				break;

			case BPF_ALU|BPF_AND|BPF_K:
				ANDid(ins->k, EAX);
				break;

			case BPF_ALU|BPF_OR|BPF_K:
				ORid(ins->k, EAX);
				break;

			case BPF_ALU|BPF_LSH|BPF_K:
				SHLib((ins->k) & 0xff, EAX);
				break;

			case BPF_ALU|BPF_RSH|BPF_K:
				SHRib((ins->k) & 0xff, EAX);
				break;

			case BPF_ALU|BPF_NEG:
				NEGd(EAX);
				break;

			case BPF_MISC|BPF_TAX:
				MOVrd(EAX, EDX);
				break;

			case BPF_MISC|BPF_TXA:
				MOVrd(EDX, EAX);
				break;
			}
			ins++;
		}

		pass++;
		if (pass == 2)
			break;

		stream.ibuf = (char *)malloc(stream.cur_ip, M_BPFJIT, M_NOWAIT);
		if (stream.ibuf == NULL) {
			free(stream.refs, M_BPFJIT);
			return (NULL);
		}

		/*
		 * modify the reference table to contain the offsets and
		 * not the lengths of the instructions
		 */
		for (i = 1; i < nins + 1; i++)
			stream.refs[i] += stream.refs[i - 1];

		/* Reset the counters */
		stream.cur_ip = 0;
		stream.bpf_pc = 0;

		/* the second pass creates the actual code */
		emitm = emit_code;
	}

	/*
	 * the reference table is needed only during compilation,
	 * now we can free it
	 */
	free(stream.refs, M_BPFJIT);

	return ((bpf_filter_func)stream.ibuf);
}
