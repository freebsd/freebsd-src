/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
 *	$Id: main.c,v 1.6 1998/09/18 02:03:30 msmith Exp $
 */

/*
 * MD bootstrap main() and assorted miscellaneous
 * commands.
 */

#include <stand.h>
#include <string.h>
#include <machine/bootinfo.h>
#include <sys/reboot.h>

#include "bootstrap.h"
#include "libi386/libi386.h"
#include "btxv86.h"

/* Arguments passed in from the boot1/boot2 loader */
static struct 
{
    u_int32_t	howto;
    u_int32_t	bootdev;
    u_int32_t	res0;
    u_int32_t	res1;
    u_int32_t	res2;
    u_int32_t	bootinfo;
} *kargs;

struct bootinfo	*initial_bootinfo;

struct arch_switch	archsw;		/* MI/MD interface boundary */

/* from vers.c */
extern	char bootprog_name[], bootprog_rev[], bootprog_date[], bootprog_maker[];

/* XXX debugging */
extern char end[];

void
main(void)
{
    struct i386_devdesc	currdev;
    int			i;

    /* Pick up arguments */
    kargs = (void *)__args;
    initial_bootinfo = (struct bootinfo *)PTOV(kargs->bootinfo);

    /* 
     * Initialise the heap as early as possible.  Once this is done, malloc() is usable.
     *
     * XXX better to locate end of memory and use that
     */
    setheap((void *)end, (void *)(end + (384 * 1024)));
    
    /* 
     * XXX Chicken-and-egg problem; we want to have console output early, but some
     * console attributes may depend on reading from eg. the boot device, which we
     * can't do yet.
     *
     * We can use printf() etc. once this is done.
     * If the previous boot stage has requested a serial console, prefer that.
     */
    if (kargs->howto & RB_SERIAL)
	setenv("console", "com", 1);
    cons_probe();

    /*
     * March through the device switch probing for things.
     */
    for (i = 0; devsw[i] != NULL; i++)
	if (devsw[i]->dv_init != NULL)
	    (devsw[i]->dv_init)();

    printf("\n");
    printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
    printf("(%s, %s)\n", bootprog_maker, bootprog_date);
    printf("memory: %d/%dkB\n", getbasemem(), getextmem());
#if 0
    printf("diskbuf at %p, %d sectors\n", &diskbuf, diskbuf_size);
    printf("using %d bytes of stack at %p\n",  (&stacktop - &stackbase), &stacktop);
#endif

    /* We're booting from a BIOS disk, try to spiff this */
    currdev.d_dev = devsw[0];				/* XXX presumes that biosdisk is first in devsw */
    currdev.d_type = currdev.d_dev->dv_type;
    currdev.d_kind.biosdisk.unit = 0;			/* XXX wrong, need to get from bootinfo etc. */
    currdev.d_kind.biosdisk.slice = -1;			/* XXX should be able to detect this, default to autoprobe */
    currdev.d_kind.biosdisk.partition = 0;		/* default to 'a' */

    /* Create i386-specific variables */
    
    env_setenv("currdev", EV_VOLATILE, i386_fmtdev(&currdev), i386_setcurrdev, env_nounset);
    env_setenv("loaddev", EV_VOLATILE,  i386_fmtdev(&currdev), env_noset, env_nounset);
    setenv("LINES", "24", 1);				/* optional */
    
    archsw.arch_autoload = i386_autoload;
    archsw.arch_getdev = i386_getdev;
    archsw.arch_copyin = i386_copyin;
    archsw.arch_copyout = i386_copyout;
    archsw.arch_readin = i386_readin;
    /*
     * XXX should these be in the MI source?
     */
#if 0
    legacy_config();		/* read old /boot.config file */
#endif
    interact();			/* doesn't return */
}

COMMAND_SET(reboot, "reboot", "reboot the system", command_reboot);

static int
command_reboot(int argc, char *argv[])
{

    printf("Rebooting...\n");
    delay(1000000);
    __exit(0);
}

/* provide this for panic, as it's not in the startup code */
void
exit(int code)
{
    __exit(code);
}

#if 0 /* XXX learn to ask BTX */

COMMAND_SET(stack, "stack", "show stack usage", command_stack);

extern char stackbase, stacktop;

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
    mallocstats();
    printf("heap base at %p, top at %p", end, sbrk(0));
    return(CMD_OK);
}
