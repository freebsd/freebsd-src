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
 * $FreeBSD$
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
#include <isa/pnpreg.h>

#define BIOS_START	0xe0000
#define BIOS_SIZE	0x20000

/* exported lookup results */
struct bios32_SDentry		PCIbios = {entry : 0};
struct PnPBIOS_table		*PnPBIOStable = 0;

static u_int			bios32_SDCI = 0;

/* start fairly early */
static void			bios32_init(void *junk);
SYSINIT(bios32, SI_SUB_CPU, SI_ORDER_ANY, bios32_init, NULL);

static void	pnpbios_scan(void);
static char 	*pnp_eisaformat(u_int8_t *data);


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
    struct PnPBIOS_table	*pt;
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
		printf("bios32: Found BIOS32 Service Directory header at %p\n", sdh);
		printf("bios32: Entry = 0x%x (%x)  Rev = %d  Len = %d\n", 
		       sdh->entry, bios32_SDCI, sdh->revision, sdh->len);
	    }
	    /* See if there's a PCI BIOS entrypoint here */
	    PCIbios.ident.id = 0x49435024;	/* PCI systems should have this */
	    if (!bios32_SDlookup(&PCIbios) && bootverbose)
		printf("pcibios: PCI BIOS entry at 0x%x\n", PCIbios.entry);
	} else {
	    printf("bios32: Bad BIOS32 Service Directory\n");
	}
    }

    /*
     * PnP BIOS
     */
    if ((sigaddr = bios_sigsearch(0, "$PnP", 4, 16, 0)) != 0) {

	/* get a virtual pointer to the structure */
	pt = (struct PnPBIOS_table *)(uintptr_t)BIOS_PADDRTOVADDR(sigaddr);
	for (cv = (u_int8_t *)pt, ck = 0, i = 0; i < pt->len; i++) {
	    ck += cv[i];
	}
	/* If checksum is OK, enable use of the entrypoint */
	if (ck == 0) {
	    PnPBIOStable = pt;
	    if (bootverbose) {
		printf("pnpbios: Found PnP BIOS data at %p\n", pt);
		printf("pnpbios: Entry = %x:%x  Rev = %d.%d\n", 
		       pt->pmentrybase, pt->pmentryoffset, pt->version >> 4, pt->version & 0xf);
		if ((pt->control & 0x3) == 0x01)
		    printf("pnpbios: Event flag at %x\n", pt->evflagaddr);
		if (pt->oemdevid != 0)
		    printf("pnpbios: OEM ID %x\n", pt->oemdevid);
		
	    }
	    pnpbios_scan();
	} else {
	    printf("pnpbios: Bad PnP BIOS data checksum\n");
	}
    }

    if (bootverbose) {
	    /* look for other know signatures */
	    printf("Other BIOS signatures found:\n");
	    printf("ACPI: %08x\n", bios_sigsearch(0, "RSD PTR ", 8, 16, 0));
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
    if ((args.eax & 0xff) == 0) {	/* success? */
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
    union descriptor *p_gdt;

#ifdef SMP
    p_gdt = &gdt[cpuid];
#else
    p_gdt = gdt;
#endif
	
    ssd.ssd_base = seg->code32.base;
    ssd.ssd_limit = seg->code32.limit;
    ssdtosd(&ssd, &p_gdt[GBIOSCODE32_SEL].sd);

    ssd.ssd_def32 = 0;
    if (flags & BIOSCODE_FLAG) {
	ssd.ssd_base = seg->code16.base;
	ssd.ssd_limit = seg->code16.limit;
	ssdtosd(&ssd, &p_gdt[GBIOSCODE16_SEL].sd);
    }

    ssd.ssd_type = SDT_MEMRWA;
    if (flags & BIOSDATA_FLAG) {
	ssd.ssd_base = seg->data.base;
	ssd.ssd_limit = seg->data.limit;
	ssdtosd(&ssd, &p_gdt[GBIOSDATA_SEL].sd);
    }

    if (flags & BIOSUTIL_FLAG) {
	ssd.ssd_base = seg->util.base;
	ssd.ssd_limit = seg->util.limit;
	ssdtosd(&ssd, &p_gdt[GBIOSUTIL_SEL].sd);
    }

    if (flags & BIOSARGS_FLAG) {
	ssd.ssd_base = seg->args.base;
	ssd.ssd_limit = seg->args.limit;
	ssdtosd(&ssd, &p_gdt[GBIOSARGS_SEL].sd);
    }
}

extern int vm86pa;
extern void bios16_jmp(void);

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
    char	*p, *stack, *stack_top;
    va_list 	ap;
    int 	flags = BIOSCODE_FLAG | BIOSDATA_FLAG;
    u_int 	i, arg_start, arg_end;
    u_int 	*pte, *ptd;

    arg_start = 0xffffffff;
    arg_end = 0;

    /*
     * Some BIOS entrypoints attempt to copy the largest-case
     * argument frame (in order to generalise handling for 
     * different entry types).  If our argument frame is 
     * smaller than this, the BIOS will reach off the top of
     * our constructed stack segment.  Pad the top of the stack
     * with some garbage to avoid this.
     */
    stack = (caddr_t)PAGE_SIZE - 32;

    va_start(ap, fmt);
    for (p = fmt; p && *p; p++) {
	switch (*p) {
	case 'p':			/* 32-bit pointer */
	    i = va_arg(ap, u_int);
	    arg_start = min(arg_start, i);
	    arg_end = max(arg_end, i);
	    flags |= BIOSARGS_FLAG;
	    stack -= 4;
	    break;

	case 'i':			/* 32-bit integer */
	    i = va_arg(ap, u_int);
	    stack -= 4;
	    break;

	case 'U':			/* 16-bit selector */
	    flags |= BIOSUTIL_FLAG;
	    /* FALLTHROUGH */
	case 'D':			/* 16-bit selector */
	case 'C':			/* 16-bit selector */
	    stack -= 2;
	    break;
	    
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
	args->seg.args.limit = 0xffff;
    }

    args->seg.code32.base = (u_int)&bios16_jmp & PG_FRAME;
    args->seg.code32.limit = 0xffff;	

    ptd = (u_int *)rcr3();
    if (ptd == IdlePTD) {
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
	    *(u_short *)stack = GSEL(GBIOSUTIL_SEL, SEL_KPL);
	    stack += 2;
	    break;

	case 'D':			/* 16-bit selector */
	    *(u_short *)stack = GSEL(GBIOSDATA_SEL, SEL_KPL);
	    stack += 2;
	    break;

	case 'C':			/* 16-bit selector */
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

/*
 * PnP BIOS interface; enumerate devices only known to the system
 * BIOS and save information about them for later use.
 */

struct pnp_sysdev 
{
    u_int16_t	size;
    u_int8_t	handle;
    u_int32_t	devid;
    u_int8_t	type[3];
    u_int16_t	attrib;
#define PNPATTR_NODISABLE	(1<<0)	/* can't be disabled */
#define PNPATTR_NOCONFIG	(1<<1)	/* can't be configured */
#define PNPATTR_OUTPUT		(1<<2)	/* can be primary output */
#define PNPATTR_INPUT		(1<<3)	/* can be primary input */
#define PNPATTR_BOOTABLE	(1<<4)	/* can be booted from */
#define PNPATTR_DOCK		(1<<5)	/* is a docking station */
#define PNPATTR_REMOVEABLE	(1<<6)	/* device is removeable */
#define PNPATTR_CONFIG_STATIC	0x00
#define PNPATTR_CONFIG_DYNAMIC	0x07
#define PNPATTR_CONFIG_DYNONLY	0x17
    /* device-specific data comes here */
    u_int8_t	devdata[0];
} __attribute__ ((packed));

/* We have to cluster arguments within a 64k range for the bios16 call */
struct pnp_sysdevargs
{
    u_int16_t	next;
    struct pnp_sysdev node;
};

/*
 * Quiz the PnP BIOS, build a list of PNP IDs and resource data.
 */
static void
pnpbios_scan(void)
{
    struct PnPBIOS_table	*pt = PnPBIOStable;
    struct bios_args		args;
    struct pnp_sysdev		*pd;
    struct pnp_sysdevargs	*pda;
    u_int16_t			ndevs, bigdev;
    int				error, currdev;
    u_int8_t			*devnodebuf, tag;
    u_int32_t			*devid, *compid;
    int				idx, left;
        
    /* no PnP BIOS information */
    if (pt == NULL)
	return;
    
    bzero(&args, sizeof(args));
    args.seg.code16.base = BIOS_PADDRTOVADDR(pt->pmentrybase);
    args.seg.code16.limit = 0xffff;		/* XXX ? */
    args.seg.data.base = BIOS_PADDRTOVADDR(pt->pmdataseg);
    args.seg.data.limit = 0xffff;
    args.entry = pt->pmentryoffset;
    
    if ((error = bios16(&args, PNP_COUNT_DEVNODES, &ndevs, &bigdev)) || (args.r.eax & 0xff))
	printf("pnpbios: error %d/%x getting device count/size limit\n", error, args.r.eax);
    ndevs &= 0xff;				/* clear high byte garbage */
    if (bootverbose)
	printf("pnpbios: %d devices, largest %d bytes\n", ndevs, bigdev);

    devnodebuf = malloc(bigdev + (sizeof(struct pnp_sysdevargs) - sizeof(struct pnp_sysdev)),
			M_DEVBUF, M_NOWAIT);
    pda = (struct pnp_sysdevargs *)devnodebuf;
    pd = &pda->node;

    for (currdev = 0, left = ndevs; (currdev != 0xff) && (left > 0); left--) {

	bzero(pd, bigdev);
	pda->next = currdev;
	/* get current configuration */
	if ((error = bios16(&args, PNP_GET_DEVNODE, &pda->next, &pda->node, (u_int16_t)1))) {
	    printf("pnpbios: error %d making BIOS16 call\n", error);
	    break;
	}
	if ((error = (args.r.eax & 0xff))) {
	    if (bootverbose)
		printf("pnpbios: %s 0x%x fetching node %d\n", error & 0x80 ? "error" : "warning", error, currdev);
	    if (error & 0x80) 
		break;
	}
	currdev = pda->next;
	if (pd->size < sizeof(struct pnp_sysdev)) {
	    printf("pnpbios: bogus system node data, aborting scan\n");
	    break;
	}
	
	/* Find device IDs */
	devid = &pd->devid;
	compid = NULL;

	/* look for a compatible device ID too */
	left = pd->size - sizeof(struct pnp_sysdev);
	idx = 0;
	while (idx < left) {
	    tag = pd->devdata[idx++];
	    if (PNP_RES_TYPE(tag) == 0) {
		/* Small resource */
		switch (PNP_SRES_NUM(tag)) {
		case PNP_TAG_COMPAT_DEVICE:
		    compid = (u_int32_t *)(pd->devdata + idx);
		    if (bootverbose)
			printf("pnpbios: node %d compat ID 0x%08x\n", pd->handle, *compid);
		    /* FALLTHROUGH */
		case PNP_TAG_END:
		    idx = left;
		    break;
		default:
		    idx += PNP_SRES_LEN(tag);
		    break;
		}
	    } else
		/* Large resource, skip it */
		idx += *(u_int16_t *)(pd->devdata + idx) + 2;
	}
	if (bootverbose) {
	    printf("pnpbios: handle %d device ID %s (%08x)", 
		   pd->handle, pnp_eisaformat((u_int8_t *)devid), *devid);
	    if (compid != NULL)
		printf(" compat ID %s (%08x)",
		       pnp_eisaformat((u_int8_t *)compid), *compid);
	    printf("\n");
	}
    }
}

/* XXX should be somewhere else */
static char *
pnp_eisaformat(u_int8_t *data)
{
    static char idbuf[8];
    const char  hextoascii[] = "0123456789abcdef";

    idbuf[0] = '@' + ((data[0] & 0x7c) >> 2);
    idbuf[1] = '@' + (((data[0] & 0x3) << 3) + ((data[1] & 0xe0) >> 5));
    idbuf[2] = '@' + (data[1] & 0x1f);
    idbuf[3] = hextoascii[(data[2] >> 4)];
    idbuf[4] = hextoascii[(data[2] & 0xf)];
    idbuf[5] = hextoascii[(data[3] >> 4)];
    idbuf[6] = hextoascii[(data[3] & 0xf)];
    idbuf[7] = 0;
    return(idbuf);
}
