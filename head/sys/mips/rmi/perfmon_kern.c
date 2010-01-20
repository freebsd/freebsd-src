/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * RMI_BSD */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <machine/smp.h>
#include <mips/rmi/perfmon.h>
#include <mips/rmi/pic.h>
#include <sys/mutex.h>
#include <mips/rmi/clock.h>


int xlr_perfmon_started = 0;
struct perf_area *xlr_shared_config_area = NULL;
uint32_t *xlr_perfmon_timer_loc;
uint32_t *xlr_cpu_sampling_interval;
uint32_t xlr_perfmon_kernel_version = 1;	/* Future use */
uint32_t xlr_perfmon_ticks;
extern int mips_cpu_online_mask;
extern uint32_t cpu_ltop_map[MAXCPU];

#ifdef SMP
static __inline__ void 
pic_send_perfmon_ipi(int cpu)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);
	int tid, pid;
	uint32_t ipi;

	tid = cpu & 0x3;
	pid = (cpu >> 2) & 0x7;
	ipi = (pid << 20) | (tid << 16) | IPI_PERFMON;

	mtx_lock_spin(&xlr_pic_lock);
	xlr_write_reg(mmio, PIC_IPI, ipi);
	mtx_unlock_spin(&xlr_pic_lock);
}

#endif


void
xlr_perfmon_clockhandler(void)
{
#ifdef SMP
	int cpu;
	int i;

#endif

	if (xlr_perfmon_ticks++ >= (*xlr_cpu_sampling_interval) / (XLR_PIC_HZ / (hz * 1024))) {

		/* update timer */
		*xlr_perfmon_timer_loc += *xlr_cpu_sampling_interval;
		xlr_perfmon_ticks = 0;
		xlr_perfmon_sampler(NULL);
#ifdef SMP
		for (i = 0; i < NCPUS; i = i + NTHREADS) {	/* oly thread 0 */
			cpu = cpu_ltop_map[i];
			if ((mips_cpu_online_mask & (1 << i)) &&
			    xlr_shared_config_area[cpu / NTHREADS].perf_config.magic ==
			    PERFMON_ACTIVE_MAGIC)
				pic_send_perfmon_ipi(cpu);
		}

#endif

	}
}

static void
xlr_perfmon_start(void)
{
	size_t size;

	size = (NCORES * sizeof(*xlr_shared_config_area)) +
	    sizeof(*xlr_perfmon_timer_loc) +
	    sizeof(*xlr_cpu_sampling_interval);

	xlr_shared_config_area = malloc(size, M_TEMP, M_WAITOK);
	if (!xlr_shared_config_area) {
		/* ERROR */
		return;
	}
	xlr_perfmon_timer_loc = (uint32_t *) (xlr_shared_config_area + NCORES);
	xlr_cpu_sampling_interval = (uint32_t *) (xlr_perfmon_timer_loc + 1);

	*xlr_cpu_sampling_interval = DEFAULT_CPU_SAMPLING_INTERVAL;
	*xlr_perfmon_timer_loc = 0;
	xlr_perfmon_ticks = 0;

	xlr_perfmon_init_cpu(NULL);
#ifdef SMP
	smp_call_function(xlr_perfmon_init_cpu, NULL,
	    PCPU_GET(other_cpus) & 0x11111111);
#endif
	xlr_perfmon_started = 1;

}

static void
xlr_perfmon_stop(void)
{
	xlr_perfmon_started = 0;
	free(xlr_shared_config_area, M_TEMP);
	xlr_shared_config_area = NULL;
}

static int
sysctl_xlr_perfmon_start_stop(SYSCTL_HANDLER_ARGS)
{
	int error, val = xlr_perfmon_started;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (!xlr_perfmon_started && val != 0)
		xlr_perfmon_start();
	else if (xlr_perfmon_started && val == 0)
		xlr_perfmon_stop();

	return (0);
}


SYSCTL_NODE(_debug, OID_AUTO, xlrperf, CTLFLAG_RW, NULL, "XLR PERF Nodes");
SYSCTL_PROC(_debug_xlrperf, OID_AUTO, start, CTLTYPE_INT | CTLFLAG_RW,
    &xlr_perfmon_started, 0, sysctl_xlr_perfmon_start_stop, "I", "set/unset to start/stop "
    "performance monitoring");
