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

	/* Allocate the reference table for the jumps */
	stream.refs = (u_int *)malloc((nins + 1) * sizeof(u_int),
	    M_BPFJIT, M_NOWAIT);
	if (stream.refs == NULL)
		return NULL;

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
		PUSH(RBP);
		MOVrq(RBP, RSP);
		MOVoqd(RBP, -8, ESI);
		MOVoqd(RBP, -12, EDX);
		PUSH(RBX);
		MOVrq(RBX, RDI);

		for (i = 0; i < nins; i++) {
			stream.bpf_pc++;

			switch (ins->code) {
			default:
				return NULL;

			case BPF_RET|BPF_K:
				MOVid(EAX, ins->k);
				POP(RBX);
				LEAVE_RET();
				break;

			case BPF_RET|BPF_A:
				POP(RBX);
				LEAVE_RET();
				break;

			case BPF_LD|BPF_W|BPF_ABS:
				MOVid(ECX, ins->k);
				MOVrd(ESI, ECX);
				ADDib(ECX, sizeof(int));
				CMPodd(ECX, RBP, -12);
				JLEb(5);
				ZERO_EAX();
				POP(RBX);
				LEAVE_RET();
				MOVobd(EAX, RBX, RSI);
				BSWAP(EAX);
				break;

			case BPF_LD|BPF_H|BPF_ABS:
				ZERO_EAX();
				MOVid(ECX, ins->k);
				MOVrd(ESI, ECX);
				ADDib(ECX, sizeof(short));
				CMPodd(ECX, RBP, -12);
				JLEb(3);
				POP(RBX);
				LEAVE_RET();
				MOVobw(AX, RBX, RSI);
				SWAP_AX();
				break;

			case BPF_LD|BPF_B|BPF_ABS:
				ZERO_EAX();
				MOVid(ECX, ins->k);
				CMPodd(ECX, RBP, -12);
				JLEb(3);
				POP(RBX);
				LEAVE_RET();
				MOVobb(AL, RBX, RCX);
				break;

			case BPF_LD|BPF_W|BPF_LEN:
				MOVodd(EAX, RBP, -8);
				break;

			case BPF_LDX|BPF_W|BPF_LEN:
				MOVodd(EDX, RBP, -8);
				break;

			case BPF_LD|BPF_W|BPF_IND:
				MOVid(ECX, ins->k);
				ADDrd(ECX, EDX);
				MOVrd(ESI, ECX);
				ADDib(ECX, sizeof(int));
				CMPodd(ECX, RBP, -12);
				JLEb(5);
				ZERO_EAX();
				POP(RBX);
				LEAVE_RET();
				MOVobd(EAX, RBX, RSI);
				BSWAP(EAX);
				break;

			case BPF_LD|BPF_H|BPF_IND:
				ZERO_EAX();
				MOVid(ECX, ins->k);
				ADDrd(ECX, EDX);
				MOVrd(ESI, ECX);
				ADDib(ECX, sizeof(short));
				CMPodd(ECX, RBP, -12);
				JLEb(3);
				POP(RBX);
				LEAVE_RET();
				MOVobw(AX, RBX, RSI);
				SWAP_AX();
				break;

			case BPF_LD|BPF_B|BPF_IND:
				ZERO_EAX();
				MOVid(ECX, ins->k);
				ADDrd(ECX, EDX);
				CMPodd(ECX, RBP, -12);
				JLEb(3);
				POP(RBX);
				LEAVE_RET();
				MOVobb(AL, RBX, RCX);
				break;

			case BPF_LDX|BPF_MSH|BPF_B:
				MOVid(ECX, ins->k);
				CMPodd(ECX, RBP, -12);
				JLEb(5);
				ZERO_EAX();
				POP(RBX);
				LEAVE_RET();
				ZERO_EDX();
				MOVobb(DL, RBX, RCX);
				ANDib(DL, 0xf);
				SHLib(EDX, 2);
				break;

			case BPF_LD|BPF_IMM:
				MOVid(EAX, ins->k);
				break;

			case BPF_LDX|BPF_IMM:
				MOVid(EDX, ins->k);
				break;

			case BPF_LD|BPF_MEM:
				MOViq(RCX, (uintptr_t)mem);
				MOVid(ESI, ins->k * 4);
				MOVobd(EAX, RCX, RSI);
				break;

			case BPF_LDX|BPF_MEM:
				MOViq(RCX, (uintptr_t)mem);
				MOVid(ESI, ins->k * 4);
				MOVobd(EDX, RCX, RSI);
				break;

			case BPF_ST:
				/*
				 * XXX this command and the following could
				 * be optimized if the previous instruction
				 * was already of this type
				 */
				MOViq(RCX, (uintptr_t)mem);
				MOVid(ESI, ins->k * 4);
				MOVomd(RCX, RSI, EAX);
				break;

			case BPF_STX:
				MOViq(RCX, (uintptr_t)mem);
				MOVid(ESI, ins->k * 4);
				MOVomd(RCX, RSI, EDX);
				break;

			case BPF_JMP|BPF_JA:
				JMP(stream.refs[stream.bpf_pc + ins->k] -
				    stream.refs[stream.bpf_pc]);
				break;

			case BPF_JMP|BPF_JGT|BPF_K:
				CMPid(EAX, ins->k);
				/* 5 is the size of the following JMP */
				JG(stream.refs[stream.bpf_pc + ins->jt] -
				    stream.refs[stream.bpf_pc] + 5 );
				JMP(stream.refs[stream.bpf_pc + ins->jf] -
				    stream.refs[stream.bpf_pc]);
				break;

			case BPF_JMP|BPF_JGE|BPF_K:
				CMPid(EAX, ins->k);
				JGE(stream.refs[stream.bpf_pc + ins->jt] -
				    stream.refs[stream.bpf_pc] + 5);
				JMP(stream.refs[stream.bpf_pc + ins->jf] -
				    stream.refs[stream.bpf_pc]);
				break;

			case BPF_JMP|BPF_JEQ|BPF_K:
				CMPid(EAX, ins->k);
				JE(stream.refs[stream.bpf_pc + ins->jt] -
				    stream.refs[stream.bpf_pc] + 5);
				JMP(stream.refs[stream.bpf_pc + ins->jf] -
				    stream.refs[stream.bpf_pc]);
				break;

			case BPF_JMP|BPF_JSET|BPF_K:
				MOVrd(ECX, EAX);
				ANDid(ECX, ins->k);
				JE(stream.refs[stream.bpf_pc + ins->jf] -
				    stream.refs[stream.bpf_pc] + 5);
				JMP(stream.refs[stream.bpf_pc + ins->jt] -
				    stream.refs[stream.bpf_pc]);
				break;

			case BPF_JMP|BPF_JGT|BPF_X:
				CMPrd(EAX, EDX);
				JA(stream.refs[stream.bpf_pc + ins->jt] -
				    stream.refs[stream.bpf_pc] + 5);
				JMP(stream.refs[stream.bpf_pc + ins->jf] -
				    stream.refs[stream.bpf_pc]);
				break;

			case BPF_JMP|BPF_JGE|BPF_X:
				CMPrd(EAX, EDX);
				JAE(stream.refs[stream.bpf_pc + ins->jt] -
				    stream.refs[stream.bpf_pc] + 5);
				JMP(stream.refs[stream.bpf_pc + ins->jf] -
				    stream.refs[stream.bpf_pc]);
				break;

			case BPF_JMP|BPF_JEQ|BPF_X:
				CMPrd(EAX, EDX);
				JE(stream.refs[stream.bpf_pc + ins->jt] -
				    stream.refs[stream.bpf_pc] + 5);
				JMP(stream.refs[stream.bpf_pc + ins->jf] -
				    stream.refs[stream.bpf_pc]);
				break;

			case BPF_JMP|BPF_JSET|BPF_X:
				MOVrd(ECX, EAX);
				ANDrd(ECX, EDX);
				JE(stream.refs[stream.bpf_pc + ins->jf] -
				    stream.refs[stream.bpf_pc] + 5);
				JMP(stream.refs[stream.bpf_pc + ins->jt] -
				    stream.refs[stream.bpf_pc]);
				break;

			case BPF_ALU|BPF_ADD|BPF_X:
				ADDrd(EAX, EDX);
				break;

			case BPF_ALU|BPF_SUB|BPF_X:
				SUBrd(EAX, EDX);
				break;

			case BPF_ALU|BPF_MUL|BPF_X:
				MOVrd(ECX, EDX);
				MULrd(EDX);
				MOVrd(EDX, ECX);
				break;

			case BPF_ALU|BPF_DIV|BPF_X:
				CMPid(EDX, 0);
				JNEb(5);
				ZERO_EAX();
				POP(RBX);
				LEAVE_RET();
				MOVrd(ECX, EDX);
				ZERO_EDX();
				DIVrd(ECX);
				MOVrd(EDX, ECX);
				break;

			case BPF_ALU|BPF_AND|BPF_X:
				ANDrd(EAX, EDX);
				break;

			case BPF_ALU|BPF_OR|BPF_X:
				ORrd(EAX, EDX);
				break;

			case BPF_ALU|BPF_LSH|BPF_X:
				MOVrd(ECX, EDX);
				SHL_CLrb(EAX);
				break;

			case BPF_ALU|BPF_RSH|BPF_X:
				MOVrd(ECX, EDX);
				SHR_CLrb(EAX);
				break;

			case BPF_ALU|BPF_ADD|BPF_K:
				ADD_EAXi(ins->k);
				break;

			case BPF_ALU|BPF_SUB|BPF_K:
				SUB_EAXi(ins->k);
				break;

			case BPF_ALU|BPF_MUL|BPF_K:
				MOVrd(ECX, EDX);
				MOVid(EDX, ins->k);
				MULrd(EDX);
				MOVrd(EDX, ECX);
				break;

			case BPF_ALU|BPF_DIV|BPF_K:
				MOVrd(ECX, EDX);
				ZERO_EDX();
				MOVid(ESI, ins->k);
				DIVrd(ESI);
				MOVrd(EDX, ECX);
				break;

			case BPF_ALU|BPF_AND|BPF_K:
				ANDid(EAX, ins->k);
				break;

			case BPF_ALU|BPF_OR|BPF_K:
				ORid(EAX, ins->k);
				break;

			case BPF_ALU|BPF_LSH|BPF_K:
				SHLib(EAX, (ins->k) & 255);
				break;

			case BPF_ALU|BPF_RSH|BPF_K:
				SHRib(EAX, (ins->k) & 255);
				break;

			case BPF_ALU|BPF_NEG:
				NEGd(EAX);
				break;

			case BPF_MISC|BPF_TAX:
				MOVrd(EDX, EAX);
				break;

			case BPF_MISC|BPF_TXA:
				MOVrd(EAX, EDX);
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
			return NULL;
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

	return (bpf_filter_func)stream.ibuf;
}
