/*-
 * Copyright (c) 2013 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/linker.h>

#include <machine/elf.h>

#include <stand.h>
#include <bootstrap.h>
#include <loader.h>
#include <mips.h>

static int	__elfN(exec)(struct preloaded_file *);

struct devsw *devsw[] = {
	&beri_disk,
	NULL
};

struct arch_switch archsw;

struct file_format *file_formats[] = {
	&beri_elf,
	NULL
};

struct fs_ops *file_system[] = {
#ifdef LOADER_UFS_SUPPORT
	&ufs_fsops,
#endif
	NULL
};

struct console *consoles[] = {
	&altera_jtag_uart_console,
	NULL
};

extern void	*__heap_base;
extern void	*__heap_top;

static int
__elfN(exec)(struct preloaded_file *fp)
{

	return (EFTYPE);
}

/*
 * Capture arguments from boot2 for later reuse when launching the kernel.
 */
int		 boot2_argc;
char		**boot2_argv;
char		**boot2_envv;
struct bootinfo	*boot2_bootinfop;

int
main(int argc, char *argv[], char *envv[], struct bootinfo *bootinfop)
{
	struct devsw **dp;

	boot2_argc = argc;
	boot2_argv = argv;
	boot2_envv = envv;
	boot2_bootinfop = bootinfop;

	setheap((void *)&__heap_base, (void *)&__heap_top);

	/*
	 * Probe for a console.
	 */
	cons_probe();

	/*
	 * Initialise devices.
	 */
	for (dp = devsw; *dp != NULL; dp++) {
		if ((*dp)->dv_init != NULL)
			(*dp)->dv_init();
	}

#if 0
	env_setenv("currdev", EV_VOLATILE, ...);
	env_setenv("loaddev", EV_VOLATILE, ...);
#endif

	printf("\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);
#if 0
	printf("bootpath=\"%s\"\n", bootpath);
#endif

	interact();
	return (0);
}

void
abort(void)
{

	printf("error: loader abort\n");
	while (1);
}

void
exit(int code)
{

	printf("error: loader exit\n");
	while (1);
}

void
longjmperror(void)
{

	printf("error: loader longjmp error\n");
	while (1);
}

time_t
time(time_t *tloc)
{

	/* We can't provide time since UTC, so just provide time since boot. */
	return (cp0_count_get() / 100000000);
}

/*
 * Delay - presumably in usecs?
 */
void
delay(int usecs)
{
	register_t t;

	t = cp0_count_get() + usecs * 100;
	while (cp0_count_get() < t);
}
