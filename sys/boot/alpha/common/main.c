/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 1998 Doug Rabson <dfr@freebsd.org>
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


#include <stand.h>
#include <string.h>

#include <sys/param.h>
#include <machine/rpb.h>
#include <machine/prom.h>
#include "bootstrap.h"
#include "libalpha/libalpha.h"

extern	char bootprog_name[], bootprog_rev[], bootprog_date[], bootprog_maker[];

struct alpha_devdesc	currdev;	/* our current device */
struct arch_switch	archsw;		/* MI/MD interface boundary */

extern char end[];
extern void OSFpal(void);
extern void halt(void);

unsigned long
memsize()
{
    struct rpb *hwrpb = (struct rpb *)HWRPB_ADDR;
    struct mddt *mddtp;
    struct mddt_cluster *memc;
    int i;
    unsigned long total = 0;

    mddtp = (struct mddt *)(((caddr_t)hwrpb) + hwrpb->rpb_memdat_off);
    for (i = 0; i < mddtp->mddt_cluster_cnt; i++) {
	memc = &mddtp->mddt_clusters[i];
	total += memc->mddt_pg_cnt << PAGE_SHIFT;
    }
    return total;
}

/* #define	XTRA_PAGES	32 */
#define	XTRA_PAGES	64

void
extend_heap(void)
{
    struct rpb *hwrpb = (struct rpb *)HWRPB_ADDR;
    struct mddt *mddtp;
    struct mddt_cluster *memc = 0;
    int i;
    unsigned long startpfn;
    vm_offset_t startva;
    vm_offset_t startpte;

    /*
     * Find the last usable memory cluster and add some of its pages
     * to our address space.  The 256k allowed by the firmware isn't quite
     * adequate for our needs.
     */
    mddtp = (struct mddt *)(((caddr_t)hwrpb) + hwrpb->rpb_memdat_off);
    for (i = mddtp->mddt_cluster_cnt - 1; i >= 0; i--) {
	memc = &mddtp->mddt_clusters[i];
	if (!(memc->mddt_usage & (MDDT_NONVOLATILE | MDDT_PALCODE)))
	    break;
    }

    /*
     * We want to extend the heap from 256k up to XTRA_PAGES more pages.
     * We take pages from the end of the last usable memory region,
     * taking care to avoid the memory used by the kernel's message
     * buffer.  We allow 4 pages for the message buffer.
     */
    startpfn = memc->mddt_pfn + memc->mddt_pg_cnt - 4 - XTRA_PAGES;
    startva = 0x20040000;
    startpte = 0x40000000
	+ (((startva >> 23) & 0x3ff) << PAGE_SHIFT)
	+ (((startva >> 13) & 0x3ff) << 3);

    for (i = 0; i < XTRA_PAGES; i++) {
	u_int64_t pte;
	pte = ((startpfn + i) << 32) | 0x1101;
	*(u_int64_t *) (startpte + 8 * i) = pte;
    }
}

int
main(void)
{
    int		i;
    char	bootfile[128];
    
    /* 
     * Initialise the heap as early as possible.  Once this is done,
     * alloc() is usable. The stack is buried inside us, so this is
     * safe.
     */
    setheap((void *)end, (void *)(0x20040000 + XTRA_PAGES * 8192));

#ifdef	LOADER
    /*
     * If this is the two stage disk loader, add the memory used by
     * the first stage to the heap.
     */
    free_region((void *)PRIMARY_LOAD_ADDRESS,
		(void *)SECONDARY_LOAD_ADDRESS);
#endif

    /* 
     * XXX Chicken-and-egg problem; we want to have console output
     * early, but some console attributes may depend on reading from
     * eg. the boot device, which we can't do yet.  We can use
     * printf() etc. once this is done.
     */
    cons_probe();

    /* switch to OSF pal code. */
    OSFpal();

    /*
     * Initialise the block cache
     */
    bcache_init(32, 512);	/* 16k XXX tune this */

    /*
     * March through the device switch probing for things.
     */
    for (i = 0; devsw[i] != NULL; i++)
	if (devsw[i]->dv_init != NULL)
	    (devsw[i]->dv_init)();

    printf("\n");
    printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
    printf("(%s, %s)\n", bootprog_maker, bootprog_date);
    printf("Memory: %ld k\n", memsize() / 1024);
    
    /* We're booting from an SRM disk, try to spiff this */
    currdev.d_dev = devsw[0];				/* XXX presumes that biosdisk is first in devsw */
    currdev.d_type = currdev.d_dev->dv_type;
    currdev.d_kind.srmdisk.unit = 0;
    currdev.d_kind.srmdisk.slice = -1;			/* XXX should be able to detect this, default to autoprobe */
    currdev.d_kind.srmdisk.partition = 0;		/* default to 'a' */

    /* Create alpha-specific variables */
    prom_getenv(PROM_E_BOOTED_FILE, bootfile, sizeof(bootfile));
    if (bootfile[0])
	setenv("bootfile", bootfile, 1);
    env_setenv("currdev", EV_VOLATILE, alpha_fmtdev(&currdev), alpha_setcurrdev, env_nounset);
    env_setenv("loaddev", EV_VOLATILE,  alpha_fmtdev(&currdev), env_noset, env_nounset);
    setenv("LINES", "24", 1);				/* optional */
    
    archsw.arch_autoload = alpha_autoload;
    archsw.arch_getdev = alpha_getdev;
    archsw.arch_copyin = alpha_copyin;
    archsw.arch_copyout = alpha_copyout;
    archsw.arch_readin = alpha_readin;

    /*
     * SRM firmware takes *ages* to open the disk device.  We hold it
     * open until the closeall() when we exec the kernel.  Note that
     * we must close it eventually since otherwise the firmware leaves
     * the ncr hardware in a broken state (at least it does on my EB164).
     */
    open("/boot", O_RDONLY);

    interact();			/* doesn't return */

    return 0;
}

COMMAND_SET(reboot, "reboot", "reboot the system", command_reboot);

static int
command_reboot(int argc, char *argv[])
{

    printf("Rebooting...\n");
    delay(1000000);
    reboot();
    /* Note: we shouldn't get to this point! */
    panic("Reboot failed!");
    exit(0);
}

COMMAND_SET(halt, "halt", "halt the system", command_halt);

static int
command_halt(int argc, char *argv[])
{
    halt();    /* never returns */
    return(CMD_OK);
}

#if 0

COMMAND_SET(stack, "stack", "show stack usage", command_stack);

static int
command_stack(int argc, char *argv[])
{
    char	*cp;

    for (cp = &stackbase; cp < &stacktop; cp++)
	if (*cp != 0)
	    break;
    
    printf("%d bytes of stack used\n", &stacktop - cp);
    return(CMD_OK);
}

#endif

COMMAND_SET(heap, "heap", "show heap usage", command_heap);

static int
command_heap(int argc, char *argv[])
{
    printf("heap base at %p, top at %p, used %ld\n", end, sbrk(0), sbrk(0) - end);
    return(CMD_OK);
}
