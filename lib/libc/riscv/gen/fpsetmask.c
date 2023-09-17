/*-
 * Copyright (c) 2020 Axiado
 * All rights reserved.
 *
 * This software was developed by Kristof Provost under
 * sponsorship from Axiado.
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
#include <sys/types.h>
#include <ieeefp.h>

/**
 * RISC-V doesn't support floating-point exceptions: RISC-V Instruction Set
 * Manual: Volume I: User-Level ISA, 11.2 Floating-Point Control and Status
 * Register: "As allowed by the standard, we do not support traps on
 * floating-point exceptions in the base ISA, but instead require explicit
 * checks of the flags in software. We considered adding branches controlled
 * directly by the contents of the floating-point accrued exception flags, but
 * ultimately chose to omit these instructions to keep the ISA simple." 
 *
 * We still need this function, because some applications (notably Perl) call
 * it, but we cannot provide a meaningful implementation.
 **/
fp_except_t
fpsetmask(fp_except_t mask)
{

	return (0);
}
