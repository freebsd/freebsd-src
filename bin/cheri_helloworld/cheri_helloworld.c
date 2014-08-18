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

#include <sys/cdefs.h>

#if !__has_feature(capabilities)
#error "This code requires a CHERI-aware compiler"
#endif

#include <sys/types.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <cheri/sandbox.h>

#include <cheritest-helper.h>
#include <err.h>
#include <stdio.h>
#include <sysexits.h>

static void
cheritest_helloworld(struct sandbox_object *objectp)
{
	register_t v;

	v = sandbox_object_cinvoke(objectp,
	    CHERITEST_HELPER_OP_CS_HELLOWORLD, 0, 0, 0, 0, 0, 0, 0,
            sandbox_object_getsystemobject(objectp).co_codecap,
            sandbox_object_getsystemobject(objectp).co_datacap,
            cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
            cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	assert(v == 123456);
}

int
main(int argc __unused, char *argv[] __unused)
{
	struct sandbox_class *classp;
	struct sandbox_object *objectp;

	if (sandbox_class_new("/usr/libexec/cheritest-helper.bin",
	    4*1024*1024, &classp) < 0)
		err(EX_OSFILE, "sandbox_class_new");
	if (sandbox_object_new(classp, &objectp) < 0)
		err(EX_OSFILE, "sandbox_object_new");
	(void)sandbox_class_method_declare(classp,
	    CHERITEST_HELPER_OP_CS_HELLOWORLD, "helloworld");

	cheritest_helloworld(objectp);

	sandbox_object_destroy(objectp);
	sandbox_class_destroy(classp);

	return (0);
}
