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
 *	$Id: main.c,v 1.1.1.1 1998/08/21 03:17:42 msmith Exp $
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

void
main(void)
{
    int		i;
    char	bootfile[128];
    
    /* 
     * Initialise the heap as early as possible.  Once this is done, alloc() is usable.
     * The stack is buried inside us, so this is safe 
     */
    setheap((void *)end, (void *)0x20040000);


    /* 
     * XXX Chicken-and-egg problem; we want to have console output early, but some
     * console attributes may depend on reading from eg. the boot device, which we
     * can't do yet.
     *
     * We can use printf() etc. once this is done.
     */
    cons_probe();

    /* switch to OSF pal code. */
    OSFpal();

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
    archsw.arch_boot = alpha_boot;
    archsw.arch_getdev = alpha_getdev;

    /*
     * SRM firmware takes *ages* to open the disk device.  We hold it
     * open until the closeall() when we exec the kernel.  Note that
     * we must close it eventually since otherwise the firmware leaves
     * the ncr hardware in a broken state (at least it does on my EB164).
     */
    open("/", O_RDONLY);

    /*
     * XXX should these be in the MI source?
     */
    source("/boot/boot.config");
    printf("\n");
    autoboot(10, NULL);		/* try to boot automatically */
    printf("\nType '?' for a list of commands, 'help' for more detailed help.\n");
    /* setenv("prompt", "$currdev>", 1); */

    interact();			/* doesn't return */
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
