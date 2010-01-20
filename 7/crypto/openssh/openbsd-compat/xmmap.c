/*
 * Copyright (c) 2002 Tim Rice.  All rights reserved.
 * MAP_FAILED code by Solar Designer.
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
 */

/* $Id: xmmap.c,v 1.14 2007/06/11 02:52:24 djm Exp $ */

#include "includes.h"

#include <sys/types.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <sys/stat.h>

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"

void *
xmmap(size_t size)
{
#ifdef HAVE_MMAP
	void *address;

# ifdef MAP_ANON
	address = mmap(NULL, size, PROT_WRITE|PROT_READ, MAP_ANON|MAP_SHARED,
	    -1, (off_t)0);
# else
	address = mmap(NULL, size, PROT_WRITE|PROT_READ, MAP_SHARED,
	    open("/dev/zero", O_RDWR), (off_t)0);
# endif

#define MM_SWAP_TEMPLATE "/var/run/sshd.mm.XXXXXXXX"
	if (address == (void *)MAP_FAILED) {
		char tmpname[sizeof(MM_SWAP_TEMPLATE)] = MM_SWAP_TEMPLATE;
		int tmpfd;
		mode_t old_umask;

		old_umask = umask(0177);
		tmpfd = mkstemp(tmpname);
		umask(old_umask);
		if (tmpfd == -1)
			fatal("mkstemp(\"%s\"): %s",
			    MM_SWAP_TEMPLATE, strerror(errno));
		unlink(tmpname);
		ftruncate(tmpfd, size);
		address = mmap(NULL, size, PROT_WRITE|PROT_READ, MAP_SHARED,
		    tmpfd, (off_t)0);
		close(tmpfd);
	}

	return (address);
#else
	fatal("%s: UsePrivilegeSeparation=yes and Compression=yes not supported",
	    __func__);
#endif /* HAVE_MMAP */

}

