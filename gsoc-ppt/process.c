#include <limits.h>
#include <paths.h>
#include <fcntl.h>
#include <sys/cdefs.h>
#include <unistd.h>
#include <kvm.h>
#include <sys/param.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "/usr/src/lib/libkvm/kvm_private.h"
#include <sys/sysctl.h>
#include <sys/user.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "librapl.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>

#include <sys/cpuctl.h>
#include <sys/ioctl.h>

/*static double 
  proc_calc_pctcpu(struct kinfo_proc *pp)
  {
  const struct kinfo_proc *oldp;

  if (previous_interval != 0) {
  oldp = get_old_proc(pp);
  if (oldp != NULL)
  return ((double)(pp->ki_runtime - oldp->ki_runtime)
  / previous_interval);
  else if (pp->ki_start.tv_sec > previous_wall_time.tv_sec ||
  (pp->ki_start.tv_sec == previous_wall_time.tv_sec &&
  pp->ki_start.tv_usec >= previous_wall_time.tv_usec))
  return ((double)pp->ki_runtime / previous_interval);
  }
  return (pctdouble(pp->ki_pctcpu));
  }*/

typedef struct {
	bool is_Full;
	int tid;
	uint64_t oldruntime;
}thread_info_t;

thread_info_t *find_thread(thread_info_t *thread_info, int num_threads, int tid){
		for (int i = 0; i < num_threads; i++){
			if(thread_info[i].tid == tid){
				return &thread_info[i];
			}
		}
		
		return NULL;
	}

thread_info_t *add_thread(thread_info_t *thread_info, int num_threads, int tid){
		for (int i = 0; i < num_threads; i++){
			if (thread_info[i].is_Full == false){
				thread_info[i].tid = tid;
				thread_info[i].oldruntime = 0;
				thread_info[i].is_Full = true;
				return &thread_info[i];
			}
		}
		printf("The array is full");
		return NULL;
	}

 
#define MSR_RAPL_POWER_UNIT		0x606
#define MSR_PKG_ENERGY_STATUS		0x611
#define MSR_PP0_ENERGY_STATUS		0x639
#define MSR_PP1_ENERGY_STATUS		0x641
#define MSR_DRAM_ENERGY_STATUS		0x619
#define MSR_PLATFORM_ENERGY_STATUS	0x64d
#define MAX_PACKAGES	16
#define MAX_CPUS	1024

#define CPU_SANDYBRIDGE		42
#define CPU_SANDYBRIDGE_EP	45
#define CPU_IVYBRIDGE		58
#define CPU_IVYBRIDGE_EP	62
#define CPU_HASWELL		60
#define CPU_HASWELL_ULT		69
#define CPU_HASWELL_GT3E	70
#define CPU_HASWELL_EP		63
#define CPU_BROADWELL		61
#define CPU_BROADWELL_GT3E	71
#define CPU_BROADWELL_EP	79
#define CPU_BROADWELL_DE	86
#define CPU_SKYLAKE		78
#define CPU_SKYLAKE_HS		94
#define CPU_SKYLAKE_X		85
#define CPU_KNIGHTS_LANDING	87
#define CPU_KNIGHTS_MILL	133
#define CPU_KABYLAKE_MOBILE	142
#define CPU_KABYLAKE		158
#define CPU_ATOM_SILVERMONT	55
#define CPU_ATOM_AIRMONT	76
#define CPU_ATOM_MERRIFIELD	74
#define CPU_ATOM_MOOREFIELD	90
#define CPU_ATOM_GOLDMONT	92
#define CPU_ATOM_GEMINI_LAKE	122
#define CPU_ATOM_DENVERTON	95

static int total_cores=0,total_packages=0;
static int package_map[MAX_PACKAGES];

struct energy_outputs{
	double package_energy;
	double powerplane0;
	double powerplane1;
	double DRAM;
	double PSYS;
};

#define NUM_RAPL_DOMAINS 5

static void do_cpuid(int fd, int level, cpuctl_cpuid_args_t *cpuid_data)
{
	cpuid_data->level = level;
	// Execute cpuid and switch 3,2 so that we get eax,ebx,ecx,edx order
	if (ioctl(fd, CPUCTL_CPUID, cpuid_data) < 0) {
		perror("detect:ioctl");
		exit(1);
	}
}

int open_msr(int core) {

	char msr_filename[BUFSIZ];
	int fd;

	sprintf(msr_filename, "/dev/cpuctl%d", core);
	fd = open(msr_filename, O_RDONLY);
	if ( fd < 0 ) {
		if ( errno == ENXIO ) {
			fprintf(stderr, "cpuctl: No CPU %d\n", core);
			exit(2);
		} else if ( errno == EIO ) {
			fprintf(stderr, "cpuctl: CPU %d doesn't support MSRs\n",
					core);
			exit(3);
		} else {
			perror("cpuctl:open");
			fprintf(stderr,"Trying to open %s\n",msr_filename);
			exit(127);
		}
	}

	return fd;
}

char rapl_domain_names[NUM_RAPL_DOMAINS][30];

int detect_cpu(void){
	
	int fd = open_msr(0);
	cpuctl_cpuid_args_t cpuid_data;

	do_cpuid(fd, 0, &cpuid_data);
	uint32_t tmp = cpuid_data.data[2];
	cpuid_data.data[2] = cpuid_data.data[3];
	cpuid_data.data[3] = tmp;
	char *vendor = (char *)(&cpuid_data.data[1]);
	if (strncmp(vendor, "GenuineIntel", 12)) {
		printf("%.12s not an Intel chip\n", vendor);
		return -1;
	}

	do_cpuid(fd, 1, &cpuid_data);
	uint8_t family = (cpuid_data.data[0] >> 8) & 0xF;
	if (family != 6) {
		printf("Wrong CPU family %d\n",family);
		return -1;
	}

	// If familiy is 6 (it is) or 15, we need to combine model id (bit4..7)
	// and extended model (bit16..19)
	int model = ((cpuid_data.data[0] >> 12) & 0xF0) |
		 ((cpuid_data.data[0] >> 4) & 0x0F);

	printf("Found ");

	switch(model) {
		case CPU_SANDYBRIDGE:
			printf("Sandybridge");
			break;
		case CPU_SANDYBRIDGE_EP:
			printf("Sandybridge-EP");
			break;
		case CPU_IVYBRIDGE:
			printf("Ivybridge");
			break;
		case CPU_IVYBRIDGE_EP:
			printf("Ivybridge-EP");
			break;
		case CPU_HASWELL:
		case CPU_HASWELL_ULT:
		case CPU_HASWELL_GT3E:
			printf("Haswell");
			break;
		case CPU_HASWELL_EP:
			printf("Haswell-EP");
			break;
		case CPU_BROADWELL:
		case CPU_BROADWELL_GT3E:
			printf("Broadwell");
			break;
		case CPU_BROADWELL_EP:
			printf("Broadwell-EP");
			break;
		case CPU_SKYLAKE:
		case CPU_SKYLAKE_HS:
			printf("Skylake");
			break;
		case CPU_SKYLAKE_X:
			printf("Skylake-X");
			break;
		case CPU_KABYLAKE:
		case CPU_KABYLAKE_MOBILE:
			printf("Kaby Lake");
			break;
		case CPU_KNIGHTS_LANDING:
			printf("Knight's Landing");
			break;
		case CPU_KNIGHTS_MILL:
			printf("Knight's Mill");
			break;
		case CPU_ATOM_GOLDMONT:
		case CPU_ATOM_GEMINI_LAKE:
		case CPU_ATOM_DENVERTON:
			printf("Atom");
			break;
		default:
			printf("Unsupported model %d\n",model);
			model=-1;
			break;
	}

	printf(" Processor type\n");
	close(fd);

	return model;
}

int detect_packages(void) {

	char filename[BUFSIZ];
	FILE *fff;
	int package;
	int i;

	for(i=0;i<MAX_PACKAGES;i++) package_map[i]=-1;
	printf("\t");
	for (i=0;i<MAX_CPUS;i++) {
		sprintf(filename, "/dev/cpuctl%d", i);
		fff = fopen(filename, "r");
		if (fff==NULL) break;

		/* FIXME: Missing detection which package this core belongs to */
		package = 0;

		printf("%d (%d)",i,package);
		if (i%8==7) printf("\n\t"); else printf(", ");
		fclose(fff);

		if (package_map[package]==-1) {
			total_packages++;
			package_map[package]=i;
		}

	}

	printf("\n");

	total_cores=i;

	printf("\tDetected %d cores in %d packages\n\n",
		total_cores,total_packages);

	return 0;
}

uint64_t read_msr(int fd, int which) {

	cpuctl_msr_args_t data = {.msr = which, 0};

	if ( ioctl(fd, CPUCTL_RDMSR, &data) < 0) {
		perror("rdmsr:ioctl");
		exit(127);
	}

	return data.data;
}

int rapl_msr(int core, int cpu_model, int delay, struct energy_outputs *eo){
	int fd;
	long long result;
	double power_units,time_units;
	double cpu_energy_units[MAX_PACKAGES],dram_energy_units[MAX_PACKAGES];
	double package_before[MAX_PACKAGES],package_after[MAX_PACKAGES];
	double pp0_before[MAX_PACKAGES],pp0_after[MAX_PACKAGES];
	double pp1_before[MAX_PACKAGES],pp1_after[MAX_PACKAGES];
	double dram_before[MAX_PACKAGES],dram_after[MAX_PACKAGES];
	double psys_before[MAX_PACKAGES],psys_after[MAX_PACKAGES];
	double thermal_spec_power,minimum_power,maximum_power,time_window;
	int j;


	int dram_avail=0,pp0_avail=0,pp1_avail=0,psys_avail=0;
	int different_units=0;

	if (cpu_model<0) {
		printf("\tUnsupported CPU model %d\n",cpu_model);
		return -1;
	}
	
	switch(cpu_model) {

		case CPU_SANDYBRIDGE_EP:
		case CPU_IVYBRIDGE_EP:
			pp0_avail=1;
			pp1_avail=0;
			dram_avail=1;
			different_units=0;
			psys_avail=0;
			break;

		case CPU_HASWELL_EP:
		case CPU_BROADWELL_EP:
		case CPU_SKYLAKE_X:
			pp0_avail=1;
			pp1_avail=0;
			dram_avail=1;
			different_units=1;
			psys_avail=0;
			break;

		case CPU_KNIGHTS_LANDING:
		case CPU_KNIGHTS_MILL:
			pp0_avail=0;
			pp1_avail=0;
			dram_avail=1;
			different_units=1;
			psys_avail=0;
			break;

		case CPU_SANDYBRIDGE:
		case CPU_IVYBRIDGE:
			pp0_avail=1;
			pp1_avail=1;
			dram_avail=0;
			different_units=0;
			psys_avail=0;
			break;

		case CPU_HASWELL:
		case CPU_HASWELL_ULT:
		case CPU_HASWELL_GT3E:
		case CPU_BROADWELL:
		case CPU_BROADWELL_GT3E:
		case CPU_ATOM_GOLDMONT:
		case CPU_ATOM_GEMINI_LAKE:
		case CPU_ATOM_DENVERTON:
			pp0_avail=1;
			pp1_avail=1;
			dram_avail=1;
			different_units=0;
			psys_avail=0;
			break;

		case CPU_SKYLAKE:
		case CPU_SKYLAKE_HS:
		case CPU_KABYLAKE:
		case CPU_KABYLAKE_MOBILE:
			pp0_avail=1;
			pp1_avail=1;
			dram_avail=1;
			different_units=0;
			psys_avail=1;
			break;

	}
	
	for(j=0;j<total_packages;j++) {

		fd=open_msr(package_map[j]);

		/* Calculate the units used */
		result=read_msr(fd,MSR_RAPL_POWER_UNIT);

		power_units=pow(0.5,(double)(result&0xf));
		cpu_energy_units[j]=pow(0.5,(double)((result>>8)&0x1f));
		time_units=pow(0.5,(double)((result>>16)&0xf));

		/* On Haswell EP and Knights Landing */
		/* The DRAM units differ from the CPU ones */
		if (different_units) {
			dram_energy_units[j]=pow(0.5,(double)16);
		}
		else {
			dram_energy_units[j]=cpu_energy_units[j];
		}
		
		/* Package Energy */
		result=read_msr(fd,MSR_PKG_ENERGY_STATUS);
		package_before[j]=(double)result*cpu_energy_units[j]; 
		eo->package_energy = package_before[j];

		/* PP0 energy */
		/* Not available on Knights* */
		/* Always returns zero on Haswell-EP? */
		if (pp0_avail) {
			result=read_msr(fd,MSR_PP0_ENERGY_STATUS);
			pp0_before[j]=(double)result*cpu_energy_units[j];
		eo->powerplane0 = pp0_before[j];
		}

		/* PP1 energy */
		/* not available on *Bridge-EP */
		if (pp1_avail) {
	 		result=read_msr(fd,MSR_PP1_ENERGY_STATUS);
			pp1_before[j]=(double)result*cpu_energy_units[j];
		eo->powerplane1 = pp1_before[j];
		}


		/* Updated documentation (but not the Vol3B) says Haswell and	*/
		/* Broadwell have DRAM support too				*/
		if (dram_avail) {
			result=read_msr(fd,MSR_DRAM_ENERGY_STATUS);
			dram_before[j]=(double)result*dram_energy_units[j];
		eo->DRAM = dram_before[j];
		}


		/* Skylake and newer for Psys				*/
		if ((cpu_model==CPU_SKYLAKE) ||
			(cpu_model==CPU_SKYLAKE_HS) ||
			(cpu_model==CPU_KABYLAKE) ||
			(cpu_model==CPU_KABYLAKE_MOBILE)) {

			result=read_msr(fd,MSR_PLATFORM_ENERGY_STATUS);
			psys_before[j]=(double)result*cpu_energy_units[j];
			eo->PSYS = psys_before[j];
		}

		close(fd);
	}

	return 0;
}

int main(){
	kvm_t *kd;
	struct kinfo_proc *procs;
	struct timespec uptime;
	int num_procs= -1;
	char errbuf[_POSIX2_LINE_MAX];
	struct timeval timeout = {1, 0};
	int tid;
	thread_info_t *t_info;
	int ncpu = sysconf(_SC_NPROCESSORS_ONLN);	
	int num_threads = 19299;
	
	// variables used by rapl_msr()
	int core=0;
	int delay=1;
	int result=-1;
	int cpu_model;
	struct energy_outputs *eo = (struct energy_outputs *)malloc(sizeof(struct energy_outputs));
	
	thread_info_t *thread_info_list = calloc(num_threads, sizeof(thread_info_t));
	
	//calling rapl functions here
	cpu_model=detect_cpu();
	detect_packages();
	//static_output_rapl(core, cpu_model);
	printf("Energy values: %lf\t%lf\t%lf\t%lf\t%lf\n", eo->package_energy, eo->powerplane0, eo->powerplane1, eo->DRAM, eo->PSYS);
	//end of rapl functions
	
	kd = kvm_openfiles(NULL, _PATH_DEVNULL, NULL, O_RDONLY, errbuf);
	for (int j = 0; j<50; j++){
		struct kinfo_proc *oldprocs;
		procs = kvm_getprocs(kd, KERN_PROC_ALL, 0, &num_procs);

		float prev_psys = eo->PSYS;
		rapl_msr(core,cpu_model,delay,eo);
		float curr_psys = eo->PSYS;
		float psys_delta = curr_psys - prev_psys;

		for (int i = 0; i < num_procs; i++){
			if (strcmp(procs[i].ki_comm, "idle") == 0){
				continue;
			}
			tid = procs[i].ki_tid;
			t_info = find_thread(thread_info_list, num_threads, tid);
			
			if (t_info == NULL){
				t_info = add_thread(thread_info_list, num_threads, tid);
				assert(t_info != NULL);					
			}

			uint64_t newruntime = procs[i].ki_runtime;
			uint64_t oldruntime = t_info->oldruntime;

			t_info->oldruntime = newruntime;
			clock_gettime(CLOCK_UPTIME, &uptime);
			float perunit = (((float)(newruntime - oldruntime))/(1000000 * 1));

			if (perunit > 1){
				perunit = 1;
			}
			float per_process_energy_usage = (perunit * psys_delta)/ncpu; 
			if (perunit > 0.05){
				printf(" %d\t%d\t%s\t%lu\t%f\t,%f J/s\n", tid, procs[i].ki_pid, procs[i].ki_comm, procs[i].ki_runtime, perunit, per_process_energy_usage);
			}
		} 
		printf("---------\n");
		select(0, NULL, NULL, NULL, &timeout);
	}
}




