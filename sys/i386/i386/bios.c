/*-
 * Copyright (c) 1997 Michael Smith
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
 *      $Id: bios.c,v 1.7 1997/10/21 07:40:22 msmith Exp $
 */

/*
 * Code for dealing with the BIOS in x86 PC systems.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/md_var.h>

#include <machine/pc/bios.h>

#define BIOS_START	0xe0000
#define BIOS_SIZE	0x20000

/* exported lookup results */
struct bios32_SDentry	PCIbios = {entry : 0};
struct SMBIOS_table	*SMBIOStable = 0;
struct DMI_table	*DMItable = 0;

static caddr_t		bios32_SDCI = NULL;

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
	sdh = (struct bios32_SDheader *)BIOS_PADDRTOVADDR(sigaddr);
	for (cv = (u_int8_t *)sdh, ck = 0, i = 0; i < (sdh->len * 16); i++) {
	    ck += cv[i];
	}
	/* If checksum is OK, enable use of the entrypoint */
	if (ck == 0) {
	    bios32_SDCI = (caddr_t)BIOS_PADDRTOVADDR(sdh->entry);
	    if (bootverbose) {
		printf("Found BIOS32 Service Directory header at %p\n", sdh);
		printf("Entry = 0x%x (%p)  Rev = %d  Len = %d\n", 
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
	sbt = (struct SMBIOS_table *)BIOS_PADDRTOVADDR(sigaddr);
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
	dmit = (struct DMI_table *)BIOS_PADDRTOVADDR(sigaddr);
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
	    printf("ACPI: %08x\n", bios_sigsearch(0, "FACP", 4, 1, 0));
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
    struct bios32_args	args;
    
    if (bios32_SDCI != NULL) {

	args.eax = ent->ident.id;		/* set up arguments */
	args.ebx = args.ecx = args.edx = 0;
	bios32(bios32_SDCI, &args);		/* make the BIOS call */
	if ((args.eax & 0xff) == 0) {		/* success? */
	    ent->base = args.ebx;
	    ent->len = args.ecx;
	    ent->entry = args.edx;
	    return(0);				/* all OK */
	}
    }
    return(1);					/* failed */
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

    
	    
