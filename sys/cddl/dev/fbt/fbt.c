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
 * $FreeBSD: src/sys/cddl/dev/fbt/fbt.c,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cpuvar.h>
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
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <machine/stdarg.h>

#include <sys/dtrace.h>
#include <sys/dtrace_bsd.h>

MALLOC_DEFINE(M_FBT, "fbt", "Function Boundary Tracing");

#define	FBT_PUSHL_EBP		0x55
#define	FBT_MOVL_ESP_EBP0_V0	0x8b
#define	FBT_MOVL_ESP_EBP1_V0	0xec
#define	FBT_MOVL_ESP_EBP0_V1	0x89
#define	FBT_MOVL_ESP_EBP1_V1	0xe5
#define	FBT_REX_RSP_RBP		0x48

#define	FBT_POPL_EBP		0x5d
#define	FBT_RET			0xc3
#define	FBT_RET_IMM16		0xc2
#define	FBT_LEAVE		0xc9

#ifdef __amd64__
#define	FBT_PATCHVAL		0xcc
#else
#define	FBT_PATCHVAL		0xf0
#endif

static d_open_t	fbt_open;
static int	fbt_unload(void);
static void	fbt_getargdesc(void *, dtrace_id_t, void *, dtrace_argdesc_t *);
static void	fbt_provide_module(void *, modctl_t *);
static void	fbt_destroy(void *, dtrace_id_t, void *);
static void	fbt_enable(void *, dtrace_id_t, void *);
static void	fbt_disable(void *, dtrace_id_t, void *);
static void	fbt_load(void *);
static void	fbt_suspend(void *, dtrace_id_t, void *);
static void	fbt_resume(void *, dtrace_id_t, void *);

#define	FBT_ENTRY	"entry"
#define	FBT_RETURN	"return"
#define	FBT_ADDR2NDX(addr)	((((uintptr_t)(addr)) >> 4) & fbt_probetab_mask)
#define	FBT_PROBETAB_SIZE	0x8000		/* 32k entries -- 128K total */

static struct cdevsw fbt_cdevsw = {
	.d_version	= D_VERSION,
	.d_open		= fbt_open,
	.d_name		= "fbt",
};

static dtrace_pattr_t fbt_attr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

static dtrace_pops_t fbt_pops = {
	NULL,
	fbt_provide_module,
	fbt_enable,
	fbt_disable,
	fbt_suspend,
	fbt_resume,
	fbt_getargdesc,
	NULL,
	NULL,
	fbt_destroy
};

typedef struct fbt_probe {
	struct fbt_probe *fbtp_hashnext;
	uint8_t		*fbtp_patchpoint;
	int8_t		fbtp_rval;
	uint8_t		fbtp_patchval;
	uint8_t		fbtp_savedval;
	uintptr_t	fbtp_roffset;
	dtrace_id_t	fbtp_id;
	const char	*fbtp_name;
	modctl_t	*fbtp_ctl;
	int		fbtp_loadcnt;
	int		fbtp_primary;
	int		fbtp_invop_cnt;
	int		fbtp_symindx;
	struct fbt_probe *fbtp_next;
} fbt_probe_t;

static struct cdev		*fbt_cdev;
static dtrace_provider_id_t	fbt_id;
static fbt_probe_t		**fbt_probetab;
static int			fbt_probetab_size;
static int			fbt_probetab_mask;
static int			fbt_verbose = 0;

static void
fbt_doubletrap(void)
{
	fbt_probe_t *fbt;
	int i;

	for (i = 0; i < fbt_probetab_size; i++) {
		fbt = fbt_probetab[i];

		for (; fbt != NULL; fbt = fbt->fbtp_next)
			*fbt->fbtp_patchpoint = fbt->fbtp_savedval;
	}
}

static int
fbt_invop(uintptr_t addr, uintptr_t *stack, uintptr_t rval)
{
	solaris_cpu_t *cpu = &solaris_cpu[curcpu];
	uintptr_t stack0, stack1, stack2, stack3, stack4;
	fbt_probe_t *fbt = fbt_probetab[FBT_ADDR2NDX(addr)];

	for (; fbt != NULL; fbt = fbt->fbtp_hashnext) {
		if ((uintptr_t)fbt->fbtp_patchpoint == addr) {
			fbt->fbtp_invop_cnt++;
			if (fbt->fbtp_roffset == 0) {
				int i = 0;
				/*
				 * When accessing the arguments on the stack,
				 * we must protect against accessing beyond
				 * the stack.  We can safely set NOFAULT here
				 * -- we know that interrupts are already
				 * disabled.
				 */
				DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
				cpu->cpu_dtrace_caller = stack[i++];
				stack0 = stack[i++];
				stack1 = stack[i++];
				stack2 = stack[i++];
				stack3 = stack[i++];
				stack4 = stack[i++];
				DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT |
				    CPU_DTRACE_BADADDR);

				dtrace_probe(fbt->fbtp_id, stack0, stack1,
				    stack2, stack3, stack4);

				cpu->cpu_dtrace_caller = 0;
			} else {
#ifdef __amd64__
				/*
				 * On amd64, we instrument the ret, not the
				 * leave.  We therefore need to set the caller
				 * to assure that the top frame of a stack()
				 * action is correct.
				 */
				DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
				cpu->cpu_dtrace_caller = stack[0];
				DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT |
				    CPU_DTRACE_BADADDR);
#endif

				dtrace_probe(fbt->fbtp_id, fbt->fbtp_roffset,
				    rval, 0, 0, 0);
				cpu->cpu_dtrace_caller = 0;
			}

			return (fbt->fbtp_rval);
		}
	}

	return (0);
}

static int
fbt_provide_module_function(linker_file_t lf, int symindx,
    linker_symval_t *symval, void *opaque)
{
	char *modname = opaque;
	const char *name = symval->name;
	fbt_probe_t *fbt, *retfbt;
	int j;
	int size;
	u_int8_t *instr, *limit;

	if (strncmp(name, "dtrace_", 7) == 0 &&
	    strncmp(name, "dtrace_safe_", 12) != 0) {
		/*
		 * Anything beginning with "dtrace_" may be called
		 * from probe context unless it explicitly indicates
		 * that it won't be called from probe context by
		 * using the prefix "dtrace_safe_".
		 */
		return (0);
	}

	if (name[0] == '_' && name[1] == '_')
		return (0);

	size = symval->size;

	instr = (u_int8_t *) symval->value;
	limit = (u_int8_t *) symval->value + symval->size;

#ifdef __amd64__
	while (instr < limit) {
		if (*instr == FBT_PUSHL_EBP)
			break;

		if ((size = dtrace_instr_size(instr)) <= 0)
			break;

		instr += size;
	}

	if (instr >= limit || *instr != FBT_PUSHL_EBP) {
		/*
		 * We either don't save the frame pointer in this
		 * function, or we ran into some disassembly
		 * screw-up.  Either way, we bail.
		 */
		return (0);
	}
#else
	if (instr[0] != FBT_PUSHL_EBP)
		return (0);

	if (!(instr[1] == FBT_MOVL_ESP_EBP0_V0 &&
	    instr[2] == FBT_MOVL_ESP_EBP1_V0) &&
	    !(instr[1] == FBT_MOVL_ESP_EBP0_V1 &&
	    instr[2] == FBT_MOVL_ESP_EBP1_V1))
		return (0);
#endif

	fbt = malloc(sizeof (fbt_probe_t), M_FBT, M_WAITOK | M_ZERO);
	fbt->fbtp_name = name;
	fbt->fbtp_id = dtrace_probe_create(fbt_id, modname,
	    name, FBT_ENTRY, 3, fbt);
	fbt->fbtp_patchpoint = instr;
	fbt->fbtp_ctl = lf;
	fbt->fbtp_loadcnt = lf->loadcnt;
	fbt->fbtp_rval = DTRACE_INVOP_PUSHL_EBP;
	fbt->fbtp_savedval = *instr;
	fbt->fbtp_patchval = FBT_PATCHVAL;
	fbt->fbtp_symindx = symindx;

	fbt->fbtp_hashnext = fbt_probetab[FBT_ADDR2NDX(instr)];
	fbt_probetab[FBT_ADDR2NDX(instr)] = fbt;

	lf->fbt_nentries++;

	retfbt = NULL;
again:
	if (instr >= limit)
		return (0);

	/*
	 * If this disassembly fails, then we've likely walked off into
	 * a jump table or some other unsuitable area.  Bail out of the
	 * disassembly now.
	 */
	if ((size = dtrace_instr_size(instr)) <= 0)
		return (0);

#ifdef __amd64__
	/*
	 * We only instrument "ret" on amd64 -- we don't yet instrument
	 * ret imm16, largely because the compiler doesn't seem to
	 * (yet) emit them in the kernel...
	 */
	if (*instr != FBT_RET) {
		instr += size;
		goto again;
	}
#else
	if (!(size == 1 &&
	    (*instr == FBT_POPL_EBP || *instr == FBT_LEAVE) &&
	    (*(instr + 1) == FBT_RET ||
	    *(instr + 1) == FBT_RET_IMM16))) {
		instr += size;
		goto again;
	}
#endif

	/*
	 * We (desperately) want to avoid erroneously instrumenting a
	 * jump table, especially given that our markers are pretty
	 * short:  two bytes on x86, and just one byte on amd64.  To
	 * determine if we're looking at a true instruction sequence
	 * or an inline jump table that happens to contain the same
	 * byte sequences, we resort to some heuristic sleeze:  we
	 * treat this instruction as being contained within a pointer,
	 * and see if that pointer points to within the body of the
	 * function.  If it does, we refuse to instrument it.
	 */
	for (j = 0; j < sizeof (uintptr_t); j++) {
		caddr_t check = (caddr_t) instr - j;
		uint8_t *ptr;

		if (check < symval->value)
			break;

		if (check + sizeof (caddr_t) > (caddr_t)limit)
			continue;

		ptr = *(uint8_t **)check;

		if (ptr >= (uint8_t *) symval->value && ptr < limit) {
			instr += size;
			goto again;
		}
	}

	/*
	 * We have a winner!
	 */
	fbt = malloc(sizeof (fbt_probe_t), M_FBT, M_WAITOK | M_ZERO);
	fbt->fbtp_name = name;

	if (retfbt == NULL) {
		fbt->fbtp_id = dtrace_probe_create(fbt_id, modname,
		    name, FBT_RETURN, 3, fbt);
	} else {
		retfbt->fbtp_next = fbt;
		fbt->fbtp_id = retfbt->fbtp_id;
	}

	retfbt = fbt;
	fbt->fbtp_patchpoint = instr;
	fbt->fbtp_ctl = lf;
	fbt->fbtp_loadcnt = lf->loadcnt;
	fbt->fbtp_symindx = symindx;

#ifndef __amd64__
	if (*instr == FBT_POPL_EBP) {
		fbt->fbtp_rval = DTRACE_INVOP_POPL_EBP;
	} else {
		ASSERT(*instr == FBT_LEAVE);
		fbt->fbtp_rval = DTRACE_INVOP_LEAVE;
	}
	fbt->fbtp_roffset =
	    (uintptr_t)(instr - (uint8_t *) symval->value) + 1;

#else
	ASSERT(*instr == FBT_RET);
	fbt->fbtp_rval = DTRACE_INVOP_RET;
	fbt->fbtp_roffset =
	    (uintptr_t)(instr - (uint8_t *) symval->value);
#endif

	fbt->fbtp_savedval = *instr;
	fbt->fbtp_patchval = FBT_PATCHVAL;
	fbt->fbtp_hashnext = fbt_probetab[FBT_ADDR2NDX(instr)];
	fbt_probetab[FBT_ADDR2NDX(instr)] = fbt;

	lf->fbt_nentries++;

	instr += size;
	goto again;
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
	 * The cyclic timer subsystem can be built as a module and DTrace
	 * depends on that, so it is ineligible too.
	 */
	if (strcmp(modname, "cyclic") == 0)
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
fbt_destroy(void *arg, dtrace_id_t id, void *parg)
{
	fbt_probe_t *fbt = parg, *next, *hash, *last;
	modctl_t *ctl;
	int ndx;

	do {
		ctl = fbt->fbtp_ctl;

		ctl->fbt_nentries--;

		/*
		 * Now we need to remove this probe from the fbt_probetab.
		 */
		ndx = FBT_ADDR2NDX(fbt->fbtp_patchpoint);
		last = NULL;
		hash = fbt_probetab[ndx];

		while (hash != fbt) {
			ASSERT(hash != NULL);
			last = hash;
			hash = hash->fbtp_hashnext;
		}

		if (last != NULL) {
			last->fbtp_hashnext = fbt->fbtp_hashnext;
		} else {
			fbt_probetab[ndx] = fbt->fbtp_hashnext;
		}

		next = fbt->fbtp_next;
		free(fbt, M_FBT);

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

	for (; fbt != NULL; fbt = fbt->fbtp_next) {
		*fbt->fbtp_patchpoint = fbt->fbtp_patchval;
	}
}

static void
fbt_disable(void *arg, dtrace_id_t id, void *parg)
{
	fbt_probe_t *fbt = parg;
	modctl_t *ctl = fbt->fbtp_ctl;

	ASSERT(ctl->nenabled > 0);
	ctl->nenabled--;

	if ((ctl->loadcnt != fbt->fbtp_loadcnt))
		return;

	for (; fbt != NULL; fbt = fbt->fbtp_next)
		*fbt->fbtp_patchpoint = fbt->fbtp_savedval;
}

static void
fbt_suspend(void *arg, dtrace_id_t id, void *parg)
{
	fbt_probe_t *fbt = parg;
	modctl_t *ctl = fbt->fbtp_ctl;

	ASSERT(ctl->nenabled > 0);

	if ((ctl->loadcnt != fbt->fbtp_loadcnt))
		return;

	for (; fbt != NULL; fbt = fbt->fbtp_next)
		*fbt->fbtp_patchpoint = fbt->fbtp_savedval;
}

static void
fbt_resume(void *arg, dtrace_id_t id, void *parg)
{
	fbt_probe_t *fbt = parg;
	modctl_t *ctl = fbt->fbtp_ctl;

	ASSERT(ctl->nenabled > 0);

	if ((ctl->loadcnt != fbt->fbtp_loadcnt))
		return;

	for (; fbt != NULL; fbt = fbt->fbtp_next)
		*fbt->fbtp_patchpoint = fbt->fbtp_patchval;
}

static int
fbt_ctfoff_init(modctl_t *lf, linker_ctf_t *lc)
{
	const Elf_Sym *symp = lc->symtab;;
	const char *name;
	const ctf_header_t *hp = (const ctf_header_t *) lc->ctftab;
	const uint8_t *ctfdata = lc->ctftab + sizeof(ctf_header_t);
	int i;
	uint32_t *ctfoff;
	uint32_t objtoff = hp->cth_objtoff;
	uint32_t funcoff = hp->cth_funcoff;
	ushort_t info;
	ushort_t vlen;

	/* Sanity check. */
	if (hp->cth_magic != CTF_MAGIC) {
		printf("Bad magic value in CTF data of '%s'\n",lf->pathname);
		return (EINVAL);
	}

	if (lc->symtab == NULL) {
		printf("No symbol table in '%s'\n",lf->pathname);
		return (EINVAL);
	}

	if ((ctfoff = malloc(sizeof(uint32_t) * lc->nsym, M_LINKER, M_WAITOK)) == NULL)
		return (ENOMEM);

	*lc->ctfoffp = ctfoff;

	for (i = 0; i < lc->nsym; i++, ctfoff++, symp++) {
		if (symp->st_name == 0 || symp->st_shndx == SHN_UNDEF) {
			*ctfoff = 0xffffffff;
			continue;
		}

		if (symp->st_name < lc->strcnt)
			name = lc->strtab + symp->st_name;
		else
			name = "(?)";

		switch (ELF_ST_TYPE(symp->st_info)) {
		case STT_OBJECT:
			if (objtoff >= hp->cth_funcoff ||
                            (symp->st_shndx == SHN_ABS && symp->st_value == 0)) {
				*ctfoff = 0xffffffff;
                                break;
                        }

                        *ctfoff = objtoff;
                        objtoff += sizeof (ushort_t);
			break;

		case STT_FUNC:
			if (funcoff >= hp->cth_typeoff) {
				*ctfoff = 0xffffffff;
				break;
			}

			*ctfoff = funcoff;

			info = *((const ushort_t *)(ctfdata + funcoff));
			vlen = CTF_INFO_VLEN(info);

			/*
			 * If we encounter a zero pad at the end, just skip it.
			 * Otherwise skip over the function and its return type
			 * (+2) and the argument list (vlen).
			 */
			if (CTF_INFO_KIND(info) == CTF_K_UNKNOWN && vlen == 0)
				funcoff += sizeof (ushort_t); /* skip pad */
			else
				funcoff += sizeof (ushort_t) * (vlen + 2);
			break;

		default:
			*ctfoff = 0xffffffff;
			break;
		}
	}

	return (0);
}

static ssize_t
fbt_get_ctt_size(uint8_t version, const ctf_type_t *tp, ssize_t *sizep,
    ssize_t *incrementp)
{
	ssize_t size, increment;

	if (version > CTF_VERSION_1 &&
	    tp->ctt_size == CTF_LSIZE_SENT) {
		size = CTF_TYPE_LSIZE(tp);
		increment = sizeof (ctf_type_t);
	} else {
		size = tp->ctt_size;
		increment = sizeof (ctf_stype_t);
	}

	if (sizep)
		*sizep = size;
	if (incrementp)
		*incrementp = increment;

	return (size);
}

static int
fbt_typoff_init(linker_ctf_t *lc)
{
	const ctf_header_t *hp = (const ctf_header_t *) lc->ctftab;
	const ctf_type_t *tbuf;
	const ctf_type_t *tend;
	const ctf_type_t *tp;
	const uint8_t *ctfdata = lc->ctftab + sizeof(ctf_header_t);
	int ctf_typemax = 0;
	uint32_t *xp;
	ulong_t pop[CTF_K_MAX + 1] = { 0 };


	/* Sanity check. */
	if (hp->cth_magic != CTF_MAGIC)
		return (EINVAL);

	tbuf = (const ctf_type_t *) (ctfdata + hp->cth_typeoff);
	tend = (const ctf_type_t *) (ctfdata + hp->cth_stroff);

	int child = hp->cth_parname != 0;

	/*
	 * We make two passes through the entire type section.  In this first
	 * pass, we count the number of each type and the total number of types.
	 */
	for (tp = tbuf; tp < tend; ctf_typemax++) {
		ushort_t kind = CTF_INFO_KIND(tp->ctt_info);
		ulong_t vlen = CTF_INFO_VLEN(tp->ctt_info);
		ssize_t size, increment;

		size_t vbytes;
		uint_t n;

		(void) fbt_get_ctt_size(hp->cth_version, tp, &size, &increment);

		switch (kind) {
		case CTF_K_INTEGER:
		case CTF_K_FLOAT:
			vbytes = sizeof (uint_t);
			break;
		case CTF_K_ARRAY:
			vbytes = sizeof (ctf_array_t);
			break;
		case CTF_K_FUNCTION:
			vbytes = sizeof (ushort_t) * (vlen + (vlen & 1));
			break;
		case CTF_K_STRUCT:
		case CTF_K_UNION:
			if (size < CTF_LSTRUCT_THRESH) {
				ctf_member_t *mp = (ctf_member_t *)
				    ((uintptr_t)tp + increment);

				vbytes = sizeof (ctf_member_t) * vlen;
				for (n = vlen; n != 0; n--, mp++)
					child |= CTF_TYPE_ISCHILD(mp->ctm_type);
			} else {
				ctf_lmember_t *lmp = (ctf_lmember_t *)
				    ((uintptr_t)tp + increment);

				vbytes = sizeof (ctf_lmember_t) * vlen;
				for (n = vlen; n != 0; n--, lmp++)
					child |=
					    CTF_TYPE_ISCHILD(lmp->ctlm_type);
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
			if (tp->ctt_type == CTF_K_UNKNOWN ||
			    tp->ctt_type >= CTF_K_MAX)
				pop[CTF_K_STRUCT]++;
			else
				pop[tp->ctt_type]++;
			/*FALLTHRU*/
		case CTF_K_UNKNOWN:
			vbytes = 0;
			break;
		case CTF_K_POINTER:
		case CTF_K_TYPEDEF:
		case CTF_K_VOLATILE:
		case CTF_K_CONST:
		case CTF_K_RESTRICT:
			child |= CTF_TYPE_ISCHILD(tp->ctt_type);
			vbytes = 0;
			break;
		default:
			printf("%s(%d): detected invalid CTF kind -- %u\n", __func__, __LINE__, kind);
			return (EIO);
		}
		tp = (ctf_type_t *)((uintptr_t)tp + increment + vbytes);
		pop[kind]++;
	}

	*lc->typlenp = ctf_typemax;

	if ((xp = malloc(sizeof(uint32_t) * ctf_typemax, M_LINKER, M_ZERO | M_WAITOK)) == NULL)
		return (ENOMEM);

	*lc->typoffp = xp;

	/* type id 0 is used as a sentinel value */
	*xp++ = 0;

	/*
	 * In the second pass, fill in the type offset.
	 */
	for (tp = tbuf; tp < tend; xp++) {
		ushort_t kind = CTF_INFO_KIND(tp->ctt_info);
		ulong_t vlen = CTF_INFO_VLEN(tp->ctt_info);
		ssize_t size, increment;

		size_t vbytes;
		uint_t n;

		(void) fbt_get_ctt_size(hp->cth_version, tp, &size, &increment);

		switch (kind) {
		case CTF_K_INTEGER:
		case CTF_K_FLOAT:
			vbytes = sizeof (uint_t);
			break;
		case CTF_K_ARRAY:
			vbytes = sizeof (ctf_array_t);
			break;
		case CTF_K_FUNCTION:
			vbytes = sizeof (ushort_t) * (vlen + (vlen & 1));
			break;
		case CTF_K_STRUCT:
		case CTF_K_UNION:
			if (size < CTF_LSTRUCT_THRESH) {
				ctf_member_t *mp = (ctf_member_t *)
				    ((uintptr_t)tp + increment);

				vbytes = sizeof (ctf_member_t) * vlen;
				for (n = vlen; n != 0; n--, mp++)
					child |= CTF_TYPE_ISCHILD(mp->ctm_type);
			} else {
				ctf_lmember_t *lmp = (ctf_lmember_t *)
				    ((uintptr_t)tp + increment);

				vbytes = sizeof (ctf_lmember_t) * vlen;
				for (n = vlen; n != 0; n--, lmp++)
					child |=
					    CTF_TYPE_ISCHILD(lmp->ctlm_type);
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
		tp = (ctf_type_t *)((uintptr_t)tp + increment + vbytes);
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

static const ctf_type_t *
ctf_lookup_by_id(linker_ctf_t *lc, ctf_id_t type)
{
	const ctf_type_t *tp;
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

	tp = (const ctf_type_t *)(lc->ctftab + offset + sizeof(ctf_header_t));

	return (tp);
}

static void
fbt_array_info(linker_ctf_t *lc, ctf_id_t type, ctf_arinfo_t *arp)
{
	const ctf_header_t *hp = (const ctf_header_t *) lc->ctftab;
	const ctf_type_t *tp;
	const ctf_array_t *ap;
	ssize_t increment;

	bzero(arp, sizeof(*arp));

	if ((tp = ctf_lookup_by_id(lc, type)) == NULL)
		return;

	if (CTF_INFO_KIND(tp->ctt_info) != CTF_K_ARRAY)
		return;

	(void) fbt_get_ctt_size(hp->cth_version, tp, NULL, &increment);

	ap = (const ctf_array_t *)((uintptr_t)tp + increment);
	arp->ctr_contents = ap->cta_contents;
	arp->ctr_index = ap->cta_index;
	arp->ctr_nelems = ap->cta_nelems;
}

static const char *
ctf_strptr(linker_ctf_t *lc, int name)
{
	const ctf_header_t *hp = (const ctf_header_t *) lc->ctftab;;
	const char *strp = "";

	if (name < 0 || name >= hp->cth_strlen)
		return(strp);

	strp = (const char *)(lc->ctftab + hp->cth_stroff + name + sizeof(ctf_header_t));

	return (strp);
}

static void
ctf_decl_push(ctf_decl_t *cd, linker_ctf_t *lc, ctf_id_t type)
{
	ctf_decl_node_t *cdp;
	ctf_decl_prec_t prec;
	uint_t kind, n = 1;
	int is_qual = 0;

	const ctf_type_t *tp;
	ctf_arinfo_t ar;

	if ((tp = ctf_lookup_by_id(lc, type)) == NULL) {
		cd->cd_err = ENOENT;
		return;
	}

	switch (kind = CTF_INFO_KIND(tp->ctt_info)) {
	case CTF_K_ARRAY:
		fbt_array_info(lc, type, &ar);
		ctf_decl_push(cd, lc, ar.ctr_contents);
		n = ar.ctr_nelems;
		prec = CTF_PREC_ARRAY;
		break;

	case CTF_K_TYPEDEF:
		if (ctf_strptr(lc, tp->ctt_name)[0] == '\0') {
			ctf_decl_push(cd, lc, tp->ctt_type);
			return;
		}
		prec = CTF_PREC_BASE;
		break;

	case CTF_K_FUNCTION:
		ctf_decl_push(cd, lc, tp->ctt_type);
		prec = CTF_PREC_FUNCTION;
		break;

	case CTF_K_POINTER:
		ctf_decl_push(cd, lc, tp->ctt_type);
		prec = CTF_PREC_POINTER;
		break;

	case CTF_K_VOLATILE:
	case CTF_K_CONST:
	case CTF_K_RESTRICT:
		ctf_decl_push(cd, lc, tp->ctt_type);
		prec = cd->cd_qualp;
		is_qual++;
		break;

	default:
		prec = CTF_PREC_BASE;
	}

	if ((cdp = malloc(sizeof (ctf_decl_node_t), M_FBT, M_WAITOK)) == NULL) {
		cd->cd_err = EAGAIN;
		return;
	}

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

			const ctf_type_t *tp =
			    ctf_lookup_by_id(lc, cdp->cd_type);
			const char *name = ctf_strptr(lc, tp->ctt_name);

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
	const ushort_t *dp;
	fbt_probe_t *fbt = parg;
	linker_ctf_t lc;
	modctl_t *ctl = fbt->fbtp_ctl;
	int ndx = desc->dtargd_ndx;
	int symindx = fbt->fbtp_symindx;
	uint32_t *ctfoff;
	uint32_t offset;
	ushort_t info, kind, n;

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

	dp = (const ushort_t *)(lc.ctftab + offset + sizeof(ctf_header_t));

	info = *dp++;
	kind = CTF_INFO_KIND(info);
	n = CTF_INFO_VLEN(info);

	if (kind == CTF_K_UNKNOWN && n == 0) {
		printf("%s(%d): Unknown function!\n",__func__,__LINE__);
		return;
	}

	if (kind != CTF_K_FUNCTION) {
		printf("%s(%d): Expected a function!\n",__func__,__LINE__);
		return;
	}

	/* Check if the requested argument doesn't exist. */
	if (ndx >= n)
		return;

	/* Skip the return type and arguments up to the one requested. */
	dp += ndx + 1;

	if (fbt_type_name(&lc, *dp, desc->dtargd_native, sizeof(desc->dtargd_native)) > 0)
		desc->dtargd_ndx = ndx;

	return;
}

static void
fbt_load(void *dummy)
{
	/* Create the /dev/dtrace/fbt entry. */
	fbt_cdev = make_dev(&fbt_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "dtrace/fbt");

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
}


static int
fbt_unload()
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

	destroy_dev(fbt_cdev);

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

static int
fbt_open(struct cdev *dev __unused, int oflags __unused, int devtype __unused, struct thread *td __unused)
{
	return (0);
}

SYSINIT(fbt_load, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, fbt_load, NULL);
SYSUNINIT(fbt_unload, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, fbt_unload, NULL);

DEV_MODULE(fbt, fbt_modevent, NULL);
MODULE_VERSION(fbt, 1);
MODULE_DEPEND(fbt, dtrace, 1, 1, 1);
MODULE_DEPEND(fbt, opensolaris, 1, 1, 1);
