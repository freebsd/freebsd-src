/*-
 * Copyright (c) 2012 SRI International
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
#include <sys/capsicum.h>
#include <sys/mman.h>

#include <machine/sysarch.h>

#include <err.h>
#include <png.h>
#include <stdlib.h>

#include "imagebox.h"
#include "iboxpriv.h"

int
main(int argc, char **argv __unused)
{
	int bfd, isfd;
	struct ibox_decode_state ids;

	if (cap_enter() == -1)
		err(1, "cap_enter");

	if (argc > 1)
		errx(1, "too many argumets");

	ids.fd = 3;
	bfd = 4;
	isfd = 5;
	if ((ids.is = mmap(NULL, sizeof(*ids.is),
	    PROT_READ | PROT_WRITE, MAP_SHARED, isfd, 0)) == MAP_FAILED)
		err(1, "mmap iboxstate");
	if ((ids.buffer = mmap(NULL,
	    ids.is->width * ids.is->height * sizeof(uint32_t),
	    PROT_READ | PROT_WRITE, MAP_SHARED, bfd, 0)) == MAP_FAILED)
		err(1, "mmap buffer");

	decode_png(&ids, NULL, NULL);
	ids.is->times[3] = sysarch(MIPS_GET_COUNT, NULL);
	return (0);
}
