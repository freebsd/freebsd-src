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
__FBSDID("$FreeBSD: src/sys/sun4v/sun4v/intr_machdep.c,v 1.7.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/vmmeter.h>

#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/cpu.h>
#include <machine/smp.h>

#include <machine/hypervisorvar.h>
#include <machine/hv_api.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#define PANIC_IF(exp) if (unlikely(exp)) {panic("%s: %s:%d", #exp, __FILE__, __LINE__);}

#define	MAX_STRAY_LOG	5

CTASSERT((1 << IV_SHIFT) == sizeof(struct intr_vector));

ih_func_t *intr_handlers[PIL_MAX];
uint16_t pil_countp[PIL_MAX];

struct intr_vector intr_vectors[IV_MAX];
uint16_t intr_countp[IV_MAX];
static u_long intr_stray_count[IV_MAX];

struct ithread_vector_handler {
	iv_func_t *ivh_handler;
	void *ivh_arg;
	u_int ivh_vec;
};

static char *pil_names[] = {
	"stray",
	"low",		/* PIL_LOW */
	"ithrd",	/* PIL_ITHREAD */
	"rndzvs",	/* PIL_RENDEZVOUS */
	"ast",		/* PIL_AST */
	"stop",		/* PIL_STOP */
	"preempt",      /* PIL_PREEMPT */
	"stray", "stray", "stray", "stray", "stray", "stray",
	"fast",		/* PIL_FAST */
	"tick",		/* PIL_TICK */
};
	
 
/*
 * XXX SUN4V_FIXME - the queue size values should
 * really be calculated based on the size of the partition
 *
 */

int cpu_q_entries = 128;
int dev_q_entries = 128;

static vm_offset_t *mondo_data_array;
static vm_offset_t *cpu_list_array;
static vm_offset_t *cpu_q_array;
static vm_offset_t *dev_q_array;
static vm_offset_t *rq_array;
static vm_offset_t *nrq_array;
static int cpu_list_size;



/* protect the intr_vectors table */
static struct mtx intr_table_lock;

static void intr_execute_handlers(void *);
static void intr_stray_level(struct trapframe *);
static void  intr_stray_vector(void *);
static int  intrcnt_setname(const char *, int);
static void intrcnt_updatename(int, const char *, int);
static void cpu_intrq_alloc(void);

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

	ps = intr_disable_all();
	if (vec != -1) {
		intr_vectors[vec].iv_func = ivf;
		intr_vectors[vec].iv_arg = iva;
		intr_vectors[vec].iv_pri = pri;
		intr_vectors[vec].iv_vec = vec;
	}
	snprintf(pilname, MAXCOMLEN + 1, "pil%d: %s", pri, pil_names[pri]);
	intrcnt_updatename(pri, pilname, 1);
	intr_handlers[pri] = ihf;
	intr_restore_all(ps);
}

static void
intr_stray_level(struct trapframe *tf)
{

	printf("stray level interrupt - pil=%ld\n", tf->tf_pil);
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

static void
intr_init(void)
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
	
#ifdef SMP
	intr_handlers[PIL_AST] = cpu_ipi_ast;
	intr_handlers[PIL_RENDEZVOUS] = (ih_func_t *)smp_rendezvous_action;
	intr_handlers[PIL_STOP]= cpu_ipi_stop;
	intr_handlers[PIL_PREEMPT]= cpu_ipi_preempt;
#endif
	mtx_init(&intr_table_lock, "intr table", NULL, MTX_SPIN);
	cpu_intrq_alloc();
	cpu_intrq_init();

}
SYSINIT(intr_init, SI_SUB_INTR, SI_ORDER_FIRST, intr_init, NULL);


static void
intr_execute_handlers(void *cookie)
{
	struct intr_vector *iv;
	struct intr_event *ie;
	struct intr_handler *ih;
	int fast, thread, ret;

	iv = cookie;
	ie = iv->iv_event;
	if (ie == NULL) {
		intr_stray_vector(iv);
		return;
	}
	
	ret = 0;
	fast = thread = 0;
	TAILQ_FOREACH(ih, &ie->ie_handlers, ih_next) {
		if (ih->ih_filter == NULL) {
			thread = 1;
			continue;
		}
		MPASS(ih->ih_filter != NULL && ih->ih_argument != NULL);
		CTR3(KTR_INTR, "%s: executing handler %p(%p)", __func__,
		    ih->ih_filter, ih->ih_argument);
		ret = ih->ih_filter(ih->ih_argument);
		fast = 1;
		/*
		 * Wrapper handler special case: see
		 * i386/intr_machdep.c::intr_execute_handlers()
		 */
		if (!thread) {
			if (ret == FILTER_SCHEDULE_THREAD)
				thread = 1;
		}
	}

	/* Schedule a heavyweight interrupt process. */
	if (thread) 
		intr_event_schedule_thread(ie);
	else if (TAILQ_EMPTY(&ie->ie_handlers))
		intr_stray_vector(iv);

	if (fast)
		hv_intr_setstate(iv->iv_vec, HV_INTR_IDLE_STATE);

}

static void
ithread_wrapper(void *arg)
{
	struct ithread_vector_handler *ivh = (struct ithread_vector_handler *)arg;
	
	ivh->ivh_handler(ivh->ivh_arg);
	/* re-enable interrupt */
	hv_intr_setstate(ivh->ivh_vec, HV_INTR_IDLE_STATE);
}

int
inthand_add(const char *name, int vec, driver_filter_t *filt, 
    void (*handler)(void *), void *arg, int flags, void **cookiep)    
{
	struct intr_vector *iv;
	struct intr_event *ie;		/* descriptor for the IRQ */
	struct intr_event *orphan;
	struct ithread_vector_handler *ivh;
	int errcode, pil;

	if (filt != NULL && handler != NULL) {
		printf("both filt and handler set is not valid\n");
		return (EINVAL);
	}
	/*
	 * Work around a race where more than one CPU may be registering
	 * handlers on the same IRQ at the same time.
	 */
	iv = &intr_vectors[vec];
	mtx_lock_spin(&intr_table_lock);
	ie = iv->iv_event;
	mtx_unlock_spin(&intr_table_lock);
	if (ie == NULL) {
		errcode = intr_event_create(&ie, (void *)(intptr_t)vec, 0, NULL,
		    NULL, "vec%d:", vec);
		if (errcode)
			return (errcode);
		mtx_lock_spin(&intr_table_lock);
		if (iv->iv_event == NULL) {
			iv->iv_event = ie;
			mtx_unlock_spin(&intr_table_lock);
		} else {
			orphan = ie;
			ie = iv->iv_event;
			mtx_unlock_spin(&intr_table_lock);
			intr_event_destroy(orphan);
		}
	}

	if (filt == NULL) {
		ivh = (struct ithread_vector_handler *)
			malloc(sizeof(struct ithread_vector_handler), M_DEVBUF, M_WAITOK);
		ivh->ivh_handler = (driver_intr_t *)handler;
		ivh->ivh_arg = arg;
		ivh->ivh_vec = vec;
		errcode = intr_event_add_handler(ie, name, NULL, ithread_wrapper, ivh,
						 intr_priority(flags), flags, cookiep);
	} else {
		ivh = NULL;
		errcode = intr_event_add_handler(ie, name, filt, NULL, arg,
						 intr_priority(flags), flags, 
						 cookiep);
	}

	if (errcode) {
		if (ivh)
			free(ivh, M_DEVBUF);
		return (errcode);
	}
	pil = (filt != NULL) ? PIL_FAST : PIL_ITHREAD;

	intr_setup(pil, intr_fast, vec, intr_execute_handlers, iv);

	intr_stray_count[vec] = 0;

	intrcnt_updatename(vec, ie->ie_fullname, 0);

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
		 * XXX: aren't the PIL's backwards below?
		 */
		iv = &intr_vectors[vec];
		mtx_lock_spin(&intr_table_lock);
		if (iv->iv_event == NULL)
			intr_setup(PIL_ITHREAD, intr_fast, vec,
			    intr_stray_vector, iv);
		else
			intr_setup(PIL_LOW, intr_fast, vec,
			    intr_execute_handlers, iv);
		mtx_unlock_spin(&intr_table_lock);
	}
	return (error);
}

/* 
 * Allocate and register intrq fields
 */
static void 
cpu_intrq_alloc(void)
{
	
	mondo_data_array = malloc(INTR_REPORT_SIZE*MAXCPU, M_DEVBUF, M_WAITOK | M_ZERO);
	PANIC_IF(mondo_data_array == NULL);

	cpu_list_size = CPU_LIST_SIZE > INTR_REPORT_SIZE ? CPU_LIST_SIZE : INTR_REPORT_SIZE;
	cpu_list_array = malloc(cpu_list_size*MAXCPU, M_DEVBUF, M_WAITOK | M_ZERO);
	PANIC_IF(cpu_list_array == NULL);
	
	cpu_q_array = malloc(INTR_CPU_Q_SIZE*MAXCPU, M_DEVBUF, M_WAITOK | M_ZERO);
	PANIC_IF(cpu_q_array == NULL);
	
	dev_q_array = malloc(INTR_DEV_Q_SIZE*MAXCPU, M_DEVBUF, M_WAITOK | M_ZERO);
	PANIC_IF(dev_q_array == NULL);

	rq_array = malloc(2*CPU_RQ_SIZE*MAXCPU, M_DEVBUF, M_WAITOK | M_ZERO);
	PANIC_IF(rq_array == NULL);
	
	nrq_array = malloc(2*CPU_NRQ_SIZE*MAXCPU, M_DEVBUF, M_WAITOK | M_ZERO);
	PANIC_IF(nrq_array == NULL);

}

void 
cpu_intrq_init()
{

	uint64_t error;

	pcpup->pc_mondo_data = (vm_offset_t *) ((char *)mondo_data_array + curcpu*INTR_REPORT_SIZE);
	pcpup->pc_mondo_data_ra = vtophys(pcpup->pc_mondo_data);

	pcpup->pc_cpu_q = (vm_offset_t *)((char *)cpu_q_array + curcpu*INTR_CPU_Q_SIZE);

	pcpup->pc_cpu_q_ra = vtophys(pcpup->pc_cpu_q);
	pcpup->pc_cpu_q_size = INTR_CPU_Q_SIZE;

	pcpup->pc_dev_q = (vm_offset_t *)((char *)dev_q_array + curcpu*INTR_DEV_Q_SIZE);
	pcpup->pc_dev_q_ra = vtophys(pcpup->pc_dev_q);
	pcpup->pc_dev_q_size = INTR_DEV_Q_SIZE;

	pcpup->pc_rq = (vm_offset_t *)((char *)rq_array + curcpu*2*CPU_RQ_SIZE);
	pcpup->pc_rq_ra = vtophys(pcpup->pc_rq);
	pcpup->pc_rq_size = CPU_RQ_SIZE;

	pcpup->pc_nrq = (vm_offset_t *)((char *)nrq_array + curcpu*2*CPU_NRQ_SIZE);
	pcpup->pc_nrq_ra = vtophys(pcpup->pc_nrq);
	pcpup->pc_nrq_size = CPU_NRQ_SIZE;


	error = hv_cpu_qconf(Q(CPU_MONDO_QUEUE_HEAD), pcpup->pc_cpu_q_ra, cpu_q_entries);
	if (error != H_EOK)
		panic("cpu_mondo queue configuration failed: %lu va=%p ra=0x%lx", error,
		      pcpup->pc_cpu_q, pcpup->pc_cpu_q_ra);

	error = hv_cpu_qconf(Q(DEV_MONDO_QUEUE_HEAD), pcpup->pc_dev_q_ra, dev_q_entries);
	if (error != H_EOK)
		panic("dev_mondo queue configuration failed: %lu", error);

	error = hv_cpu_qconf(Q(RESUMABLE_ERROR_QUEUE_HEAD), pcpup->pc_rq_ra, CPU_RQ_ENTRIES);
	if (error != H_EOK)
		panic("resumable error queue configuration failed: %lu", error);

	error = hv_cpu_qconf(Q(NONRESUMABLE_ERROR_QUEUE_HEAD), pcpup->pc_nrq_ra, CPU_NRQ_ENTRIES);
	if (error != H_EOK)
		panic("non-resumable error queue configuration failed: %lu", error);

}
