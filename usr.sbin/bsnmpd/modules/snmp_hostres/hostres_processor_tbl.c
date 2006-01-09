/*-
 * Copyright (c) 2005-2006 The FreeBSD Project
 * All rights reserved.
 *
 * Author: Victor Cruceru <soc-victor@freebsd.org>
 *
 * Redistribution of this software and documentation and use in source and
 * binary forms, with or without modification, are permitted provided that
 * the following conditions are met:
 *
 * 1. Redistributions of source code or documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
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

/*
 * Host Resources MIB for SNMPd. Implementation for hrProcessorTable
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "hostres_snmp.h"
#include "hostres_oid.h"
#include "hostres_tree.h"

/*
 * This structure is used to hold a SNMP table entry
 * for HOST-RESOURCES-MIB's hrProcessorTable.
 * Note that index is external being allocated & maintained
 * by the hrDeviceTable code..
 */
struct processor_entry {
	int32_t		index;
	struct asn_oid	frwId;
	int32_t		load;
	TAILQ_ENTRY(processor_entry) link;
	u_char 		cpu_no;		/* which cpu, counted from 0 */
	pid_t		idle_pid;	/* PID of idle process for this CPU */

	/* the samples from the last minute, as required by MIB */
	double		samples[MAX_CPU_SAMPLES];

	/* current sample to fill in next time, must be < MAX_CPU_SAMPLES */
	uint32_t	cur_sample_idx;

	/* number of useful samples */
	uint32_t	sample_cnt;
};
TAILQ_HEAD(processor_tbl, processor_entry);

/* the head of the list with hrDeviceTable's entries */
static struct processor_tbl processor_tbl =
    TAILQ_HEAD_INITIALIZER(processor_tbl);

/* number of processors in dev tbl */
static int32_t detected_processor_count;

/* sysctlbyname(hw.ncpu) */
static int hw_ncpu;

/* sysctlbyname(kern.{ccpu,fscale}) */
static fixpt_t ccpu;
static int fscale;

/* tick of PDU where we have refreshed the processor table last */
static uint64_t proctbl_tick;

/* periodic timer used to get cpu load stats */
static void *cpus_load_timer;

/*
 * Average the samples. The entire algorithm seems to be wrong XXX.
 */
static int
get_avg_load(struct processor_entry *e)
{
	u_int i;
	double sum = 0.0;

	assert(e != NULL);

	if (e->sample_cnt == 0)
		return (0);

	for (i = 0; i < e->sample_cnt; i++)
		sum += e->samples[i];

	return ((int)floor((double)sum/(double)e->sample_cnt));
}

/*
 * Stolen from /usr/src/bin/ps/print.c. The idle process should never
 * be swapped out :-)
 */
static double
processor_getpcpu(struct kinfo_proc *ki_p)
{

	if (ccpu == 0 || fscale == 0)
		return (0.0);

#define	fxtofl(fixpt) ((double)(fixpt) / fscale)
	return (100.0 * fxtofl(ki_p->ki_pctcpu) /
	    (1.0 - exp(ki_p->ki_swtime * log(fxtofl(ccpu)))));
}

/**
 * Save a new sample
 */
static void
save_sample(struct processor_entry *e, struct kinfo_proc *kp)
{

	e->samples[e->cur_sample_idx] = 100.0 - processor_getpcpu(kp);
	e->load = get_avg_load(e);
	e->cur_sample_idx = (e->cur_sample_idx + 1) % MAX_CPU_SAMPLES;

	if (++e->sample_cnt > MAX_CPU_SAMPLES)
		e->sample_cnt = MAX_CPU_SAMPLES;
}

/**
 * Create a new entry into the processor table.
 */
static struct processor_entry *
proc_create_entry(u_int cpu_no, struct device_map_entry *map)
{
	struct device_entry *dev;
	struct processor_entry *entry;
	char name[128];

	/*
	 * If there is no map entry create one by creating a device table
	 * entry.
	 */
	if (map == NULL) {
		snprintf(name, sizeof(name), "cpu%u", cpu_no);
		if ((dev = device_entry_create(name, "", "")) == NULL)
			return (NULL);
		dev->flags |= HR_DEVICE_IMMUTABLE;
		STAILQ_FOREACH(map, &device_map, link)
			if (strcmp(map->name_key, name) == 0)
				break;
		if (map == NULL)
			abort();
	}
		
	if ((entry = malloc(sizeof(*entry))) == NULL) {
		syslog(LOG_ERR, "hrProcessorTable: %s malloc "
		    "failed: %m", __func__);
		return (NULL);
	}
	memset(entry, 0, sizeof(*entry));

	entry->index = map->hrIndex;
	entry->load = 0;
	entry->cpu_no = (u_char)cpu_no;
	entry->idle_pid = 0;
	entry->frwId = oid_zeroDotZero; /* unknown id FIXME */

	INSERT_OBJECT_INT(entry, &processor_tbl);

	HRDBG("CPU %d added with SNMP index=%d",
	    entry->cpu_no, entry->index);

	return (entry);
}

/**
 * Get the PIDs for the idle processes of the CPUs.
 */
static void
processor_get_pids(void)
{
	struct kinfo_proc *plist, *kp;
	int i;
	int nproc;
	int cpu;
	int nchars;
	struct processor_entry *entry;

	plist = kvm_getprocs(hr_kd, KERN_PROC_ALL, 0, &nproc);
	if (plist == NULL || nproc < 0) {
		syslog(LOG_ERR, "hrProcessor: kvm_getprocs() failed: %m");
		return;
	}

	for (i = 0, kp = plist; i < nproc; i++, kp++) {
		if (!IS_KERNPROC(kp))
			continue;

		if (strcmp(kp->ki_ocomm, "idle") == 0) {
			/* single processor system */
			cpu = 0;
		} else if (sscanf(kp->ki_ocomm, "idle: cpu%d%n", &cpu, &nchars)
		    == 1 && (u_int)nchars == strlen(kp->ki_ocomm)) {
			/* MP system */
		} else
			/* not an idle process */
			continue;

		HRDBG("'%s' proc with pid %d is on CPU #%d (last on #%d)",
		    kp->ki_ocomm, kp->ki_pid, kp->ki_oncpu, kp->ki_lastcpu);

		TAILQ_FOREACH(entry, &processor_tbl, link)
			if (entry->cpu_no == kp->ki_lastcpu)
				break;

		if (entry == NULL) {
			/* create entry on non-ACPI systems */
		   	if ((entry = proc_create_entry(cpu, NULL)) == NULL)
				continue;

			detected_processor_count++;
		}

		entry->idle_pid = kp->ki_pid;
		HRDBG("CPU no. %d with SNMP index=%d has idle PID %d",
		    entry->cpu_no, entry->index, entry->idle_pid);

		save_sample(entry, plist);
	}
}

/**
 * Scan the device map table for CPUs and create an entry into the
 * processor table for each CPU. Then fetch the idle PIDs for all CPUs.
 */
static void
create_proc_table(void)
{
	struct device_map_entry *map;
	struct processor_entry *entry;
	int cpu_no;

	detected_processor_count = 0;

	/*
	 * Because hrProcessorTable depends on hrDeviceTable,
	 * the device detection must be performed at this point.
	 * If not, no entries will be present in the hrProcessor Table.
	 *
	 * For non-ACPI system the processors are not in the device table,
	 * therefor insert them when getting the idle pids. XXX
	 */
	STAILQ_FOREACH(map, &device_map, link)
		if (strncmp(map->name_key, "cpu", strlen("cpu")) == 0 &&
		    strstr(map->location_key, ".CPU") != NULL) {
			if (sscanf(map->name_key,"cpu%d", &cpu_no) != 1) {
				syslog(LOG_ERR, "hrProcessorTable: Failed to "
				    "get cpu no. from device named '%s'",
				    map->name_key);
				continue;
			}

			if ((entry = proc_create_entry(cpu_no, map)) == NULL)
				continue;

			detected_processor_count++;
		}

	HRDBG("%d CPUs detected", detected_processor_count);

	processor_get_pids();
}

/**
 * Free the processor table
 */
static void
free_proc_table(void)
{
	struct processor_entry *n1;

	while ((n1 = TAILQ_FIRST(&processor_tbl)) != NULL) {
		TAILQ_REMOVE(&processor_tbl, n1, link);
		free(n1);
		detected_processor_count--;
	}

	assert(detected_processor_count == 0);
	detected_processor_count = 0;
}

/**
 * Init the things for hrProcessorTable.
 * Scan the device table for processor entries.
 */
void
init_processor_tbl(void)
{
	size_t len;

	/* get various parameters from the kernel */
	len = sizeof(ccpu);
	if (sysctlbyname("kern.ccpu", &ccpu, &len, NULL, 0) == -1) {
		syslog(LOG_ERR, "hrProcessorTable: sysctl(kern.ccpu) failed");
		ccpu = 0;
	}

	len = sizeof(fscale);
	if (sysctlbyname("kern.fscale", &fscale, &len, NULL, 0) == -1) {
		syslog(LOG_ERR, "hrProcessorTable: sysctl(kern.fscale) failed");
		fscale = 0;
	}

	/* create the initial processor table */
	create_proc_table();
}

/**
 * Finalization routine for hrProcessorTable.
 * It destroys the lists and frees any allocated heap memory.
 */
void
fini_processor_tbl(void)
{

	if (cpus_load_timer != NULL) {
		timer_stop(cpus_load_timer);
		cpus_load_timer = NULL;
	}

	free_proc_table();
}

/**
 * Make sure that the number of processors announced by the kernel hw.ncpu
 * is equal to the number of processors we have found in the device table.
 * If they differ rescan the device table.
 */
static void
processor_refill_tbl(void)
{

	HRDBG("hw_ncpu=%d detected_processor_count=%d", hw_ncpu,
	    detected_processor_count);

	if (hw_ncpu <= 0) {
		size_t size = sizeof(hw_ncpu);

		if (sysctlbyname("hw.ncpu", &hw_ncpu, &size, NULL, 0) == -1 ||
		    size != sizeof(hw_ncpu)) {
			syslog(LOG_ERR, "hrProcessorTable: "
			    "sysctl(hw.ncpu) failed: %m");
			hw_ncpu = 0;
			return;
		}
	}

	if (hw_ncpu != detected_processor_count) {
		free_proc_table();
		create_proc_table();
	}
}

/**
 * Refresh all values in the processor table. We call this once for
 * every PDU that accesses the table.
 */
static void
refresh_processor_tbl(void)
{
	struct processor_entry *entry;
	int need_pids;
	struct kinfo_proc *plist;
	int nproc;

	processor_refill_tbl();

	need_pids = 0;
	TAILQ_FOREACH(entry, &processor_tbl, link) {
		if (entry->idle_pid <= 0) {
			need_pids = 1;
			continue;
		}

		assert(hrState_g.kd != NULL);

		plist = kvm_getprocs(hr_kd, KERN_PROC_PID,
		    entry->idle_pid, &nproc);
		if (plist == NULL || nproc != 1) {
			syslog(LOG_ERR, "%s: missing item with "
			    "PID = %d for CPU #%d\n ", __func__,
			    entry->idle_pid, entry->cpu_no);
			need_pids = 1;
			continue;
		}
		save_sample(entry, plist);
	}

	if (need_pids == 1)
		processor_get_pids();

	proctbl_tick = this_tick;
}

/**
 * This function is called MAX_CPU_SAMPLES times per minute to collect the
 * CPU load.
 */
static void
get_cpus_samples(void *arg __unused)
{

	HRDBG("[%llu] ENTER", get_ticks());
	refresh_processor_tbl();
	HRDBG("[%llu] EXIT", get_ticks());
}

/**
 * Called to start this table. We need to start the periodic idle
 * time collection.
 */
void
start_processor_tbl(struct lmodule *mod)
{

	/*
	 * Start the cpu stats collector
	 * The semantics of timer_start parameters is in "SNMP ticks";
	 * we have 100 "SNMP ticks" per second, thus we are trying below
	 * to get MAX_CPU_SAMPLES per minute
	 */
	cpus_load_timer = timer_start_repeat(100, 100 * 60 / MAX_CPU_SAMPLES,
	    get_cpus_samples, NULL, mod);
}

/**
 * Access routine for the processor table.
 */
int
op_hrProcessorTable(struct snmp_context *ctx __unused,
    struct snmp_value *value, u_int sub, u_int iidx __unused,
    enum snmp_op curr_op)
{
	struct processor_entry *entry;

	if (this_tick != proctbl_tick)
		refresh_processor_tbl();

	switch (curr_op) {

	case SNMP_OP_GETNEXT:
		if ((entry = NEXT_OBJECT_INT(&processor_tbl,
		    &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		value->var.len = sub + 1;
		value->var.subs[sub] = entry->index;
		goto get;

	case SNMP_OP_GET:
		if ((entry = FIND_OBJECT_INT(&processor_tbl,
		    &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		goto get;

	case SNMP_OP_SET:
		if ((entry = FIND_OBJECT_INT(&processor_tbl,
		    &value->var, sub)) == NULL)
			return (SNMP_ERR_NO_CREATION);
		return (SNMP_ERR_NOT_WRITEABLE);

	case SNMP_OP_ROLLBACK:
	case SNMP_OP_COMMIT:
		abort();
	}
	abort();

  get:
	switch (value->var.subs[sub - 1]) {

	case LEAF_hrProcessorFrwID:
		value->v.oid = entry->frwId;
		return (SNMP_ERR_NOERROR);

	case LEAF_hrProcessorLoad:
		value->v.integer = entry->load;
		return (SNMP_ERR_NOERROR);
	}
	abort();
}
