/*-
 * Copyright (c) 2012-2014 Robert N. M. Watson
 * Copyright (c) 2014 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#if !__has_feature(capabilities)
#error "This code requires a CHERI-aware compiler"
#endif

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <machine/cheri.h>
#include <machine/cheric.h>
#include <machine/cpuregs.h>

#include <cheri/cheri_fd.h>
#include <cheri/sandbox.h>

#include <cheritest-helper.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "cheritest.h"

void
test_fault_overrun(void)
{
#define	ARRAY_LEN	2
	char array[ARRAY_LEN];
	__capability char *arrayp = cheri_ptr(array, sizeof(array));
	int i;

	for (i = 0; i < ARRAY_LEN; i++)
		arrayp[i] = 0;
	arrayp[i] = 0;
}

void
test_fault_ccheck_user_fail(void)
{
	__capability void *cp;
	char ch;

	cp = cheri_ptrperm(&ch, sizeof(ch), 0);
	cheri_ccheckperm(cp, CHERI_PERM_USER0);
}

void
test_nofault_ccheck_user_pass(void)
{
	__capability void *cp;
	char ch;

	cp = cheri_ptrperm(&ch, sizeof(ch), CHERI_PERM_USER0);
	cheri_ccheckperm(cp, CHERI_PERM_USER0);
}

void
test_fault_cgetcause(void)
{
	register_t cause;

	cause = cheri_getcause();
	printf("CP2 cause register: %ju\n", (uintmax_t)cause);
}

void
test_fault_read_kr1c(void)
{

	CHERI_CAPREG_PRINT(27);
}

void
test_fault_read_kr2c(void)
{

	CHERI_CAPREG_PRINT(28);
}

void
test_fault_read_kcc(void)
{

	CHERI_CAPREG_PRINT(29);
}

void
test_fault_read_kdc(void)
{

	CHERI_CAPREG_PRINT(30);
}

void
test_fault_read_epcc(void)
{

	CHERI_CAPREG_PRINT(31);
}
