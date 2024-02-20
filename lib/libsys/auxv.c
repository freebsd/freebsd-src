/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2010, 2012 Konstantin Belousov <kib@FreeBSD.ORG>.
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
 *
 */

#include "namespace.h"
#include <elf.h>
#include <errno.h>
#include <link.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <sys/auxv.h>
#include "un-namespace.h"
#include "libc_private.h"

extern int _DYNAMIC;
#pragma weak _DYNAMIC

void *__elf_aux_vector;

#ifndef PIC
static pthread_once_t aux_vector_once = PTHREAD_ONCE_INIT;

static void
init_aux_vector_once(void)
{
	Elf_Addr *sp;

	sp = (Elf_Addr *)environ;
	while (*sp++ != 0)
		;
	__elf_aux_vector = (Elf_Auxinfo *)sp;
}

void
__init_elf_aux_vector(void)
{

	if (&_DYNAMIC != NULL)
		return;
	_once(&aux_vector_once, init_aux_vector_once);
}
#endif

static bool aux_once = false;
static int pagesize, osreldate, canary_len, ncpus, pagesizes_len, bsdflags;
static int hwcap_present, hwcap2_present;
static char *canary, *pagesizes, *execpath;
static void *ps_strings, *timekeep;
static u_long hwcap, hwcap2;
static void *fxrng_seed_version;
static u_long usrstackbase, usrstacklim;

#ifdef __powerpc__
static int powerpc_new_auxv_format = 0;
static void _init_aux_powerpc_fixup(void);
int _powerpc_elf_aux_info(int, void *, int);
#endif

/*
 * This function might be called and actual body executed more than
 * once in multithreading environment.  Due to this, it is and must
 * continue to be idempotent.  All stores are atomic (no store
 * tearing), because we only assign to int/long/ptr.
 */
static void
init_aux(void)
{
	Elf_Auxinfo *aux;

	if (aux_once)
		return;
	for (aux = __elf_aux_vector; aux->a_type != AT_NULL; aux++) {
		switch (aux->a_type) {
		case AT_BSDFLAGS:
			bsdflags = aux->a_un.a_val;
			break;

		case AT_CANARY:
			canary = (char *)(aux->a_un.a_ptr);
			break;

		case AT_CANARYLEN:
			canary_len = aux->a_un.a_val;
			break;

		case AT_EXECPATH:
			execpath = (char *)(aux->a_un.a_ptr);
			break;

		case AT_HWCAP:
			hwcap_present = 1;
			hwcap = (u_long)(aux->a_un.a_val);
			break;

		case AT_HWCAP2:
			hwcap2_present = 1;
			hwcap2 = (u_long)(aux->a_un.a_val);
			break;

		case AT_PAGESIZES:
			pagesizes = (char *)(aux->a_un.a_ptr);
			break;

		case AT_PAGESIZESLEN:
			pagesizes_len = aux->a_un.a_val;
			break;

		case AT_PAGESZ:
			pagesize = aux->a_un.a_val;
			break;

		case AT_OSRELDATE:
			osreldate = aux->a_un.a_val;
			break;

		case AT_NCPUS:
			ncpus = aux->a_un.a_val;
			break;

		case AT_TIMEKEEP:
			timekeep = aux->a_un.a_ptr;
			break;

		case AT_PS_STRINGS:
			ps_strings = aux->a_un.a_ptr;
			break;

		case AT_FXRNG:
			fxrng_seed_version = aux->a_un.a_ptr;
			break;

		case AT_USRSTACKBASE:
			usrstackbase = aux->a_un.a_val;
			break;

		case AT_USRSTACKLIM:
			usrstacklim = aux->a_un.a_val;
			break;
#ifdef __powerpc__
		/*
		 * Since AT_STACKPROT is always set, and the common
		 * value 23 is mutually exclusive with the legacy powerpc
		 * value 21, the existence of AT_STACKPROT proves we are
		 * on the common format.
		 */
		case AT_STACKPROT:	/* 23 */
			powerpc_new_auxv_format = 1;
			break;
#endif
		}
	}
#ifdef __powerpc__
	if (!powerpc_new_auxv_format)
		_init_aux_powerpc_fixup();
#endif

	aux_once = true;
}

#ifdef __powerpc__
static void
_init_aux_powerpc_fixup(void)
{
	Elf_Auxinfo *aux;

	/*
	 * Before 1300070, PowerPC platforms had nonstandard numbering for
	 * the aux vector. When running old binaries, the kernel will pass
	 * the vector using the old numbering. Reload affected variables.
	 */
	for (aux = __elf_aux_vector; aux->a_type != AT_NULL; aux++) {
		switch (aux->a_type) {
		case AT_OLD_CANARY:
			canary = (char *)(aux->a_un.a_ptr);
			break;
		case AT_OLD_CANARYLEN:
			canary_len = aux->a_un.a_val;
			break;
		case AT_OLD_EXECPATH:
			execpath = (char *)(aux->a_un.a_ptr);
			break;
		case AT_OLD_PAGESIZES:
			pagesizes = (char *)(aux->a_un.a_ptr);
			break;
		case AT_OLD_PAGESIZESLEN:
			pagesizes_len = aux->a_un.a_val;
			break;
		case AT_OLD_OSRELDATE:
			osreldate = aux->a_un.a_val;
			break;
		case AT_OLD_NCPUS:
			ncpus = aux->a_un.a_val;
			break;
		}
	}
}

int
_powerpc_elf_aux_info(int aux, void *buf, int buflen)
{

	/*
	 * If we are in the old auxv format, we need to translate the aux
	 * parameter of elf_aux_info() calls into the common auxv format.
	 * Internal libc calls always use the common format, and they
	 * directly call _elf_aux_info instead of using the weak symbol.
	 */
	if (!powerpc_new_auxv_format) {
		switch (aux) {
		case AT_OLD_EXECPATH:
			aux = AT_EXECPATH;
			break;
		case AT_OLD_CANARY:
			aux = AT_CANARY;
			break;
		case AT_OLD_CANARYLEN:
			aux = AT_CANARYLEN;
			break;
		case AT_OLD_OSRELDATE:
			aux = AT_OSRELDATE;
			break;
		case AT_OLD_NCPUS:
			aux = AT_NCPUS;
			break;
		case AT_OLD_PAGESIZES:
			aux = AT_PAGESIZES;
			break;
		case AT_OLD_PAGESIZESLEN:
			aux = AT_PAGESIZESLEN;
			break;
		case AT_OLD_STACKPROT:
			aux = AT_STACKPROT;
			break;
		}
	}
	return _elf_aux_info(aux, buf, buflen);
}
__weak_reference(_powerpc_elf_aux_info, elf_aux_info);
#else
__weak_reference(_elf_aux_info, elf_aux_info);
#endif

int
_elf_aux_info(int aux, void *buf, int buflen)
{
	int res;

#ifndef PIC
	__init_elf_aux_vector();
#endif
	if (__elf_aux_vector == NULL)
		return (ENOSYS);
	init_aux();	/* idempotent */

	if (buflen < 0)
		return (EINVAL);

	switch (aux) {
	case AT_CANARY:
		if (canary != NULL && canary_len >= buflen) {
			memcpy(buf, canary, buflen);
			memset(canary, 0, canary_len);
			canary = NULL;
			res = 0;
		} else
			res = ENOENT;
		break;
	case AT_EXECPATH:
		if (execpath == NULL)
			res = ENOENT;
		else if (buf == NULL)
			res = EINVAL;
		else {
			if (strlcpy(buf, execpath, buflen) >=
			    (unsigned int)buflen)
				res = EINVAL;
			else
				res = 0;
		}
		break;
	case AT_HWCAP:
		if (hwcap_present && buflen == sizeof(u_long)) {
			*(u_long *)buf = hwcap;
			res = 0;
		} else
			res = ENOENT;
		break;
	case AT_HWCAP2:
		if (hwcap2_present && buflen == sizeof(u_long)) {
			*(u_long *)buf = hwcap2;
			res = 0;
		} else
			res = ENOENT;
		break;
	case AT_PAGESIZES:
		if (pagesizes != NULL && pagesizes_len >= buflen) {
			memcpy(buf, pagesizes, buflen);
			res = 0;
		} else
			res = ENOENT;
		break;
	case AT_PAGESZ:
		if (buflen == sizeof(int)) {
			if (pagesize != 0) {
				*(int *)buf = pagesize;
				res = 0;
			} else
				res = ENOENT;
		} else
			res = EINVAL;
		break;
	case AT_OSRELDATE:
		if (buflen == sizeof(int)) {
			if (osreldate != 0) {
				*(int *)buf = osreldate;
				res = 0;
			} else
				res = ENOENT;
		} else
			res = EINVAL;
		break;
	case AT_NCPUS:
		if (buflen == sizeof(int)) {
			if (ncpus != 0) {
				*(int *)buf = ncpus;
				res = 0;
			} else
				res = ENOENT;
		} else
			res = EINVAL;
		break;
	case AT_TIMEKEEP:
		if (buflen == sizeof(void *)) {
			if (timekeep != NULL) {
				*(void **)buf = timekeep;
				res = 0;
			} else
				res = ENOENT;
		} else
			res = EINVAL;
		break;
	case AT_BSDFLAGS:
		if (buflen == sizeof(int)) {
			*(int *)buf = bsdflags;
			res = 0;
		} else
			res = EINVAL;
		break;
	case AT_PS_STRINGS:
		if (buflen == sizeof(void *)) {
			if (ps_strings != NULL) {
				*(void **)buf = ps_strings;
				res = 0;
			} else
				res = ENOENT;
		} else
			res = EINVAL;
		break;
	case AT_FXRNG:
		if (buflen == sizeof(void *)) {
			if (fxrng_seed_version != NULL) {
				*(void **)buf = fxrng_seed_version;
				res = 0;
			} else
				res = ENOENT;
		} else
			res = EINVAL;
		break;
	case AT_USRSTACKBASE:
		if (buflen == sizeof(u_long)) {
			if (usrstackbase != 0) {
				*(u_long *)buf = usrstackbase;
				res = 0;
			} else
				res = ENOENT;
		} else
			res = EINVAL;
		break;
	case AT_USRSTACKLIM:
		if (buflen == sizeof(u_long)) {
			if (usrstacklim != 0) {
				*(u_long *)buf = usrstacklim;
				res = 0;
			} else
				res = ENOENT;
		} else
			res = EINVAL;
		break;
	default:
		res = ENOENT;
		break;
	}
	return (res);
}
