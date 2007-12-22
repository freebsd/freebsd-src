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
#include <sys/errno.h>
#include <sys/interrupt.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
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

static const char *pil_names[] = {
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

static void intr_enable_eoi(void *);
static void intr_execute_handlers(void *);
static void intr_stray_level(struct trapframe *);
static void intr_stray_vector(void *);
static int intrcnt_setname(const char *, int);
static void intrcnt_updatename(int, const char *, int);

/*
 * not MPSAFE
 */
static void
intrcnt_updatename(int vec, const char *name, int ispil)
{
	static int intrcnt_index, stray_pil_index, stray_vec_index;
	int name_index;

	if (intrnames[0] == '\0') {
		/* for bitbucket */
		if (bootverbose)
			printf("initalizing intr_countp\n");
		intrcnt_setname("???", intrcnt_index++);

		stray_vec_index = intrcnt_index++;
		intrcnt_setname("stray", stray_vec_index);
		for (name_index = 0; name_index < IV_MAX; name_index++)
			intr_countp[name_index] = stray_vec_index;

		stray_pil_index = intrcnt_index++;
		intrcnt_setname("pil", stray_pil_index);
		for (name_index = 0; name_index < PIL_MAX; name_index++)
			pil_countp[name_index] = stray_pil_index;
	}

	if (name == NULL)
		name = "???";

	if (!ispil && intr_countp[vec] != stray_vec_index)
		name_index = intr_countp[vec];
	else if (ispil && pil_countp[vec] != stray_pil_index)
		name_index = pil_countp[vec];
	else
		name_index = intrcnt_index++;

	if (intrcnt_setname(name, name_index))
		name_index = 0;

	if (!ispil)
		intr_countp[vec] = name_index;
	else
		pil_countp[vec] = name_index;
}

static int
intrcnt_setname(const char *name, int index)
{

	if (intrnames + (MAXCOMLEN + 1) * index >= eintrnames)
		return (E2BIG);
	snprintf(intrnames + (MAXCOMLEN + 1) * index, MAXCOMLEN + 1, "%-*s",
	    MAXCOMLEN, name);
	return (0);
}

void
intr_setup(int pri, ih_func_t *ihf, int vec, iv_func_t *ivf, void *iva)
{
	char pilname[MAXCOMLEN + 1];
	u_long ps;

	ps = intr_disable();
	if (vec != -1) {
		intr_vectors[vec].iv_func = ivf;
		intr_vectors[vec].iv_arg = iva;
		intr_vectors[vec].iv_pri = pri;
		intr_vectors[vec].iv_vec = vec;
	}
	snprintf(pilname, MAXCOMLEN + 1, "pil%d: %s", pri, pil_names[pri]);
	intrcnt_updatename(pri, pilname, 1);
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
		intr_stray_count[iv->iv_vec]++;
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
		intr_vectors[i].iv_refcnt = 0;
	}
	intr_handlers[PIL_LOW] = intr_fast;
}

void
intr_init2()
{

	mtx_init(&intr_table_lock, "intr table", NULL, MTX_SPIN);
}

static void
intr_enable_eoi(void *arg)
{
	struct intr_vector *iv;
	const struct intr_controller *ic;

	iv = arg;
	ic = iv->iv_ic;
	ic->ic_enable(iv);
	ic->ic_eoi(iv);
}

static void
intr_execute_handlers(void *cookie)
{
	struct intr_vector *iv;
#ifndef INTR_FILTER
	struct intr_event *ie;
	struct intr_handler *ih;
	int error, thread, ret;
#endif

	iv = cookie;
#ifndef INTR_FILTER
	ie = iv->iv_event;
	if (iv->iv_ic == NULL || ie == NULL) {
		intr_stray_vector(iv);
		return;
	}

	/* Execute fast interrupt handlers directly. */
	ret = 0;
	thread = 0;
	critical_enter();
	TAILQ_FOREACH(ih, &ie->ie_handlers, ih_next) {
		if (ih->ih_filter == NULL) {
			thread = 1;
			continue;
		}
		MPASS(ih->ih_filter != NULL && ih->ih_argument != NULL);
		CTR3(KTR_INTR, "%s: executing handler %p(%p)", __func__,
		    ih->ih_filter, ih->ih_argument);
		ret = ih->ih_filter(ih->ih_argument);
		/*
		 * Wrapper handler special case: see
		 * i386/intr_machdep.c::intr_execute_handlers()
		 */
		if (!thread) {
			if (ret == FILTER_SCHEDULE_THREAD)
				thread = 1;
		}
	}
	if (!thread)
		intr_enable_eoi(iv);

	/* Schedule a heavyweight interrupt process. */
	if (thread)
		error = intr_event_schedule_thread(ie);
	else if (TAILQ_EMPTY(&ie->ie_handlers))
		error = EINVAL;
	else
		error = 0;
	critical_exit();
	if (error == EINVAL)
#else
	if (intr_event_handle(iv->iv_event, NULL) != 0)
#endif
		intr_stray_vector(iv);
}

int
intr_controller_register(int vec, const struct intr_controller *ic,
    void *icarg)
{
	struct intr_event *ie;
	struct intr_vector *iv;
	int error;

	iv = &intr_vectors[vec];
	mtx_lock_spin(&intr_table_lock);
	ie = iv->iv_event;
	mtx_unlock_spin(&intr_table_lock);
	if (ie != NULL)
		return (EEXIST);
	/*
	 * Testing shows that at least with the interrupt controllers of
	 * Psycho and Schizo bridges enabling an interrupt doesn't cause
	 * an outstanding interrupt to be issued to the CPU. Thus we can't
	 * use a function doing disable+EOI for the "disable" pointer as
	 * done on other architectures because this would lead to a lost
	 * interrupt if it triggers while we are still processing the
	 * previous one. Instead we use an enable+EOI approach because as
	 * outlined in the Tomatillo documentation clearing an interrupt
	 * in the interrupt controller causes it to be (re)issued to the
	 * CPU as long as the source of a level sensitive interrupt is
	 * not cleared.
	 */
	error = intr_event_create(&ie, iv, 0, intr_enable_eoi,
#ifdef INTR_FILTER
	    ic->ic_eoi, ic->ic_disable, "vec%d:", vec);
#else
	    "vec%d:", vec);
#endif
	if (error != 0)
		return (error);
	mtx_lock_spin(&intr_table_lock);
	if (iv->iv_event != NULL) {
		mtx_unlock_spin(&intr_table_lock);
		intr_event_destroy(ie);
		return (EEXIST);
	}
	iv->iv_ic = ic;
	iv->iv_icarg = icarg;
	iv->iv_event = ie;
	iv->iv_mid = PCPU_GET(mid);
	mtx_unlock_spin(&intr_table_lock);
	return (0);
}

int
inthand_add(const char *name, int vec, driver_filter_t *filt,
    driver_intr_t *handler, void *arg, int flags, void **cookiep)
{
	const struct intr_controller *ic;
	struct intr_event *ie;
	struct intr_handler *ih;
	struct intr_vector *iv;
	int error, fast;

	iv = &intr_vectors[vec];
	mtx_lock_spin(&intr_table_lock);
	ic = iv->iv_ic;
	ie = iv->iv_event;
	mtx_unlock_spin(&intr_table_lock);
	if (ic == NULL || ie == NULL)
		return (EINVAL);

	error = intr_event_add_handler(ie, name, filt, handler, arg,
	    intr_priority(flags), flags, cookiep);
	if (error != 0)
		return (error);

	mtx_lock_spin(&intr_table_lock);
	/* Disable the interrupt while we fiddle with it. */
	ic->ic_disable(iv);
	iv->iv_refcnt++;
	if (iv->iv_refcnt == 1)
		intr_setup(filt != NULL ? PIL_FAST : PIL_ITHREAD, intr_fast,
		    vec, intr_execute_handlers, iv);
	else if (filt != NULL) {
		/*
		 * Check if we need to upgrade from PIL_ITHREAD to PIL_FAST.
		 * Given that apart from the on-board SCCs and UARTs shared
		 * interrupts are rather uncommon on sparc64 this sould be
		 * pretty rare in practice.
		 */
		fast = 0;
		TAILQ_FOREACH(ih, &ie->ie_handlers, ih_next) {
			if (ih->ih_filter != NULL && ih->ih_filter != filt) {
				fast = 1;
				break;
			}
		}
		if (fast == 0)
			intr_setup(PIL_FAST, intr_fast, vec,
			    intr_execute_handlers, iv);
	}
	intr_stray_count[vec] = 0;
	intrcnt_updatename(vec, ie->ie_fullname, 0);
	/* Ensure the interrupt is cleared, it might have triggered before. */
	intr_enable_eoi(iv);
	mtx_unlock_spin(&intr_table_lock);
	return (0);
}

int
inthand_remove(int vec, void *cookie)
{
	struct intr_vector *iv;
	int error;

	error = intr_event_remove_handler(cookie);
	if (error == 0) {
		/*
		 * XXX: maybe this should be done regardless of whether
		 * intr_event_remove_handler() succeeded?
		 */
		iv = &intr_vectors[vec];
		mtx_lock_spin(&intr_table_lock);
		iv->iv_refcnt--;
		if (iv->iv_refcnt == 0) {
			/*
			 * Don't disable the interrupt for now, so that
			 * stray interrupts get detected...
			 */
			intr_setup(PIL_LOW, intr_fast, vec,
			    intr_stray_vector, iv);
		}
		mtx_unlock_spin(&intr_table_lock);
	}
	return (error);
}
