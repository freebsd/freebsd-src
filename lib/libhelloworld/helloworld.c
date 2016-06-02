/*-
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

#include <cheri/cheri.h>
#include <cheri/sandbox.h>

#include <err.h>
#include <sysexits.h>

#include "helloworld.h"

#define	COMPARTMENT_PATH	"/usr/libcheri/helloworld.co.0"

struct cheri_object	 __helloworld;
struct sandbox_class	*__helloworld_classp;
static struct sandbox_object	*__helloworld_objectp;

__attribute__ ((constructor)) static void
cheri_helloworld_init(void)
{
	
	if (sandbox_class_new(COMPARTMENT_PATH, 0, &__helloworld_classp) < 0)
		err(EX_OSFILE, "sandbox_class_new(%s)", COMPARTMENT_PATH);
	if (sandbox_object_new(__helloworld_classp, 2*1024*1024,
	    &__helloworld_objectp) < 0)
		err(EX_OSFILE, "sandbox_object_new");
	__helloworld = sandbox_object_getobject(__helloworld_objectp);
}

#if 0
void
cheri_helloworld_fini(void)
{

	sandbox_object_destroy(__helloworld_objectp);
	sandbox_class_destroy(__helloworld_classp);
}
#endif
