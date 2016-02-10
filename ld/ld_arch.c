/*-
 * Copyright (c) 2011,2012 Kai Wang
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

#include "ld.h"
#include "ld_arch.h"
#include "i386.h"
#include "amd64.h"
#include "mips.h"

ELFTC_VCSID("$Id: ld_arch.c 3281 2015-12-11 21:39:23Z kaiwang27 $");

#define	LD_DEFAULT_ARCH		"amd64"

static struct ld_arch *_get_arch_from_target(struct ld *ld, char *target);

void
ld_arch_init(struct ld *ld)
{
	char *end;
	char arch[MAX_ARCH_NAME_LEN + 1], target[MAX_TARGET_NAME_LEN + 1];
	size_t len;

	/*
	 * Register supported architectures.
	 */

	i386_register(ld);
	amd64_register(ld);
	mips_register(ld);

	/*
	 * Find out default arch for output object.
	 */

	if ((end = strrchr(ld->ld_progname, '-')) != NULL &&
	    end != ld->ld_progname) {
		len = end - ld->ld_progname + 1;
		if (len > MAX_TARGET_NAME_LEN)
			return;
		strncpy(target, ld->ld_progname, len);
		target[len] = '\0';
		ld->ld_arch = _get_arch_from_target(ld, target);
	}

	if (ld->ld_arch == NULL) {
		snprintf(arch, sizeof(arch), "%s", LD_DEFAULT_ARCH);
		ld->ld_arch = ld_arch_find(ld, arch);
		if (ld->ld_arch == NULL)
			ld_fatal(ld, "Internal: could not determine output"
			    " object architecture");
	}
}

void
ld_arch_set(struct ld *ld, char *arch)
{

	ld->ld_arch = ld_arch_find(ld, arch);
	if (ld->ld_arch == NULL)
		ld_fatal(ld, "arch '%s' is not supported", arch);
}

void
ld_arch_set_from_target(struct ld *ld)
{

	if (ld->ld_otgt != NULL) {
		ld->ld_arch = _get_arch_from_target(ld, ld->ld_otgt_name);
		if (ld->ld_arch == NULL)
			ld_fatal(ld, "target '%s' is not supported",
			    ld->ld_otgt_name);
	}
}

int
ld_arch_equal(struct ld_arch *a1, struct ld_arch *a2)
{

	assert(a1 != NULL && a2 != NULL);

	if (a1 == a2)
		return (1);

	if (a1->alias == a2 || a2->alias == a1)
		return (1);

	if (a1->alias != NULL && a1->alias == a2->alias)
		return (1);

	return (0);
}

void
ld_arch_verify(struct ld *ld, const char *name, int mach, int endian,
	unsigned flags)
{
	struct ld_arch *la;
	struct ld_state *ls;

	assert(ld->ld_arch != NULL);
	ls = &ld->ld_state;

	if ((la = ld_arch_guess_arch_name(ld, mach, endian)) == NULL)
		ld_fatal(ld, "%s: ELF object architecture %#x not supported",
		    name, mach);

	if (!ld_arch_equal(la, ld->ld_arch)) {
		ls->ls_arch_conflict = 1;
		if (ls->ls_rerun || !ls->ls_first_elf_object)
			ld_fatal(ld, "%s: ELF object architecture `%s' "
			    "conflicts with output object architecture `%s'",
			    name, la->name, ld->ld_arch->name);
		ld->ld_arch = la;
	}

	if (ls->ls_first_elf_object) {
		la->flags = flags;
	} else if (la->merge_flags) {
		la->merge_flags(ld, flags);
	}

	ls->ls_first_elf_object = 0;
}

struct ld_arch *
ld_arch_guess_arch_name(struct ld *ld, int mach, int endian)
{
	char arch[MAX_ARCH_NAME_LEN + 1];

	/* TODO: we should also consider elf class and endianess. */

	switch (mach) {
	case EM_386:
		snprintf(arch, sizeof(arch), "%s", "i386");
		break;
	case EM_ARM:
		snprintf(arch, sizeof(arch), "%s", "arm");
		break;
	case EM_MIPS:
	case EM_MIPS_RS3_LE:
		snprintf(arch, sizeof(arch), "%s",
		    endian==ELFDATA2MSB ? "bigmips" : "littlemips");
		break;
	case EM_PPC:
	case EM_PPC64:
		snprintf(arch, sizeof(arch), "%s", "ppc");
		break;
	case EM_SPARC:
	case EM_SPARCV9:
		snprintf(arch, sizeof(arch), "%s", "sparc");
		break;
	case EM_X86_64:
		snprintf(arch, sizeof(arch), "%s", "amd64");
		break;
	default:
		return (NULL);
	}

	return (ld_arch_find(ld, arch));
}

static struct ld_arch *
_get_arch_from_target(struct ld *ld, char *target)
{
	struct ld_arch *la;
	char *begin, *end, name[MAX_TARGET_NAME_LEN + 1];

	if ((begin = strchr(target, '-')) == NULL) {
		la = ld_arch_find(ld, target);
		return (la);
	}
	la = ld_arch_find(ld, begin + 1);
	if (la != NULL)
		return (la);

	strncpy(name, begin + 1, sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';
	while ((end = strrchr(name, '-')) != NULL) {
		*end = '\0';
		la = ld_arch_find(ld, name);
		if (la != NULL)
			return (la);
	}

	return (NULL);
}

struct ld_arch *
ld_arch_find(struct ld *ld, char *arch)
{
	struct ld_arch *la;

	HASH_FIND_STR(ld->ld_arch_list, arch, la);

	return (la);
}
