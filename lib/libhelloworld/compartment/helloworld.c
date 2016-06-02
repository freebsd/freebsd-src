/*-
 * Copyright (c) 2014-2016 Robert N. M. Watson
 * Copyright (c) 2015 SRI International
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

#include <sys/types.h>
#include <sys/stat.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <cheri/cheri_enter.h>
#include <cheri/cheri_fd.h>
#include <cheri/cheri_invoke.h>
#include <cheri/cheri_system.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define HELLOWORLD_COMPARTMENT
#include "../helloworld.h"

void	invoke(void);
void	invoke(void) { abort(); }

static char hello_world_str[] = "hello world";
static char hello_world_str_nl[] = "hello world\n";

int
call_cheri_system_helloworld(void)
{

	return (cheri_system_helloworld());
}

int
call_cheri_system_puts(void)
{
	__capability char *hello_world_str_c;

	hello_world_str_c = cheri_ptrperm(&hello_world_str,
	    sizeof(hello_world_str), CHERI_PERM_LOAD); /* Nul-terminated. */

	return (cheri_system_puts(hello_world_str_c));
}

int
call_cheri_fd_write_c(struct cheri_object fd_object)
{
	__capability char *hello_world_buf_c;
	size_t len_buf_c;

	len_buf_c = strlen(hello_world_str_nl);
	hello_world_buf_c = cheri_ptrperm(&hello_world_str_nl, len_buf_c,
	    CHERI_PERM_LOAD); /* Just the text. */
	return (cheri_fd_write_c(fd_object,
	    hello_world_buf_c, len_buf_c).cfr_retval0);
}
