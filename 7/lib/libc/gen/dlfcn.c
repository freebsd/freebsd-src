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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Linkage to services provided by the dynamic linker.
 */
#include <dlfcn.h>
#include <link.h>
#include <stddef.h>

static const char sorry[] = "Service unavailable";

/*
 * For ELF, the dynamic linker directly resolves references to its
 * services to functions inside the dynamic linker itself.  These
 * weak-symbol stubs are necessary so that "ld" won't complain about
 * undefined symbols.  The stubs are executed only when the program is
 * linked statically, or when a given service isn't implemented in the
 * dynamic linker.  They must return an error if called, and they must
 * be weak symbols so that the dynamic linker can override them.
 */

#pragma weak _rtld_error
void
_rtld_error(const char *fmt, ...)
{
}

#pragma weak dladdr
int
dladdr(const void *addr, Dl_info *dlip)
{
	_rtld_error(sorry);
	return 0;
}

#pragma weak dlclose
int
dlclose(void *handle)
{
	_rtld_error(sorry);
	return -1;
}

#pragma weak dlerror
const char *
dlerror(void)
{
	return sorry;
}

#pragma weak dllockinit
void
dllockinit(void *context,
	   void *(*lock_create)(void *context),
	   void (*rlock_acquire)(void *lock),
	   void (*wlock_acquire)(void *lock),
	   void (*lock_release)(void *lock),
	   void (*lock_destroy)(void *lock),
	   void (*context_destroy)(void *context))
{
	if (context_destroy != NULL)
		context_destroy(context);
}

#pragma weak dlopen
void *
dlopen(const char *name, int mode)
{
	_rtld_error(sorry);
	return NULL;
}

#pragma weak dlsym
void *
dlsym(void * __restrict handle, const char * __restrict name)
{
	_rtld_error(sorry);
	return NULL;
}

#pragma weak dlvsym
void *
dlvsym(void * __restrict handle, const char * __restrict name,
    const char * __restrict version)
{
	_rtld_error(sorry);
	return NULL;
}

#pragma weak dlinfo
int
dlinfo(void * __restrict handle, int request, void * __restrict p)
{
	_rtld_error(sorry);
	return 0;
}

#pragma weak _rtld_thread_init
void
_rtld_thread_init(void * li)
{
	_rtld_error(sorry);
}

#pragma weak dl_iterate_phdr
int
dl_iterate_phdr(int (*callback)(struct dl_phdr_info *, size_t, void *),
    void *data)
{
	_rtld_error(sorry);
	return 0;
}

#pragma weak _rtld_atfork_pre
void
_rtld_atfork_pre(int *locks)
{
}

#pragma weak _rtld_atfork_post
void
_rtld_atfork_post(int *locks)
{
}
