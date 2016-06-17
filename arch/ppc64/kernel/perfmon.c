/*
 * Code to configure and utilize the ppc64 pmc hardware.  Generally, the
 * following functions are supported:
 *    1. Kernel profile based on decrementer ticks
 *    2. Kernel profile based on PMC execution cycles
 *    3. Trace based on arbitrary PMC counter
 *    4. Timeslice data capture of arbitrary collections of PMCs
 *
 * Copyright (C) 2002 David Engebretsen <engebret@us.ibm.com>
 */

#include <asm/proc_fs.h>
#include <asm/paca.h>
#include <asm/iSeries/ItLpPaca.h>
#include <asm/iSeries/ItLpQueue.h>
#include <asm/processor.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <asm/pmc.h>
#include <asm/uaccess.h>
#include <asm/naca.h>
#include <asm/perfmon.h>
#include <asm/iSeries/HvCall.h>
#include <asm/hvcall.h>
#include <asm/cputable.h>

extern char _stext[], _etext[], _end[];
struct perfmon_base_struct perfmon_base = {0, 0, 0, 0, 0, 0, 0, PMC_STATE_INITIAL};

int alloc_perf_buffer(int size);
int free_perf_buffer(void);
int clear_buffers(void);
void pmc_stop(void *data);
void pmc_clear(void *data);
void pmc_start(void *data);
void pmc_touch_bolted(void *data);
void dump_pmc_struct(struct perfmon_struct *perfdata);
void dump_hardware_pmc_struct(void *perfdata);
int  decr_profile(void *data);
int  pmc_profile(struct perfmon_struct *perfdata);
int  pmc_set_general(struct perfmon_struct *perfdata);
int  pmc_set_user_general(struct perfmon_struct *perfdata);
void pmc_configure(void *data);
int pmc_timeslice_enable(struct perfmon_struct *perfdata);
int pmc_timeslice_disable(struct perfmon_struct *perfdata);
int pmc_timeslice_set(struct perfmon_struct *perfdata);
void pmc_dump_timeslice(struct perfmon_struct *perfdata);
void pmc_trace_rec_type(unsigned long type);
void pmc_timeslice_data_collect(void *data);
int pmc_timeslice_tick(void);
int perfmon_buffer_ctl(void *data);
int perfmon_dump_ctl(void *data);
int perfmon_profile_ctl(void *data);
int perfmon_trace_ctl(void *data);
int perfmon_timeslice_ctl(void *data);

#define PMC_MAX_CPUS     48
#define PMC_MAX_COUNTERS 8
#define PMC_SLICES_STAR  64
#define PMC_SLICES_GP    28
#define PMC_SLICES_MAX   64
#define PMC_TICK_FACTOR  10

int pmc_timeslice_enabled = 0, pmc_timeslice_top = 0;
int pmc_tick_count[PMC_MAX_CPUS], pmc_timeslice[PMC_MAX_CPUS];

unsigned long pmc_timeslice_data[PMC_SLICES_MAX*PMC_MAX_COUNTERS*PMC_MAX_CPUS];

/*
 * DRENG: todo:
 * add api to add config entries (entry, values), and bump pmc_timeslice_top
 *   value
 * add api to get data from kernel (entry, values)
 */
unsigned long pmc_timeslice_config[PMC_SLICES_MAX * 5];

static spinlock_t pmc_lock = SPIN_LOCK_UNLOCKED;
asmlinkage int
sys_perfmonctl (int cmd, void *data) 
{ 
	struct perfmon_struct *pdata;
	int err;

	printk("sys_perfmonctl: cmd = 0x%x\n", cmd); 
	pdata = kmalloc(sizeof(struct perfmon_struct), GFP_USER);
	err = __copy_from_user(pdata, data, sizeof(struct perfmon_struct));
	switch(cmd) {
	case PMC_CMD_BUFFER:
		perfmon_buffer_ctl(data); 
		break;
	case PMC_CMD_DUMP:
		perfmon_dump_ctl(data); 
		break;
	case PMC_CMD_DECR_PROFILE: /* NIA time sampling */
		decr_profile(data); 
		break;
	case PMC_CMD_PROFILE:
		perfmon_profile_ctl(pdata); 
		break;
	case PMC_CMD_TRACE:
		perfmon_trace_ctl(pdata); 
		break;
	case PMC_CMD_TIMESLICE:
		perfmon_timeslice_ctl(pdata); 
		break;
#if 0
	case PMC_OP_TIMESLICE:
		pmc_enable_timeslice(pdata); 
		break;
	case PMC_OP_DUMP_TIMESLICE:
		pmc_dump_timeslice(pdata); 
		smp_call_function(pmc_dump_timeslice, (void *)pdata, 0, 1);
		break;
#endif
	default:
		printk("Perfmon: Unknown command\n");
		break;
	}

	kfree(pdata); 
	return 0;
}

int perfmon_buffer_ctl(void *data) {
	struct perfmon_struct *pdata;
	int err;

	pdata = kmalloc(sizeof(struct perfmon_struct), GFP_USER);
	err = __copy_from_user(pdata, data, sizeof(struct perfmon_struct));

	switch(pdata->header.subcmd) {
	case PMC_SUBCMD_BUFFER_ALLOC:
		alloc_perf_buffer(0); 
		break;
	case PMC_SUBCMD_BUFFER_FREE:
		free_perf_buffer(); 
		break;
	case PMC_SUBCMD_BUFFER_CLEAR:
		clear_buffers();
		break;
	default:
		return(-1); 
	}

	return(0); 
}

int alloc_perf_buffer(int size) 
{
	int i;

	printk("Perfmon: allocate buffer\n");
	if(perfmon_base.state == PMC_STATE_INITIAL) {
		perfmon_base.profile_length = (((unsigned long) &_etext - 
				   (unsigned long) &_stext) >> 2) * sizeof(int);
		perfmon_base.profile_buffer = (unsigned long)btmalloc(perfmon_base.profile_length);
		perfmon_base.trace_length = 1024*1024*16;
		perfmon_base.trace_buffer = (unsigned long)btmalloc(perfmon_base.trace_length);

		perfmon_base.timeslice_length = PMC_SLICES_MAX*PMC_MAX_COUNTERS*PMC_MAX_CPUS;
		perfmon_base.timeslice_buffer = (unsigned long)pmc_timeslice_data;

		if(perfmon_base.profile_buffer && perfmon_base.trace_buffer) {
			memset((char *)perfmon_base.profile_buffer, 0, perfmon_base.profile_length);
			printk("Profile buffer created at address 0x%lx of length 0x%lx\n",
			       perfmon_base.profile_buffer, perfmon_base.profile_length); 
		} else {
			printk("Profile buffer creation failed\n");
			return 0;
		}

		/* Fault in the first bolted segment - it then remains in the stab for all time */
		pmc_touch_bolted(NULL); 
		smp_call_function(pmc_touch_bolted, (void *)NULL, 0, 1);

		for (i=0; i<MAX_PACAS; ++i) {
			paca[i].prof_shift = 2;
			paca[i].prof_len = perfmon_base.profile_length;
			paca[i].prof_buffer = (unsigned *)(perfmon_base.profile_buffer);
			paca[i].prof_stext = (unsigned *)&_stext;

			paca[i].prof_etext = (unsigned *)&_etext;
			mb();
		} 

		perfmon_base.state = PMC_STATE_READY; 
	}

	return 0;
}

int free_perf_buffer() 
{
	printk("Perfmon: free buffer\n");

	if(perfmon_base.state == PMC_STATE_INITIAL) {
		printk("Perfmon: free buffer failed - no buffer was allocated.\n"); 
		return -1;
	}

	btfree((void *)perfmon_base.profile_buffer); 
	btfree((void *)perfmon_base.trace_buffer); 

	perfmon_base.profile_length = 0;
	perfmon_base.profile_buffer = 0;
	perfmon_base.trace_buffer   = 0;
	perfmon_base.trace_length   = 0;
	perfmon_base.trace_end      = 0;
	perfmon_base.state = PMC_STATE_INITIAL; 

	return(0); 
}

int clear_buffers() 
{
	int i, j;

	if(perfmon_base.state == PMC_STATE_INITIAL) {
		printk("Perfmon: clear buffer failed - no buffer was allocated.\n"); 
		return -1;
	}

	printk("Perfmon: clear buffer\n");
	
	/* Stop counters on all [PMC_MAX_CPUS]processors -- blocking */
	pmc_stop(NULL); 
	smp_call_function(pmc_stop, (void *)NULL, 0, 1);
	
	/* Clear the buffers */
	memset((char *)perfmon_base.profile_buffer, 0, perfmon_base.profile_length);
	memset((char *)perfmon_base.trace_buffer, 0, perfmon_base.trace_length);
	memset((char *)perfmon_base.timeslice_buffer, 0, perfmon_base.timeslice_length);
	
	/* Reset the trace buffer point */
	perfmon_base.trace_end = 0;
	
	for (i=0; i<MAX_PACAS; ++i) {
		for(j=0; j<8; j++) {
			paca[i].pmcc[j] = 0;
		}
	}
#if 0
	/* Reset the timeslice data */
	for(i=0; i<(PMC_MAX_CPUS*PMC_MAX_COUNTERS*PMC_SLICES_MAX); i++) {
		pmc_timeslice_data[i] = 0;
	}
#endif
	/* Restart counters on all processors -- blocking */
	pmc_start(NULL); 
	smp_call_function(pmc_start, (void *)NULL, 0, 1);

	return(0); 
}

void pmc_stop(void *data) 
{
	/* Freeze all counters, leave everything else alone */
	mtspr(MMCR0, mfspr( MMCR0 ) | 0x80000000);
}

void pmc_clear(void *data) 
{
	mtspr(PMC1, 0); mtspr(PMC2, 0);
	mtspr(PMC3, 0); mtspr(PMC4, 0);
	mtspr(PMC5, 0); mtspr(PMC6, 0);
	mtspr(PMC7, 0); mtspr(PMC8, 0);
	mtspr(MMCR0, 0); mtspr(MMCR1, 0); mtspr(MMCRA, 0);
}

void pmc_start(void *data) 
{
	/* Free all counters, leave everything else alone */
	mtspr(MMCR0, mfspr( MMCR0 ) & 0x7fffffff);
}

void pmc_touch_bolted(void *data) 
{
	volatile int touch;

	/* Hack to fault the buffer into the segment table */
	touch = *((int *)(perfmon_base.profile_buffer));
}

int perfmon_dump_ctl(void *data) {
	struct perfmon_struct *pdata;
	int err;

	pdata = kmalloc(sizeof(struct perfmon_struct), GFP_USER);
	err = __copy_from_user(pdata, data, sizeof(struct perfmon_struct));

	switch(pdata->header.subcmd) {
	case PMC_SUBCMD_DUMP_COUNTERS:
		dump_pmc_struct(pdata);
		// copy_to_user(data, pdata, sizeof(struct perfmon_struct));
		break;
	case PMC_SUBCMD_DUMP_HARDWARE:
		dump_hardware_pmc_struct(pdata);
		smp_call_function(dump_hardware_pmc_struct, (void *)pdata, 0, 1);
		break;
	}

	return(0); 
}

void dump_pmc_struct(struct perfmon_struct *perfdata) 
{
	unsigned int cpu = perfdata->vdata.pmc_info.cpu, i;

	if(cpu > MAX_PACAS) return;

	printk("PMC Control Mode: 0x%lx\n", perfmon_base.state);
	printk("PMC[1 - 2] = 0x%16.16lx 0x%16.16lx\n",
	       paca[cpu].pmcc[0], paca[cpu].pmcc[1]);
	printk("PMC[3 - 4] = 0x%16.16lx 0x%16.16lx\n",
	       paca[cpu].pmcc[2], paca[cpu].pmcc[3]);
	printk("PMC[5 - 6] = 0x%16.16lx 0x%16.16lx\n",
	       paca[cpu].pmcc[4], paca[cpu].pmcc[5]);
	printk("PMC[7 - 8] = 0x%16.16lx 0x%16.16lx\n",
	       paca[cpu].pmcc[6], paca[cpu].pmcc[7]);

	perfdata->vdata.pmc_info.mode = perfmon_base.state;
	for(i = 0; i < 11; i++) 
		perfdata->vdata.pmc_info.pmc_base[i]  = paca[cpu].pmc[i];

	for(i = 0; i < 8; i++) 
		perfdata->vdata.pmc_info.pmc_cumulative[i]  = paca[cpu].pmcc[i];
}

void dump_hardware_pmc_struct(void *perfdata) 
{
	unsigned int cpu = smp_processor_id();

	printk("PMC[%2.2d][1 - 4]  = 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x\n",
	       cpu, (u32) mfspr(PMC1),(u32) mfspr(PMC2),(u32) mfspr(PMC3),(u32) mfspr(PMC4));
	printk("PMC[%2.2d][5 - 8]  = 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x\n",
	       cpu, (u32) mfspr(PMC5),(u32) mfspr(PMC6),(u32) mfspr(PMC7),(u32) mfspr(PMC8));
	printk("MMCR[%2.2d][0,1,A] = 0x%8.8x 0x%8.8x 0x%8.8x\n",
	       cpu, (u32) mfspr(MMCR0),(u32) mfspr(MMCR1),(u32) mfspr(MMCRA));
}

int decr_profile(void *data)
{
	int i;

	printk("Perfmon: NIA decrementer profile\n");

	if(perfmon_base.state == PMC_STATE_INITIAL) {
		printk("Perfmon: failed - no buffer was allocated.\n"); 
		return -1;
	}
	
	/* Stop counters on all processors -- blocking */
	pmc_stop(NULL); 
	smp_call_function(pmc_stop, (void *)NULL, 0, 1);
	
	for (i=0; i<MAX_PACAS; ++i) {
		paca[i].prof_mode = PMC_STATE_DECR_PROFILE;
	}
	
	perfmon_base.state = PMC_STATE_DECR_PROFILE; 
	mb(); 

	return 0;
}

int perfmon_profile_ctl(void *data) {
	struct perfmon_struct *pdata;
	int err;

	pdata = kmalloc(sizeof(struct perfmon_struct), GFP_USER);
	err = __copy_from_user(pdata, data, sizeof(struct perfmon_struct));

	switch(pdata->header.subcmd) {
	case PMC_SUBCMD_PROFILE_CYCLE:
		pmc_profile(pdata); 
		break;
	default:
		return(-1);
	}

	return(0); 
}

int perfmon_trace_ctl(void *data) {
	struct perfmon_struct *pdata;
	int err;

	pdata = kmalloc(sizeof(struct perfmon_struct), GFP_USER);
	err = __copy_from_user(pdata, data, sizeof(struct perfmon_struct));

	pmc_set_general(pdata); 

#if 0
        /* Reimplement at some point ... */
	pmc_set_user_general(pdata); 
#endif
	return(0); 
}

int perfmon_timeslice_ctl(void *data) {
	struct perfmon_struct *pdata;
	int err;

	pdata = kmalloc(sizeof(struct perfmon_struct), GFP_USER);
	err = __copy_from_user(pdata, data, sizeof(struct perfmon_struct));

	switch(pdata->header.subcmd) {
	case PMC_SUBCMD_TIMESLICE_ENABLE:
		pmc_timeslice_enable(pdata); 
		break;
	case PMC_SUBCMD_TIMESLICE_DISABLE:
		pmc_timeslice_disable(pdata); 
		break;
	case PMC_SUBCMD_TIMESLICE_SET:
		pmc_timeslice_set(pdata); 
		break;
	default:
		return(-1);
	}

	return(0); 
}

static long plpar_perfmon(int mode)
{
        return plpar_hcall_norets(H_PERFMON, mode, 0); 
}

static void pmc_configure_hardware() {
	/* 
	 * Debug bus enabled is required on GP for timeslice mode.
	 * Flood enabled is required on GP for PMC cycle profile mode
	 *   iSeries SP sets this by default.  pSeries requires the OS to enable.
	 */
	if (cur_cpu_spec->cpu_features & CPU_FTR_SLB) {
		/* Set up the debug bus to pmc mode - a feature of GP */
		switch(systemcfg->platform) {
		case PLATFORM_ISERIES_LPAR:
			HvCall_setDebugBus(1);
			break;
		case PLATFORM_PSERIES_LPAR:
			plpar_perfmon(1);
			break;
		case PLATFORM_PSERIES:
			mtspr(HID0, mfspr(HID0) | 0x0000080000000000);
		} 
	} 
}

/*
 * pmc_profile
 *
 * Profile the kernel based on cycle counts.  This is made a special case of the more 
 * general trace functions because it is high use.  Benefits of special casing this
 * include a buffer of sufficient size to profile the entire kernel is available, and CPI
 * can be collected along with the profile.
 */
int pmc_profile(struct perfmon_struct *perfdata) 
{
	struct pmc_struct *pdata = &(perfdata->vdata.pmc);
	int i;

	printk("Perfmon: NIA PMC profile\n");

	pmc_timeslice_disable(NULL); // fixme

	if(perfmon_base.state == PMC_STATE_INITIAL) {
		printk("Perfmon: failed - no buffer was allocated.\n"); 
		return -1;
	}

	/* Stop counters on all processors -- blocking */
	pmc_stop(NULL); 
	smp_call_function(pmc_stop, (void *)NULL, 0, 1);
	
	for (i=0; i<MAX_PACAS; ++i) {
		paca[i].prof_mode = PMC_STATE_PROFILE_KERN;
	}
	perfmon_base.state = PMC_STATE_PROFILE_KERN; 

	if (cur_cpu_spec->cpu_features & CPU_FTR_SLB) {
		for(i = 0; i < 8; i++) 
			pdata->pmc[i] = 0x0;
		pdata->pmc[1] = 0x7f000000;
		/* Freeze in problem state, exception enable, freeze on event, */
		/*   enable counter negative condition, PMC1, PMC2             */
		pdata->pmc[8] = 0x2600cd0e;
		pdata->pmc[9] = 0x4a5675ac;

		/* Freeze while in wait state */
		pdata->pmc[10] = 0x00022002;
	} else {
		pdata->pmc[0] = 0x7f000000;
		for(i = 1; i < 8; i++) 
			pdata->pmc[i] = 0x0;
		pdata->pmc[8] = 0x26000000 | (0x01 << (31 - 25) | (0x1));
		pdata->pmc[9] = (0x3 << (31-4)); /* Instr completed */

		/* Freeze while in wait state */
		pdata->pmc[10] = 0x00000000 | (0x1 << (31 - 30));
	}

	pmc_configure_hardware();

	mb();

	pmc_trace_rec_type(perfdata->header.vdata.type);
	pmc_configure((void *)perfdata);
	smp_call_function(pmc_configure, (void *)perfdata, 0, 0);

	return 0;
}

int pmc_set_general(struct perfmon_struct *perfdata) 
{
	int i;

	printk("Perfmon: PMC sampling - General\n");

	if(perfmon_base.state == PMC_STATE_INITIAL) {
		printk("Perfmon: failed - no buffer was allocated.\n"); 
		return -1;
	}

	/* Stop counters on all processors -- blocking */
	pmc_stop(NULL); 
	smp_call_function(pmc_stop, (void *)NULL, 0, 1);
	
	for (i=0; i<MAX_PACAS; ++i) {
		paca[i].prof_mode = PMC_STATE_TRACE_KERN;
	}
	perfmon_base.state = PMC_STATE_TRACE_KERN; 
	mb();

	pmc_trace_rec_type(perfdata->header.vdata.type);
	pmc_configure((void *)perfdata);
	smp_call_function(pmc_configure, (void *)perfdata, 0, 0);

	return 0;
}

int pmc_set_user_general(struct perfmon_struct *perfdata) 
{
#if 0
	struct pmc_struct *pdata = &(perfdata->vdata.pmc);
#endif
	int pid = perfdata->header.vdata.pid;
	struct task_struct *task;
	int i;

	printk("Perfmon: PMC sampling - general user\n");

	if(perfmon_base.state == PMC_STATE_INITIAL) {
		printk("Perfmon: failed - no buffer was allocated.\n"); 
		return -1;
	}

	if(pid) {
		printk("Perfmon: pid = 0x%x\n", pid);
		read_lock(&tasklist_lock);
		task = find_task_by_pid(pid);
		if (task) {
			printk("Perfmon: task = 0x%lx\n", (u64) task);
			task->thread.regs->msr |= 0x4;
#if 0
			for(i = 0; i < 11; i++)
				task->thread.pmc[i] = pdata->pmc[i];
#endif
		} else {
			printk("Perfmon: task not found\n");
			read_unlock(&tasklist_lock);
			return -1;
		}
	}
	read_unlock(&tasklist_lock);

	/* Stop counters on all processors -- blocking */
	pmc_stop(NULL); 
	smp_call_function(pmc_stop, (void *)NULL, 0, 1);
	
	for (i=0; i<MAX_PACAS; ++i) {
		paca[i].prof_mode = PMC_STATE_TRACE_USER;
	}
	perfmon_base.state = PMC_STATE_TRACE_USER; 
	mb();

	pmc_trace_rec_type(perfdata->header.vdata.type);
	pmc_configure((void *)perfdata);
	smp_call_function(pmc_configure, (void *)perfdata, 0, 0);

	return 0;
}

void pmc_trace_rec_type(unsigned long type)
{
	unsigned long cmd_rec;

	cmd_rec = 0xFFUL << 56;
	cmd_rec |= type;
	*((unsigned long *)(perfmon_base.trace_buffer + 
			    perfmon_base.trace_end)) = cmd_rec;
	perfmon_base.trace_end += 8;
	if(perfmon_base.trace_end >= perfmon_base.trace_length)
		perfmon_base.trace_end = 0;
}

void pmc_configure(void *data)
{
	struct paca_struct *lpaca = get_paca();
	struct perfmon_struct *perfdata = (struct perfmon_struct *)data;
	struct pmc_struct *pdata = &(perfdata->vdata.pmc);
	unsigned long i;

	/* Indicate to hypervisor that we are using the PMCs */
	if(systemcfg->platform == PLATFORM_ISERIES_LPAR)
		lpaca->xLpPacaPtr->xPMCRegsInUse = 1;

	/* Freeze all counters */
	mtspr(MMCR0, 0x80000000); mtspr(MMCR1, 0x00000000);

	/* Clear all the PMCs */
	mtspr(PMC1, 0); mtspr(PMC2, 0); mtspr(PMC3, 0); mtspr(PMC4, 0); 
	mtspr(PMC5, 0); mtspr(PMC6, 0); mtspr(PMC7, 0); mtspr(PMC8, 0);

	/* Set the PMCs to the new values */
	for(i = 0; i < 11; i++) {
		lpaca->pmc[i]  = pdata->pmc[i];
		printk("pmc_configure: 0x%lx\n", pdata->pmc[i]);
	}

	mtspr(PMC1, lpaca->pmc[0]); mtspr(PMC2, lpaca->pmc[1]);
	mtspr(PMC3, lpaca->pmc[2]); mtspr(PMC4, lpaca->pmc[3]);
	mtspr(PMC5, lpaca->pmc[4]); mtspr(PMC6, lpaca->pmc[5]);
	mtspr(PMC7, lpaca->pmc[6]); mtspr(PMC8, lpaca->pmc[7]);
	mtspr(MMCR1, lpaca->pmc[9]); mtspr(MMCRA, lpaca->pmc[10]);

	mb();
	
	/* Start all counters - MMCR0 contains the PMC enable control bit */
	mtspr(MMCR0, lpaca->pmc[8]);
}

/* Increment the timeslice counters for the current timeslice */ 
void pmc_timeslice_data_collect(void *data) {
	struct paca_struct *lpaca = get_paca();
	int i, cpu = smp_processor_id(); 
	int slice_rec = cpu*PMC_MAX_COUNTERS*PMC_SLICES_MAX +
		        pmc_timeslice[cpu]*PMC_MAX_COUNTERS;

	/* First get any cumulative data that may have accrued */
	for(i=0; i<8; i++) {
		pmc_timeslice_data[slice_rec + i] += lpaca->pmcc[i];
		lpaca->pmcc[i] = 0;
		lpaca->pmc[i]  = 0;
	}

	/* Next get current hardware values */
	pmc_timeslice_data[slice_rec + 0] += ((u32)mfspr(PMC1));
	pmc_timeslice_data[slice_rec + 1] += ((u32)mfspr(PMC2));
	pmc_timeslice_data[slice_rec + 2] += ((u32)mfspr(PMC3));
	pmc_timeslice_data[slice_rec + 3] += ((u32)mfspr(PMC4));
	pmc_timeslice_data[slice_rec + 4] += ((u32)mfspr(PMC5));
	pmc_timeslice_data[slice_rec + 5] += ((u32)mfspr(PMC6));
	pmc_timeslice_data[slice_rec + 6] += ((u32)mfspr(PMC7));
	pmc_timeslice_data[slice_rec + 7] += ((u32)mfspr(PMC8));
}

/* Handle a timeslice tick.  Each processor executes independantly */
int pmc_timeslice_tick() {
	int i;
	struct perfmon_struct perfdata;
	unsigned int cpu = smp_processor_id();

	/* Switch timeslice every decrementer, reduced by some factor */
	pmc_tick_count[cpu]++;
	if(!pmc_timeslice_enabled || pmc_tick_count[cpu] < PMC_TICK_FACTOR) 
		return 0;
	pmc_tick_count[cpu] = 0;

	/* Stop counters and collect current state */
	pmc_stop(NULL); 
	pmc_timeslice_data_collect(NULL); 

	/* Move on to the next timeslice */
	pmc_timeslice[cpu]++; 
	if(pmc_timeslice[cpu] >= pmc_timeslice_top) pmc_timeslice[cpu] = 0;

	/* Set up the counters to reflect the new data to collect */
	for(i = 0; i < 8; i++) {
		perfdata.vdata.pmc.pmc[i] = 0;
	}
	for(i = 0; i < 3; i++) {
		perfdata.vdata.pmc.pmc[i+8] = 
			pmc_timeslice_config[pmc_timeslice[cpu]*5+i];
	}
	mb();

	/* Configure the new counters */
	pmc_configure((void *)&perfdata);

	return 0;
}

int pmc_timeslice_set(struct perfmon_struct *perfdata) 
{
	int slice; 

	printk("Perfmon: Timeslice set\n");

	slice = perfdata->header.vdata.slice;
	if(slice >= PMC_SLICES_MAX) {
		printk("Perfmon: invalid timeslice specified %d\n", slice);
		return(-1); 
	}

	if(slice > pmc_timeslice_top) pmc_timeslice_top = slice;

	pmc_timeslice_config[slice * 5 + 0] = perfdata->vdata.pmc.pmc[0];
	pmc_timeslice_config[slice * 5 + 1] = perfdata->vdata.pmc.pmc[1];
	pmc_timeslice_config[slice * 5 + 2] = perfdata->vdata.pmc.pmc[2];
	pmc_timeslice_config[slice * 5 + 3] = perfdata->vdata.pmc.pmc[3];
	pmc_timeslice_config[slice * 5 + 4] = perfdata->vdata.pmc.pmc[4];
}

int pmc_timeslice_enable(struct perfmon_struct *perfdata) 
{
	int i, j;

	printk("Perfmon: Timeslice mode\n");

	if(perfmon_base.state == PMC_STATE_INITIAL) {
		printk("Perfmon: failed - no buffer was allocated.\n"); 
		return -1;
	}

	pmc_timeslice_disable(NULL); // fixme
	mb();

	for (i=0; i<MAX_PACAS; ++i) {
		paca[i].prof_mode = PMC_STATE_TIMESLICE;
	}
	perfmon_base.state = PMC_STATE_TIMESLICE; 

	pmc_configure_hardware(); 

	for(i=0; i<PMC_MAX_CPUS; i++) {
		pmc_tick_count[i] = 0;
		pmc_timeslice[i]  = 0;
	}

	/* Stop counters on all processors -- blocking */
	pmc_stop(NULL); 
	smp_call_function(pmc_stop, (void *)NULL, 0, 1);

	/* Clear out the PMC counters, cumulative and current */
	for (i=0; i<MAX_PACAS; ++i) {
		paca[i].prof_mode = PMC_STATE_TIMESLICE;
		for(j=0; j<8; j++) {
			paca[i].pmcc[j] = 0;
		}
		for(j=0; j<11; j++) {
			paca[i].pmc[j] = 0;
		}
	}	

	memset((char *)perfmon_base.timeslice_buffer, 
	       0, perfmon_base.timeslice_length);

	pmc_trace_rec_type(PMC_TYPE_TIMESLICE);

	/* Clear counters on all processors -- blocking */
	pmc_clear(NULL); 
	smp_call_function(pmc_clear, (void *)NULL, 0, 1);

	mb();

	pmc_timeslice_enabled  = 1;

	/* 
	 * We do not actually setup the PMC hardware here.  That occurs
	 * after the first timeslice tick occurs. Close enough.
	 */
	return 0;
}

int pmc_timeslice_disable(struct perfmon_struct *perfdata) 
{
	pmc_timeslice_enabled = 0;
}

void pmc_dump_timeslice(struct perfmon_struct *perfdata) 
{
	unsigned long rec, i, j, idx;
	int cpu = smp_processor_id(); 

	spin_lock(&pmc_lock);

	pmc_trace_rec_type(PMC_TYPE_TIMESLICE_DUMP); /* DRENG put cpu num in */
	for(i=0; i<pmc_timeslice_top; i++) {
		for(j=0; j<8; j++) {
			idx = cpu*PMC_MAX_COUNTERS*PMC_SLICES_MAX +
				i*PMC_MAX_COUNTERS + j;
			rec = pmc_timeslice_data[idx];
			*((unsigned long *)(perfmon_base.trace_buffer + 
					    perfmon_base.trace_end)) = rec;
			perfmon_base.trace_end += 8;
			if(perfmon_base.trace_end >= perfmon_base.trace_length)
				perfmon_base.trace_end = 0;
		}
	}
	spin_unlock(&pmc_lock);
}
