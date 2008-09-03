/*-
 * Copyright (c) 2000 Benno Rice <benno@jeamland.net>
 * Copyright (c) 2000 Stephane Potvin <sepotvin@videotron.ca>
 * Copyright (c) 2007-2008 Semihalf, Rafal Jaworowski <raj@semihalf.com>
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

struct uboot_devdesc currdev;
struct arch_switch archsw;		/* MI/MD interface boundary */
int devs_no;

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

static void
dump_sig(struct api_signature *sig)
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
	printf(" _etext (sdata) = 0x%08x\n", (uint32_t)_etext);
	printf(" _edata         = 0x%08x\n", (uint32_t)_edata);
	printf(" __sbss_start   = 0x%08x\n", (uint32_t)__sbss_start);
	printf(" __sbss_end     = 0x%08x\n", (uint32_t)__sbss_end);
	printf(" __sbss_start   = 0x%08x\n", (uint32_t)__bss_start);
	printf(" _end           = 0x%08x\n", (uint32_t)_end);
	printf(" syscall entry  = 0x%08x\n", (uint32_t)syscall_ptr);
#endif
}

static uint64_t
memsize(struct sys_info *si, int flags)
{
	uint64_t size;
	int i;

	size = 0;
	for (i = 0; i < si->mr_no; i++)
		if (si->mr[i].flags == flags && si->mr[i].size)
			size += (si->mr[i].size);

	return (size);
}

static void
meminfo(void)
{
	uint64_t size;
	struct sys_info *si;
	int t[3] = { MR_ATTR_DRAM, MR_ATTR_FLASH, MR_ATTR_SRAM };
	int i;

	if ((si = ub_get_sys_info()) == NULL)
		panic("could not retrieve system info");

	for (i = 0; i < 3; i++) {
		size = memsize(si, t[i]);
		if (size > 0)
			printf("%s:\t %lldMB\n", ub_mem_type(t[i]),
			    size / 1024 / 1024);
	}
}

int
main(void)
{
	struct api_signature *sig = NULL;
	int i;

	if (!api_search_sig(&sig))
		return (-1);

	syscall_ptr = sig->syscall;
	if (syscall_ptr == NULL)
		return (-2);

	if (sig->version > API_SIG_VERSION)
		return (-3);

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
		panic("no U-Boot devices found");
	printf("Number of U-Boot devices: %d\n", devs_no);

	/*
	 * March through the device switch probing for things.
	 */
	for (i = 0; devsw[i] != NULL; i++)
		if (devsw[i]->dv_init != NULL)
			(devsw[i]->dv_init)();

	printf("\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);
	meminfo();

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

	return (0);
}


COMMAND_SET(heap, "heap", "show heap usage", command_heap);
static int
command_heap(int argc, char *argv[])
{

	printf("heap base at %p, top at %p, used %d\n", end, sbrk(0),
	    sbrk(0) - end);

	return (CMD_OK);
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

COMMAND_SET(devinfo, "devinfo", "show U-Boot devices", command_devinfo);
static int
command_devinfo(int argc, char *argv[])
{
	int i;

	if ((devs_no = ub_dev_enum()) == 0) {
		command_errmsg = "no U-Boot devices found!?";
		return (CMD_ERROR);
	}
	
	printf("U-Boot devices:\n");
	for (i = 0; i < devs_no; i++) {
		ub_dump_di(i);
		printf("\n");
	}
	return (CMD_OK);
}

COMMAND_SET(sysinfo, "sysinfo", "show U-Boot system info", command_sysinfo);
static int
command_sysinfo(int argc, char *argv[])
{
	struct sys_info *si;

	if ((si = ub_get_sys_info()) == NULL) {
		command_errmsg = "could not retrieve U-Boot sys info!?";
		return (CMD_ERROR);
	}

	printf("U-Boot system info:\n");
	ub_dump_si(si);
	return (CMD_OK);
}
