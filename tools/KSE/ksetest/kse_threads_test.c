/*-
 * Copyright (c) 2002 Jonathan Mini (mini@freebsd.org).
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

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/kse.h>
#include <sys/ucontext.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#undef TRACE_UTS

#ifdef TRACE_UTS
#define	UPFMT(fmt...)	pfmt(#fmt)
#define	UPSTR(s)	pstr(s)
#define	UPCHAR(c)	pchar(c)
#else
#define	UPFMT(fmt...)	/* Nothing. */
#define	UPSTR(s)	/* Nothing. */
#define	UPCHAR(c)	/* Nothing. */
#endif

#define MAIN_STACK_SIZE			(1024 * 1024)
#define THREAD_STACK_SIZE		(32 * 1024)

static struct kse_mailbox	uts_mb;
static struct thread_mailbox	*run_queue;
static struct thread_mailbox	*aa;

static int progress = 0;

static void	init_uts(void);
static void	enter_uts(void);
static void	pchar(char c);
static void	pfmt(const char *fmt, ...);
static void	pstr(const char *s);
static void	runq_insert(struct thread_mailbox *tm);
static struct thread_mailbox *runq_remove(void);
static void	thread_start(const void *func, int arg);
static void	uts(struct kse_mailbox *km);

extern int uts_to_thread(struct thread_mailbox *tdp, struct thread_mailbox **curthreadp);

static void
nano(int len)
{
	struct timespec time_to_sleep;
	struct timespec time_remaining;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = len * 10000;
	nanosleep(&time_to_sleep, &time_remaining);
}

void
aaaa(int c)
{
	for (;;) {
		pchar(c);
		nano(1);
	}
}

static void
foof(int sig)
{
	pfmt("\n[%d]\n", sig);
	thread_start(aaaa, '0' + progress++);
}

void
spin(int arg)
{
	for (;;) enter_uts(); sched_yield();
}

/*
 * Test Userland Thread Scheduler (UTS) suite for KSE.
 */
int
main(void)
{
	int i;

	thread_start(spin, '.');
	// thread_start(spin);
	init_uts();
	for (i = 0;1;i++) {
//		if (i < 1000)
//			thread_start(aaaa, 'a' + (i % 26));
		pchar('A' + (i % 26));
		nano(5);
	}
	pstr("\n** main() exiting **\n");
	return (EX_OK);
}


/*
 * Enter the UTS from a thread.
 */
static void
enter_uts(void)
{
	struct thread_mailbox	*td;

	/* XXX: We should atomically exchange these two. */
	td = uts_mb.km_curthread;
	uts_mb.km_curthread = NULL;

	thread_to_uts(td, &uts_mb);
}

/*
 * Initialise threading.
 */
static void
init_uts(void)
{
	struct thread_mailbox *tm;
	int mib[2];
	char	*p;
	size_t len;

	/*
	 * Create initial thread.
	 */
	tm = (struct thread_mailbox *)calloc(1, sizeof(struct thread_mailbox));

	/* Throw us into its context. */
	getcontext(&tm->tm_context);

	/* Find our stack. */
	mib[0] = CTL_KERN;
	mib[1] = KERN_USRSTACK;
	len = sizeof(p);
	if (sysctl(mib, 2, &p, &len, NULL, 0) == -1)
		pstr("sysctl(CTL_KER.KERN_USRSTACK) failed.\n");
	pfmt("main() : 0x%x\n", tm);
	pfmt("eip -> 0x%x\n", tm->tm_context.uc_mcontext.mc_eip);
	tm->tm_context.uc_stack.ss_sp = p - MAIN_STACK_SIZE;
	tm->tm_context.uc_stack.ss_size = MAIN_STACK_SIZE;

	/*
	 * Create KSE mailbox.
	 */
	p = (char *)malloc(THREAD_STACK_SIZE);
	bzero(&uts_mb, sizeof(struct kse_mailbox));
	uts_mb.km_stack.ss_sp = p;
	uts_mb.km_stack.ss_size = THREAD_STACK_SIZE;
	uts_mb.km_func = (void *)uts;
	pfmt("uts() at : 0x%x\n", uts);
	pfmt("uts stack at : 0x%x - 0x%x\n", p, p + THREAD_STACK_SIZE);

	/*
	 * Start KSE scheduling.
	 */
	pfmt("kse_new() -> %d\n", kse_new(&uts_mb, 0));
	uts_mb.km_curthread = tm;

	/*
	 * Arrange to deliver signals via KSE.
	 */
	signal(SIGURG, foof);
}

/*
 * Write a single character to stdout, in a thread-safe manner.
 */
static void
pchar(char c)
{

	write(STDOUT_FILENO, &c, 1);
}

/*
 * Write formatted output to stdout, in a thread-safe manner.
 *
 * Recognises the following conversions:
 *	%c	-> char
 *	%d	-> signed int (base 10)
 *	%s	-> string
 *	%u	-> unsigned int (base 10)
 *	%x	-> unsigned int (base 16)
 */
static void
pfmt(const char *fmt, ...)
{
	static const char digits[16] = "0123456789abcdef";
	va_list	 ap;
	char buf[10];
	char *s;
	unsigned r, u;
	int c, d;

	va_start(ap, fmt);
	while ((c = *fmt++)) {
		if (c == '%') {
			c = *fmt++;
			switch (c) {
			case 'c':
				pchar(va_arg(ap, int));
				continue;
			case 's':
				pstr(va_arg(ap, char *));
				continue;
			case 'd':
			case 'u':
			case 'x':
				r = ((c == 'u') || (c == 'd')) ? 10 : 16;
				if (c == 'd') {
					d = va_arg(ap, unsigned);
					if (d < 0) {
						pchar('-');
						u = (unsigned)(d * -1);
					} else
						u = (unsigned)d;
				} else
					u = va_arg(ap, unsigned);
				s = buf;
				do {
					*s++ = digits[u % r];
				} while (u /= r);
				while (--s >= buf)
					pchar(*s);
				continue;
			}
		}
		pchar(c);
	}
	va_end(ap);
}

static void
pstr(const char *s)
{

	write(STDOUT_FILENO, s, strlen(s));
}

/*
 * Insert a thread into the run queue.
 */
static void
runq_insert(struct thread_mailbox *tm)
{

	tm->tm_next = run_queue;
	run_queue = tm;
}

/*
 * Select and remove a thread from the run queue.
 */
static struct thread_mailbox *
runq_remove(void)
{
	struct thread_mailbox *p, *p1;

	if (run_queue == NULL)
		return (NULL);
	p1 = NULL;
	for (p = run_queue; p->tm_next != NULL; p = p->tm_next)
		p1 = p;
	if (p1 == NULL)
		run_queue = NULL;
	else
		p1->tm_next = NULL;
	return (p);
}

/*
 * Userland thread scheduler.
 */
static void
uts(struct kse_mailbox *km)
{
	struct thread_mailbox *tm, *p;
	int ret, i;

	UPSTR("\n--uts() start--\n");
	UPFMT("mailbox -> %x\n", km);

	/*
	 * Insert any processes back from being blocked
	 * in the kernel into the run queue.
	 */
	p = km->km_completed;
	uts_mb.km_completed = NULL;
	UPFMT("km_completed -> 0x%x", p);
	while ((tm = p) != NULL) {
		p = tm->tm_next;
		UPFMT(" 0x%x", p);
		runq_insert(tm);
	}
	UPCHAR('\n');

	/*
	 * Process any signals we've recieved (but only if we have
	 * somewhere to deliver them to).
	 */
	if ((run_queue != NULL) && SIGNOTEMPTY(km->km_sigscaught)) {
		for (i = 0;i < _SIG_MAXSIG;i++)
			if (SIGISMEMBER(km->km_sigscaught, i)) {
				signalcontext(&run_queue->tm_context, i, foof);
				break;
			}
		bzero(&km->km_sigscaught, sizeof(sigset_t));
	}

	/*
	 * Pull a thread off the run queue.
	 */
	p = runq_remove();
#if 0
	if ((p == aa) && (progress > 0)) {
		--progress;
		signalcontext(&p->tm_context, 1, foof);
	}
#endif

	/*
	 * Either schedule a thread, or idle if none ready to run.
	 */
	if (p != NULL) {
		UPFMT("\n-- uts() scheduling 0x%x--\n", p);
		UPFMT("eip -> 0x%x progress -> %d\n",
		    p->tm_context.uc_mcontext.mc_eip, progress);
		UPSTR("curthread set\n");
		uts_to_thread(p, &km->km_curthread);
		UPSTR("\n-- uts_to_thread() failed --\n");
	}
	kse_yield();
	pstr("** uts() exiting **\n");
	exit(EX_SOFTWARE);
}

/*
 * Start a thread.
 */
static void
thread_start(const void *func, int arg)
{
	struct thread_mailbox *tm;
	char *p;

	aa = tm = (struct thread_mailbox *)calloc(1, sizeof(struct thread_mailbox));
	pfmt("thread_start() : 0x%x %x\n", tm, &aa->tm_context);
	getcontext(&tm->tm_context);
	p = (char *)malloc(THREAD_STACK_SIZE);
	tm->tm_context.uc_stack.ss_sp = p;
	tm->tm_context.uc_stack.ss_size = THREAD_STACK_SIZE;
	makecontext(&tm->tm_context, func, 2, arg);
	// setcontext(&tm->tm_context);
	runq_insert(tm);
}
