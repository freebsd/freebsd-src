/*-
 * Copyright (c) 2014-2015 SRI International
 * Copyright (c) 2015-2016 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
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
#include <sys/mman.h>
#include <sys/param.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>

#include <assert.h>
#include <elf.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sandbox_methods.h"

/*
 * Description of a method provided by a sandbox to be called via ccall.
 * This data is used to create the 'sandbox class' vtable, resolve symbols
 * to fill in caller .CHERI_CALLER variables, and store the corresponding
 * method numbers in the .CHERI_CALLEE variables of each 'sandbox object'.
 */
struct sandbox_provided_method {
	char		*spm_method;		/* Method name */
	vm_offset_t	 spm_index_offset;	/* Offset of callee variable */
};

/*
 * List of methods provided by a sandbox.  Sandbox method numbers (and
 * by extension vtable indexs) are defined by method position in the
 * spms_methods array.
 *
 * XXX: rename to sandbox_provided_class
 */
struct sandbox_provided_methods {
	char				*spms_class;	/* Class name */
	size_t				 spms_nmethods;	/* Number of methods */
	size_t				 spms_maxmethods; /* Array size */
	struct sandbox_provided_method	*spms_methods;	/* Array of methods */
};

/*
 * List of classes provided by a sandbox binary.  Each binary can
 * support one or more classes.  No two binaries can provide the same
 * class as vtable offsets would be inconsistant.
 */
struct sandbox_provided_classes {
	struct sandbox_provided_methods	**spcs_classes;	/* Class pointers */
	size_t				  spcs_nclasses; /* Number of methods */
	size_t				  spcs_maxclasses; /* Array size */
	size_t				  spcs_nmethods; /* Total methods */
	vm_offset_t			  spcs_base;	/* Base of vtable */
};

/*
 * Description of a method required by a sandbox to be called by ccall.
 */
struct sandbox_required_method {
	char		*srm_class;		/* Class name */
	char		*srm_method;		/* Method name */
	vm_offset_t	 srm_index_offset;	/* Offset of caller variable */
	vm_offset_t	 srm_vtable_offset;	/* Method number */
	bool		 srm_resolved;		/* Resolved? */
};

/*
 * List of methods required by a sandbox (or program).  Sandbox objects
 * must not be created until all symbols are resolved.
 */
struct sandbox_required_methods {
	size_t				 srms_nmethods;	/* Number of methods */
	size_t				 srms_unresolved_methods; /* Number */
	struct sandbox_required_method	*srms_methods;	/* Array of methods */
};

static void	sandbox_free_provided_methods(
		    struct sandbox_provided_methods *provided_methods);

#define	CHERI_CALLEE_SYM_PREFIX	"__cheri_callee_method."
#define	CHERI_CALLER_SYM_PREFIX	"__cheri_method."

extern int sb_verbose;

#define SB_GET_CLASS_ALLOC	0x1
static struct sandbox_provided_methods *
sandbox_get_class_flag(struct sandbox_provided_classes *pcs,
    const char *class, int flags)
{
	struct sandbox_provided_methods *pms;
	size_t i;

	assert(class != NULL);
	assert(pcs->spcs_nclasses <= pcs->spcs_maxclasses);

	for (i = 0; i < pcs->spcs_nclasses; i++)
		if (strcmp(pcs->spcs_classes[i]->spms_class, class) == 0)
			return(pcs->spcs_classes[i]);

	if (!(flags & SB_GET_CLASS_ALLOC))
		return (NULL);
	if (pcs->spcs_nclasses == pcs->spcs_maxclasses) {
		if (pcs->spcs_maxclasses == 0)
			pcs->spcs_maxclasses = 4;
		else
			pcs->spcs_maxclasses *= 2;
		pcs->spcs_classes = reallocf(pcs->spcs_classes,
		    pcs->spcs_maxclasses * sizeof(*pcs->spcs_classes));
		if (pcs->spcs_classes == NULL)	/* No recovery possible */
			err(1, "%s: realloc spcs_maxclasses", __func__);
		memset(pcs->spcs_classes + pcs->spcs_nclasses, 0,
		    (pcs->spcs_maxclasses - pcs->spcs_nclasses) *
		    sizeof(*pcs->spcs_classes));
	}
	if ((pms = calloc(1, sizeof(*pms))) == NULL) {
		warn("%s: calloc struct sandbox_provided_methods", __func__);
		return (NULL);
	}
	pms->spms_class = strdup(class);
	if (pms->spms_class == NULL) {
		warn("%s: strdup class name", __func__);
		free(pms);
		return (NULL);
	}
	pcs->spcs_classes[pcs->spcs_nclasses++] = pms;
	return (pms);
}

int
sandbox_parse_ccall_methods(int fd,
    struct sandbox_provided_classes **provided_classesp,
    struct sandbox_required_methods **required_methodsp)
{
	size_t i, nsyms;
	int cheri_caller_idx, cheri_callee_idx;
	size_t maxpmethods, npmethods;
	struct sandbox_provided_classes *pcs;
	struct sandbox_provided_method *pmethods;
	struct sandbox_provided_methods *pms;
	size_t maxrmethods, nrmethods;
	struct sandbox_required_method *rmethods = NULL;
	struct sandbox_required_methods *rms = NULL;
	char *class_name = NULL;
	size_t class_name_len;
	char *required_class_name = NULL;
	size_t required_class_name_len;
	char *sname;
	char *shstrtab = NULL;
	char *strtab = NULL;
	int maxoffset = 0;
	ssize_t rlen;
	Elf64_Ehdr ehdr;
	Elf64_Shdr shdr, shstrtabhdr;
	Elf64_Sym *symtab = NULL;

	if ((pcs = calloc(1, sizeof(*pcs))) == NULL) {
		warn("%s: calloc pcs", __func__);
		goto bad;
	}
	if ((rms = calloc(1, sizeof(*rms))) == NULL) {
		warn("%s: calloc rms", __func__);
		goto bad;
	}

	if (provided_classesp == NULL) {
		warnx("%s: provided_classesp must be non-null", __func__);
		goto bad;
	}
	if (required_methodsp == NULL) {
		warnx("%s: required_methodsp must be non-null", __func__);
		goto bad;
	}

	if ((rlen = pread(fd, &ehdr, sizeof(ehdr), 0)) != sizeof(ehdr)) {
		warn("%s: read ELF header", __func__);
		goto bad;
	}

	/* read section names header */
	if ((rlen = pread(fd, &shstrtabhdr, sizeof(shstrtabhdr), ehdr.e_shoff +
	    ehdr.e_shentsize * ehdr.e_shstrndx)) != sizeof(shstrtabhdr)) {
		warn("%s: reading string section header", __func__);
		return (-1);
	}
	if ((shstrtab = malloc(shstrtabhdr.sh_size)) == NULL) {
		warn("%s: malloc for string section header", __func__);
		return (-1);
	}
	if (pread(fd, shstrtab, shstrtabhdr.sh_size, shstrtabhdr.sh_offset) !=
	    (ssize_t)shstrtabhdr.sh_size) {
		warn("%s: reading string section", __func__);
		return (-1);
	}

	cheri_caller_idx = cheri_callee_idx = 0;
	for (i = 1; i < ehdr.e_shnum; i++) {	/* Skip hdr 0 */
		if ((rlen = pread(fd, &shdr, sizeof(shdr), ehdr.e_shoff +
		    ehdr.e_shentsize * i)) != sizeof(shdr)) {
			warn("%s: reading %zu section header", __func__, i+1);
			return (-1);
		}
		sname = shstrtab + shdr.sh_name;
#if defined(DEBUG) && DEBUG > 1
		printf("shdr[%zu] name     %s\n", i, sname);
		printf("shdr[%zu] type     %jx\n", i, (intmax_t)shdr.sh_type);
		if (shdr.sh_flags != 0)
			printf("shdr[%zu] flags    %jx\n", i,
			    (intmax_t)shdr.sh_flags);
		if (shdr.sh_addr != 0)
			printf("shdr[%zu] addr     %jx\n", i,
			    (intmax_t)shdr.sh_addr);
		printf("shdr[%zu] offset   %jx\n", i,
		    (intmax_t)shdr.sh_offset);
		printf("shdr[%zu] size     %jx\n", i,
		    (intmax_t)shdr.sh_size);
		if (shdr.sh_link != 0)
			printf("shdr[%zu] link     %jx\n", i,
			    (intmax_t)shdr.sh_link);
		if (shdr.sh_info != 0)
			printf("shdr[%zu] info     %jx\n", i,
			    (intmax_t)shdr.sh_info);
		printf("shdr[%zu] align    %jx\n", i,
		    (intmax_t)shdr.sh_addralign);
		printf("shdr[%zu] entsize  %jx\n", i,
		    (intmax_t)shdr.sh_entsize);
#endif

		if (shdr.sh_type == SHT_SYMTAB &&
		    strcmp(".symtab", sname) == 0) {
			if (symtab != NULL) {
				warnx("%s: second symtab found\n", __func__);
				goto bad;
			}
			if (shdr.sh_entsize != sizeof(Elf64_Sym)) {
				warnx("%s: unexpected symbol size.  "
				    "Expected %zu, got %zu", __func__,
				    sizeof(Elf64_Sym), (size_t)shdr.sh_entsize);
				goto bad;
			}
			if ((symtab = malloc(shdr.sh_size)) == NULL) {
				warn("%s: malloc symtab", __func__);
				goto bad;
			}
			if (pread(fd, symtab, shdr.sh_size, shdr.sh_offset) !=
			    (ssize_t)shdr.sh_size) {
				warn("%s: pread symtab", __func__);
				goto bad;
			}
			nsyms = shdr.sh_size / shdr.sh_entsize;
#ifdef DEBUG
			printf("loaded .symtab\n");
#endif
		} else if (shdr.sh_type == SHT_STRTAB &&
		    strcmp(".strtab", sname) == 0) {
			if (strtab != NULL) {
				warn("%s: second strtab found\n", __func__);
				goto bad;
			}
			if ((strtab = malloc(shdr.sh_size)) == NULL) {
				warn("%s: malloc strtab", __func__);
				goto bad;
			}
			if (pread(fd, strtab, shdr.sh_size, shdr.sh_offset) !=
			    (ssize_t)shdr.sh_size) {
				warn("%s: pread strtab", __func__);
				goto bad;
			}
#ifdef DEBUG
			printf("loaded .strtab\n");
#endif
		} else if (shdr.sh_type == SHT_PROGBITS &&
		    strcmp(".CHERI_CALLEE", sname) == 0) {
			pcs->spcs_base = shdr.sh_addr;
			cheri_callee_idx = i;
#ifdef DEBUG
			printf("found .CHERI_CALLEE\n");
#endif
		} else if (shdr.sh_type == SHT_PROGBITS &&
		    strcmp(".CHERI_CALLER", sname) == 0) {
			cheri_caller_idx = i;
#ifdef DEBUG
			printf("found .CHERI_CALLER\n");
#endif
		}
	}
	if (symtab == NULL) {
		warnx("%s: no .symtab section", __func__);
		goto bad;
	}
	if (strtab == NULL) {
		warnx("%s: no .strtab section", __func__);
		goto bad;
	}

	nrmethods = 0;

	/* No symbols provided or required */
	if (cheri_callee_idx == 0 && cheri_caller_idx == 0)
		goto good;

	/*
	 * Scan the symbol table for ccall methods we provide and methods
	 * we require;
	 */
	maxrmethods = 16;
	if ((rmethods = calloc(maxrmethods, sizeof(*rmethods))) == NULL) {
		warn("%s: calloc rmethods", __func__);
		goto bad;
	}
	for (i = 1; i < nsyms; i++) {
		sname = strtab + symtab[i].st_name;
#if defined(DEBUG) && DEBUG > 1
		printf("symtab[%d] name     %s\n", i, sname);
		printf("symtab[%d] section  %d\n", i, symtab[i].st_shndx);
		printf("symtab[%d] addr     0x%lx\n", i, symtab[i].st_value);
		printf("symtab[%d] size     0x%zx\n", i, symtab[i].st_size);
#endif
		if (symtab[i].st_name == 0)
			continue;
		if (cheri_callee_idx != 0 &&
		    symtab[i].st_shndx == cheri_callee_idx) {
			/* Check that the prefix is right */
			if (strncmp(sname, CHERI_CALLEE_SYM_PREFIX,
			    strlen(CHERI_CALLEE_SYM_PREFIX)) != 0) {
				warnx("%s: malformed .CHERI_CALLEE symbol '%s'",
				    __func__, sname);
				goto bad;
			}
			sname += strlen(CHERI_CALLEE_SYM_PREFIX);

			/* Extract the class name */
			class_name_len = strcspn(sname, ".");
			if ((class_name = strndup(sname,
			    class_name_len)) == NULL) {
				warn("%s: strdup", __func__);
				goto bad;
			}

			/* Check for the '.' after the class name */
			if (*(sname + class_name_len) != '.') {
				warnx("%s: malformed .CHERI_CALLEE "
				    "symbol '%s' missing second '.'",
				    __func__, sname);
				goto bad;
			}

			if ((pms = sandbox_get_class_flag(pcs, class_name,
			    SB_GET_CLASS_ALLOC)) == NULL)
				goto bad;
			pmethods = pms->spms_methods;
			npmethods = pms->spms_nmethods;
			maxpmethods = pms->spms_maxmethods;

			assert(npmethods <= maxpmethods);
			if (npmethods == maxpmethods) {
				if (maxpmethods == 0)
					maxpmethods = 16;
				else
					maxpmethods *= 2;
				if ((pmethods = reallocf(pmethods,
				    maxpmethods * sizeof(*pmethods))) == NULL) {
					warn("%s: realloc pmethods",
					    __func__);
					goto bad;
				}
				memset(pmethods + npmethods, 0,
				    (maxpmethods - npmethods) *
				    sizeof(*pmethods));
				pms->spms_methods = pmethods;
				pms->spms_maxmethods = maxpmethods;
			}
			assert(pmethods != NULL);

			if ((pmethods[npmethods].spm_method = strdup(sname +
			    class_name_len + 1)) == NULL) {
				warn("%s: strdup method name", __func__);
				goto bad;
			}
			pmethods[npmethods].spm_index_offset =
			    symtab[i].st_value;
#ifdef DEBUG
			printf("provided method: %s->%s (index offset 0x%lx)\n",
			    class_name,
			    pmethods[npmethods].spm_method,
			    pmethods[npmethods].spm_index_offset);
#endif
			pms->spms_nmethods++;
			pcs->spcs_nmethods++;
		} else if (cheri_caller_idx != 0 &&
		    symtab[i].st_shndx == cheri_caller_idx) {
			if (nrmethods >= maxrmethods) {
				maxrmethods *= 2;
				if ((rmethods = reallocf(rmethods,
				    maxrmethods * sizeof(*rmethods))) == NULL) {
					warn("%s: realloc rmethods",
					    __func__);
					goto bad;
				}
				memset(rmethods + nrmethods, 0,
				    (maxrmethods - nrmethods) * sizeof(*rmethods));
			}

			/* Check that the prefix is right */
			if (strncmp(sname, CHERI_CALLER_SYM_PREFIX,
			    strlen(CHERI_CALLER_SYM_PREFIX)) != 0) {
				warnx("%s: malformed .CHERI_CALLER symbol '%s'",
				    __func__, sname);
				goto bad;
			}
			sname += strlen(CHERI_CALLER_SYM_PREFIX);

			required_class_name_len = strcspn(sname, ".");
			if (*(sname + required_class_name_len) != '.' ||
			    *(sname + required_class_name_len + 1) == '\0') {
				warnx("%s: malformed .CHERI_CALLER symbol '%s'",
				    __func__, sname);
				goto bad;
			}
			if ((required_class_name = strndup(sname,
			    required_class_name_len)) == NULL) {
				warn("%s: strndup", __func__);
				goto bad;
			}
			rmethods[nrmethods].srm_class = required_class_name;
			if ((rmethods[nrmethods].srm_method = strdup(sname +
			    required_class_name_len + 1)) == NULL) {
				warn("%s: strdup", __func__);
				goto bad;
			}
			required_class_name = NULL; /* prevent double free */
			rmethods[nrmethods].srm_index_offset =
			    symtab[i].st_value;
#ifdef DEBUG
			printf("required method: %s class: %s "
			    "(index offset 0x%lx)\n",
			    rmethods[nrmethods].srm_method,
			    rmethods[nrmethods].srm_class,
			    rmethods[nrmethods].srm_index_offset);
#endif
			nrmethods++;
		}
	}

#ifdef DEBUG
	if (nrmethods > 0)
		printf("%zu methods required\n", nrmethods);
#endif

good:
	*provided_classesp = pcs;

	rms->srms_nmethods = nrmethods;
	rms->srms_unresolved_methods = nrmethods;
	if (nrmethods > 0)
		rms->srms_methods = rmethods;
	else {
		rms->srms_methods = NULL;
		free(rmethods);
	}
	*required_methodsp = rms;

	if (sandbox_resolve_methods(pcs, rms)
	    == -1) {
		warnx("%s: failed to resolve self called methods", __func__);
		goto bad;
	}

	free(shstrtab);
	free(symtab);
	free(strtab);
	return (maxoffset);

bad:
	free(shstrtab);
	free(symtab);
	free(strtab);
	free(class_name);
	if (rmethods != NULL) {
		for (i = 0; i < nrmethods; i++) {
			free(rmethods[i].srm_class);
			free(rmethods[i].srm_method);
		}
		free(rmethods);
	}
	sandbox_free_provided_classes(pcs);
	free(rms);
	return (-1);
}

static int
sandbox_resolve_methods_one_class(vm_offset_t vtable_base,
    struct sandbox_provided_methods *provided_methods,
    struct sandbox_required_methods *required_methods)
{
	size_t p, r, resolved = 0;
	struct sandbox_provided_method *pmethods;
	struct sandbox_required_method *rmethods;

	if (provided_methods == NULL) {
		warnx("%s: provided_methods must be non-NULL", __func__);
		return (-1);
	}
	if (required_methods == NULL) {
		warnx("%s: required_methods must be non-NULL", __func__);
		return (-1);
	}
	if (provided_methods->spms_nmethods == 0)
		return (0);
	if (required_methods->srms_unresolved_methods == 0)
		return (0);

	/*
	 * We won't get here if no class name as that only happens if
	 * there are no methods.
	 */
	assert(provided_methods->spms_class != NULL);

	pmethods = provided_methods->spms_methods;
	rmethods = required_methods->srms_methods;

	for (r = 0; r < required_methods->srms_nmethods; r++) {
		if (rmethods[r].srm_resolved)
			continue;
		if (strcmp(rmethods[r].srm_class,
		    provided_methods->spms_class) != 0)
			continue;
		for (p = 0; p < provided_methods->spms_nmethods; p++) {
			if (strcmp(rmethods[r].srm_method,
			    pmethods[p].spm_method) != 0)
				continue;
			rmethods[r].srm_vtable_offset =
			    pmethods[p].spm_index_offset - vtable_base;
			rmethods[r].srm_resolved = 1;
			resolved++;
#ifdef DEBUG
			printf("%s->%s resolved as method %lu\n",
			    rmethods[r].srm_class, rmethods[r].srm_method,
			    rmethods[r].srm_vtable_offset);
#endif
			break;
		}
	}
#ifdef DEBUG
	printf("resolved %zu symbols from class %s\n", resolved,
	    provided_methods->spms_class);
#endif
	required_methods->srms_unresolved_methods -= resolved;
	return (resolved);
}

int
sandbox_resolve_methods(struct sandbox_provided_classes *provided_classes,
    struct sandbox_required_methods *required_methods)
{
	size_t i, resolved = 0;

	for (i = 0; i < provided_classes->spcs_nclasses; i++)
		resolved += sandbox_resolve_methods_one_class(
		    provided_classes->spcs_base,
		    provided_classes->spcs_classes[i], required_methods);

	return (resolved);
}

void
sandbox_free_required_methods(struct sandbox_required_methods *required_methods)
{
	size_t i, nrmethods;
	struct sandbox_required_method *rmethods;

	nrmethods = required_methods->srms_nmethods;
	rmethods = required_methods->srms_methods;
	if (rmethods != NULL) {
		for (i = 0; i < nrmethods; i++) {
			free(rmethods[i].srm_class);
			free(rmethods[i].srm_method);
		}
		free(rmethods);
	}
	free(required_methods);
}

static void
sandbox_free_provided_methods(struct sandbox_provided_methods *provided_methods)
{
	size_t i, npmethods;
	struct sandbox_provided_method *pmethods;

	assert(provided_methods != NULL);

	npmethods = provided_methods->spms_nmethods;
	pmethods = provided_methods->spms_methods;
	if (pmethods != NULL) {
		for (i = 0; i < npmethods; i++)
			free(pmethods[i].spm_method);
		free(pmethods);
	}
	free(provided_methods->spms_class);
	free(provided_methods);
}

void
sandbox_free_provided_classes(struct sandbox_provided_classes *provided_classes)
{
	size_t i;

	assert(provided_classes != NULL);

	for (i = 0; i < provided_classes->spcs_nclasses; i++)
		sandbox_free_provided_methods(provided_classes->spcs_classes[i]);
	free(provided_classes->spcs_classes);
	free(provided_classes);
}

size_t
sandbox_get_unresolved_methods(
    struct sandbox_required_methods *required_methods)
{

	assert(required_methods != NULL);

	return(required_methods->srms_unresolved_methods);
}

void
sandbox_warn_unresolved_methods(
    struct sandbox_required_methods *required_methods)
{
	size_t i, nrmethods;
	struct sandbox_required_method *rmethods;

	assert(required_methods);

	rmethods = required_methods->srms_methods;
	nrmethods = required_methods->srms_nmethods;
	for (i = 0; i < nrmethods; i++) {
		if (rmethods[i].srm_resolved)
			continue;
		warnx("%s: Unresolved method: %s->%s", __func__,
		    rmethods[i].srm_class, rmethods[i].srm_method);
	}
}

__capability vm_offset_t *
sandbox_make_vtable(void *dataptr, const char *class,
    struct sandbox_provided_classes *provided_classes)
{
	__capability vm_offset_t *vtable;
	vm_offset_t *cheri_ccallee_base;
	size_t i, index, length, m;
	struct sandbox_provided_method *pm;
	struct sandbox_provided_methods *pms;

	if (provided_classes->spcs_nclasses == 0)
		return (NULL);

	if ((vtable = (__capability vm_offset_t *)calloc(
	    provided_classes->spcs_nmethods, sizeof(*vtable))) == NULL) {
		warnx("%s: calloc", __func__);
		return (NULL);
	}

#ifdef __CHERI_PURE_CAPABILITY__
	/*
	 * XXXRW: For system classes, a NULL dataptr is passed in, signifying
	 * that bases are relative to virtual address 0x0.  This isn't really
	 * the right thing for CheriABI, where we should instead pass in the
	 * default capability with an offset of zero.  For now, handle that
	 * here, but probably the caller should do that instead.
	 */
	if (dataptr == NULL)
		dataptr = cheri_getdefault();
#endif
	cheri_ccallee_base = (vm_offset_t *)dataptr + provided_classes->spcs_base
	    / sizeof(vm_offset_t);
	length = provided_classes->spcs_nmethods * sizeof(*vtable);

	if (class == NULL) {
		memcpy_c_tocap(vtable, cheri_ccallee_base, length);
		return (cheri_andperm(vtable, CHERI_PERM_LOAD));
	}

	for (i = 0; i < provided_classes->spcs_nclasses; i++) {
		pms = provided_classes->spcs_classes[i];
		if (strcmp(pms->spms_class, class) != 0)
			continue;

		for (m = 0; m < pms->spms_nmethods; m++) {
			pm = pms->spms_methods + m;
			index = (pm->spm_index_offset -
			    provided_classes->spcs_base) / sizeof(*vtable);
			vtable[index] = cheri_ccallee_base[index];
		}
		return (cheri_andperm(vtable, CHERI_PERM_LOAD));
	}
	free((void *)vtable);
	return (NULL);
}

int
sandbox_set_required_method_variables(__capability void *datacap,
    struct sandbox_required_methods *required_methods)
{
	size_t i;
	__capability vm_offset_t *method_var_p;
	struct sandbox_required_method *rmethods;

	assert(required_methods != NULL);
	rmethods = required_methods->srms_methods;

	/* Ensure the capability is capability aligned. */
	assert(!(cheri_getbase(datacap) & (sizeof(datacap) - 1)));

	for (i = 0; i < required_methods->srms_nmethods; i++) {
		if (!rmethods[i].srm_resolved)
			continue;

		/* Zero offsets can't be sane. */
		assert(rmethods[i].srm_index_offset != 0);

		method_var_p = cheri_setoffset(datacap,
		    rmethods[i].srm_index_offset);
		*method_var_p = rmethods[i].srm_vtable_offset;
	}
	return(0);
}

#ifdef TEST_METHODS
int
main(int argc, char **argv)
{
	struct sandbox_provided_methods *provided_methods;
	struct sandbox_required_methods *required_methods;
	int fd;

	if (argc != 2)
		errx(1, "usage: elf_methods <file>");

	if ((fd = open(argv[1], O_RDONLY)) == -1)
		err(1, "%s: open(%s)", __func__, argv[1]);

	if (sandbox_parse_ccall_methods(fd, &provided_methods,
	    &required_methods) == -1)
		err(1, "%s: sandbox_parse_ccall_methods", __func__);
	if (sandbox_resolve_methods(provided_methods, required_methods) == -1)
		err(1, "%s: sandbox_resolve_symbols", __func__);
	printf("%zu unresolved methods\n",
	    required_methods->srms_unresolved_methods);
	sandbox_free_provided_methods(provided_methods);
	sandbox_free_required_methods(required_methods);

	return (0);
}
#endif
