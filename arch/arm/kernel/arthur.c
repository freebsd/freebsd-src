/*
 *  linux/arch/arm/kernel/arthur.c
 *
 *  Copyright (C) 1998, 1999, 2000, 2001 Philip Blundell
 *
 * Arthur personality
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/personality.h>
#include <linux/stddef.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <asm/ptrace.h>

/* Arthur doesn't have many signals, and a lot of those that it does
   have don't map easily to any Linux equivalent.  Never mind.  */

#define ARTHUR_SIGABRT		1
#define ARTHUR_SIGFPE		2
#define ARTHUR_SIGILL		3
#define ARTHUR_SIGINT		4
#define ARTHUR_SIGSEGV		5
#define ARTHUR_SIGTERM		6
#define ARTHUR_SIGSTAK		7
#define ARTHUR_SIGUSR1		8
#define ARTHUR_SIGUSR2		9
#define ARTHUR_SIGOSERROR	10

static unsigned long arthur_to_linux_signals[32] = {
	0,	1,	2,	3,	4,	5,	6,	7,
	8,	9,	10,	11,	12,	13,	14,	15,
	16,	17,	18,	19,	20,	21,	22,	23,
	24,	25,	26,	27,	28,	29,	30,	31
};

/*
 * Linux to Arthur signal map.
 */
static unsigned long arthur_invmap[32] = {
	[0]		= 0,
	[SIGHUP]	= -1,
	[SIGINT]	= ARTHUR_SIGINT,
	[SIGQUIT]	= -1,
	[SIGILL]	= ARTHUR_SIGILL,
	[SIGTRAP]	= 5,
	[SIGABRT]	= ARTHUR_SIGABRT,
	[SIGBUS]	= 7,
	[SIGFPE]	= ARTHUR_SIGFPE,
	[SIGKILL]	= 9,
	[SIGUSR1]	= ARTHUR_SIGUSR1,
	[SIGSEGV]	= ARTHUR_SIGSEGV,	
	[SIGUSR2]	= ARTHUR_SIGUSR2,
	[SIGPIPE]	= 13,
	[SIGALRM]	= 14,
	[SIGTERM]	= ARTHUR_SIGTERM,
	[SIGSTKFLT]	= 16,
	[SIGCHLD]	= 17,
	[SIGCONT]	= 18,
	[SIGSTOP]	= 19,
	[SIGTSTP]	= 20,
	[SIGTTIN]	= 21,
	[SIGTTOU]	= 22,
	[SIGURG]	= 23,
	[SIGXCPU]	= 24,
	[SIGXFSZ]	= 25,
	[SIGVTALRM]	= 26,
	[SIGPROF]	= 27,
	[SIGWINCH]	= 28,
	[SIGIO]		= 29,
	[SIGPWR]	= 30,
	[SIGSYS]	= 31
};

static void arthur_lcall7(int nr, struct pt_regs *regs)
{
	struct siginfo info;

	info.si_signo = SIGSWI;
	info.si_errno = nr;
	/* Bounce it to the emulator */
	send_sig_info(SIGSWI, &info, current);
}

static struct exec_domain arthur_exec_domain = {
	.name		= "Arthur",	/* name */
	.handler	= arthur_lcall7,
	.pers_low	= PER_RISCOS,
	.pers_high	= PER_RISCOS,
	.signal_map	= arthur_to_linux_signals,
	.signal_invmap	= arthur_invmap,
	.module		= THIS_MODULE,
};

/*
 * We could do with some locking to stop Arthur being removed while
 * processes are using it.
 */

static int __init arthur_init(void)
{
	return register_exec_domain(&arthur_exec_domain);
}

static void __exit arthur_exit(void)
{
	unregister_exec_domain(&arthur_exec_domain);
}

module_init(arthur_init);
module_exit(arthur_exit);

MODULE_AUTHOR("Philip Blundell");
MODULE_LICENSE("GPL");
