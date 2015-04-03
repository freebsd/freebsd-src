/*-
 * Copyright (c) 2014-2015 Robert N. M. Watson
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

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <cheri/cheri_fd.h>
#include <cheri/sandbox.h>

#include <cheri_helloworld-helper.h>
#include <err.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

struct cheri_object	cheri_helloworld;

int
main(int argc, char *argv[])
{
	struct sandbox_class *classp;
	struct sandbox_object *objectp;
	struct cheri_object stdout_fd;

	if (sandbox_program_init(argc, argv) == -1)
		errx(EX_OSFILE, "sandbox_program_init");

	if (cheri_fd_new(STDOUT_FILENO, &stdout_fd) < 0)
		err(EX_OSFILE, "cheri_fd_new: stdout");
	if (sandbox_class_new("/usr/libexec/cheri_helloworld-helper",
	    4*1024*1024, &classp) < 0)
		err(EX_OSFILE, "sandbox_class_new");
	if (sandbox_program_finalize() == -1)
		errx(EX_SOFTWARE, "sandbox_program_finalize");
	if (sandbox_object_new(classp, 2*1024*1024, &objectp) < 0)
		err(EX_OSFILE, "sandbox_object_new");
	cheri_helloworld = sandbox_object_getobject(objectp);

	int ret;
	ret = call_cheri_system_helloworld();
	assert(ret == 123456);
	ret = call_cheri_system_puts();
	assert(ret >= 0);
	ret = call_cheri_fd_write_c(stdout_fd);
	assert(ret == 12);

	sandbox_object_destroy(objectp);
	sandbox_class_destroy(classp);
	cheri_fd_destroy(stdout_fd);

	return (0);
}
