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
 * $FreeBSD$
 */

/*
 * Linkage to services provided by the dynamic linker.  These are
 * implemented differently in ELF and a.out, because the dynamic
 * linkers have different interfaces.
 */

#ifdef __ELF__

#include <dlfcn.h>
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
dlsym(void *handle, const char *name)
{
	_rtld_error(sorry);
	return NULL;
}

#pragma weak dlinfo
int
dlinfo(void *handle, int request, void *p)
{
	_rtld_error(sorry);
	return NULL;
}

#else /* a.out format */

#include <sys/types.h>
#include <nlist.h>		/* XXX - Required by link.h */
#include <dlfcn.h>
#include <link.h>
#include <stddef.h>

/*
 * For a.out, entry to the dynamic linker is via these trampolines.
 * They enter the dynamic linker through the ld_entry struct that was
 * passed back from the dynamic linker at startup time.
 */

/* GCC is needed because we use its __builtin_return_address construct. */

#ifndef __GNUC__
#error "GCC is needed to compile this file"
#endif

/*
 * These variables are set by code in crt0.o.  For compatibility with
 * old executables, they must be common, not extern.
 */
struct ld_entry	*__ldso_entry;		/* Entry points to dynamic linker */
int		 __ldso_version;	/* Dynamic linker version number */

int
dladdr(const void *addr, Dl_info *dlip)
{
	if (__ldso_entry == NULL || __ldso_version < LDSO_VERSION_HAS_DLADDR)
		return 0;
	return (__ldso_entry->dladdr)(addr, dlip);
}

int
dlclose(void *handle)
{
	if (__ldso_entry == NULL)
		return -1;
	return (__ldso_entry->dlclose)(handle);
}

const char *
dlerror(void)
{
	if (__ldso_entry == NULL)
		return "Service unavailable";
	return (__ldso_entry->dlerror)();
}

void *
dlopen(const char *name, int mode)
{
	if (__ldso_entry == NULL)
		return NULL;
	return (__ldso_entry->dlopen)(name, mode);
}

void *
dlsym(void *handle, const char *name)
{
	if (__ldso_entry == NULL)
		return NULL;
	if (__ldso_version >= LDSO_VERSION_HAS_DLSYM3) {
		void *retaddr = __builtin_return_address(0); /* __GNUC__ only */
		return (__ldso_entry->dlsym3)(handle, name, retaddr);
	} else
		return (__ldso_entry->dlsym)(handle, name);
}

#endif /* __ELF__ */
