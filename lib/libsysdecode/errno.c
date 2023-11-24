/*-
 * Copyright (c) 2015 John H. Baldwin <jhb@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/acl.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <sysdecode.h>

#if defined(__aarch64__) || defined(__amd64__) || defined(__i386__)
#include <compat/linux/linux_errno.h>
#include <compat/linux/linux_errno.inc>
#endif

int
sysdecode_abi_to_freebsd_errno(enum sysdecode_abi abi, int error)
{

	switch (abi) {
	case SYSDECODE_ABI_FREEBSD:
	case SYSDECODE_ABI_FREEBSD32:
		return (error);
#if defined(__aarch64__) || defined(__amd64__) || defined(__i386__)
	case SYSDECODE_ABI_LINUX:
	case SYSDECODE_ABI_LINUX32: {
		unsigned int i;

		/*
		 * This is imprecise since it returns the first
		 * matching errno.
		 */
		for (i = 0; i < nitems(linux_errtbl); i++) {
			if (error == linux_errtbl[i])
				return (i);
		}
		break;
	}
#endif
	default:
		break;
	}
	return (INT_MAX);
}

int
sysdecode_freebsd_to_abi_errno(enum sysdecode_abi abi, int error)
{

	switch (abi) {
	case SYSDECODE_ABI_FREEBSD:
	case SYSDECODE_ABI_FREEBSD32:
		return (error);
#if defined(__aarch64__) || defined(__amd64__) || defined(__i386__)
	case SYSDECODE_ABI_LINUX:
	case SYSDECODE_ABI_LINUX32:
		if (error >= 0 && error <= ELAST)
			return (linux_errtbl[error]);
		break;
#endif
	default:
		break;
	}
	return (INT_MAX);
}

