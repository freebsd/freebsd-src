/*-
 * Copyright (c) 2014 Robert N. M. Watson
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

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <cheri/cheri_enter.h>
#include <cheri/cheri_fd.h>
#include <cheri/cheri_invoke.h>
#include <cheri/cheri_system.h>

#include <stdio.h>
#include <string.h>

#include "cheri_helloworld-helper.h"

int	invoke(register_t op, struct cheri_object system_object,
	    struct cheri_object fd_object);

static char hello_world_str[] = "hello world\n";

/*
 * Print "hello world" in one of three ways, depending on the "op" argument:
 * via the system-class hello-world service, via the system-class puts
 * service, and by writing it to a cheri_fd object passed as an argument.
 */
int
invoke(register_t op, struct cheri_object system_object,
    struct cheri_object fd_object)
{
	__capability char *hello_world_str_c;
	__capability char *hello_world_buf_c;

	/*
	 * Save reference to our system object; NB: stores in a global
	 * variable so this reference can't currently be ephemeral.
	 */
	cheri_system_setup(system_object);

	/*
	 * Construct a capability to our "hello world" string.
	 */
	hello_world_str_c = cheri_ptrperm(&hello_world_str,
	    sizeof(hello_world_str), CHERI_PERM_LOAD); /* Nul-terminated. */
	hello_world_buf_c = cheri_ptrperm(&hello_world_str,
	    strlen(hello_world_str), CHERI_PERM_LOAD); /* Just the text. */

	/*
	 * Select a print method.
	 */
	switch (op) {
	case CHERI_HELLOWORLD_HELPER_OP_HELLOWORLD:
		return (cheri_system_helloworld());

	case CHERI_HELLOWORLD_HELPER_OP_PUTS:
		return (cheri_system_puts(hello_world_str_c));

	case CHERI_HELLOWORLD_HELPER_OP_FD_WRITE_C:
		return (cheri_fd_write_c(fd_object,
		    hello_world_buf_c).cfr_retval0);

	default:
		return (-1);
	}
}
