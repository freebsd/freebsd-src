/*
 * SPDX-License-Identifier: CDDL 1.0
 *
 * Copyright 2023 Christos Margiolis <christos@FreeBSD.org>
 */

#include <sys/types.h>
#include <sys/dtrace.h>

#include <machine/riscvreg.h>

#define RVC_MASK 0x03

int
dtrace_instr_size(uint8_t *instr)
{
	/* Detect compressed instructions. */
	if ((~(*instr) & RVC_MASK) == 0)
		return (INSN_SIZE);
	else
		return (INSN_C_SIZE);
}
