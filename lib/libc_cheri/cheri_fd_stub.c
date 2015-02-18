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

#include <sys/types.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <cheri/cheri_fd.h>

#include <unistd.h>

/*
 * This C file contains stubs to invoke the CHERI fd class, one per method.
 */

/*
 * XXXRW: Not very pretty: cheri_fd uses a different prototype so we have to
 * re-prototype cheri_invoke() ourselves.  Compiler help is the long-term
 * solution.
 */
struct cheri_fd_ret	cheri_invoke(struct cheri_object fd_object,
			    register_t methodnum,
			    register_t a0, register_t a1,
			    __capability void *c3)
			    __attribute__((cheri_ccall));

register_t cheri_fd_methodnum_fstat_c = CHERI_FD_METHOD_FSTAT_C;
struct cheri_fd_ret
cheri_fd_fstat_c(struct cheri_object fd_object,
    __capability struct stat *sb_c)
{

	return (cheri_invoke(fd_object, cheri_fd_methodnum_fstat_c, 0, 0,
	    sb_c));
}

register_t cheri_fd_methodnum_lseek_c = CHERI_FD_METHOD_LSEEK_C;
struct cheri_fd_ret
cheri_fd_lseek_c(struct cheri_object fd_object, off_t offset, int whence)
{

	return (cheri_invoke(fd_object, cheri_fd_methodnum_lseek_c, offset,
	    whence, cheri_zerocap()));
}

register_t cheri_fd_methodnum_read_c = CHERI_FD_METHOD_READ_C;
struct cheri_fd_ret
cheri_fd_read_c(struct cheri_object fd_object, __capability void *buf_c)
{

	return (cheri_invoke(fd_object, cheri_fd_methodnum_read_c, 0, 0,
	    buf_c));
}

register_t cheri_fd_methodnum_write_c = CHERI_FD_METHOD_WRITE_C;
struct cheri_fd_ret
cheri_fd_write_c(struct cheri_object fd_object, __capability const void *buf_c)
{

	return (cheri_invoke(fd_object, cheri_fd_methodnum_write_c, 0, 0,
	    (__capability void *)buf_c));
}
