/*-
 * Copyright (c) 2000 Daniel Capo Sobral
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
 *	$FreeBSD: src/sys/boot/ficl/loader.c,v 1.1.2.1 2000/07/06 23:44:59 obrien Exp $
 */

/*******************************************************************
** l o a d e r . c
** Additional FICL words designed for FreeBSD's loader
** 
*******************************************************************/

#include <stand.h>
#include "bootstrap.h"
#include <string.h>
#include "ficl.h"

/*		FreeBSD's loader interaction words
 *
 * 		setenv      ( value n name n' -- )
 * 		setenv?     ( value n name n' flag -- )
 * 		getenv      ( addr n -- addr' n' | -1 )
 * 		unsetenv    ( addr n -- )
 * 		copyin      ( addr addr' len -- )
 * 		copyout     ( addr addr' len -- )
 */

void
ficlSetenv(FICL_VM *pVM)
{
	char	*namep, *valuep, *name, *value;
	int	names, values;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 4, 0);
#endif
	names = stackPopINT(pVM->pStack);
	namep = (char*) stackPopPtr(pVM->pStack);
	values = stackPopINT(pVM->pStack);
	valuep = (char*) stackPopPtr(pVM->pStack);

	name = (char*) ficlMalloc(names+1);
	if (!name)
		vmThrowErr(pVM, "Error: out of memory");
	strncpy(name, namep, names);
	name[names] = '\0';
	value = (char*) ficlMalloc(values+1);
	if (!value)
		vmThrowErr(pVM, "Error: out of memory");
	strncpy(value, valuep, values);
	value[values] = '\0';

	setenv(name, value, 1);
	ficlFree(name);
	ficlFree(value);

	return;
}

void
ficlSetenvq(FICL_VM *pVM)
{
	char	*namep, *valuep, *name, *value;
	int	names, values, overwrite;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 5, 0);
#endif
	overwrite = stackPopINT(pVM->pStack);
	names = stackPopINT(pVM->pStack);
	namep = (char*) stackPopPtr(pVM->pStack);
	values = stackPopINT(pVM->pStack);
	valuep = (char*) stackPopPtr(pVM->pStack);

	name = (char*) ficlMalloc(names+1);
	if (!name)
		vmThrowErr(pVM, "Error: out of memory");
	strncpy(name, namep, names);
	name[names] = '\0';
	value = (char*) ficlMalloc(values+1);
	if (!value)
		vmThrowErr(pVM, "Error: out of memory");
	strncpy(value, valuep, values);
	value[values] = '\0';

	setenv(name, value, overwrite);
	ficlFree(name);
	ficlFree(value);

	return;
}

void
ficlGetenv(FICL_VM *pVM)
{
	char	*namep, *name, *value;
	int	names;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 2, 2);
#endif
	names = stackPopINT(pVM->pStack);
	namep = (char*) stackPopPtr(pVM->pStack);

	name = (char*) ficlMalloc(names+1);
	if (!name)
		vmThrowErr(pVM, "Error: out of memory");
	strncpy(name, namep, names);
	name[names] = '\0';

	value = getenv(name);
	ficlFree(name);

	if(value != NULL) {
		stackPushPtr(pVM->pStack, value);
		stackPushINT(pVM->pStack, strlen(value));
	} else
		stackPushINT(pVM->pStack, -1);

	return;
}

void
ficlUnsetenv(FICL_VM *pVM)
{
	char	*namep, *name;
	int	names;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 2, 0);
#endif
	names = stackPopINT(pVM->pStack);
	namep = (char*) stackPopPtr(pVM->pStack);

	name = (char*) ficlMalloc(names+1);
	if (!name)
		vmThrowErr(pVM, "Error: out of memory");
	strncpy(name, namep, names);
	name[names] = '\0';

	unsetenv(name);
	ficlFree(name);

	return;
}

void
ficlCopyin(FICL_VM *pVM)
{
	void*		src;
	vm_offset_t	dest;
	size_t		len;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 3, 0);
#endif

	len = stackPopINT(pVM->pStack);
	dest = stackPopINT(pVM->pStack);
	src = stackPopPtr(pVM->pStack);

	archsw.arch_copyin(src, dest, len);

	return;
}

void
ficlCopyout(FICL_VM *pVM)
{
	void*		dest;
	vm_offset_t	src;
	size_t		len;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 3, 0);
#endif

	len = stackPopINT(pVM->pStack);
	dest = stackPopPtr(pVM->pStack);
	src = stackPopINT(pVM->pStack);

	archsw.arch_copyout(src, dest, len);

	return;
}

