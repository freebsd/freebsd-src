/*-
 * Copyright (c) 2016 SRI International
 * Copyright (c) 2004 Doug Rabson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/types.h>

#include <machine/elf.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef __CHERI_PURE_CAPABILITY__
extern char **environ;
#define	ALTPTR
#else
extern Elf_Auxinfo *__auxargs;
#define	ALTPTR	"#"
#endif

static const char *auxtypenames[] = {
	[AT_BASE] = "AT_BASE",
	[AT_CANARY] = "AT_CANARY",
	[AT_CANARYLEN] = "AT_CANARYLEN",
#ifdef AT_DCACHEBSIZE
	[AT_DCACHEBSIZE] = "AT_DCACHEBSIZE",
#endif
	[AT_EGID] = "AT_EGID",
#ifdef AT_EHDRFLAGS
	[AT_EHDRFLAGS] = "AT_EHDRFLAGS",
#endif
	[AT_ENTRY] = "AT_ENTRY",
	[AT_EUID] = "AT_EUID",
	[AT_EXECFD] = "AT_EXECFD",
	[AT_EXECPATH] = "AT_EXECPATH",
	[AT_FLAGS] = "AT_FLAGS",
	[AT_GID] = "AT_GID",
#ifdef AT_ICACHEBSIZE
	[AT_ICACHEBSIZE] = "AT_ICACHEBSIZE",
#endif
	[AT_IGNORE] = "AT_IGNORE",
	[AT_NCPUS] = "AT_NCPUS",
	[AT_NOTELF] = "AT_NOTELF",
	[AT_NULL] = "AT_NULL",
	[AT_OSRELDATE] = "AT_OSRELDATE",
	[AT_PAGESIZES] = "AT_PAGESIZES",
	[AT_PAGESIZESLEN] = "AT_PAGESIZESLEN",
	[AT_PAGESZ] = "AT_PAGESZ",
	[AT_PHDR] = "AT_PHDR",
	[AT_PHENT] = "AT_PHENT",
	[AT_PHNUM] = "AT_PHNUM",
	[AT_STACKPROT] = "AT_STACKPROT",
	[AT_TIMEKEEP] = "AT_TIMEKEEP",
#ifdef AT_UCACHEBSIZE
	[AT_UCACHEBSIZE] = "AT_UCACHEBSIZE",
#endif
	[AT_UID] = "AT_UID",
};
_Static_assert(sizeof(auxtypenames) / sizeof(*auxtypenames) == AT_COUNT,
    "Wrong number of items in auxtypenames");

static void
usage(void)
{
	printf("usage: auxargs [-cpP]\n");
	printf(" -c	print stack canary\n");
	printf(" -p	print page size array\n");
	printf(" -P	print program header\n");
	exit (1);
}

int
main(int argc, char **argv)
{
	int ch, printphdr, printpagesizes, printcanary;
	size_t i;
	Elf_Auxinfo *aux, *auxp;
	const unsigned char *canary;
	Elf_Phdr *phdr;
	unsigned long *pagesizes;
	size_t canarylen, phnum, pagesizeslen;

#ifndef __CHERI_PURE_CAPABILITY__
	Elf_Addr *sp;
	sp = (Elf_Addr *) environ;
	while (*sp++ != 0)
		;
	aux = (Elf_Auxinfo *) sp;
#else
	aux = __auxargs;
#endif

	printcanary = printpagesizes = printphdr = 0;
	while ((ch = getopt(argc, argv, "cpP")) != -1) {
		switch (ch) {
		case 'c':
			printcanary = 1;
			break;
		case 'p':
			printpagesizes = 1;
			break;
		case 'P':
			printphdr = 1;
			break;
		default:
			usage();
		}
	}

	canary = NULL;
	pagesizes = NULL;
	phdr = NULL;
	canarylen = pagesizeslen = phnum = 0;
	for (auxp = aux; auxp->a_type != AT_NULL; auxp++) {
		printf("%s: ", auxtypenames[auxp->a_type]);
		switch (auxp->a_type) {
		case AT_BASE:
			printf("%" ALTPTR "p", auxp->a_un.a_ptr);
			break;
		case AT_CANARY:
			printf("%" ALTPTR "p", auxp->a_un.a_ptr);
			canary = auxp->a_un.a_ptr;
			break;
		case AT_CANARYLEN:
			printf("%zu", (size_t)auxp->a_un.a_val);
			canarylen = auxp->a_un.a_val;
			break;
#ifdef AT_DCACHEBSIZE
		case AT_DCACHEBSIZE:
			printf("0x%zx", (size_t)auxp->a_un.a_val);
			break;
#endif
		case AT_EGID:
			printf("%jd", (intmax_t)auxp->a_un.a_val);
			break;
#ifdef AT_EHDRFLAGS
		case AT_EHDRFLAGS:
			printf("0x%jx", (uintmax_t)auxp->a_un.a_val);
			break;
#endif
		case AT_ENTRY:
			printf("%" ALTPTR "p", auxp->a_un.a_ptr);
			break;
		case AT_EUID:
			printf("%jd", (intmax_t)auxp->a_un.a_val);
			break;
		case AT_EXECFD:
			printf("%jd", (intmax_t)auxp->a_un.a_val);
			break;
		case AT_EXECPATH:
			printf("%s", (char *)auxp->a_un.a_ptr);
			break;
		case AT_FLAGS:
			printf("0x%jx", (uintmax_t)auxp->a_un.a_val);
			break;
		case AT_GID:
			printf("%jd", (intmax_t)auxp->a_un.a_val);
			break;
#ifdef AT_ICACHEBSIZE
		case AT_ICACHEBSIZE:
			printf("0x%jx", (uintmax_t)auxp->a_un.a_val);
			break;
#endif
		case AT_IGNORE:
			break;
		case AT_NCPUS:
			printf("%jd", (intmax_t)auxp->a_un.a_val);
			break;
		case AT_NOTELF:
			break;
		case AT_OSRELDATE:
			printf("%jd", (intmax_t)auxp->a_un.a_val);
			break;
		case AT_PAGESIZES:
			printf("%" ALTPTR "p", auxp->a_un.a_ptr);
			pagesizes = auxp->a_un.a_ptr;
			break;
		case AT_PAGESIZESLEN:
			printf("%jd", (intmax_t)auxp->a_un.a_val);
			break;
		case AT_PAGESZ:
			printf("0x%jx", (uintmax_t)auxp->a_un.a_val);
			break;
		case AT_PHDR:
			printf("%" ALTPTR "p", auxp->a_un.a_ptr);
			phdr = auxp->a_un.a_ptr;
			break;
		case AT_PHENT:
			printf("%jd", (intmax_t)auxp->a_un.a_val);
			if (auxp->a_un.a_val != sizeof(*phdr)) {
				printf(" wrong size, not printing list");
				printphdr = 0;
			}
			break;
		case AT_PHNUM:
			printf("%jd", (intmax_t)auxp->a_un.a_val);
			phnum = auxp->a_un.a_val;
			break;
		case AT_STACKPROT:
			printf("0x%jx", (uintmax_t)auxp->a_un.a_val);
			break;
		case AT_TIMEKEEP:
			printf("%" ALTPTR "p", auxp->a_un.a_ptr);
			break;
#ifdef AT_UCACHEBSIZE
		case AT_UCACHEBSIZE:
			printf("0x%jx", (uintmax_t)auxp->a_un.a_val);
			break;
#endif
		case AT_UID:
			printf("%jd", (intmax_t)auxp->a_un.a_val);
			break;
		}
		printf("\n");
	}

	if (printcanary && canary != NULL) {
		printf("\ncanary: 0x");
		for (i = 0; i < canarylen; i++)
			printf("%02x", canary[i]);
		printf("\n");
	}

	if (printpagesizes && pagesizes != NULL) {
		printf("\npagesizes:");
		for (i = 0; i <= pagesizeslen / sizeof(*pagesizes); i++)
			printf(" 0x%lx", pagesizes[i]);
		printf("\n");
	}

	if (printphdr && phdr != NULL) {
		printf("\nprogram headers\n");
		for (i = 0; i < phnum; i++) {
			printf("\nentry: %zu\n", i);
			printf("\tp_type: %jx\n",
			    (uintmax_t)phdr[i].p_type);
			printf("\tp_offset: %ju\n",
			    (uintmax_t)phdr[i].p_offset);
			printf("\tp_vaddr: 0x%jx\n",
			    (uintmax_t)phdr[i].p_vaddr);
			printf("\tp_paddr: 0x%jx\n",
			    (uintmax_t)phdr[i].p_paddr);
			printf("\tp_filesz: 0x%jx\n",
			    (uintmax_t)phdr[i].p_filesz);
			printf("\tp_memsz: 0x%jx\n",
			    (uintmax_t)phdr[i].p_memsz);
			printf("\tp_flags: 0x%jx\n",
			    (uintmax_t)phdr[i].p_flags);
			printf("\tp_align: %ju\n",
			    (uintmax_t)phdr[i].p_offset);
		}
	}

	return (0);
}
