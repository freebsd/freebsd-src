/*-
 * Copyright (c) 1995-1996 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *	$Id: imgact_elf.h,v 1.2 1996/10/16 17:51:04 sos Exp $
 */

#ifndef _SYS_IMGACT_ELF_H_
#define _SYS_IMGACT_ELF_H_

#include <machine/elf.h>

#ifdef KERNEL

extern int elf_trace;

#define AUXARGS_ENTRY(pos, id, val) {suword(pos++, id); suword(pos++, val);}

/*
 * Structure used to pass infomation from the loader to the
 * stack fixup routine.
 */
typedef struct {
	Elf32_Sword	execfd;
	Elf32_Word	phdr;
	Elf32_Word	phent;
	Elf32_Word	phnum;
	Elf32_Word	pagesz;
	Elf32_Word	base;
	Elf32_Word	flags;
	Elf32_Word	entry;
	Elf32_Word	trace;
} Elf32_Auxargs;

typedef struct {
	char *brand;
	char *emul_path;
	char *interp_path;
        struct sysentvec *sysvec;
} Elf32_Brandinfo;

#define MAX_BRANDS      8

int elf_insert_brand_entry __P((Elf32_Brandinfo *entry));
int elf_remove_brand_entry __P((Elf32_Brandinfo *entry));

#endif /* KERNEL */

#endif /* !_SYS_IMGACT_ELF_H_ */
