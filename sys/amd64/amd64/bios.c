/*-
 * Copyright (c) 1997 Michael Smith
 * Copyright (c) 1998 Jonathan Lemon
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
 *      $Id: bios.c,v 1.13 1999/07/29 01:49:17 msmith Exp $
 */

/*
 * Code for dealing with the BIOS in x86 PC systems.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/md_var.h>
#include <machine/segments.h>
#include <machine/stdarg.h>
#include <machine/tss.h>
#include <machine/vmparam.h>
#include <machine/pc/bios.h>

#define BIOS_START	0xe0000
#define BIOS_SIZE	0x20000

/* exported lookup results */
struct bios32_SDentry		PCIbios = {entry : 0};
static struct SMBIOS_table	*SMBIOStable = 0;
static struct DMI_table		*DMItable = 0;

static u_int		bios32_SDCI = 0;

static void		bios32_init(void *junk);

/* start fairly early */
SYSINIT(bios32, SI_SUB_CPU, SI_ORDER_ANY, bios32_init, NULL);

/*
 * bios32_init
 *
 * Locate various bios32 entities.
 */
static void
bios32_init(void *junk)
{
    u_long			sigaddr;
    struct bios32_SDheader	*sdh;
    struct SMBIOS_table		*sbt;
    struct DMI_table		*dmit;
    u_int8_t			ck, *cv;
    int				i;
    

    /*
     * BIOS32 Service Directory
     */
    
    /* look for the signature */
    if ((sigaddr = bios_sigsearch(0, "_32_", 4, 16, 0)) != 0) {

	/* get a virtual pointer to the structure */
	sdh = (struct bios32_SDheader *)(uintptr_t)BIOS_PADDRTOVADDR(sigaddr);
	for (cv = (u_int8_t *)sdh, ck = 0, i = 0; i < (sdh->len * 16); i++) {
	    ck += cv[i];
	}
	/* If checksum is OK, enable use of the entrypoint */
	if ((ck == 0) && (sdh->entry < (BIOS_START + BIOS_SIZE))) {
	    bios32_SDCI = BIOS_PADDRTOVADDR(sdh->entry);
	    if (bootverbose) {
		printf("Found BIOS32 Service Directory header at %p\n", sdh);
		printf("Entry = 0x%x (%x)  Rev = %d  Len = %d\n", 
		       sdh->entry, bios32_SDCI, sdh->revision, sdh->len);
	    }
	    /* See if there's a PCI BIOS entrypoint here */
	    PCIbios.ident.id = 0x49435024;	/* PCI systems should have this */
	    if (!bios32_SDlookup(&PCIbios) && bootverbose)
		printf("PCI BIOS entry at 0x%x\n", PCIbios.entry);
	} else {
	    printf("Bad BIOS32 Service Directory!\n");
	}
    }

    /*
     * System Management BIOS
     */
    /* look for the SMBIOS signature */
    if ((sigaddr = bios_sigsearch(0, "_SM_", 4, 16, 0)) != 0) {

	/* get a virtual pointer to the structure */
	sbt = (struct SMBIOS_table *)(uintptr_t)BIOS_PADDRTOVADDR(sigaddr);
	for (cv = (u_int8_t *)sbt, ck = 0, i = 0; i < sbt->len; i++) {
	    ck += cv[i];
	}
	/* if checksum is OK, we have action */
	if (ck == 0) {
	    SMBIOStable = sbt;		/* save reference */
	    DMItable = &sbt->dmi;	/* contained within */
	    if (bootverbose) {
		printf("SMIBIOS header at %p\n", sbt);
		printf("Version %d.%d\n", sbt->major, sbt->minor);
		printf("Table at 0x%x, %d entries, %d bytes, largest entry %d bytes\n",
		       sbt->dmi.st_base, (int)sbt->dmi.st_entries, (int)sbt->dmi.st_size,
		       (int)sbt->st_maxsize);
	    }
	} else {
	    printf("Bad SMBIOS table checksum!\n");
	}
	
    }

    /* look for the DMI signature */
    if ((sigaddr = bios_sigsearch(0, "_DMI_", 5, 16, 0)) != 0) {

	/* get a virtual pointer to the structure */
	dmit = (struct DMI_table *)(uintptr_t)BIOS_PADDRTOVADDR(sigaddr);
	for (cv = (u_int8_t *)dmit, ck = 0, i = 0; i < 15; i++) {
	    ck += cv[i];
	}
	/* if checksum is OK, we have action */
	if (ck == 0) {
	    DMItable = dmit;		/* save reference */
	    if (bootverbose) {
		printf("DMI header at %p\n", dmit);
		printf("Version %d.%d\n", (dmit->bcd_revision >> 4),
		       (dmit->bcd_revision & 0x0f));
		printf("Table at 0x%x, %d entries, %d bytes\n",
		       dmit->st_base, (int)dmit->st_entries,
		       (int)dmit->st_size);
	    }
	} else {
	    printf("Bad DMI table checksum!\n");
	}
    }
    if (bootverbose) {
	    /* look for other know signatures */
	    printf("Other BIOS signatures found:\n");
	    printf("ACPI: %08x\n", bios_sigsearch(0, "RST PTR", 8, 16, 0));
	    printf("$PnP: %08x\n", bios_sigsearch(0, "$PnP", 4, 16, 0));
    }
}

/*
 * bios32_SDlookup
 *
 * Query the BIOS32 Service Directory for the service named in (ent),
 * returns nonzero if the lookup fails.  The caller must fill in
 * (ent->ident), the remainder are populated on a successful lookup.
 */
int
bios32_SDlookup(struct bios32_SDentry *ent)
{
	struct bios_regs args;

	if (bios32_SDCI == 0)
		return (1);

	args.eax = ent->ident.id;		/* set up arguments */
	args.ebx = args.ecx = args.edx = 0;
	bios32(&args, bios32_SDCI, GSEL(GCODE_SEL, SEL_KPL));
	if ((args.eax & 0xff) == 0) {		/* success? */
		ent->base = args.ebx;
		ent->len = args.ecx;
		ent->entry = args.edx;
		return (0);			/* all OK */
	}
	return (1);				/* failed */
}


/*
 * bios_sigsearch
 *
 * Search some or all of the BIOS region for a signature string.
 *
 * (start)	Optional offset returned from this function 
 *		(for searching for multiple matches), or NULL
 *		to start the search from the base of the BIOS.
 *		Note that this will be a _physical_ address in
 *		the range 0xe0000 - 0xfffff.
 * (sig)	is a pointer to the byte(s) of the signature.
 * (siglen)	number of bytes in the signature.
 * (paralen)	signature paragraph (alignment) size.
 * (sigofs)	offset of the signature within the paragraph.
 *
 * Returns the _physical_ address of the found signature, 0 if the
 * signature was not found.
 */

u_int32_t
bios_sigsearch(u_int32_t start, u_char *sig, int siglen, int paralen, int sigofs)
{
    u_char	*sp, *end;
    
    /* compute the starting address */
    if ((start >= BIOS_START) && (start <= (BIOS_START + BIOS_SIZE))) {
	sp = (char *)BIOS_PADDRTOVADDR(start);
    } else if (start == 0) {
	sp = (char *)BIOS_PADDRTOVADDR(BIOS_START);
    } else {
	return 0;				/* bogus start address */
    }

    /* compute the end address */
    end = (u_char *)BIOS_PADDRTOVADDR(BIOS_START + BIOS_SIZE);

    /* loop searching */
    while ((sp + sigofs + siglen) < end) {
	
	/* compare here */
	if (!bcmp(sp + sigofs, sig, siglen)) {
	    /* convert back to physical address */
	    return((u_int32_t)BIOS_VADDRTOPADDR(sp));
	}
	sp += paralen;
    }
    return(0);
}

/*
 * do not staticize, used by bioscall.s
 */
union {
	struct {
		u_short	offset;
		u_short	segment;
	} vec16;
	struct {
		u_int	offset;
		u_short	segment;
	} vec32;
} bioscall_vector;			/* bios jump vector */

void
set_bios_selectors(struct bios_segments *seg, int flags)
{
	static u_int curgen = 1;
	struct soft_segment_descriptor ssd = {
		0,			/* segment base address (overwritten) */
		0,			/* length (overwritten) */
		SDT_MEMERA,		/* segment type (overwritten) */
		0,			/* priority level */
		1,			/* descriptor present */
		0, 0,
		1,			/* descriptor size (overwritten) */
		0			/* granularity == byte units */
	};

	if (seg->generation == curgen)
		return;
	if (++curgen == 0)
		curgen = 1;
	seg->generation = curgen;
	
	ssd.ssd_base = seg->code32.base;
	ssd.ssd_limit = seg->code32.limit;
	ssdtosd(&ssd, &gdt[GBIOSCODE32_SEL].sd);

	ssd.ssd_def32 = 0;
	if (flags & BIOSCODE_FLAG) {
		ssd.ssd_base = seg->code16.base;
		ssd.ssd_limit = seg->code16.limit;
		ssdtosd(&ssd, &gdt[GBIOSCODE16_SEL].sd);
	}

	ssd.ssd_type = SDT_MEMRWA;
	if (flags & BIOSDATA_FLAG) {
		ssd.ssd_base = seg->data.base;
		ssd.ssd_limit = seg->data.limit;
		ssdtosd(&ssd, &gdt[GBIOSDATA_SEL].sd);
	}

	if (flags & BIOSUTIL_FLAG) {
		ssd.ssd_base = seg->util.base;
		ssd.ssd_limit = seg->util.limit;
		ssdtosd(&ssd, &gdt[GBIOSUTIL_SEL].sd);
	}

	if (flags & BIOSARGS_FLAG) {
		ssd.ssd_base = seg->args.base;
		ssd.ssd_limit = seg->args.limit;
		ssdtosd(&ssd, &gdt[GBIOSARGS_SEL].sd);
	}
}

/*
 * for pointers, we don't know how much space is supposed to be allocated,
 * so we assume a minimum size of 256 bytes.  If more than this is needed,
 * then this can be revisited, such as adding a length specifier.
 */
#define	ASSUMED_ARGSIZE		256

extern int vm86pa;

/*
 * this routine is really greedy with selectors, and uses 5:
 *
 * 32-bit code selector:	to return to kernel
 * 16-bit code selector:	for running code
 *        data selector:	for 16-bit data
 *        util selector:	extra utility selector
 *        args selector:	to handle pointers
 *
 * the util selector is set from the util16 entry in bios16_args, if a
 * "U" specifier is seen.
 *
 * See <machine/pc/bios.h> for description of format specifiers
 */
int
bios16(struct bios_args *args, char *fmt, ...)
{
	char *p, *stack, *stack_top;
	va_list ap;
	int flags = BIOSCODE_FLAG | BIOSDATA_FLAG;
	u_int i, arg_start, arg_end;
	u_int *pte, *ptd;

	arg_start = 0xffffffff;
	arg_end = 0;

	stack = (caddr_t)PAGE_SIZE;
	va_start(ap, fmt);
	for (p = fmt; p && *p; p++) {
		switch (*p) {
		case 'p':			/* 32-bit pointer */
			i = va_arg(ap, u_int);
			arg_start = min(arg_start, i);
			arg_end = max(arg_end, i + ASSUMED_ARGSIZE);
			flags |= BIOSARGS_FLAG;
			stack -= 4;
			break;

		case 'i':			/* 32-bit integer */
			i = va_arg(ap, u_int);
			stack -= 4;
			break;

		case 'U':			/* 16-bit selector */
			flags |= BIOSUTIL_FLAG;
			/* FALL THROUGH */
		case 'D':			/* 16-bit selector */
		case 'C':			/* 16-bit selector */
		case 's':			/* 16-bit integer */
			i = va_arg(ap, u_short);
			stack -= 2;
			break;

		default:
			return (EINVAL);
		}
	}

	if (flags & BIOSARGS_FLAG) {
		if (arg_end - arg_start > ctob(16))
			return (EACCES);
		args->seg.args.base = arg_start;
		args->seg.args.limit = arg_end - arg_start;
	}

	args->seg.code32.base = (u_int)&bios16_call & PG_FRAME;
	args->seg.code32.limit = 0xffff;	

	ptd = (u_int *)rcr3();
	if (ptd == IdlePTD)
	{
		/*
		 * no page table, so create one and install it.
		 */
		pte = (u_int *)malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
		ptd = (u_int *)((u_int)ptd + KERNBASE);
		*ptd = vtophys(pte) | PG_RW | PG_V;
	} else {
		/*
		 * this is a user-level page table 
		 */
		pte = (u_int *)&PTmap;
	}
	/*
	 * install pointer to page 0.  we don't need to flush the tlb,
	 * since there should not be a previous mapping for page 0.
	 */
	*pte = (vm86pa - PAGE_SIZE) | PG_RW | PG_V; 

	stack_top = stack;
	va_start(ap, fmt);
	for (p = fmt; p && *p; p++) {
		switch (*p) {
		case 'p':			/* 32-bit pointer */
			i = va_arg(ap, u_int);
			*(u_int *)stack = (i - arg_start) |
			    (GSEL(GBIOSARGS_SEL, SEL_KPL) << 16);
			stack += 4;
			break;

		case 'i':			/* 32-bit integer */
			i = va_arg(ap, u_int);
			*(u_int *)stack = i;
			stack += 4;
			break;

		case 'U':			/* 16-bit selector */
			i = va_arg(ap, u_short);
			*(u_short *)stack = GSEL(GBIOSUTIL_SEL, SEL_KPL);
			stack += 2;
			break;

		case 'D':			/* 16-bit selector */
			i = va_arg(ap, u_short);
			*(u_short *)stack = GSEL(GBIOSDATA_SEL, SEL_KPL);
			stack += 2;
			break;

		case 'C':			/* 16-bit selector */
			i = va_arg(ap, u_short);
			*(u_short *)stack = GSEL(GBIOSCODE16_SEL, SEL_KPL);
			stack += 2;
			break;

		case 's':			/* 16-bit integer */
			i = va_arg(ap, u_short);
			*(u_short *)stack = i;
			stack += 2;
			break;

		default:
			return (EINVAL);
		}
	}

	args->seg.generation = 0;			/* reload selectors */
	set_bios_selectors(&args->seg, flags);
	bioscall_vector.vec16.offset = (u_short)args->entry;
	bioscall_vector.vec16.segment = GSEL(GBIOSCODE16_SEL, SEL_KPL);

	i = bios16_call(&args->r, stack_top);

	if (pte == (u_int *)&PTmap) {
		*pte = 0;			/* remove entry */
	} else {
		*ptd = 0;			/* remove page table */
		free(pte, M_TEMP);		/* ... and free it */
	}


	/*
	 * XXX only needs to be invlpg(0) but that doesn't work on the 386 
	 */
	invltlb();

	return (i);
}
