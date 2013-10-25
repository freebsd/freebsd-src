/*-
 * Copyright (c) 2012 Robert N. M. Watson
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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static int	stub_errno;

int *
__error(void)
{

	return (&stub_errno);
}

int
close(int d __unused)
{

	errno = ECAPMODE;
	return (-1);
}

size_t
fread(void *ptr __unused, size_t size __unused, size_t nmemb __unused,
	      void *stream __unused)
{
	errno = ECAPMODE;
	return (-1);
}

int
fstat(int fd __unused, struct stat *sb __unused)
{

	errno = ECAPMODE;
	return (-1);
}

off_t
lseek(int fildes __unused, off_t offset __unused, int whence __unused)
{

	errno = ECAPMODE;
	return (-1);
}

int
open(const char *path __unused, int flags __unused, ...)
{

	errno = ECAPMODE;
	return (-1);
}

ssize_t
read(int d __unused, void *buf __unused, size_t nbytes __unused)
{

	errno = ECAPMODE;
	return (-1);
}
