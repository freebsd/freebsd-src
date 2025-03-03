/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1998 Michael Smith
 * All rights reserved.
 * Copyright (c) 2020 NetApp Inc.
 * Copyright (c) 2020 Klara Inc.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/linker.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>

#include <machine/metadata.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

/*
 * Preloaded module support
 */

vm_offset_t preload_addr_relocate = 0;
caddr_t preload_metadata, preload_kmdp;

const char preload_modtype[] = MODTYPE;
const char preload_kerntype[] = KERNTYPE;
const char preload_modtype_obj[] = MODTYPE_OBJ;

void
preload_initkmdp(bool fatal)
{
	preload_kmdp = preload_search_by_type(preload_kerntype);

	if (preload_kmdp == NULL && fatal)
		panic("unable to find kernel metadata");
}

/*
 * Search for the preloaded module (name)
 */
caddr_t
preload_search_by_name(const char *name)
{
    caddr_t	curp;
    uint32_t	*hdr;
    int		next;
    
    if (preload_metadata != NULL) {
	curp = preload_metadata;
	for (;;) {
	    hdr = (uint32_t *)curp;
	    if (hdr[0] == 0 && hdr[1] == 0)
		break;

	    /* Search for a MODINFO_NAME field */
	    if ((hdr[0] == MODINFO_NAME) &&
		!strcmp(name, curp + sizeof(uint32_t) * 2))
		return(curp);

	    /* skip to next field */
	    next = sizeof(uint32_t) * 2 + hdr[1];
	    next = roundup(next, sizeof(u_long));
	    curp += next;
	}
    }
    return(NULL);
}

/*
 * Search for the first preloaded module of (type)
 */
caddr_t
preload_search_by_type(const char *type)
{
    caddr_t	curp, lname;
    uint32_t	*hdr;
    int		next;

    if (preload_metadata != NULL) {
	curp = preload_metadata;
	lname = NULL;
	for (;;) {
	    hdr = (uint32_t *)curp;
	    if (hdr[0] == 0 && hdr[1] == 0)
		break;

	    /* remember the start of each record */
	    if (hdr[0] == MODINFO_NAME)
		lname = curp;

	    /* Search for a MODINFO_TYPE field */
	    if ((hdr[0] == MODINFO_TYPE) &&
		!strcmp(type, curp + sizeof(uint32_t) * 2))
		return(lname);

	    /* skip to next field */
	    next = sizeof(uint32_t) * 2 + hdr[1];
	    next = roundup(next, sizeof(u_long));
	    curp += next;
	}
    }
    return(NULL);
}

/*
 * Walk through the preloaded module list
 */
caddr_t
preload_search_next_name(caddr_t base)
{
    caddr_t	curp;
    uint32_t	*hdr;
    int		next;
    
    if (preload_metadata != NULL) {
	/* Pick up where we left off last time */
	if (base) {
	    /* skip to next field */
	    curp = base;
	    hdr = (uint32_t *)curp;
	    next = sizeof(uint32_t) * 2 + hdr[1];
	    next = roundup(next, sizeof(u_long));
	    curp += next;
	} else
	    curp = preload_metadata;

	for (;;) {
	    hdr = (uint32_t *)curp;
	    if (hdr[0] == 0 && hdr[1] == 0)
		break;

	    /* Found a new record? */
	    if (hdr[0] == MODINFO_NAME)
		return curp;

	    /* skip to next field */
	    next = sizeof(uint32_t) * 2 + hdr[1];
	    next = roundup(next, sizeof(u_long));
	    curp += next;
	}
    }
    return(NULL);
}

/*
 * Given a preloaded module handle (mod), return a pointer
 * to the data for the attribute (inf).
 */
caddr_t
preload_search_info(caddr_t mod, int inf)
{
    caddr_t	curp;
    uint32_t	*hdr;
    uint32_t	type = 0;
    int		next;

    if (mod == NULL)
    	return (NULL);

    curp = mod;
    for (;;) {
	hdr = (uint32_t *)curp;
	/* end of module data? */
	if (hdr[0] == 0 && hdr[1] == 0)
	    break;
	/* 
	 * We give up once we've looped back to what we were looking at 
	 * first - this should normally be a MODINFO_NAME field.
	 */
	if (type == 0) {
	    type = hdr[0];
	} else {
	    if (hdr[0] == type)
		break;
	}

	/* 
	 * Attribute match? Return pointer to data.
	 * Consumer may safely assume that size value precedes	
	 * data.
	 */
	if (hdr[0] == inf)
	    return(curp + (sizeof(uint32_t) * 2));

	/* skip to next field */
	next = sizeof(uint32_t) * 2 + hdr[1];
	next = roundup(next, sizeof(u_long));
	curp += next;
    }
    return(NULL);
}

/*
 * Delete a preload record by name.
 */
void
preload_delete_name(const char *name)
{
    caddr_t	addr, curp;
    uint32_t	*hdr, sz;
    int		next;
    int		clearing;

    addr = 0;
    sz = 0;
    
    if (preload_metadata != NULL) {
	clearing = 0;
	curp = preload_metadata;
	for (;;) {
	    hdr = (uint32_t *)curp;
	    if (hdr[0] == MODINFO_NAME || (hdr[0] == 0 && hdr[1] == 0)) {
		/* Free memory used to store the file. */
		if (addr != 0 && sz != 0)
		    kmem_bootstrap_free((vm_offset_t)addr, sz);
		addr = 0;
		sz = 0;

		if (hdr[0] == 0)
		    break;
		if (!strcmp(name, curp + sizeof(uint32_t) * 2))
		    clearing = 1;	/* got it, start clearing */
		else if (clearing) {
		    clearing = 0;	/* at next one now.. better stop */
		}
	    }
	    if (clearing) {
		if (hdr[0] == MODINFO_ADDR)
		    addr = *(caddr_t *)(curp + sizeof(uint32_t) * 2);
		else if (hdr[0] == MODINFO_SIZE)
		    sz = *(uint32_t *)(curp + sizeof(uint32_t) * 2);
		hdr[0] = MODINFO_EMPTY;
	    }

	    /* skip to next field */
	    next = sizeof(uint32_t) * 2 + hdr[1];
	    next = roundup(next, sizeof(u_long));
	    curp += next;
	}
    }
}

void *
preload_fetch_addr(caddr_t mod)
{
	caddr_t *mdp;

	mdp = (caddr_t *)preload_search_info(mod, MODINFO_ADDR);
	if (mdp == NULL)
		return (NULL);
	return (*mdp + preload_addr_relocate);
}

size_t
preload_fetch_size(caddr_t mod)
{
	size_t *mdp;

	mdp = (size_t *)preload_search_info(mod, MODINFO_SIZE);
	if (mdp == NULL)
		return (0);
	return (*mdp);
}

/* Called from locore.  Convert physical pointers to kvm. Sigh. */
void
preload_bootstrap_relocate(vm_offset_t offset)
{
    caddr_t	curp;
    uint32_t	*hdr;
    vm_offset_t	*ptr;
    int		next;
    
    if (preload_metadata != NULL) {
	curp = preload_metadata;
	for (;;) {
	    hdr = (uint32_t *)curp;
	    if (hdr[0] == 0 && hdr[1] == 0)
		break;

	    /* Deal with the ones that we know we have to fix */
	    switch (hdr[0]) {
	    case MODINFO_ADDR:
	    case MODINFO_METADATA|MODINFOMD_FONT:
	    case MODINFO_METADATA|MODINFOMD_SPLASH:
	    case MODINFO_METADATA|MODINFOMD_SSYM:
	    case MODINFO_METADATA|MODINFOMD_ESYM:
		ptr = (vm_offset_t *)(curp + (sizeof(uint32_t) * 2));
		*ptr += offset;
		break;
	    }
	    /* The rest is beyond us for now */

	    /* skip to next field */
	    next = sizeof(uint32_t) * 2 + hdr[1];
	    next = roundup(next, sizeof(u_long));
	    curp += next;
	}
    }
}

/*
 * Parse the modinfo type and append to the provided sbuf.
 */
static void
preload_modinfo_type(struct sbuf *sbp, int type)
{

	if ((type & MODINFO_METADATA) == 0) {
		switch (type) {
		case MODINFO_END:
			sbuf_cat(sbp, "MODINFO_END");
			break;
		case MODINFO_NAME:
			sbuf_cat(sbp, "MODINFO_NAME");
			break;
		case MODINFO_TYPE:
			sbuf_cat(sbp, "MODINFO_TYPE");
			break;
		case MODINFO_ADDR:
			sbuf_cat(sbp, "MODINFO_ADDR");
			break;
		case MODINFO_SIZE:
			sbuf_cat(sbp, "MODINFO_SIZE");
			break;
		case MODINFO_EMPTY:
			sbuf_cat(sbp, "MODINFO_EMPTY");
			break;
		case MODINFO_ARGS:
			sbuf_cat(sbp, "MODINFO_ARGS");
			break;
		default:
			sbuf_cat(sbp, "unrecognized modinfo attribute");
		}

		return;
	}

	sbuf_cat(sbp, "MODINFO_METADATA | ");
	switch (type & ~MODINFO_METADATA) {
	case MODINFOMD_ELFHDR:
		sbuf_cat(sbp, "MODINFOMD_ELFHDR");
		break;
	case MODINFOMD_SSYM:
		sbuf_cat(sbp, "MODINFOMD_SSYM");
		break;
	case MODINFOMD_ESYM:
		sbuf_cat(sbp, "MODINFOMD_ESYM");
		break;
	case MODINFOMD_DYNAMIC:
		sbuf_cat(sbp, "MODINFOMD_DYNAMIC");
		break;
	case MODINFOMD_ENVP:
		sbuf_cat(sbp, "MODINFOMD_ENVP");
		break;
	case MODINFOMD_HOWTO:
		sbuf_cat(sbp, "MODINFOMD_HOWTO");
		break;
	case MODINFOMD_KERNEND:
		sbuf_cat(sbp, "MODINFOMD_KERNEND");
		break;
	case MODINFOMD_SHDR:
		sbuf_cat(sbp, "MODINFOMD_SHDR");
		break;
	case MODINFOMD_CTORS_ADDR:
		sbuf_cat(sbp, "MODINFOMD_CTORS_ADDR");
		break;
	case MODINFOMD_CTORS_SIZE:
		sbuf_cat(sbp, "MODINFOMD_CTORS_SIZE");
		break;
	case MODINFOMD_FW_HANDLE:
		sbuf_cat(sbp, "MODINFOMD_FW_HANDLE");
		break;
	case MODINFOMD_KEYBUF:
		sbuf_cat(sbp, "MODINFOMD_KEYBUF");
		break;
#ifdef MODINFOMD_SMAP
	case MODINFOMD_SMAP:
		sbuf_cat(sbp, "MODINFOMD_SMAP");
		break;
#endif
#ifdef MODINFOMD_SMAP_XATTR
	case MODINFOMD_SMAP_XATTR:
		sbuf_cat(sbp, "MODINFOMD_SMAP_XATTR");
		break;
#endif
#ifdef MODINFOMD_DTBP
	case MODINFOMD_DTBP:
		sbuf_cat(sbp, "MODINFOMD_DTBP");
		break;
#endif
#ifdef MODINFOMD_EFI_MAP
	case MODINFOMD_EFI_MAP:
		sbuf_cat(sbp, "MODINFOMD_EFI_MAP");
		break;
#endif
#ifdef MODINFOMD_EFI_FB
	case MODINFOMD_EFI_FB:
		sbuf_cat(sbp, "MODINFOMD_EFI_FB");
		break;
#endif
#ifdef MODINFOMD_MODULEP
	case MODINFOMD_MODULEP:
		sbuf_cat(sbp, "MODINFOMD_MODULEP");
		break;
#endif
#ifdef MODINFOMD_VBE_FB
	case MODINFOMD_VBE_FB:
		sbuf_cat(sbp, "MODINFOMD_VBE_FB");
		break;
#endif
#ifdef MODINFOMD_FONT
	case MODINFOMD_FONT:
		sbuf_cat(sbp, "MODINFOMD_FONT");
		break;
#endif
#ifdef MODINFOMD_SPLASH
	case MODINFOMD_SPLASH:
		sbuf_cat(sbp, "MODINFOMD_SPLASH");
		break;
#endif
#ifdef MODINFOMD_BOOT_HARTID
	case MODINFOMD_BOOT_HARTID:
		sbuf_cat(sbp, "MODINFOMD_BOOT_HARTID");
		break;
#endif
	default:
		sbuf_cat(sbp, "unrecognized metadata type");
	}
}

/*
 * Print the modinfo value, depending on type.
 */
static void
preload_modinfo_value(struct sbuf *sbp, uint32_t *bptr, int type, int len)
{
#ifdef __LP64__
#define sbuf_print_vmoffset(sb, o)	sbuf_printf(sb, "0x%016lx", o);
#else
#define sbuf_print_vmoffset(sb, o)	sbuf_printf(sb, "0x%08x", o);
#endif

	switch (type) {
	case MODINFO_NAME:
	case MODINFO_TYPE:
	case MODINFO_ARGS:
		sbuf_printf(sbp, "%s", (char *)bptr);
		break;
	case MODINFO_SIZE:
	case MODINFO_METADATA | MODINFOMD_CTORS_SIZE:
		sbuf_printf(sbp, "%lu", *(u_long *)bptr);
		break;
	case MODINFO_ADDR:
	case MODINFO_METADATA | MODINFOMD_SSYM:
	case MODINFO_METADATA | MODINFOMD_ESYM:
	case MODINFO_METADATA | MODINFOMD_DYNAMIC:
	case MODINFO_METADATA | MODINFOMD_KERNEND:
	case MODINFO_METADATA | MODINFOMD_ENVP:
	case MODINFO_METADATA | MODINFOMD_CTORS_ADDR:
#ifdef MODINFOMD_SMAP
	case MODINFO_METADATA | MODINFOMD_SMAP:
#endif
#ifdef MODINFOMD_SMAP_XATTR
	case MODINFO_METADATA | MODINFOMD_SMAP_XATTR:
#endif
#ifdef MODINFOMD_DTBP
	case MODINFO_METADATA | MODINFOMD_DTBP:
#endif
#ifdef MODINFOMD_EFI_FB
	case MODINFO_METADATA | MODINFOMD_EFI_FB:
#endif
#ifdef MODINFOMD_VBE_FB
	case MODINFO_METADATA | MODINFOMD_VBE_FB:
#endif
#ifdef MODINFOMD_FONT
	case MODINFO_METADATA | MODINFOMD_FONT:
#endif
#ifdef MODINFOMD_SPLASH
	case MODINFO_METADATA | MODINFOMD_SPLASH:
#endif
		sbuf_print_vmoffset(sbp, *(vm_offset_t *)bptr);
		break;
	case MODINFO_METADATA | MODINFOMD_HOWTO:
		sbuf_printf(sbp, "0x%08x", *bptr);
		break;
#ifdef MODINFOMD_BOOT_HARTID
	case MODINFO_METADATA | MODINFOMD_BOOT_HARTID:
		sbuf_printf(sbp, "0x%lu", *(uint64_t *)bptr);
		break;
#endif
	case MODINFO_METADATA | MODINFOMD_SHDR:
	case MODINFO_METADATA | MODINFOMD_ELFHDR:
	case MODINFO_METADATA | MODINFOMD_FW_HANDLE:
	case MODINFO_METADATA | MODINFOMD_KEYBUF:
#ifdef MODINFOMD_EFI_MAP
	case MODINFO_METADATA | MODINFOMD_EFI_MAP:
#endif
		/* Don't print data buffers. */
		sbuf_cat(sbp, "buffer contents omitted");
		break;
	default:
		break;
	}
#undef sbuf_print_vmoffset
}

static void
preload_dump_internal(struct sbuf *sbp)
{
	uint32_t *bptr, type, len;

	KASSERT(preload_metadata != NULL,
	    ("%s called without setting up preload_metadata", __func__));

	/*
	 * Iterate through the TLV-encoded sections.
	 */
	bptr = (uint32_t *)preload_metadata;
	sbuf_putc(sbp, '\n');
	while (bptr[0] != MODINFO_END || bptr[1] != MODINFO_END) {
		sbuf_printf(sbp, " %p:\n", bptr);
		type = *bptr++;
		len = *bptr++;

		sbuf_printf(sbp, "\ttype:\t(%#04x) ", type);
		preload_modinfo_type(sbp, type);
		sbuf_putc(sbp, '\n');
		sbuf_printf(sbp, "\tlen:\t%u\n", len);
		sbuf_cat(sbp, "\tvalue:\t");
		preload_modinfo_value(sbp, bptr, type, len);
		sbuf_putc(sbp, '\n');

		bptr += roundup(len, sizeof(u_long)) / sizeof(uint32_t);
	}
}

/*
 * Print the preloaded data to the console. Called from the machine-dependent
 * initialization routines, e.g. hammer_time().
 */
void
preload_dump(void)
{
	char buf[512];
	struct sbuf sb;

	/*
	 * This function is expected to be called before malloc is available,
	 * so use a static buffer and struct sbuf.
	 */
	sbuf_new(&sb, buf, sizeof(buf), SBUF_FIXEDLEN);
	sbuf_set_drain(&sb, sbuf_printf_drain, NULL);
	preload_dump_internal(&sb);

	sbuf_finish(&sb);
	sbuf_delete(&sb);
}

static int
sysctl_preload_dump(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sb;
	int error;

	if (preload_metadata == NULL)
		return (EINVAL);

	sbuf_new_for_sysctl(&sb, NULL, 512, req);
	preload_dump_internal(&sb);

	error = sbuf_finish(&sb);
	sbuf_delete(&sb);

	return (error);
}
SYSCTL_PROC(_debug, OID_AUTO, dump_modinfo,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
    NULL, 0, sysctl_preload_dump, "A",
    "pretty-print the bootloader metadata");
