/*-
 * Copyright (c) 2000 Robert N. M. Watson
 * All rights reserved.
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
 *
 * $FreeBSD$
 */
/*
 * TrustedBSD Project - support for POSIX.1e process capabilities
 */

#include <sys/types.h>
#include <sys/capability.h>
#include <sys/errno.h>

int
cap_set_flag(cap_t cap_p, cap_flag_t flag, int ncap, cap_value_t caps[],
	     cap_flag_value_t value)
{
	u_int64_t	*mask;
	int	i;

	switch(flag) {
	case CAP_EFFECTIVE:
		mask = &cap_p->c_effective;
		break;
	case CAP_INHERITABLE:
		mask = &cap_p->c_inheritable;
		break;
	case CAP_PERMITTED:
		mask = &cap_p->c_permitted;
		break;
	default:
		return (EINVAL);
	}

	if (value != CAP_SET && value != CAP_CLEAR)
		return (EINVAL);

	for (i = 0; i < ncap; i++)
		if (value == CAP_SET)
			SET_CAPABILITY(*mask, caps[i]);
		else
			UNSET_CAPABILITY(*mask, caps[i]);
	
	return (0);
}
