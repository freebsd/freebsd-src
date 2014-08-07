/*-
 * Copyright (C) 2010 Nathan Whitehorn
 * Copyright (C) 2011 glevand (geoffrey.levand@mail.ru)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <sys/param.h>

#define _KERNEL
#include <machine/cpufunc.h>

#include "bootstrap.h"
#include "lv1call.h"
#include "ps3.h"
#include "ps3devdesc.h"

struct arch_switch	archsw;
extern void *_end;

extern char bootprog_name[];
extern char bootprog_rev[];
extern char bootprog_date[];
extern char bootprog_maker[];

int ps3_getdev(void **vdev, const char *devspec, const char **path);
ssize_t ps3_copyin(const void *src, vm_offset_t dest, const size_t len);
ssize_t ps3_copyout(vm_offset_t src, void *dest, const size_t len);
ssize_t ps3_readin(const int fd, vm_offset_t dest, const size_t len);
int ps3_autoload(void);
int ps3_setcurrdev(struct env_var *ev, int flags, const void *value);

static uint64_t basetb;

int
main(void)
{
	uint64_t maxmem = 0;
	void *heapbase;
	int i, err;
	struct ps3_devdesc currdev;
	struct open_file f;

	lv1_get_physmem(&maxmem);
	
	ps3mmu_init(maxmem);

	/*
	 * Set up console.
	 */
	cons_probe();

	/*
	 * Set the heap to one page after the end of the loader.
	 */
	heapbase = (void *)(maxmem - 0x80000);
	setheap(heapbase, maxmem);

	/*
	 * March through the device switch probing for things.
	 */
	for (i = 0; devsw[i] != NULL; i++) {
		if (devsw[i]->dv_init != NULL) {
			err = (devsw[i]->dv_init)();
			if (err) {
				printf("\n%s: initialization failed err=%d\n",
					devsw[i]->dv_name, err);
				continue;
			}
		}

		currdev.d_dev = devsw[i];
		currdev.d_type = currdev.d_dev->dv_type;

		if (strcmp(devsw[i]->dv_name, "cd") == 0) {
			f.f_devdata = &currdev;
			currdev.d_unit = 0;

			if (devsw[i]->dv_open(&f, &currdev) == 0)
				break;
		}

		if (strcmp(devsw[i]->dv_name, "disk") == 0) {
			f.f_devdata = &currdev;
			currdev.d_unit = 3;
			currdev.d_disk.pnum = 1;
			currdev.d_disk.ptype = PTYPE_GPT;

			if (devsw[i]->dv_open(&f, &currdev) == 0)
				break;
		}

		if (strcmp(devsw[i]->dv_name, "net") == 0)
			break;
	}

	if (devsw[i] == NULL)
		panic("No boot device found!");
	else
		printf("Boot device: %s\n", devsw[i]->dv_name);

	/*
	 * Get timebase at boot.
	 */
	basetb = mftb();

	archsw.arch_getdev = ps3_getdev;
	archsw.arch_copyin = ps3_copyin;
	archsw.arch_copyout = ps3_copyout;
	archsw.arch_readin = ps3_readin;
	archsw.arch_autoload = ps3_autoload;

	printf("\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);
	printf("Memory: %lldKB\n", maxmem / 1024);

	env_setenv("currdev", EV_VOLATILE, ps3_fmtdev(&currdev),
	    ps3_setcurrdev, env_nounset);
	env_setenv("loaddev", EV_VOLATILE, ps3_fmtdev(&currdev), env_noset,
	    env_nounset);
	setenv("LINES", "24", 1);
	setenv("hw.platform", "ps3", 1);

	interact(NULL);			/* doesn't return */

	return (0);
}

void
ppc_exception(int code, vm_offset_t where, register_t msr)
{
	mtmsr(PSL_IR | PSL_DR | PSL_RI);
	printf("Exception %x at %#lx!\n", code, where);
	printf("Rebooting in 5 seconds...\n");
	delay(10000000);
	lv1_panic(1);
}

const u_int ns_per_tick = 12;

void
exit(int code)
{
	lv1_panic(code);
}

void
delay(int usecs)
{
	uint64_t tb,ttb;
	tb = mftb();

	ttb = tb + (usecs * 1000 + ns_per_tick - 1) / ns_per_tick;
	while (tb < ttb)
		tb = mftb();
}

int
getsecs()
{
	return ((mftb() - basetb)*ns_per_tick/1000000000);
}

time_t
time(time_t *tloc)
{
	time_t rv;
	
	rv = getsecs();
	if (tloc != NULL)
		*tloc = rv;

	return (rv);
}

ssize_t
ps3_copyin(const void *src, vm_offset_t dest, const size_t len)
{
	bcopy(src, (void *)dest, len);
	return (len);
}

ssize_t
ps3_copyout(vm_offset_t src, void *dest, const size_t len)
{
	bcopy((void *)src, dest, len);
	return (len);
}

ssize_t
ps3_readin(const int fd, vm_offset_t dest, const size_t len)
{
	void            *buf;
	size_t          resid, chunk, get;
	ssize_t         got;
	vm_offset_t     p;

	p = dest;

	chunk = min(PAGE_SIZE, len);
	buf = malloc(chunk);
	if (buf == NULL) {
		printf("ps3_readin: buf malloc failed\n");
		return(0);
	}

	for (resid = len; resid > 0; resid -= got, p += got) {
		get = min(chunk, resid);
		got = read(fd, buf, get);
		if (got <= 0) {
			if (got < 0)
				printf("ps3_readin: read failed\n");
			break;
		}

		bcopy(buf, (void *)p, got);
	}

	free(buf);
	return (len - resid);
}

int
ps3_autoload(void)
{

	return (0);
}

