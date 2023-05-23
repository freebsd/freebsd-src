/*
 * SPDX-License-Identifier: CDDL 1.0
 *
 * Copyright 2023 Christos Margiolis <christos@FreeBSD.org>
 */

#include <sys/types.h>
#include <sys/dtrace.h>

int
dtrace_instr_size(uint8_t *instr __unused)
{
	return (INSN_SIZE);
}
