/*
 * Copyright (c) 2023 Warner Losh
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Test program for smbios support in the boot loader. This program will mmap
 * physical memory, and print the smbios table at the passed in PA. This is
 * intended to test the code and debug problems in a debugger friendly
 * environment.
 */

#include <sys/param.h>
#define setenv my_setenv

#define SMBIOS_SERIAL_NUMBERS 1
#define SMBIOS_LITTLE_ENDIAN_UUID 1

#include <arpa/inet.h>

#include "smbios.h"
#include "smbios.c"

#include <sys/mman.h>

#define MAX_MAP	10
#define PAGE	(64<<10)

static struct mapping 
{
	uintptr_t pa;
	caddr_t va;
} map[MAX_MAP];
static int fd;
static int nmap;

caddr_t ptov(uintptr_t pa)
{
	caddr_t va;
	uintptr_t pa2;
	struct mapping *m = map;

	pa2 = rounddown(pa, PAGE);
	for (int i = 0; i < nmap; i++, m++) {
		if (m->pa == pa2) {
			return (m->va + pa - m->pa);
		}
	}
	if (nmap == MAX_MAP)
		errx(1, "Too many maps");
	va = mmap(0, PAGE, PROT_READ, MAP_SHARED, fd, pa2);
	if (va == MAP_FAILED)
		err(1, "mmap offset %#lx", (long)pa2);
	m = &map[nmap++];
	m->pa = pa2;
	m->va = va;
	return (m->va + pa - m->pa);
}

static void
cleanup(void)
{
	for (int i = 0; i < nmap; i++) {
		munmap(map[i].va, PAGE);
	}
}

int
my_setenv(const char *name, const char *value, int overwrite __unused)
{
	printf("%s=%s\n", name, value);
	return 0;
}

static void
usage(void)
{
	errx(1, "smbios address");
}

int
main(int argc, char **argv)
{
	uintptr_t addr;
	
	if (argc != 2)
		usage();
	addr = strtoull(argv[1], NULL, 0);
	/* For mmap later */
	fd = open("/dev/mem", O_RDONLY);
	if (fd < 0)
		err(1, "Opening /dev/mem");
	smbios_detect(ptov(addr));
	cleanup();
}
