/*-
 * Copyright (c) 1998 John D. Polstra
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
 *	$Id$
 */

/*
 * Trampolines to services provided by the dynamic linker.
 */

#include <sys/types.h>
#include <nlist.h>		/* XXX - Required by link.h */
#include <dlfcn.h>
#include <link.h>
#include <stddef.h>

/*
 * These variables are set by code in crt0.o.  For compatibility with
 * old executables, they must be common, not extern.
 */
struct ld_entry	*__ldso_entry;		/* Entry points to dynamic linker */
int		 __ldso_version;	/* Dynamic linker version number */

void *
dlopen(name, mode)
	const char	*name;
	int		 mode;
{
	if (__ldso_entry == NULL)
		return NULL;
	return (__ldso_entry->dlopen)(name, mode);
}

int
dlclose(fd)
	void		*fd;
{
	if (__ldso_entry == NULL)
		return -1;
	return (__ldso_entry->dlclose)(fd);
}

void *
dlsym(fd, name)
	void		*fd;
	const char	*name;
{
	if (__ldso_entry == NULL)
		return NULL;
	if (__ldso_version >= LDSO_VERSION_HAS_DLSYM3) {
		void *retaddr = *(&fd - 1);  /* XXX - ABI/machine dependent */
		return (__ldso_entry->dlsym3)(fd, name, retaddr);
	} else
		return (__ldso_entry->dlsym)(fd, name);
}


const char *
dlerror()
{
	if (__ldso_entry == NULL)
		return "Service unavailable";
	return (__ldso_entry->dlerror)();
}

int
dladdr(addr, dlip)
	const void	*addr;
	Dl_info		*dlip;
{
	if (__ldso_entry == NULL || __ldso_version < LDSO_VERSION_HAS_DLADDR)
		return 0;
	return (__ldso_entry->dladdr)(addr, dlip);
}
