/*-
 * Copyright (c) 2014 Netflix, Inc
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
 *	$FreeBSD$
 */

/*******************************************************************
** e f i . c
** Additional words for EFI
** 
*******************************************************************/

#ifdef TESTMAIN
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#else
#include <stand.h>
#endif
#include "bootstrap.h"
#include <string.h>
#include "ficl.h"

/*
 *		FreeBSD's loader interaction words and extras
 *
 * 		efi-setenv  ( value n name n guid n attr -- 0 | -1)
 * 		efi-getenv  ( guid n addr n -- addr' n' | -1 )
 * 		efi-unsetenv ( name n guid n'' -- )
 */

/*
 * efi-setenv
 * 		efi-setenv  ( value n name n guid n attr -- 0 | -1)
 *
 * Set environment variables using the SetVariable EFI runtime service.
 *
 * Value and guid are passed through in binary form (so guid needs to be
 * converted to binary form from its string form). Name is converted from
 * ASCII to CHAR16. Since ficl doesn't have support for internationalization,
 * there's no native CHAR16 interface provided.
 *
 * attr is an int in the bitmask of the following attributes for this variable.
 *
 *	1	Non volatile
 *	2	Boot service access
 *	4	Run time access
 * (corresponding to the same bits in the UEFI spec).
 */
void
ficlEfiSetenv(FICL_VM *pVM)
{
#ifndef TESTMAIN
	char	*value, *guid;
	CHAR16	*name
	int	i;
#endif
	char	*namep, *valuep, *guidp;
	int	names, values, guids, attr;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 6, 0);
#endif
	attr = stackPopINT(pVM->pStack);
	guids = stackPopINT(pVM->pStack);
	guidp = (char*)stackPopPtr(pVM->pStack);
	names = stackPopINT(pVM->pStack);
	namep = (char*)stackPopPtr(pVM->pStack);
	values = stackPopINT(pVM->pStack);
	valuep = (char*)stackPopPtr(pVM->pStack);

#ifndef TESTMAIN
	guid = (char*)ficlMalloc(guids);
	if (guid != NULL)
		vmThrowErr(pVM, "Error: out of memory");
	memcpy(guid, guidp, guids);

	name = (char*)ficlMalloc((names + 1) * sizeof(CHAR16));
	if (name == NULL)
		vmThrowErr(pVM, "Error: out of memory");
	for (i = 0; i < names; i++)
		name[i] = namep[i];
	name[names] = (CHAR16)0;

	value = (char*)ficlMalloc(values + 1);
	if (value != NULL)
		vmThrowErr(pVM, "Error: out of memory");
	memcpy(value, valuep, values);

	status = efi_set_variable(name, guid, attr, value);
	if (status == EFI_SUCCESS)
		stackPushINT(pVM->pStack, 0);
	else
		stackPushINT(pVM->pStack, -1);

	ficlFree(name);
	ficlFree(value);
	ficlFree(guid);
#endif

	return;
}

void
ficlEfiGetenv(FICL_VM *pVM)
{
#ifndef TESTMAIN
	char	*name, *value;
#endif
	char	*namep;
	int	names;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 2, 2);
#endif
	names = stackPopINT(pVM->pStack);
	namep = (char*) stackPopPtr(pVM->pStack);

#ifndef TESTMAIN
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
#endif
		stackPushINT(pVM->pStack, -1);

	return;
}

void
ficlEfiUnsetenv(FICL_VM *pVM)
{
#ifndef TESTMAIN
	char	*name;
#endif
	char	*namep;
	int	names;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 2, 0);
#endif
	names = stackPopINT(pVM->pStack);
	namep = (char*) stackPopPtr(pVM->pStack);

#ifndef TESTMAIN
	name = (char*) ficlMalloc(names+1);
	if (!name)
		vmThrowErr(pVM, "Error: out of memory");
	strncpy(name, namep, names);
	name[names] = '\0';

	unsetenv(name);
	ficlFree(name);
#endif

	return;
}
/**************************************************************************

** Build FreeBSD platform extensions into the system dictionary
**************************************************************************/
void ficlEfiCompilePlatform(FICL_SYSTEM *pSys)
{
    FICL_DICT *dp = pSys->dp;
    assert (dp);

    dictAppendWord(dp, "efi-setenv",    ficlEfiSetenv,	    FW_DEFAULT);
    dictAppendWord(dp, "efi-getenv",    ficlEfiGetenv,	    FW_DEFAULT);
    dictAppendWord(dp, "efi-unsetenv",  ficlEfiUnsetenv,    FW_DEFAULT);

    return;
}
