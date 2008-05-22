/*-
 * Copyright (c) 2008 John Birrell (jb@freebsd.org)
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

#include "_libproc.h"
#include <stdio.h>

char *
proc_objname(struct proc_handle *p, uintptr_t addr, char *objname,
    size_t objnamesz)
{
printf("%s(%d): Not implemented. p %p addr 0x%lx objname %p objnamesz %zd\n",__func__,__LINE__,p,(u_long) addr,objname,objnamesz);
	return (NULL);
}

const prmap_t *
proc_addr2map(struct proc_handle *p, uintptr_t addr)
{
printf("%s(%d): Not implemented. p %p addr 0x%lx\n",__func__,__LINE__,p,(u_long) addr);
	return (NULL);
}

int
proc_addr2sym(struct proc_handle *p, uintptr_t addr, char *name,
    size_t namesz, GElf_Sym *sym)
{
printf("%s(%d): Not implemented. p %p addr 0x%lx name %p namesz %zd sym %p\n",__func__,__LINE__,p,(u_long) addr,name,namesz,sym);
	return (0);
}

const prmap_t *
proc_name2map(struct proc_handle *p, const char *name)
{
printf("%s(%d): Not implemented. p %p name %p\n",__func__,__LINE__,p,name);
	return (NULL);
}

int
proc_name2sym(struct proc_handle *p, const char *object, const char *symbol,
    GElf_Sym *sym)
{
printf("%s(%d): Not implemented. p %p object %p symbol %p sym %p\n",__func__,__LINE__,p,object,symbol,sym);
	return (0);
}
