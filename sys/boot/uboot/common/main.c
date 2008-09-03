/*-
 * Copyright (c) 2000 Benno Rice <benno@jeamland.net>
 * Copyright (c) 2000 Stephane Potvin <sepotvin@videotron.ca>
 * Copyright (c) 2007 Semihalf, Rafal Jaworowski <raj@semihalf.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>

#include "api_public.h"
#include "bootstrap.h"
#include "glue.h"
#include "libuboot.h"

struct uboot_devdesc	currdev;
struct arch_switch	archsw;		/* MI/MD interface boundary */
int			devs_no;

extern char end[];
extern char bootprog_name[];
extern char bootprog_rev[];
extern char bootprog_date[];
extern char bootprog_maker[];

extern unsigned char _etext[];
extern unsigned char _edata[];
extern unsigned char __bss_start[];
extern unsigned char __sbss_start[];
extern unsigned char __sbss_end[];
extern unsigned char _end[];

void dump_si(struct sys_info *si)
{
#ifdef DEBUG
	printf("sys info:\n");
	printf("  clkbus\t= 0x%08x\n", si->clk_bus);
	printf("  clkcpu\t= 0x%08x\n", si->clk_cpu);
	printf("  bar\t\t= 0x%08x\n", si->bar);
#endif
}

static void dump_sig(struct api_signature *sig)
{
#ifdef DEBUG
	printf("signature:\n");
	printf("  version\t= %d\n", sig->version);
	printf("  checksum\t= 0x%08x\n", sig->checksum);
	printf("  sc entry\t= 0x%08x\n", sig->syscall);
#endif
}
static void
dump_addr_info(void)
{
#ifdef DEBUG
	printf("\naddresses info:\n");
	printf(" _etext (sdata) = 0x%08x\n", (u_int32_t)_etext);
	printf(" _edata         = 0x%08x\n", (u_int32_t)_edata);
	printf(" __sbss_start   = 0x%08x\n", (u_int32_t)__sbss_start);
	printf(" __sbss_end     = 0x%08x\n", (u_int32_t)__sbss_end);
	printf(" __sbss_start   = 0x%08x\n", (u_int32_t)__bss_start);
	printf(" _end           = 0x%08x\n", (u_int32_t)_end);
	printf(" syscall entry  = 0x%08x\n", (u_int32_t)syscall_ptr);
#endif
}

static uint64_t
memsize(int flags)
{
	int		i;
	struct sys_info	*si;
	uint64_t	size;

	if ((si = ub_get_sys_info()) == NULL)
		return 0;

	size = 0;
	for (i = 0; i < si->mr_no; i++)
		if (si->mr[i].flags == flags && si->mr[i].size)
			size += (si->mr[i].size);

	return (size);
}

int
main(void)
{
	struct api_signature *sig = NULL;
	int i;

	if (!api_search_sig(&sig))
		return -1;

	syscall_ptr = sig->syscall;
	if (syscall_ptr == NULL)
		return -2;

	if (sig->version > API_SIG_VERSION)
		return -3;

        /* Clear BSS sections */
	bzero(__sbss_start, __sbss_end - __sbss_start);
	bzero(__bss_start, _end - __bss_start);

	/*
         * Set up console.
         */
	cons_probe();

	printf("Compatible API signature found @%x\n", (uint32_t)sig);

	dump_sig(sig);
	dump_addr_info();

	/*
	 * Initialise the heap as early as possible.  Once this is done,
	 * alloc() is usable. The stack is buried inside us, so this is
	 * safe.
	 */
	setheap((void *)end, (void *)(end + 512 * 1024));

	/*
	 * Enumerate U-Boot devices
	 */
	if ((devs_no = ub_dev_enum()) == 0)
		panic("no devices found");
	printf("Number of U-Boot devices found %d\n", devs_no);

	/* XXX all our dv_init()s currently don't do anything... */
	/*
	 * March through the device switch probing for things.
	 */
	for (i = 0; devsw[i] != NULL; i++)
		if (devsw[i]->dv_init != NULL)
			(devsw[i]->dv_init)();

	printf("\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);
	printf("Memory: %lldMB\n", memsize(MR_ATTR_DRAM) / 1024 / 1024);
	printf("FLASH:  %lldMB\n", memsize(MR_ATTR_FLASH) / 1024 / 1024);
//	printf("SRAM:   %lldMB\n", memsize(MR_ATTR_SRAM) / 1024 / 1024);

	/* XXX only support netbooting for now */
	for (i = 0; devsw[i] != NULL; i++)
		if (strncmp(devsw[i]->dv_name, "net",
		    strlen(devsw[i]->dv_name)) == 0)
			break;

	if (devsw[i] == NULL)
		panic("no network devices?!");

	currdev.d_dev = devsw[i];
	currdev.d_type = currdev.d_dev->dv_type;
	currdev.d_unit = 0;

	env_setenv("currdev", EV_VOLATILE, uboot_fmtdev(&currdev),
	    uboot_setcurrdev, env_nounset);
	env_setenv("loaddev", EV_VOLATILE, uboot_fmtdev(&currdev),
	    env_noset, env_nounset);

	setenv("LINES", "24", 1);		/* optional */
	setenv("prompt", "loader>", 1);

	archsw.arch_getdev = uboot_getdev;
	archsw.arch_copyin = uboot_copyin;
	archsw.arch_copyout = uboot_copyout;
	archsw.arch_readin = uboot_readin;
	archsw.arch_autoload = uboot_autoload;

	interact();				/* doesn't return */

	return 0;
}


COMMAND_SET(heap, "heap", "show heap usage", command_heap);
static int
command_heap(int argc, char *argv[])
{

	printf("heap base at %p, top at %p, used %d\n", end, sbrk(0),
	    sbrk(0) - end);

	return(CMD_OK);
}

COMMAND_SET(reboot, "reboot", "reboot the system", command_reboot);
static int
command_reboot(int argc, char *argv[])
{
	printf("Resetting...\n");
	ub_reset();

	printf("Reset failed!\n");
	while(1);
}
