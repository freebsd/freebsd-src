/*-
 * Copyright (c) 2000 Benno Rice <benno@jeamland.net>
 * Copyright (c) 2000 Stephane Potvin <sepotvin@videotron.ca>
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
 *
 * $FreeBSD$
 */

#include <stand.h>
#include "openfirm.h"
#include "libofw.h"
#include "bootstrap.h"

struct arch_switch	archsw;		/* MI/MD interface boundary */

extern char end[];
extern char bootprog_name[];
extern char bootprog_rev[];
extern char bootprog_date[];
extern char bootprog_maker[];

struct ofw_reg
{
	uint32_t	base;
        uint32_t	size;
};

void
init_heap(void)
{
	phandle_t	chosen, memory;
	ihandle_t	meminstance;
        struct ofw_reg	available;
        void *		aligned_end;

	chosen = OF_finddevice("/chosen");
	OF_getprop(chosen, "memory", &meminstance, sizeof(meminstance));
	memory = OF_instance_to_package(meminstance);
	OF_getprop(memory, "available", &available, sizeof(available));
        printf("available.base = 0x%08x\n", available.base);
        printf("available.size = 0x%08x\n", available.size);

        if (OF_claim((void *)available.base, 0x00040000, 0) ==
	    (void *) 0xffffffff) {
        	printf("Heap memory claim failed!\n");
        	OF_enter();
        }

        aligned_end = (void *)(((int)end + sizeof(int) - 1) &
	    ~(sizeof(int) - 1));
        printf("end = 0x%08x, aligned_end = 0x%08x\n", (uint32_t)end,
	    (uint32_t)aligned_end);
        setheap((void *)aligned_end, (void *)(available.base + available.size));
}

uint32_t
memsize(void)
{
	phandle_t	chosen, memory;
	ihandle_t	meminstance;
        struct ofw_reg	reg;

	chosen = OF_finddevice("/chosen");
	OF_getprop(chosen, "memory", &meminstance, sizeof(meminstance));
	memory = OF_instance_to_package(meminstance);

        OF_getprop(memory, "reg", &reg, sizeof(reg));

        return(reg.size);
}

int
main(int (*openfirm)(void *))
{
#if 0
        void *	test;
#endif
	int	i;

	/*
	 * Initalise the OpenFirmware routines by giving them the entry point.
	 */
	OF_init(openfirm);

	/*
         * Set up console.
         */
	cons_probe();

	printf(">>> hello?\n");

	/*
         * Initialise the heap as early as possible.  Once this is done,
         * alloc() is usable. The stack is buried inside us, so this is
         * safe.
         */
 	init_heap();

        /*
         * Initialise the block cache
         */
        bcache_init(32, 512);		/* 16k XXX tune this */

        /*
         * March through the device switch probing for things.
         */
        for(i = 0; devsw[i] != NULL; i++)
        	if(devsw[i]->dv_init != NULL)
                	(devsw[i]->dv_init)();

        printf("\n");
        printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
        printf("(%s, %s)\n", bootprog_maker, bootprog_date);
        printf("Memory: %dKB\n", memsize() / 1024);

	archsw.arch_getdev = ofw_getdev;

        interact();			/* doesn't return */

	OF_exit();

	return 0;
}

COMMAND_SET(halt, "halt", "halt the system", command_halt);

static int
command_halt(int argc, char *argv[])
{
	OF_exit();
	return(CMD_OK);
}
