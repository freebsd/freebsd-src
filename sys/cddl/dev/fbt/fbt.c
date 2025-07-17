/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 * Portions Copyright 2006-2008 John Birrell jb@freebsd.org
 *
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cpuvar.h>
#include <sys/endian.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/limits.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/selinfo.h>
#include <sys/smp.h>
#include <sys/stdarg.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/uio.h>
#include <sys/unistd.h>

#include <sys/dtrace.h>
#include <sys/dtrace_bsd.h>

#include "fbt.h"

MALLOC_DEFINE(M_FBT, "fbt", "Function Boundary Tracing");

dtrace_provider_id_t	fbt_id;
fbt_probe_t		**fbt_probetab;
int			fbt_probetab_mask;

static int	fbt_unload(void);
static void	fbt_getargdesc(void *, dtrace_id_t, void *, dtrace_argdesc_t *);
static void	fbt_provide_module(void *, modctl_t *);
static void	fbt_destroy(void *, dtrace_id_t, void *);
static void	fbt_enable(void *, dtrace_id_t, void *);
static void	fbt_disable(void *, dtrace_id_t, void *);
static void	fbt_load(void *);
static void	fbt_suspend(void *, dtrace_id_t, void *);
static void	fbt_resume(void *, dtrace_id_t, void *);

static dtrace_pattr_t fbt_attr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

static dtrace_pops_t fbt_pops = {
	.dtps_provide =		NULL,
	.dtps_provide_module =	fbt_provide_module,
	.dtps_enable =		fbt_enable,
	.dtps_disable =		fbt_disable,
	.dtps_suspend =		fbt_suspend,
	.dtps_resume =		fbt_resume,
	.dtps_getargdesc =	fbt_getargdesc,
	.dtps_getargval =	NULL,
	.dtps_usermode =	NULL,
	.dtps_destroy =		fbt_destroy
};

static int			fbt_probetab_size;
static int			fbt_verbose = 0;

int
fbt_excluded(const char *name)
{

	if (strncmp(name, "dtrace_", 7) == 0 &&
	    strncmp(name, "dtrace_safe_", 12) != 0) {
		/*
		 * Anything beginning with "dtrace_" may be called
		 * from probe context unless it explicitly indicates
		 * that it won't be called from probe context by
		 * using the prefix "dtrace_safe_".
		 */
		return (1);
	}

	/*
	 * Omit instrumentation of functions that are probably in DDB.  It
	 * makes it too hard to debug broken FBT.
	 *
	 * NB: kdb_enter() can be excluded, but its call to printf() can't be.
	 * This is generally OK since we're not yet in debugging context.
	 */
	if (strncmp(name, "db_", 3) == 0 ||
	    strncmp(name, "kdb_", 4) == 0)
		return (1);

	/*
	 * Lock owner methods may be called from probe context.
	 */
	if (strcmp(name, "owner_mtx") == 0 ||
	    strcmp(name, "owner_rm") == 0 ||
	    strcmp(name, "owner_rw") == 0 ||
	    strcmp(name, "owner_sx") == 0)
		return (1);

	/*
	 * The KMSAN runtime can't be instrumented safely.
	 */
	if (strncmp(name, "__msan", 6) == 0 ||
	    strncmp(name, "kmsan_", 6) == 0)
		return (1);

	/*
	 * Stack unwinders may be called from probe context on some
	 * platforms.
	 */
#if defined(__aarch64__) || defined(__riscv)
	if (strcmp(name, "unwind_frame") == 0)
		return (1);
#endif

	/*
	 * When DTrace is built into the kernel we need to exclude
	 * the FBT functions from instrumentation.
	 */
#ifndef _KLD_MODULE
	if (strncmp(name, "fbt_", 4) == 0)
		return (1);
#endif

	return (0);
}

static void
fbt_doubletrap(void)
{
	fbt_probe_t *fbt;
	int i;

	for (i = 0; i < fbt_probetab_size; i++) {
		fbt = fbt_probetab[i];

		for (; fbt != NULL; fbt = fbt->fbtp_probenext)
			fbt_patch_tracepoint(fbt, fbt->fbtp_savedval);
	}
}

static void
fbt_provide_module(void *arg, modctl_t *lf)
{
	char modname[MAXPATHLEN];
	int i;
	size_t len;

	strlcpy(modname, lf->filename, sizeof(modname));
	len = strlen(modname);
	if (len > 3 && strcmp(modname + len - 3, ".ko") == 0)
		modname[len - 3] = '\0';

	/*
	 * Employees of dtrace and their families are ineligible.  Void
	 * where prohibited.
	 */
	if (strcmp(modname, "dtrace") == 0)
		return;

	/*
	 * To register with DTrace, a module must list 'dtrace' as a
	 * dependency in order for the kernel linker to resolve
	 * symbols like dtrace_register(). All modules with such a
	 * dependency are ineligible for FBT tracing.
	 */
	for (i = 0; i < lf->ndeps; i++)
		if (strncmp(lf->deps[i]->filename, "dtrace", 6) == 0)
			return;

	if (lf->fbt_nentries) {
		/*
		 * This module has some FBT entries allocated; we're afraid
		 * to screw with it.
		 */
		return;
	}

	/*
	 * List the functions in the module and the symbol values.
	 */
	(void) linker_file_function_listall(lf, fbt_provide_module_function, modname);
}

static void
fbt_destroy_one(fbt_probe_t *fbt)
{
	fbt_probe_t *hash, *hashprev, *next;
	int ndx;

	ndx = FBT_ADDR2NDX(fbt->fbtp_patchpoint);
	for (hash = fbt_probetab[ndx], hashprev = NULL; hash != NULL;
	    hashprev = hash, hash = hash->fbtp_hashnext) {
		if (hash == fbt) {
			if ((next = fbt->fbtp_tracenext) != NULL)
				next->fbtp_hashnext = hash->fbtp_hashnext;
			else
				next = hash->fbtp_hashnext;
			if (hashprev != NULL)
				hashprev->fbtp_hashnext = next;
			else
				fbt_probetab[ndx] = next;
			goto free;
		} else if (hash->fbtp_patchpoint == fbt->fbtp_patchpoint) {
			for (next = hash; next->fbtp_tracenext != NULL;
			    next = next->fbtp_tracenext) {
				if (fbt == next->fbtp_tracenext) {
					next->fbtp_tracenext =
					    fbt->fbtp_tracenext;
					goto free;
				}
			}
		}
	}
	panic("probe %p not found in hash table", fbt);
free:
	free(fbt, M_FBT);
}

static void
fbt_destroy(void *arg, dtrace_id_t id, void *parg)
{
	fbt_probe_t *fbt = parg, *next;
	modctl_t *ctl;

	do {
		ctl = fbt->fbtp_ctl;
		ctl->fbt_nentries--;

		next = fbt->fbtp_probenext;
		fbt_destroy_one(fbt);
		fbt = next;
	} while (fbt != NULL);
}

static void
fbt_enable(void *arg, dtrace_id_t id, void *parg)
{
	fbt_probe_t *fbt = parg;
	modctl_t *ctl = fbt->fbtp_ctl;

	ctl->nenabled++;

	/*
	 * Now check that our modctl has the expected load count.  If it
	 * doesn't, this module must have been unloaded and reloaded -- and
	 * we're not going to touch it.
	 */
	if (ctl->loadcnt != fbt->fbtp_loadcnt) {
		if (fbt_verbose) {
			printf("fbt is failing for probe %s "
			    "(module %s reloaded)",
			    fbt->fbtp_name, ctl->filename);
		}

		return;
	}

	for (; fbt != NULL; fbt = fbt->fbtp_probenext) {
		fbt_patch_tracepoint(fbt, fbt->fbtp_patchval);
		fbt->fbtp_enabled++;
	}
}

static void
fbt_disable(void *arg, dtrace_id_t id, void *parg)
{
	fbt_probe_t *fbt = parg, *hash;
	modctl_t *ctl = fbt->fbtp_ctl;

	ASSERT(ctl->nenabled > 0);
	ctl->nenabled--;

	if ((ctl->loadcnt != fbt->fbtp_loadcnt))
		return;

	for (; fbt != NULL; fbt = fbt->fbtp_probenext) {
		fbt->fbtp_enabled--;

		for (hash = fbt_probetab[FBT_ADDR2NDX(fbt->fbtp_patchpoint)];
		    hash != NULL; hash = hash->fbtp_hashnext) {
			if (hash->fbtp_patchpoint == fbt->fbtp_patchpoint) {
				for (; hash != NULL; hash = hash->fbtp_tracenext)
					if (hash->fbtp_enabled > 0)
						break;
				break;
			}
		}
		if (hash == NULL)
			fbt_patch_tracepoint(fbt, fbt->fbtp_savedval);
	}
}

static void
fbt_suspend(void *arg, dtrace_id_t id, void *parg)
{
	fbt_probe_t *fbt = parg;
	modctl_t *ctl = fbt->fbtp_ctl;

	ASSERT(ctl->nenabled > 0);

	if ((ctl->loadcnt != fbt->fbtp_loadcnt))
		return;

	for (; fbt != NULL; fbt = fbt->fbtp_probenext)
		fbt_patch_tracepoint(fbt, fbt->fbtp_savedval);
}

static void
fbt_resume(void *arg, dtrace_id_t id, void *parg)
{
	fbt_probe_t *fbt = parg;
	modctl_t *ctl = fbt->fbtp_ctl;

	ASSERT(ctl->nenabled > 0);

	if ((ctl->loadcnt != fbt->fbtp_loadcnt))
		return;

	for (; fbt != NULL; fbt = fbt->fbtp_probenext)
		fbt_patch_tracepoint(fbt, fbt->fbtp_patchval);
}

static int
fbt_ctfoff_init(modctl_t *lf, linker_ctf_t *lc)
{
	const Elf_Sym *symp = lc->symtab;
	const ctf_header_t *hp = (const ctf_header_t *) lc->ctftab;
	const uint8_t *ctfdata = lc->ctftab + sizeof(ctf_header_t);
	size_t idwidth;
	int i;
	uint32_t *ctfoff;
	uint32_t objtoff = hp->cth_objtoff;
	uint32_t funcoff = hp->cth_funcoff;
	uint_t kind, info, vlen;

	/* Sanity check. */
	if (hp->cth_magic != CTF_MAGIC) {
		printf("Bad magic value in CTF data of '%s'\n",lf->pathname);
		return (EINVAL);
	}

	if (lc->symtab == NULL) {
		printf("No symbol table in '%s'\n",lf->pathname);
		return (EINVAL);
	}

	ctfoff = malloc(sizeof(uint32_t) * lc->nsym, M_LINKER, M_WAITOK);
	*lc->ctfoffp = ctfoff;

	idwidth = hp->cth_version == CTF_VERSION_2 ? 2 : 4;

	for (i = 0; i < lc->nsym; i++, ctfoff++, symp++) {
		if (symp->st_name == 0 || symp->st_shndx == SHN_UNDEF) {
			*ctfoff = 0xffffffff;
			continue;
		}

		switch (ELF_ST_TYPE(symp->st_info)) {
		case STT_OBJECT:
			if (objtoff >= hp->cth_funcoff ||
			    (symp->st_shndx == SHN_ABS && symp->st_value == 0)) {
				*ctfoff = 0xffffffff;
				break;
			}

			*ctfoff = objtoff;
			objtoff += idwidth;
			break;

		case STT_FUNC:
			if (funcoff >= hp->cth_typeoff) {
				*ctfoff = 0xffffffff;
				break;
			}

			*ctfoff = funcoff;

			info = 0;
			memcpy(&info, ctfdata + funcoff, idwidth);
			if (hp->cth_version == CTF_VERSION_2) {
				kind = CTF_V2_INFO_KIND(info);
				vlen = CTF_V2_INFO_VLEN(info);
			} else {
				kind = CTF_V3_INFO_KIND(info);
				vlen = CTF_V3_INFO_VLEN(info);
			}

			/*
			 * If we encounter a zero pad at the end, just skip it.
			 * Otherwise skip over the function and its return type
			 * (+2) and the argument list (vlen).
			 */
			if (kind == CTF_K_UNKNOWN && vlen == 0)
				funcoff += idwidth;
			else
				funcoff += idwidth * (vlen + 2);
			break;

		default:
			*ctfoff = 0xffffffff;
			break;
		}
	}

	return (0);
}

static void
fbt_get_ctt_index(uint8_t version, const void *v, uint_t *indexp,
    uint_t *typep, int *ischildp)
{
	uint_t index, type;
	int ischild;

	if (version == CTF_VERSION_2) {
		const struct ctf_type_v2 *ctt = v;

		type = ctt->ctt_type;
		index = CTF_V2_TYPE_TO_INDEX(ctt->ctt_type);
		ischild = CTF_V2_TYPE_ISCHILD(ctt->ctt_type);
	} else {
		const struct ctf_type_v3 *ctt = v;

		type = ctt->ctt_type;
		index = CTF_V3_TYPE_TO_INDEX(ctt->ctt_type);
		ischild = CTF_V3_TYPE_ISCHILD(ctt->ctt_type);
	}

	if (indexp != NULL)
		*indexp = index;
	if (typep != NULL)
		*typep = type;
	if (ischildp != NULL)
		*ischildp = ischild;
}

static ssize_t
fbt_get_ctt_size(uint8_t version, const void *tp, ssize_t *sizep,
    ssize_t *incrementp)
{
	ssize_t size, increment;

	if (version == CTF_VERSION_2) {
		const struct ctf_type_v2 *ctt = tp;

		if (ctt->ctt_size == CTF_V2_LSIZE_SENT) {
			size = CTF_TYPE_LSIZE(ctt);
			increment = sizeof (struct ctf_type_v2);
		} else {
			size = ctt->ctt_size;
			increment = sizeof (struct ctf_stype_v2);
		}
	} else {
		const struct ctf_type_v3 *ctt = tp;

		if (ctt->ctt_size == CTF_V3_LSIZE_SENT) {
			size = CTF_TYPE_LSIZE(ctt);
			increment = sizeof (struct ctf_type_v3);
		} else {
			size = ctt->ctt_size;
			increment = sizeof (struct ctf_stype_v3);
		}
	}

	if (sizep)
		*sizep = size;
	if (incrementp)
		*incrementp = increment;

	return (size);
}

static void
fbt_get_ctt_info(uint8_t version, const void *tp, uint_t *kindp, uint_t *vlenp,
    int *isrootp)
{
	uint_t kind, vlen;
	int isroot;

	if (version == CTF_VERSION_2) {
		const struct ctf_type_v2 *ctt = tp;

		kind = CTF_V2_INFO_KIND(ctt->ctt_info);
		vlen = CTF_V2_INFO_VLEN(ctt->ctt_info);
		isroot = CTF_V2_INFO_ISROOT(ctt->ctt_info);
	} else {
		const struct ctf_type_v3 *ctt = tp;

		kind = CTF_V3_INFO_KIND(ctt->ctt_info);
		vlen = CTF_V3_INFO_VLEN(ctt->ctt_info);
		isroot = CTF_V3_INFO_ISROOT(ctt->ctt_info);
	}

	if (kindp != NULL)
		*kindp = kind;
	if (vlenp != NULL)
		*vlenp = vlen;
	if (isrootp != NULL)
		*isrootp = isroot;
}

static int
fbt_typoff_init(linker_ctf_t *lc)
{
	const ctf_header_t *hp = (const ctf_header_t *) lc->ctftab;
	const void *tbuf, *tend, *tp;
	const uint8_t *ctfdata = lc->ctftab + sizeof(ctf_header_t);
	size_t idwidth;
	int ctf_typemax = 0;
	uint32_t *xp;
	ulong_t pop[CTF_K_MAX + 1] = { 0 };
	uint8_t version;

	/* Sanity check. */
	if (hp->cth_magic != CTF_MAGIC)
		return (EINVAL);

	version = hp->cth_version;
	idwidth = version == CTF_VERSION_2 ? 2 : 4;

	tbuf = (const void *) (ctfdata + hp->cth_typeoff);
	tend = (const void *) (ctfdata + hp->cth_stroff);

	/*
	 * We make two passes through the entire type section.  In this first
	 * pass, we count the number of each type and the total number of types.
	 */
	for (tp = tbuf; tp < tend; ctf_typemax++) {
		uint_t kind, type, vlen;
		ssize_t size, increment;
		size_t vbytes;

		(void) fbt_get_ctt_size(version, tp, &size, &increment);
		fbt_get_ctt_info(version, tp, &kind, &vlen, NULL);
		fbt_get_ctt_index(version, tp, NULL, &type, NULL);

		switch (kind) {
		case CTF_K_INTEGER:
		case CTF_K_FLOAT:
			vbytes = sizeof (uint_t);
			break;
		case CTF_K_ARRAY:
			if (version == CTF_VERSION_2)
				vbytes = sizeof (struct ctf_array_v2);
			else
				vbytes = sizeof (struct ctf_array_v3);
			break;
		case CTF_K_FUNCTION:
			vbytes = roundup2(idwidth * vlen, sizeof(uint32_t));
			break;
		case CTF_K_STRUCT:
		case CTF_K_UNION:
			if (version == CTF_VERSION_2) {
				if (size < CTF_V2_LSTRUCT_THRESH)
					vbytes =
					    sizeof (struct ctf_member_v2) * vlen;
				else
					vbytes =
					    sizeof (struct ctf_lmember_v2) * vlen;
			} else {
				if (size < CTF_V3_LSTRUCT_THRESH)
					vbytes =
					    sizeof (struct ctf_member_v3) * vlen;
				else
					vbytes =
					    sizeof (struct ctf_lmember_v3) * vlen;
			}
			break;
		case CTF_K_ENUM:
			vbytes = sizeof (ctf_enum_t) * vlen;
			break;
		case CTF_K_FORWARD:
			/*
			 * For forward declarations, ctt_type is the CTF_K_*
			 * kind for the tag, so bump that population count too.
			 * If ctt_type is unknown, treat the tag as a struct.
			 */
			if (type == CTF_K_UNKNOWN || type >= CTF_K_MAX)
				pop[CTF_K_STRUCT]++;
			else
				pop[type]++;
			/*FALLTHRU*/
		case CTF_K_UNKNOWN:
			vbytes = 0;
			break;
		case CTF_K_POINTER:
		case CTF_K_TYPEDEF:
		case CTF_K_VOLATILE:
		case CTF_K_CONST:
		case CTF_K_RESTRICT:
			vbytes = 0;
			break;
		default:
			printf("%s(%d): detected invalid CTF kind -- %u\n", __func__, __LINE__, kind);
			return (EIO);
		}
		tp = (const void *)((uintptr_t)tp + increment + vbytes);
		pop[kind]++;
	}

	/* account for a sentinel value below */
	ctf_typemax++;
	*lc->typlenp = ctf_typemax;

	xp = malloc(sizeof(uint32_t) * ctf_typemax, M_LINKER,
	    M_ZERO | M_WAITOK);

	*lc->typoffp = xp;

	/* type id 0 is used as a sentinel value */
	*xp++ = 0;

	/*
	 * In the second pass, fill in the type offset.
	 */
	for (tp = tbuf; tp < tend; xp++) {
		ssize_t size, increment;
		uint_t kind, vlen;

		size_t vbytes;

		(void) fbt_get_ctt_size(version, tp, &size, &increment);
		fbt_get_ctt_info(version, tp, &kind, &vlen, NULL);

		switch (kind) {
		case CTF_K_INTEGER:
		case CTF_K_FLOAT:
			vbytes = sizeof (uint_t);
			break;
		case CTF_K_ARRAY:
			if (version == CTF_VERSION_2)
				vbytes = sizeof (struct ctf_array_v2);
			else
				vbytes = sizeof (struct ctf_array_v3);
			break;
		case CTF_K_FUNCTION:
			vbytes = roundup2(idwidth * vlen, sizeof(uint32_t));
			break;
		case CTF_K_STRUCT:
		case CTF_K_UNION:
			if (version == CTF_VERSION_2) {
				if (size < CTF_V2_LSTRUCT_THRESH)
					vbytes =
					    sizeof (struct ctf_member_v2) * vlen;
				else
					vbytes =
					    sizeof (struct ctf_lmember_v2) * vlen;
			} else {
				if (size < CTF_V3_LSTRUCT_THRESH)
					vbytes =
					    sizeof (struct ctf_member_v3) * vlen;
				else
					vbytes =
					    sizeof (struct ctf_lmember_v3) * vlen;
			}
			break;
		case CTF_K_ENUM:
			vbytes = sizeof (ctf_enum_t) * vlen;
			break;
		case CTF_K_FORWARD:
		case CTF_K_UNKNOWN:
			vbytes = 0;
			break;
		case CTF_K_POINTER:
		case CTF_K_TYPEDEF:
		case CTF_K_VOLATILE:
		case CTF_K_CONST:
		case CTF_K_RESTRICT:
			vbytes = 0;
			break;
		default:
			printf("%s(%d): detected invalid CTF kind -- %u\n", __func__, __LINE__, kind);
			return (EIO);
		}
		*xp = (uint32_t)((uintptr_t) tp - (uintptr_t) ctfdata);
		tp = (const void *)((uintptr_t)tp + increment + vbytes);
	}

	return (0);
}

/*
 * CTF Declaration Stack
 *
 * In order to implement ctf_type_name(), we must convert a type graph back
 * into a C type declaration.  Unfortunately, a type graph represents a storage
 * class ordering of the type whereas a type declaration must obey the C rules
 * for operator precedence, and the two orderings are frequently in conflict.
 * For example, consider these CTF type graphs and their C declarations:
 *
 * CTF_K_POINTER -> CTF_K_FUNCTION -> CTF_K_INTEGER  : int (*)()
 * CTF_K_POINTER -> CTF_K_ARRAY -> CTF_K_INTEGER     : int (*)[]
 *
 * In each case, parentheses are used to raise operator * to higher lexical
 * precedence, so the string form of the C declaration cannot be constructed by
 * walking the type graph links and forming the string from left to right.
 *
 * The functions in this file build a set of stacks from the type graph nodes
 * corresponding to the C operator precedence levels in the appropriate order.
 * The code in ctf_type_name() can then iterate over the levels and nodes in
 * lexical precedence order and construct the final C declaration string.
 */
typedef struct ctf_list {
	struct ctf_list *l_prev; /* previous pointer or tail pointer */
	struct ctf_list *l_next; /* next pointer or head pointer */
} ctf_list_t;

#define	ctf_list_prev(elem)	((void *)(((ctf_list_t *)(elem))->l_prev))
#define	ctf_list_next(elem)	((void *)(((ctf_list_t *)(elem))->l_next))

typedef enum {
	CTF_PREC_BASE,
	CTF_PREC_POINTER,
	CTF_PREC_ARRAY,
	CTF_PREC_FUNCTION,
	CTF_PREC_MAX
} ctf_decl_prec_t;

typedef struct ctf_decl_node {
	ctf_list_t cd_list;			/* linked list pointers */
	ctf_id_t cd_type;			/* type identifier */
	uint_t cd_kind;				/* type kind */
	uint_t cd_n;				/* type dimension if array */
} ctf_decl_node_t;

typedef struct ctf_decl {
	ctf_list_t cd_nodes[CTF_PREC_MAX];	/* declaration node stacks */
	int cd_order[CTF_PREC_MAX];		/* storage order of decls */
	ctf_decl_prec_t cd_qualp;		/* qualifier precision */
	ctf_decl_prec_t cd_ordp;		/* ordered precision */
	char *cd_buf;				/* buffer for output */
	char *cd_ptr;				/* buffer location */
	char *cd_end;				/* buffer limit */
	size_t cd_len;				/* buffer space required */
	int cd_err;				/* saved error value */
} ctf_decl_t;

/*
 * Simple doubly-linked list append routine.  This implementation assumes that
 * each list element contains an embedded ctf_list_t as the first member.
 * An additional ctf_list_t is used to store the head (l_next) and tail
 * (l_prev) pointers.  The current head and tail list elements have their
 * previous and next pointers set to NULL, respectively.
 */
static void
ctf_list_append(ctf_list_t *lp, void *new)
{
	ctf_list_t *p = lp->l_prev;	/* p = tail list element */
	ctf_list_t *q = new;		/* q = new list element */

	lp->l_prev = q;
	q->l_prev = p;
	q->l_next = NULL;

	if (p != NULL)
		p->l_next = q;
	else
		lp->l_next = q;
}

/*
 * Prepend the specified existing element to the given ctf_list_t.  The
 * existing pointer should be pointing at a struct with embedded ctf_list_t.
 */
static void
ctf_list_prepend(ctf_list_t *lp, void *new)
{
	ctf_list_t *p = new;		/* p = new list element */
	ctf_list_t *q = lp->l_next;	/* q = head list element */

	lp->l_next = p;
	p->l_prev = NULL;
	p->l_next = q;

	if (q != NULL)
		q->l_prev = p;
	else
		lp->l_prev = p;
}

static void
ctf_decl_init(ctf_decl_t *cd, char *buf, size_t len)
{
	int i;

	bzero(cd, sizeof (ctf_decl_t));

	for (i = CTF_PREC_BASE; i < CTF_PREC_MAX; i++)
		cd->cd_order[i] = CTF_PREC_BASE - 1;

	cd->cd_qualp = CTF_PREC_BASE;
	cd->cd_ordp = CTF_PREC_BASE;

	cd->cd_buf = buf;
	cd->cd_ptr = buf;
	cd->cd_end = buf + len;
}

static void
ctf_decl_fini(ctf_decl_t *cd)
{
	ctf_decl_node_t *cdp, *ndp;
	int i;

	for (i = CTF_PREC_BASE; i < CTF_PREC_MAX; i++) {
		for (cdp = ctf_list_next(&cd->cd_nodes[i]);
		    cdp != NULL; cdp = ndp) {
			ndp = ctf_list_next(cdp);
			free(cdp, M_FBT);
		}
	}
}

static const void *
ctf_lookup_by_id(linker_ctf_t *lc, ctf_id_t type)
{
	const void *tp;
	uint32_t offset;
	uint32_t *typoff = *lc->typoffp;

	if (type >= *lc->typlenp) {
		printf("%s(%d): type %d exceeds max %ld\n",__func__,__LINE__,(int) type,*lc->typlenp);
		return(NULL);
	}

	/* Check if the type isn't cross-referenced. */
	if ((offset = typoff[type]) == 0) {
		printf("%s(%d): type %d isn't cross referenced\n",__func__,__LINE__, (int) type);
		return(NULL);
	}

	tp = (const void *) (lc->ctftab + offset + sizeof(ctf_header_t));

	return (tp);
}

static void
fbt_array_info(linker_ctf_t *lc, ctf_id_t type, ctf_arinfo_t *arp)
{
	const ctf_header_t *hp = (const ctf_header_t *) lc->ctftab;
	const void *tp;
	ssize_t increment;
	uint_t kind;

	bzero(arp, sizeof(*arp));

	if ((tp = ctf_lookup_by_id(lc, type)) == NULL)
		return;

	fbt_get_ctt_info(hp->cth_version, tp, &kind, NULL, NULL);
	if (kind != CTF_K_ARRAY)
		return;

	(void) fbt_get_ctt_size(hp->cth_version, tp, NULL, &increment);

	if (hp->cth_version == CTF_VERSION_2) {
		const struct ctf_array_v2 *ap;

		ap = (const struct ctf_array_v2 *)((uintptr_t)tp + increment);
		arp->ctr_contents = ap->cta_contents;
		arp->ctr_index = ap->cta_index;
		arp->ctr_nelems = ap->cta_nelems;
	} else {
		const struct ctf_array_v3 *ap;

		ap = (const struct ctf_array_v3 *)((uintptr_t)tp + increment);
		arp->ctr_contents = ap->cta_contents;
		arp->ctr_index = ap->cta_index;
		arp->ctr_nelems = ap->cta_nelems;
	}
}

static const char *
ctf_strptr(linker_ctf_t *lc, int name)
{
	const ctf_header_t *hp = (const ctf_header_t *) lc->ctftab;
	const char *strp = "";

	if (name < 0 || name >= hp->cth_strlen)
		return(strp);

	strp = (const char *)(lc->ctftab + hp->cth_stroff + name + sizeof(ctf_header_t));

	return (strp);
}

static const char *
ctf_type_rname(linker_ctf_t *lc, const void *v)
{
	const ctf_header_t *hp = (const ctf_header_t *) lc->ctftab;
	uint_t name;

	if (hp->cth_version == CTF_VERSION_2) {
		const struct ctf_type_v2 *ctt = v;

		name = ctt->ctt_name;
	} else {
		const struct ctf_type_v3 *ctt = v;

		name = ctt->ctt_name;
	}

	return (ctf_strptr(lc, name));
}

static void
ctf_decl_push(ctf_decl_t *cd, linker_ctf_t *lc, ctf_id_t type)
{
	const ctf_header_t *hp = (const ctf_header_t *) lc->ctftab;
	ctf_decl_node_t *cdp;
	ctf_decl_prec_t prec;
	uint_t kind, n = 1, t;
	int is_qual = 0;

	const void *tp;
	ctf_arinfo_t ar;

	if ((tp = ctf_lookup_by_id(lc, type)) == NULL) {
		cd->cd_err = ENOENT;
		return;
	}

	fbt_get_ctt_info(hp->cth_version, tp, &kind, NULL, NULL);
	fbt_get_ctt_index(hp->cth_version, tp, NULL, &t, NULL);

	switch (kind) {
	case CTF_K_ARRAY:
		fbt_array_info(lc, type, &ar);
		ctf_decl_push(cd, lc, ar.ctr_contents);
		n = ar.ctr_nelems;
		prec = CTF_PREC_ARRAY;
		break;

	case CTF_K_TYPEDEF:
		if (ctf_type_rname(lc, tp)[0] == '\0') {
			ctf_decl_push(cd, lc, t);
			return;
		}
		prec = CTF_PREC_BASE;
		break;

	case CTF_K_FUNCTION:
		ctf_decl_push(cd, lc, t);
		prec = CTF_PREC_FUNCTION;
		break;

	case CTF_K_POINTER:
		ctf_decl_push(cd, lc, t);
		prec = CTF_PREC_POINTER;
		break;

	case CTF_K_VOLATILE:
	case CTF_K_CONST:
	case CTF_K_RESTRICT:
		ctf_decl_push(cd, lc, t);
		prec = cd->cd_qualp;
		is_qual++;
		break;

	default:
		prec = CTF_PREC_BASE;
	}

	cdp = malloc(sizeof(*cdp), M_FBT, M_WAITOK);
	cdp->cd_type = type;
	cdp->cd_kind = kind;
	cdp->cd_n = n;

	if (ctf_list_next(&cd->cd_nodes[prec]) == NULL)
		cd->cd_order[prec] = cd->cd_ordp++;

	/*
	 * Reset cd_qualp to the highest precedence level that we've seen so
	 * far that can be qualified (CTF_PREC_BASE or CTF_PREC_POINTER).
	 */
	if (prec > cd->cd_qualp && prec < CTF_PREC_ARRAY)
		cd->cd_qualp = prec;

	/*
	 * C array declarators are ordered inside out so prepend them.  Also by
	 * convention qualifiers of base types precede the type specifier (e.g.
	 * const int vs. int const) even though the two forms are equivalent.
	 */
	if (kind == CTF_K_ARRAY || (is_qual && prec == CTF_PREC_BASE))
		ctf_list_prepend(&cd->cd_nodes[prec], cdp);
	else
		ctf_list_append(&cd->cd_nodes[prec], cdp);
}

static void
ctf_decl_sprintf(ctf_decl_t *cd, const char *format, ...)
{
	size_t len = (size_t)(cd->cd_end - cd->cd_ptr);
	va_list ap;
	size_t n;

	va_start(ap, format);
	n = vsnprintf(cd->cd_ptr, len, format, ap);
	va_end(ap);

	cd->cd_ptr += MIN(n, len);
	cd->cd_len += n;
}

static ssize_t
fbt_type_name(linker_ctf_t *lc, ctf_id_t type, char *buf, size_t len)
{
	ctf_decl_t cd;
	ctf_decl_node_t *cdp;
	ctf_decl_prec_t prec, lp, rp;
	int ptr, arr;
	uint_t k;

	if (lc == NULL && type == CTF_ERR)
		return (-1); /* simplify caller code by permitting CTF_ERR */

	ctf_decl_init(&cd, buf, len);
	ctf_decl_push(&cd, lc, type);

	if (cd.cd_err != 0) {
		ctf_decl_fini(&cd);
		return (-1);
	}

	/*
	 * If the type graph's order conflicts with lexical precedence order
	 * for pointers or arrays, then we need to surround the declarations at
	 * the corresponding lexical precedence with parentheses.  This can
	 * result in either a parenthesized pointer (*) as in int (*)() or
	 * int (*)[], or in a parenthesized pointer and array as in int (*[])().
	 */
	ptr = cd.cd_order[CTF_PREC_POINTER] > CTF_PREC_POINTER;
	arr = cd.cd_order[CTF_PREC_ARRAY] > CTF_PREC_ARRAY;

	rp = arr ? CTF_PREC_ARRAY : ptr ? CTF_PREC_POINTER : -1;
	lp = ptr ? CTF_PREC_POINTER : arr ? CTF_PREC_ARRAY : -1;

	k = CTF_K_POINTER; /* avoid leading whitespace (see below) */

	for (prec = CTF_PREC_BASE; prec < CTF_PREC_MAX; prec++) {
		for (cdp = ctf_list_next(&cd.cd_nodes[prec]);
		    cdp != NULL; cdp = ctf_list_next(cdp)) {

			const void *tp = ctf_lookup_by_id(lc, cdp->cd_type);
			const char *name = ctf_type_rname(lc, tp);

			if (k != CTF_K_POINTER && k != CTF_K_ARRAY)
				ctf_decl_sprintf(&cd, " ");

			if (lp == prec) {
				ctf_decl_sprintf(&cd, "(");
				lp = -1;
			}

			switch (cdp->cd_kind) {
			case CTF_K_INTEGER:
			case CTF_K_FLOAT:
			case CTF_K_TYPEDEF:
				ctf_decl_sprintf(&cd, "%s", name);
				break;
			case CTF_K_POINTER:
				ctf_decl_sprintf(&cd, "*");
				break;
			case CTF_K_ARRAY:
				ctf_decl_sprintf(&cd, "[%u]", cdp->cd_n);
				break;
			case CTF_K_FUNCTION:
				ctf_decl_sprintf(&cd, "()");
				break;
			case CTF_K_STRUCT:
			case CTF_K_FORWARD:
				ctf_decl_sprintf(&cd, "struct %s", name);
				break;
			case CTF_K_UNION:
				ctf_decl_sprintf(&cd, "union %s", name);
				break;
			case CTF_K_ENUM:
				ctf_decl_sprintf(&cd, "enum %s", name);
				break;
			case CTF_K_VOLATILE:
				ctf_decl_sprintf(&cd, "volatile");
				break;
			case CTF_K_CONST:
				ctf_decl_sprintf(&cd, "const");
				break;
			case CTF_K_RESTRICT:
				ctf_decl_sprintf(&cd, "restrict");
				break;
			}

			k = cdp->cd_kind;
		}

		if (rp == prec)
			ctf_decl_sprintf(&cd, ")");
	}

	ctf_decl_fini(&cd);
	return (cd.cd_len);
}

static void
fbt_getargdesc(void *arg __unused, dtrace_id_t id __unused, void *parg, dtrace_argdesc_t *desc)
{
	const ctf_header_t *hp;
	const char *dp;
	fbt_probe_t *fbt = parg;
	linker_ctf_t lc;
	modctl_t *ctl = fbt->fbtp_ctl;
	size_t idwidth;
	int ndx = desc->dtargd_ndx;
	int symindx = fbt->fbtp_symindx;
	uint32_t *ctfoff;
	uint32_t offset, type;
	uint_t info, n;
	ushort_t kind;

	if (fbt->fbtp_roffset != 0 && desc->dtargd_ndx == 0) {
		(void) strcpy(desc->dtargd_native, "int");
		return;
	}

	desc->dtargd_ndx = DTRACE_ARGNONE;

	/* Get a pointer to the CTF data and it's length. */
	if (linker_ctf_get(ctl, &lc) != 0)
		/* No CTF data? Something wrong? *shrug* */
		return;

	/* Check if this module hasn't been initialised yet. */
	if (*lc.ctfoffp == NULL) {
		/*
		 * Initialise the CTF object and function symindx to
		 * byte offset array.
		 */
		if (fbt_ctfoff_init(ctl, &lc) != 0)
			return;

		/* Initialise the CTF type to byte offset array. */
		if (fbt_typoff_init(&lc) != 0)
			return;
	}

	ctfoff = *lc.ctfoffp;

	if (ctfoff == NULL || *lc.typoffp == NULL)
		return;

	/* Check if the symbol index is out of range. */
	if (symindx >= lc.nsym)
		return;

	/* Check if the symbol isn't cross-referenced. */
	if ((offset = ctfoff[symindx]) == 0xffffffff)
		return;

	hp = (const ctf_header_t *) lc.ctftab;
	idwidth = hp->cth_version == CTF_VERSION_2 ? 2 : 4;
	dp = (const char *)(lc.ctftab + offset + sizeof(ctf_header_t));

	info = 0;
	memcpy(&info, dp, idwidth);
	dp += idwidth;
	if (hp->cth_version == CTF_VERSION_2) {
		kind = CTF_V2_INFO_KIND(info);
		n = CTF_V2_INFO_VLEN(info);
	} else {
		kind = CTF_V3_INFO_KIND(info);
		n = CTF_V3_INFO_VLEN(info);
	}

	if (kind == CTF_K_UNKNOWN && n == 0) {
		printf("%s(%d): Unknown function!\n",__func__,__LINE__);
		return;
	}

	if (kind != CTF_K_FUNCTION) {
		printf("%s(%d): Expected a function!\n",__func__,__LINE__);
		return;
	}

	if (fbt->fbtp_roffset != 0) {
		/* Only return type is available for args[1] in return probe. */
		if (ndx > 1)
			return;
		ASSERT(ndx == 1);
	} else {
		/* Check if the requested argument doesn't exist. */
		if (ndx >= n)
			return;

		/* Skip the return type and arguments up to the one requested. */
		dp += idwidth * (ndx + 1);
	}

	type = 0;
	memcpy(&type, dp, idwidth);
	if (fbt_type_name(&lc, type, desc->dtargd_native, sizeof(desc->dtargd_native)) > 0)
		desc->dtargd_ndx = ndx;
}

static int
fbt_linker_file_cb(linker_file_t lf, void *arg)
{

	fbt_provide_module(arg, lf);

	return (0);
}

static void
fbt_load(void *dummy)
{
	/* Default the probe table size if not specified. */
	if (fbt_probetab_size == 0)
		fbt_probetab_size = FBT_PROBETAB_SIZE;

	/* Choose the hash mask for the probe table. */
	fbt_probetab_mask = fbt_probetab_size - 1;

	/* Allocate memory for the probe table. */
	fbt_probetab =
	    malloc(fbt_probetab_size * sizeof (fbt_probe_t *), M_FBT, M_WAITOK | M_ZERO);

	dtrace_doubletrap_func = fbt_doubletrap;
	dtrace_invop_add(fbt_invop);

	if (dtrace_register("fbt", &fbt_attr, DTRACE_PRIV_USER,
	    NULL, &fbt_pops, NULL, &fbt_id) != 0)
		return;

	/* Create probes for the kernel and already-loaded modules. */
	linker_file_foreach(fbt_linker_file_cb, NULL);
}

static int
fbt_unload(void)
{
	int error = 0;

	/* De-register the invalid opcode handler. */
	dtrace_invop_remove(fbt_invop);

	dtrace_doubletrap_func = NULL;

	/* De-register this DTrace provider. */
	if ((error = dtrace_unregister(fbt_id)) != 0)
		return (error);

	/* Free the probe table. */
	free(fbt_probetab, M_FBT);
	fbt_probetab = NULL;
	fbt_probetab_mask = 0;

	return (error);
}

static int
fbt_modevent(module_t mod __unused, int type, void *data __unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		break;

	case MOD_UNLOAD:
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}

	return (error);
}

SYSINIT(fbt_load, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, fbt_load, NULL);
SYSUNINIT(fbt_unload, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, fbt_unload, NULL);

DEV_MODULE(fbt, fbt_modevent, NULL);
MODULE_VERSION(fbt, 1);
MODULE_DEPEND(fbt, dtrace, 1, 1, 1);
MODULE_DEPEND(fbt, opensolaris, 1, 1, 1);
