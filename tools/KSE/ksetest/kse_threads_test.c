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
#include "simplelock.h"

#undef TRACE_UTS
//#define TRACE_KSE

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

struct uts_runq {
	struct kse_thr_mailbox	*head;
	struct simplelock	lock;
};

struct uts_data {
	struct kse_mailbox	mb;
	struct uts_runq		*runq;
	struct kse_thr_mailbox	*cur_thread;
};

static struct uts_runq runq1;
static struct uts_data data1, data2;
static struct uts_runq runq2;
static struct uts_data data3, data4;
static struct kse_thr_mailbox	*aa;

#ifdef TRACE_UTS
static int progress = 0;
#endif

static void	init_uts(struct uts_data *data, struct uts_runq *q);
static void	start_uts(struct uts_data *data, int newgrp);
static void	enter_uts(struct uts_data *);
static void	pchar(char c);
static void	pfmt(const char *fmt, ...);
static void	pstr(const char *s);
static void	runq_init(struct uts_runq *q);
static void	runq_insert(struct uts_runq *q, struct kse_thr_mailbox *tm);
static struct	kse_thr_mailbox *runq_remove(struct uts_runq *q);
static struct	kse_thr_mailbox *runq_remove_nolock(struct uts_runq *q);
static void	thread_start(struct uts_data *data, const void *func, int arg);
static void	uts(struct kse_mailbox *km);

/* Functions implemented in assembly */
extern int	uts_to_thread(struct kse_thr_mailbox *tdp,
			struct kse_thr_mailbox **curthreadp);
extern int	thread_to_uts(struct kse_thr_mailbox *tm,
			struct kse_mailbox *km);

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
//	thread_start(aaaa, '0' + progress++);
}

static void
newkse(int v)
{
	start_uts(&data4, 0);
}

#if 0
void
spin(int arg)
{
	for (;;) enter_uts(); sched_yield();
}
#endif
/*
 * Test Userland Thread Scheduler (UTS) suite for KSE.
 */
int
main(void)
{
	int i;

	runq_init(&runq1);
	init_uts(&data1, &runq1);
	init_uts(&data2, &runq1);
	thread_start(&data1, aaaa, '+');
	thread_start(&data1, aaaa, '-');
	start_uts(&data1, 0);
	start_uts(&data2, 0);

//	start second ksegrp
	runq_init(&runq2);
	init_uts(&data3, &runq2);
	init_uts(&data4, &runq2);
	thread_start(&data3, newkse, 0);
	thread_start(&data3, aaaa, '*');
	thread_start(&data3, aaaa, '.');
	start_uts(&data3, 1);

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
enter_uts(struct uts_data *data)
{
	struct kse_thr_mailbox	*td;

	/* XXX: We should atomically exchange these two. */
	td = data->mb.km_curthread;
	data->mb.km_curthread = NULL;

	thread_to_uts(td, &data->mb);
}

/*
 * Initialise threading.
 */
static void
init_uts(struct uts_data *data, struct uts_runq *q)
{
	struct kse_thr_mailbox *tm;
	int mib[2];
	char	*p;
#if 0
	size_t len;
#endif

	/*
	 * Create initial thread.
	 */
	tm = (struct kse_thr_mailbox *)calloc(1, sizeof(struct kse_thr_mailbox));

	/* Throw us into its context. */
	getcontext(&tm->tm_context);

	/* Find our stack. */
	mib[0] = CTL_KERN;
	mib[1] = KERN_USRSTACK;
#if 0
	len = sizeof(p);
	if (sysctl(mib, 2, &p, &len, NULL, 0) == -1)
		pstr("sysctl(CTL_KER.KERN_USRSTACK) failed.\n");
#endif
	p = (char *)malloc(MAIN_STACK_SIZE) + MAIN_STACK_SIZE;
	pfmt("main() : 0x%x\n", tm);
	pfmt("eip -> 0x%x\n", tm->tm_context.uc_mcontext.mc_eip);
	tm->tm_context.uc_stack.ss_sp = p - MAIN_STACK_SIZE;
	tm->tm_context.uc_stack.ss_size = MAIN_STACK_SIZE;

	/*
	 * Create KSE mailbox.
	 */
	p = (char *)malloc(THREAD_STACK_SIZE);
	bzero(&data->mb, sizeof(struct kse_mailbox));
	data->mb.km_stack.ss_sp = p;
	data->mb.km_stack.ss_size = THREAD_STACK_SIZE;
	data->mb.km_func = (void *)uts;
	data->mb.km_udata = data;
	data->cur_thread = tm;
	data->runq = q;
	pfmt("uts() at : 0x%x\n", uts);
	pfmt("uts stack at : 0x%x - 0x%x\n", p, p + THREAD_STACK_SIZE);
}

static void
start_uts(struct uts_data *data, int newgrp)
{
	/*
	 * Start KSE scheduling.
	 */
	pfmt("kse_create() -> %d\n", kse_create(&data->mb, newgrp));
	data->mb.km_curthread = data->cur_thread;

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

static void
runq_init(struct uts_runq *q)
{
	q->head = NULL;
	simplelock_init(&q->lock);
}

/*
 * Insert a thread into the run queue.
 */
static void
runq_insert(struct uts_runq *q, struct kse_thr_mailbox *tm)
{
	simplelock_lock(&q->lock);
	tm->tm_next = q->head;
	q->head = tm;
	simplelock_unlock(&q->lock);
}

/*
 * Select and remove a thread from the run queue.
 */
static struct kse_thr_mailbox *
runq_remove(struct uts_runq *q)
{
	struct kse_thr_mailbox *tm;

	simplelock_lock(&q->lock);
	tm = runq_remove_nolock(q);
	simplelock_unlock(&q->lock);
	return tm;
}

static struct kse_thr_mailbox *
runq_remove_nolock(struct uts_runq *q)
{
	struct kse_thr_mailbox *p, *p1;
	
	if (q->head == NULL)
		return (NULL);
	p1 = NULL;
	for (p = q->head; p->tm_next != NULL; p = p->tm_next)
		p1 = p;
	if (p1 == NULL)
		q->head = NULL;
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
#ifdef TRACE_KSE
	static struct uts_data *prev_data;
#endif
	struct kse_thr_mailbox *tm, *p;
	struct uts_data *data;
	int i;

	UPSTR("\n--uts() start--\n");
	UPFMT("mailbox -> %x\n", km);

	/*
	 * Insert any processes back from being blocked
	 * in the kernel into the run queue.
	 */
	data = km->km_udata;
	p = km->km_completed;
	km->km_completed = NULL;
	UPFMT("km_completed -> 0x%x", p);
#ifdef TRACE_KSE
	if (data != prev_data) {
		prev_data = data;
		pfmt("uts data: 0x%x\n", data);
	}
#endif
	while ((tm = p) != NULL) {
		p = tm->tm_next;
		UPFMT(" 0x%x", p);
		runq_insert(data->runq, tm);
	}
	UPCHAR('\n');

	simplelock_lock(&data->runq->lock);
	/*
	 * Process any signals we've received (but only if we have
	 * somewhere to deliver them to).
	 */
	if ((data->runq->head != NULL) && SIGNOTEMPTY(km->km_sigscaught)) {
		for (i = 0;i < _SIG_MAXSIG;i++)
			if (SIGISMEMBER(km->km_sigscaught, i)) {
				signalcontext(&data->runq->head->tm_context,
				 i, foof);
				break;
			}
		bzero(&km->km_sigscaught, sizeof(sigset_t));
	}

	/*
	 * Pull a thread off the run queue.
	 */
	p = runq_remove_nolock(data->runq);
	simplelock_unlock(&data->runq->lock);
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
	kse_release(NULL);
	pstr("** uts() exiting **\n");
	exit(EX_SOFTWARE);
}

/*
 * Start a thread.
 */
static struct kse_thr_mailbox *
thread_create(const void *func, int arg)
{
	struct kse_thr_mailbox *tm;
	char *p;

	aa = tm = (struct kse_thr_mailbox *)calloc(1, sizeof(struct kse_thr_mailbox));
	getcontext(&tm->tm_context);
	p = (char *)malloc(THREAD_STACK_SIZE);
	tm->tm_context.uc_stack.ss_sp = p;
	tm->tm_context.uc_stack.ss_size = THREAD_STACK_SIZE;
	makecontext(&tm->tm_context, func, 2, arg);
	// setcontext(&tm->tm_context);
	return tm;
}

static void
thread_start(struct uts_data *data, const void *func, int arg)
{
	struct kse_thr_mailbox *tm;
	struct kse_thr_mailbox *tm2;

	tm = thread_create(func, arg);
	tm2 = thread_create(enter_uts, (int)data);
	tm->tm_context.uc_link = &tm2->tm_context;
	runq_insert(data->runq, tm);
	pfmt("thread_start() : 0x%x %x\n", tm, &tm->tm_context);
}
