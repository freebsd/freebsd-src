/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 *	from: @(#)isa.c	7.2 (Berkeley) 5/13/91
 *	form: src/sys/i386/isa/intr_machdep.c,v 1.57 2001/07/20
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/vmmeter.h>

#include <machine/frame.h>
#include <machine/intr_machdep.h>

#define	MAX_STRAY_LOG	5

CTASSERT((1 << IV_SHIFT) == sizeof(struct intr_vector));

ih_func_t *intr_handlers[PIL_MAX];
uint16_t pil_countp[PIL_MAX];

struct intr_vector intr_vectors[IV_MAX];
uint16_t intr_countp[IV_MAX];
static u_long intr_stray_count[IV_MAX];

static char *pil_names[] = {
	"stray",
	"low",		/* PIL_LOW */
	"ithrd",	/* PIL_ITHREAD */
	"rndzvs",	/* PIL_RENDEZVOUS */
	"ast",		/* PIL_AST */
	"stop",		/* PIL_STOP */
	"stray", "stray", "stray", "stray", "stray", "stray", "stray",
	"fast",		/* PIL_FAST */
	"tick",		/* PIL_TICK */
};
	
/* protect the intr_vectors table */
static struct mtx intr_table_lock;

static void intr_stray_level(struct trapframe *);
static void intr_stray_vector(void *);
static void update_intrname(int, const char *, int);

/*
 * not MPSAFE
 */
static void
update_intrname(int vec, const char *name, int ispil)
{
	char buf[32];
	char *cp;
	int off, name_index;

	if (intrnames[0] == '\0') {
		/* for bitbucket */
		if (bootverbose)
			printf("initalizing intr_countp\n");
		off = sprintf(intrnames, "???") + 1;

		off += sprintf(intrnames + off, "stray") + 1;
		for (name_index = 0; name_index < IV_MAX; name_index++)
			intr_countp[name_index] = 1;

		off += sprintf(intrnames + off, "pil") + 1;
		for (name_index = 0; name_index < PIL_MAX; name_index++)
			pil_countp[name_index] = 2;
	}

	if (name == NULL)
		name = "???";

	if (snprintf(buf, sizeof(buf), "%s %s%d", name, ispil ? "pil" : "vec",
	    vec) >= sizeof(buf))
		goto use_bitbucket;

	/*
	 * Search for `buf' in `intrnames'.  In the usual case when it is
	 * not found, append it to the end if there is enough space (the \0
	 * terminator for the previous string, if any, becomes a separator).
	 */
	for (cp = intrnames, name_index = 0; cp != eintrnames &&
	    name_index < IV_MAX; cp += strlen(cp) + 1, name_index++) {
		if (*cp == '\0') {
			if (strlen(buf) >= eintrnames - cp)
				break;
			strcpy(cp, buf);
			goto found;
		}
		if (strcmp(cp, buf) == 0)
			goto found;
	}

use_bitbucket:
	name_index = 0;
found:
	if (!ispil)
		intr_countp[vec] = name_index;
	else
		pil_countp[vec] = name_index;
}

void
intr_setup(int pri, ih_func_t *ihf, int vec, iv_func_t *ivf, void *iva)
{
	u_long ps;

	ps = intr_disable();
	if (vec != -1) {
		intr_vectors[vec].iv_func = ivf;
		intr_vectors[vec].iv_arg = iva;
		intr_vectors[vec].iv_pri = pri;
		intr_vectors[vec].iv_vec = vec;
	}
	update_intrname(pri, pil_names[pri], 1);
	intr_handlers[pri] = ihf;
	intr_restore(ps);
}

static void
intr_stray_level(struct trapframe *tf)
{

	printf("stray level interrupt %ld\n", tf->tf_level);
}

static void
intr_stray_vector(void *cookie)
{
	struct intr_vector *iv;

	iv = cookie;
	if (intr_stray_count[iv->iv_vec] < MAX_STRAY_LOG) {
		printf("stray vector interrupt %d\n", iv->iv_vec);
		atomic_add_long(&intr_stray_count[iv->iv_vec], 1);
		if (intr_stray_count[iv->iv_vec] >= MAX_STRAY_LOG)
			printf("got %d stray interrupt %d's: not logging "
			    "anymore\n", MAX_STRAY_LOG, iv->iv_vec);
	}
}

void
intr_init1()
{
	int i;

	/* Mark all interrupts as being stray. */
	for (i = 0; i < PIL_MAX; i++)
		intr_handlers[i] = intr_stray_level;
	for (i = 0; i < IV_MAX; i++) {
		intr_vectors[i].iv_func = intr_stray_vector;
		intr_vectors[i].iv_arg = &intr_vectors[i];
		intr_vectors[i].iv_pri = PIL_LOW;
		intr_vectors[i].iv_vec = i;
	}
	intr_handlers[PIL_LOW] = intr_fast;
}

void
intr_init2()
{

	mtx_init(&intr_table_lock, "ithread table lock", NULL, MTX_SPIN);
}

/* Schedule a heavyweight interrupt process. */
static void 
sched_ithd(void *cookie)
{
	struct intr_vector *iv;
	int error;

	iv = cookie;
	error = ithread_schedule(iv->iv_ithd);
	if (error == EINVAL)
		intr_stray_vector(iv);
}

int
inthand_add(const char *name, int vec, void (*handler)(void *), void *arg,
    int flags, void **cookiep)
{
	struct intr_vector *iv;
	struct ithd *ithd;		/* descriptor for the IRQ */
	struct ithd *orphan;
	int errcode = 0;

	/*
	 * Work around a race where more than one CPU may be registering
	 * handlers on the same IRQ at the same time.
	 */
	iv = &intr_vectors[vec];
	mtx_lock_spin(&intr_table_lock);
	ithd = iv->iv_ithd;
	mtx_unlock_spin(&intr_table_lock);
	if (ithd == NULL) {
		errcode = ithread_create(&ithd, vec, 0, NULL, NULL, "intr%d:",
		    vec);
		if (errcode)
			return (errcode);
		mtx_lock_spin(&intr_table_lock);
		if (iv->iv_ithd == NULL) {
			iv->iv_ithd = ithd;
			mtx_unlock_spin(&intr_table_lock);
		} else {
			orphan = ithd;
			ithd = iv->iv_ithd;
			mtx_unlock_spin(&intr_table_lock);
			ithread_destroy(orphan);
		}
	}

	errcode = ithread_add_handler(ithd, name, handler, arg,
	    ithread_priority(flags), flags, cookiep);
	
	if ((flags & INTR_FAST) == 0 || errcode) {
		intr_setup(PIL_ITHREAD, intr_fast, vec, sched_ithd, iv);
		errcode = 0;
	}

	if (errcode)
		return (errcode);
	
	if (flags & INTR_FAST)
		intr_setup(PIL_FAST, intr_fast, vec, handler, arg);

	intr_stray_count[vec] = 0;

	update_intrname(vec, name, 0);

	return (0);
}

int
inthand_remove(int vec, void *cookie)
{
	struct intr_vector *iv;
	int error;
	
	error = ithread_remove_handler(cookie);
	if (error == 0) {
		/*
		 * XXX: maybe this should be done regardless of whether
		 * ithread_remove_handler() succeeded?
		 */
		iv = &intr_vectors[vec];
		mtx_lock_spin(&intr_table_lock);
		if (iv->iv_ithd == NULL)
			intr_setup(PIL_ITHREAD, intr_fast, vec,
			    intr_stray_vector, iv);
		else
			intr_setup(PIL_LOW, intr_fast, vec, sched_ithd, iv);
		mtx_unlock_spin(&intr_table_lock);
	}
	return (error);
}
