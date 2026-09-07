/*
 * Copyright 2026 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include "opt_sched.h"

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/runq.h>
#include <sys/sbuf.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <machine/ifunc.h>

static const struct sched_instance *active_sched;

SCHED_STAT_DEFINE(ithread_demotions, "Interrupt thread priority demotions");
SCHED_STAT_DEFINE(ithread_preemptions,
    "Interrupt thread preemptions due to time-sharing");

SDT_PROVIDER_DEFINE(sched);

SDT_PROBE_DEFINE3(sched, , , change__pri, "struct thread *",
    "struct proc *", "uint8_t");
SDT_PROBE_DEFINE3(sched, , , dequeue, "struct thread *",
    "struct proc *", "void *");
SDT_PROBE_DEFINE4(sched, , , enqueue, "struct thread *",
    "struct proc *", "void *", "int");
SDT_PROBE_DEFINE4(sched, , , lend__pri, "struct thread *",
    "struct proc *", "uint8_t", "struct thread *");
SDT_PROBE_DEFINE2(sched, , , load__change, "int", "int");
SDT_PROBE_DEFINE2(sched, , , off__cpu, "struct thread *",
    "struct proc *");
SDT_PROBE_DEFINE(sched, , , on__cpu);
SDT_PROBE_DEFINE(sched, , , remain__cpu);
SDT_PROBE_DEFINE2(sched, , , surrender, "struct thread *",
    "struct proc *");

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>
int __read_mostly		dtrace_vtime_active;
dtrace_vtime_switch_func_t	dtrace_vtime_switch_func;
#endif

/*
 * Define dispatchers for all functions described by SCHED_ITF_DISPATCH().
 */
#define DEFINE_SHIM(args, ret_type, fn, ...)				\
	DEFINE_IFUNC(, ret_type, SCHED_ITF_FUNCTION_NAME(fn),		\
	    (SCHED_ITF_ARGS_PROTO_NAME(__VA_ARGS__)))			\
	{								\
		return (active_sched->SCHED_ITF_FIELD_NAME(fn));	\
	}
SCHED_ITF_DISPATCH(DEFINE_SHIM, );
#undef DEFINE_SHIM

static char sched_name[32] = "ULE";

SET_DECLARE(sched_instance_set, struct sched_selection);

void
sched_instance_select(void)
{
	struct sched_selection *s, **ss;
	int i;

	TUNABLE_STR_FETCH("kern.sched.name", sched_name, sizeof(sched_name));
	SET_FOREACH(ss, sched_instance_set) {
		s = *ss;
		for (i = 0; s->name[i] == sched_name[i]; i++) {
			if (s->name[i] == '\0') {
				active_sched = s->instance;
				return;
			}
		}
	}

	/*
	 * No scheduler matching the configuration was found.  If
	 * there is any scheduler compiled in, at all, use the first
	 * scheduler from the linker set.
	 */
	if (SET_BEGIN(sched_instance_set) < SET_LIMIT(sched_instance_set)) {
		s = *SET_BEGIN(sched_instance_set);
		active_sched = s->instance;
		for (i = 0;; i++) {
			sched_name[i] = s->name[i];
			if (s->name[i] == '\0')
				break;
		}
	}
}

void
sched_init(void)
{
	if (active_sched == NULL)
		panic("Cannot find scheduler %s", sched_name);
	active_sched->SCHED_ITF_FIELD_NAME(init)();
}

struct cpu_group __read_mostly *cpu_top;		/* CPU topology */

static void
sched_setup(void)
{
	cpu_top = smp_topo();
	active_sched->SCHED_ITF_FIELD_NAME(setup)();
}
SYSINIT(sched_setup, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, sched_setup, NULL);

static void
sched_initticks(void)
{
	active_sched->SCHED_ITF_FIELD_NAME(initticks)();
}
SYSINIT(sched_initticks, SI_SUB_CLOCKS, SI_ORDER_THIRD, sched_initticks,
    NULL);

static void
sched_sysinit(void)
{
	active_sched->SCHED_ITF_FIELD_NAME(sysinit)();
}
SYSINIT(sched_sysinit, SI_SUB_LAST, SI_ORDER_FIRST, sched_sysinit, NULL);

SYSCTL_NODE(_kern, OID_AUTO, sched, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Scheduler");

SYSCTL_STRING(_kern_sched, OID_AUTO, name, CTLFLAG_RD, sched_name, 0,
    "Scheduler name");

static int
sysctl_kern_sched_available(SYSCTL_HANDLER_ARGS)
{
	struct sched_selection *s, **ss;
	struct sbuf *sb, sm;
	int error;
	bool first;

	sb = sbuf_new_for_sysctl(&sm, NULL, 0, req);
	if (sb == NULL)
		return (ENOMEM);
	first = true;
	SET_FOREACH(ss, sched_instance_set) {
		s = *ss;
		if (first)
			first = false;
		else
			sbuf_cat(sb, ",");
		sbuf_cat(sb, s->name);
	}
	error = sbuf_finish(sb);
	sbuf_delete(sb);
	return (error);
}

SYSCTL_PROC(_kern_sched, OID_AUTO, available,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
    NULL, 0, sysctl_kern_sched_available, "A",
    "List of available schedulers");

fixpt_t ccpu;
SYSCTL_UINT(_kern, OID_AUTO, ccpu, CTLFLAG_RD, &ccpu, 0,
    "Decay factor used for updating %CPU");

/*
 * Build the CPU topology dump string. Is recursively called to collect
 * the topology tree.
 */
static int
sysctl_kern_sched_topology_spec_internal(struct sbuf *sb,
    struct cpu_group *cg, int indent)
{
	char cpusetbuf[CPUSETBUFSIZ];
	int i, first;

	if (cpu_top == NULL) {
		sbuf_printf(sb, "%*s<group level=\"1\" cache-level=\"1\">\n",
		    indent, "");
		sbuf_printf(sb, "%*s</group>\n", indent, "");
		return (0);
	}

	sbuf_printf(sb, "%*s<group level=\"%d\" cache-level=\"%d\">\n", indent,
	    "", 1 + indent / 2, cg->cg_level);
	sbuf_printf(sb, "%*s <cpu count=\"%d\" mask=\"%s\">", indent, "",
	    cg->cg_count, cpusetobj_strprint(cpusetbuf, &cg->cg_mask));
	first = TRUE;
	for (i = cg->cg_first; i <= cg->cg_last; i++) {
		if (CPU_ISSET(i, &cg->cg_mask)) {
			if (!first)
				sbuf_cat(sb, ", ");
			else
				first = FALSE;
			sbuf_printf(sb, "%d", i);
		}
	}
	sbuf_cat(sb, "</cpu>\n");

	if (cg->cg_flags != 0) {
		sbuf_printf(sb, "%*s <flags>", indent, "");
		if ((cg->cg_flags & CG_FLAG_HTT) != 0)
			sbuf_cat(sb, "<flag name=\"HTT\">HTT group</flag>");
		if ((cg->cg_flags & CG_FLAG_THREAD) != 0)
			sbuf_cat(sb, "<flag name=\"THREAD\">THREAD group</flag>");
		if ((cg->cg_flags & CG_FLAG_SMT) != 0)
			sbuf_cat(sb, "<flag name=\"SMT\">SMT group</flag>");
		if ((cg->cg_flags & CG_FLAG_NODE) != 0)
			sbuf_cat(sb, "<flag name=\"NODE\">NUMA node</flag>");
		sbuf_cat(sb, "</flags>\n");
	}

	if (cg->cg_children > 0) {
		sbuf_printf(sb, "%*s <children>\n", indent, "");
		for (i = 0; i < cg->cg_children; i++)
			sysctl_kern_sched_topology_spec_internal(sb,
			    &cg->cg_child[i], indent + 2);
		sbuf_printf(sb, "%*s </children>\n", indent, "");
	}
	sbuf_printf(sb, "%*s</group>\n", indent, "");
	return (0);
}

/*
 * Sysctl handler for retrieving topology dump. It's a wrapper for
 * the recursive sysctl_kern_smp_topology_spec_internal().
 */
static int
sysctl_kern_sched_topology_spec(SYSCTL_HANDLER_ARGS)
{
	struct sbuf *topo;
	int err;

	topo = sbuf_new_for_sysctl(NULL, NULL, 512, req);
	if (topo == NULL)
		return (ENOMEM);

	sbuf_cat(topo, "<groups>\n");
	err = sysctl_kern_sched_topology_spec_internal(topo, cpu_top, 1);
	sbuf_cat(topo, "</groups>\n");

	if (err == 0)
		err = sbuf_finish(topo);
	sbuf_delete(topo);
	return (err);
}

SYSCTL_PROC(_kern_sched, OID_AUTO, topology_spec, CTLTYPE_STRING |
    CTLFLAG_MPSAFE | CTLFLAG_RD, NULL, 0,
    sysctl_kern_sched_topology_spec, "A",
    "XML dump of detected CPU topology");
