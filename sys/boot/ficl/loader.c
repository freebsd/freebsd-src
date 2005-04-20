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
 *	$FreeBSD$
 */

/*******************************************************************
** l o a d e r . c
** Additional FICL words designed for FreeBSD's loader
** 
*******************************************************************/

#ifdef TESTMAIN
#include <stdlib.h>
#else
#include <stand.h>
#endif
#include "bootstrap.h"
#include <string.h>
#include "ficl.h"

/*		FreeBSD's loader interaction words and extras
 *
 * 		setenv      ( value n name n' -- )
 * 		setenv?     ( value n name n' flag -- )
 * 		getenv      ( addr n -- addr' n' | -1 )
 * 		unsetenv    ( addr n -- )
 * 		copyin      ( addr addr' len -- )
 * 		copyout     ( addr addr' len -- )
 * 		findfile    ( name len type len' -- addr )
 * 		pnpdevices  ( -- addr )
 * 		pnphandlers ( -- addr )
 * 		ccall       ( [[...[p10] p9] ... p1] n addr -- result )
 * 		.#	    ( value -- )
 */

#ifndef TESTMAIN
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

void
ficlFindfile(FICL_VM *pVM)
{
	char	*name, *type, *namep, *typep;
	struct	preloaded_file* fp;
	int	names, types;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 4, 1);
#endif

	types = stackPopINT(pVM->pStack);
	typep = (char*) stackPopPtr(pVM->pStack);
	names = stackPopINT(pVM->pStack);
	namep = (char*) stackPopPtr(pVM->pStack);
	name = (char*) ficlMalloc(names+1);
	if (!name)
		vmThrowErr(pVM, "Error: out of memory");
	strncpy(name, namep, names);
	name[names] = '\0';
	type = (char*) ficlMalloc(types+1);
	if (!type)
		vmThrowErr(pVM, "Error: out of memory");
	strncpy(type, typep, types);
	type[types] = '\0';

	fp = file_findfile(name, type);
	stackPushPtr(pVM->pStack, fp);

	return;
}

#ifdef HAVE_PNP

void
ficlPnpdevices(FICL_VM *pVM)
{
	static int pnp_devices_initted = 0;
#if FICL_ROBUST > 1
	vmCheckStack(pVM, 0, 1);
#endif

	if(!pnp_devices_initted) {
		STAILQ_INIT(&pnp_devices);
		pnp_devices_initted = 1;
	}

	stackPushPtr(pVM->pStack, &pnp_devices);

	return;
}

void
ficlPnphandlers(FICL_VM *pVM)
{
#if FICL_ROBUST > 1
	vmCheckStack(pVM, 0, 1);
#endif

	stackPushPtr(pVM->pStack, pnphandlers);

	return;
}

#endif

#endif /* ndef TESTMAIN */

void
ficlCcall(FICL_VM *pVM)
{
	int (*func)(int, ...);
	int result, p[10];
	int nparam, i;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 2, 0);
#endif

	func = stackPopPtr(pVM->pStack);
	nparam = stackPopINT(pVM->pStack);

#if FICL_ROBUST > 1
	vmCheckStack(pVM, nparam, 1);
#endif

	for (i = 0; i < nparam; i++)
		p[i] = stackPopINT(pVM->pStack);

	result = func(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8],
	    p[9]);

	stackPushINT(pVM->pStack, result);

	return;
}

/**************************************************************************
                        f i c l E x e c F D
** reads in text from file fd and passes it to ficlExec()
 * returns VM_OUTOFTEXT on success or the ficlExec() error code on
 * failure.
 */ 
#define nLINEBUF 256
int ficlExecFD(FICL_VM *pVM, int fd)
{
    char    cp[nLINEBUF];
    int     nLine = 0, rval = VM_OUTOFTEXT;
    char    ch;
    CELL    id;

    id = pVM->sourceID;
    pVM->sourceID.i = fd;

    /* feed each line to ficlExec */
    while (1) {
	int status, i;

	i = 0;
	while ((status = read(fd, &ch, 1)) > 0 && ch != '\n')
	    cp[i++] = ch;
        nLine++;
	if (!i) {
	    if (status < 1)
		break;
	    continue;
	}
        rval = ficlExecC(pVM, cp, i);
	if(rval != VM_QUIT && rval != VM_USEREXIT && rval != VM_OUTOFTEXT)
        {
            pVM->sourceID = id;
            return rval; 
        }
    }
    /*
    ** Pass an empty line with SOURCE-ID == -1 to flush
    ** any pending REFILLs (as required by FILE wordset)
    */
    pVM->sourceID.i = -1;
    ficlExec(pVM, "");

    pVM->sourceID = id;
    return rval;
}

static void displayCellNoPad(FICL_VM *pVM)
{
    CELL c;
#if FICL_ROBUST > 1
    vmCheckStack(pVM, 1, 0);
#endif
    c = stackPop(pVM->pStack);
    ltoa((c).i, pVM->pad, pVM->base);
    vmTextOut(pVM, pVM->pad, 0);
    return;
}

/*          fopen - open a file and return new fd on stack.
 *
 * fopen ( ptr count mode -- fd )
 */
static void pfopen(FICL_VM *pVM)
{
    int     mode, fd, count;
    char    *ptr, *name;

#if FICL_ROBUST > 1
    vmCheckStack(pVM, 3, 1);
#endif

    mode = stackPopINT(pVM->pStack);    /* get mode */
    count = stackPopINT(pVM->pStack);   /* get count */
    ptr = stackPopPtr(pVM->pStack);     /* get ptr */

    if ((count < 0) || (ptr == NULL)) {
        stackPushINT(pVM->pStack, -1);
        return;
    }

    /* ensure that the string is null terminated */
    name = (char *)malloc(count+1);
    bcopy(ptr,name,count);
    name[count] = 0;

    /* open the file */
    fd = open(name, mode);
    free(name);
    stackPushINT(pVM->pStack, fd);
    return;
}
 
/*          fclose - close a file who's fd is on stack.
 *
 * fclose ( fd -- )
 */
static void pfclose(FICL_VM *pVM)
{
    int fd;

#if FICL_ROBUST > 1
    vmCheckStack(pVM, 1, 0);
#endif
    fd = stackPopINT(pVM->pStack); /* get fd */
    if (fd != -1)
	close(fd);
    return;
}

/*          fread - read file contents
 *
 * fread  ( fd buf nbytes  -- nread )
 */
static void pfread(FICL_VM *pVM)
{
    int     fd, len;
    char *buf;

#if FICL_ROBUST > 1
    vmCheckStack(pVM, 3, 1);
#endif
    len = stackPopINT(pVM->pStack); /* get number of bytes to read */
    buf = stackPopPtr(pVM->pStack); /* get buffer */
    fd = stackPopINT(pVM->pStack); /* get fd */
    if (len > 0 && buf && fd != -1)
	stackPushINT(pVM->pStack, read(fd, buf, len));
    else
	stackPushINT(pVM->pStack, -1);
    return;
}

/*          fload - interpret file contents
 *
 * fload  ( fd -- )
 */
static void pfload(FICL_VM *pVM)
{
    int     fd;

#if FICL_ROBUST > 1
    vmCheckStack(pVM, 1, 0);
#endif
    fd = stackPopINT(pVM->pStack); /* get fd */
    if (fd != -1)
	ficlExecFD(pVM, fd);
    return;
}

/*          fwrite - write file contents
 *
 * fwrite  ( fd buf nbytes  -- nwritten )
 */
static void pfwrite(FICL_VM *pVM)
{
    int     fd, len;
    char *buf;

#if FICL_ROBUST > 1
    vmCheckStack(pVM, 3, 1);
#endif
    len = stackPopINT(pVM->pStack); /* get number of bytes to read */
    buf = stackPopPtr(pVM->pStack); /* get buffer */
    fd = stackPopINT(pVM->pStack); /* get fd */
    if (len > 0 && buf && fd != -1)
	stackPushINT(pVM->pStack, write(fd, buf, len));
    else
	stackPushINT(pVM->pStack, -1);
    return;
}

/*          fseek - seek to a new position in a file
 *
 * fseek  ( fd ofs whence  -- pos )
 */
static void pfseek(FICL_VM *pVM)
{
    int     fd, pos, whence;

#if FICL_ROBUST > 1
    vmCheckStack(pVM, 3, 1);
#endif
    whence = stackPopINT(pVM->pStack);
    pos = stackPopINT(pVM->pStack);
    fd = stackPopINT(pVM->pStack);
    stackPushINT(pVM->pStack, lseek(fd, pos, whence));
    return;
}

/*           key - get a character from stdin
 *
 * key ( -- char )
 */
static void key(FICL_VM *pVM)
{
#if FICL_ROBUST > 1
    vmCheckStack(pVM, 0, 1);
#endif
    stackPushINT(pVM->pStack, getchar());
    return;
}

/*           key? - check for a character from stdin (FACILITY)
 *
 * key? ( -- flag )
 */
static void keyQuestion(FICL_VM *pVM)
{
#if FICL_ROBUST > 1
    vmCheckStack(pVM, 0, 1);
#endif
#ifdef TESTMAIN
    /* XXX Since we don't fiddle with termios, let it always succeed... */
    stackPushINT(pVM->pStack, FICL_TRUE);
#else
    /* But here do the right thing. */
    stackPushINT(pVM->pStack, ischar()? FICL_TRUE : FICL_FALSE);
#endif
    return;
}

/* seconds - gives number of seconds since beginning of time
 *
 * beginning of time is defined as:
 *
 *	BTX	- number of seconds since midnight
 *	FreeBSD	- number of seconds since Jan 1 1970
 *
 * seconds ( -- u )
 */
static void pseconds(FICL_VM *pVM)
{
#if FICL_ROBUST > 1
    vmCheckStack(pVM,0,1);
#endif
    stackPushUNS(pVM->pStack, (FICL_UNS) time(NULL));
    return;
}

/* ms - wait at least that many milliseconds (FACILITY)
 *
 * ms ( u -- )
 *
 */
static void ms(FICL_VM *pVM)
{
#if FICL_ROBUST > 1
    vmCheckStack(pVM,1,0);
#endif
#ifdef TESTMAIN
    usleep(stackPopUNS(pVM->pStack)*1000);
#else
    delay(stackPopUNS(pVM->pStack)*1000);
#endif
    return;
}

/*           fkey - get a character from a file
 *
 * fkey ( file -- char )
 */
static void fkey(FICL_VM *pVM)
{
    int i, fd;
    char ch;

#if FICL_ROBUST > 1
    vmCheckStack(pVM, 1, 1);
#endif
    fd = stackPopINT(pVM->pStack);
    i = read(fd, &ch, 1);
    stackPushINT(pVM->pStack, i > 0 ? ch : -1);
    return;
}

/*
** Retrieves free space remaining on the dictionary
*/

static void freeHeap(FICL_VM *pVM)
{
    stackPushINT(pVM->pStack, dictCellsAvail(ficlGetDict(pVM->pSys)));
}


/******************* Increase dictionary size on-demand ******************/
 
static void ficlDictThreshold(FICL_VM *pVM)
{
    stackPushPtr(pVM->pStack, &dictThreshold);
}
 
static void ficlDictIncrease(FICL_VM *pVM)
{
    stackPushPtr(pVM->pStack, &dictIncrease);
}


/**************************************************************************
                        f i c l C o m p i l e P l a t f o r m
** Build FreeBSD platform extensions into the system dictionary
**************************************************************************/
void ficlCompilePlatform(FICL_SYSTEM *pSys)
{
    FICL_DICT *dp = pSys->dp;
    assert (dp);

    dictAppendWord(dp, ".#",        displayCellNoPad,    FW_DEFAULT);
    dictAppendWord(dp, "fopen",	    pfopen,	    FW_DEFAULT);
    dictAppendWord(dp, "fclose",    pfclose,	    FW_DEFAULT);
    dictAppendWord(dp, "fread",	    pfread,	    FW_DEFAULT);
    dictAppendWord(dp, "fload",	    pfload,	    FW_DEFAULT);
    dictAppendWord(dp, "fkey",	    fkey,	    FW_DEFAULT);
    dictAppendWord(dp, "fseek",     pfseek,	    FW_DEFAULT);
    dictAppendWord(dp, "fwrite",    pfwrite,	    FW_DEFAULT);
    dictAppendWord(dp, "key",	    key,	    FW_DEFAULT);
    dictAppendWord(dp, "key?",	    keyQuestion,    FW_DEFAULT);
    dictAppendWord(dp, "ms",        ms,             FW_DEFAULT);
    dictAppendWord(dp, "seconds",   pseconds,       FW_DEFAULT);
    dictAppendWord(dp, "heap?",     freeHeap,       FW_DEFAULT);
    dictAppendWord(dp, "dictthreshold", ficlDictThreshold, FW_DEFAULT);
    dictAppendWord(dp, "dictincrease", ficlDictIncrease, FW_DEFAULT);

#ifndef TESTMAIN
#ifdef __i386__
    dictAppendWord(dp, "outb",      ficlOutb,       FW_DEFAULT);
    dictAppendWord(dp, "inb",       ficlInb,        FW_DEFAULT);
#endif
    dictAppendWord(dp, "setenv",    ficlSetenv,	    FW_DEFAULT);
    dictAppendWord(dp, "setenv?",   ficlSetenvq,    FW_DEFAULT);
    dictAppendWord(dp, "getenv",    ficlGetenv,	    FW_DEFAULT);
    dictAppendWord(dp, "unsetenv",  ficlUnsetenv,   FW_DEFAULT);
    dictAppendWord(dp, "copyin",    ficlCopyin,	    FW_DEFAULT);
    dictAppendWord(dp, "copyout",   ficlCopyout,    FW_DEFAULT);
    dictAppendWord(dp, "findfile",  ficlFindfile,   FW_DEFAULT);
#ifdef HAVE_PNP
    dictAppendWord(dp, "pnpdevices",ficlPnpdevices, FW_DEFAULT);
    dictAppendWord(dp, "pnphandlers",ficlPnphandlers, FW_DEFAULT);
#endif
    dictAppendWord(dp, "ccall",	    ficlCcall,	    FW_DEFAULT);
#endif

#if defined(PC98)
    ficlSetEnv(pSys, "arch-pc98",         FICL_TRUE);
#elif defined(__i386__)
    ficlSetEnv(pSys, "arch-i386",         FICL_TRUE);
    ficlSetEnv(pSys, "arch-alpha",        FICL_FALSE);
    ficlSetEnv(pSys, "arch-ia64",         FICL_FALSE);
    ficlSetEnv(pSys, "arch-powerpc",      FICL_FALSE);
#elif defined(__alpha__)
    ficlSetEnv(pSys, "arch-i386",         FICL_FALSE);
    ficlSetEnv(pSys, "arch-alpha",        FICL_TRUE);
    ficlSetEnv(pSys, "arch-ia64",         FICL_FALSE);
    ficlSetEnv(pSys, "arch-powerpc",      FICL_FALSE);
#elif defined(__ia64__)
    ficlSetEnv(pSys, "arch-i386",         FICL_FALSE);
    ficlSetEnv(pSys, "arch-alpha",        FICL_FALSE);
    ficlSetEnv(pSys, "arch-ia64",         FICL_TRUE);
    ficlSetEnv(pSys, "arch-powerpc",      FICL_FALSE);
#elif defined(__powerpc__)
    ficlSetEnv(pSys, "arch-i386",         FICL_FALSE);
    ficlSetEnv(pSys, "arch-alpha",        FICL_FALSE);
    ficlSetEnv(pSys, "arch-ia64",         FICL_FALSE);
    ficlSetEnv(pSys, "arch-powerpc",      FICL_TRUE);
#endif

    return;
}

