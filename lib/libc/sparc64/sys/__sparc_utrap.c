/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <machine/utrap.h>
#include <machine/sysarch.h>

#include <stdio.h>
#include <stdlib.h>

#include "__sparc_utrap_private.h"

static const char *utrap_msg[] = {
	"reserved",
	"instruction access exception",
	"instruction access error",
	"instruction access protection",
	"illtrap instruction",
	"illegal instruction",
	"privileged opcode",
	"floating point disabled",
	"floating point exception ieee 754",
	"floating point exception other",
	"tag overflow",
	"division by zero",
	"data access exception",
	"data access error",
	"data access protection",
	"memory address not aligned",
	"privileged action",
	"async data error",
	"trap instruction 16",
	"trap instruction 17",
	"trap instruction 18",
	"trap instruction 19",
	"trap instruction 20",
	"trap instruction 21",
	"trap instruction 22",
	"trap instruction 23",
	"trap instruction 24",
	"trap instruction 25",
	"trap instruction 26",
	"trap instruction 27",
	"trap instruction 28",
	"trap instruction 29",
	"trap instruction 30",
	"trap instruction 31",
};

void
__sparc_utrap(struct utrapframe *uf)
{

	switch (uf->uf_type) {
	case UT_FP_EXCEPTION_IEEE_754:
	case UT_FP_EXCEPTION_OTHER:
		__fpu_exception(uf);
		UF_DONE(uf);
		return;
	case UT_ILLEGAL_INSTRUCTION:
	case UT_MEM_ADDRESS_NOT_ALIGNED:
		break;
	case UT_TRAP_INSTRUCTION_16:
		UF_DONE(uf);
		return;
	default:
		break;
	}
	printf("__sparc_utrap: type=%s pc=%#lx npc=%#lx\n",
	    utrap_msg[uf->uf_type], uf->uf_pc, uf->uf_npc);
	abort();
}
