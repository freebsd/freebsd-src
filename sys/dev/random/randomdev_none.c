/*-
 * Copyright (c) 2015 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/random.h>
#include <sys/systm.h>

#include <dev/random/randomdev.h>

#include "opt_random.h"

#if defined(RANDOM_DUMMY) || defined(RANDOM_YARROW)
#error "Cannot define any of RANDOM_DUMMY and RANDOM_YARROW without 'device random'"
#endif

/*-
 * Dummy "not even here" device. Stub out all routines that the kernel would need.
 */

/* ARGSUSED */
u_int
read_random(void *random_buf __unused, u_int len __unused)
{

	return (0);
}

/* ARGSUSED */
void
random_harvest_direct(const void *entropy __unused, u_int count __unused, u_int bits __unused, enum random_entropy_source origin __unused)
{
}

/* ARGSUSED */
void
random_harvest_queue(const void *entropy __unused, u_int count __unused, u_int bits __unused, enum random_entropy_source origin __unused)
{
}

/* ARGSUSED */
void
random_harvest_fast(const void *entropy __unused, u_int count __unused, u_int bits __unused, enum random_entropy_source origin __unused)
{
}
