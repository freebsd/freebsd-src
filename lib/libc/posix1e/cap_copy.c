/*
 * Copyright 2001 by Thomas Moestl.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * TrustedBSD implementation of cap_copy_ext()/cap_copy_int()
 *
 * These are largely nops currently, because our internal format is contiguous.
 * We just copy our representation out, and do some minumum validations on
 * external data.
 *
 * XXX: we cannot detect cap being invalid. If it is, the program will probably
 * segfault.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/capability.h>

#include <errno.h>
#include <stdlib.h>

ssize_t
cap_copy_ext(void *ext_p, cap_t cap, ssize_t size)
{
	if (size < 0) {
		errno = EINVAL;
		return (-1);
	}
	if (size < sizeof(struct cap)) {
		errno = ERANGE;
		return (-1);
	}
	memcpy(ext_p, cap, sizeof(struct cap));
	return (sizeof(struct cap));
}

cap_t
cap_copy_int(const void *ext_p)
{
	cap_t c;
	/* We can use cap_dup here, because the format is the same */
	if ((c = cap_dup((cap_t)ext_p)) == NULL)
		return ((cap_t)NULL);
	/* Basic validation */
	if ((c->c_effective & ~CAP_ALL_ON) || (c->c_permitted & ~CAP_ALL_ON) ||
	    (c->c_inheritable & ~CAP_ALL_ON)) {
		cap_free(c);
		errno = EINVAL;
		return ((cap_t)NULL);
	}
	return (c);
}
	    
ssize_t
cap_size(cap_t cap)
{
	(void)cap; /* silence warning */
	return (sizeof(struct cap));
}
