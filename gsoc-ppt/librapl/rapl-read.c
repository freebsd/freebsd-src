/* Read the RAPL registers on recent (>sandybridge) Intel processors	*/
/*									*/
/* MSR Code originally based on a (never made it upstream) linux-kernel	*/
/*	RAPL driver by Zhang Rui <rui.zhang@intel.com>			*/
/*	https://lkml.org/lkml/2011/5/26/93				*/
/* Additional contributions by:						*/
/*	Romain Dolbeau -- romain @ dolbeau.org				*/
/*									*/
/* Compile with:   gcc -O2 -Wall -o rapl-read rapl-read.c -lm		*/
/*									*/
/* Vince Weaver -- vincent.weaver @ maine.edu -- 11 September 2015	*/
/*									*/
/* Ported to FreeBSD (MSR only) by sven.koehler @ hpi de -- May 2022    */
/*									*/
/* For raw MSR access the /dev/cpuctl?? driver must be enabled and	*/
/*	permissions set to allow read access.				*/
/*	You might need to "kldload cpuctl" before it will work.		*/
/*									*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <sys/cpuctl.h>
#include <sys/ioctl.h>

#include <librapl.h>

#define MSR_RAPL_POWER_UNIT		0x606

/*
 * Platform specific RAPL Domains.
 * Note that PP1 RAPL Domain is supported on 062A only
 * And DRAM RAPL Domain is supported on 062D only
 */
/* Package RAPL Domain */
#define MSR_PKG_RAPL_POWER_LIMIT	0x610
#define MSR_PKG_ENERGY_STATUS		0x611
#define MSR_PKG_PERF_STATUS		0x613
#define MSR_PKG_POWER_INFO		0x614

/* PP0 RAPL Domain */
#define MSR_PP0_POWER_LIMIT		0x638
#define MSR_PP0_ENERGY_STATUS		0x639
#define MSR_PP0_POLICY			0x63A
#define MSR_PP0_PERF_STATUS		0x63B

/* PP1 RAPL Domain, may reflect to uncore devices */
#define MSR_PP1_POWER_LIMIT		0x640
#define MSR_PP1_ENERGY_STATUS		0x641
#define MSR_PP1_POLICY			0x642

/* DRAM RAPL Domain */
#define MSR_DRAM_POWER_LIMIT		0x618
#define MSR_DRAM_ENERGY_STATUS		0x619
#define MSR_DRAM_PERF_STATUS		0x61B
#define MSR_DRAM_POWER_INFO		0x61C

/* PSYS RAPL Domain */
#define MSR_PLATFORM_ENERGY_STATUS	0x64d

/* RAPL UNIT BITMASK */
#define POWER_UNIT_OFFSET	0
#define POWER_UNIT_MASK		0x0F

#define ENERGY_UNIT_OFFSET	0x08
#define ENERGY_UNIT_MASK	0x1F00

#define TIME_UNIT_OFFSET	0x10
#define TIME_UNIT_MASK		0xF000

static int open_msr(int core) {

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

static uint64_t read_msr(int fd, int which) {

	cpuctl_msr_args_t data = {.msr = which, 0};

	if ( ioctl(fd, CPUCTL_RDMSR, &data) < 0) {
		perror("rdmsr:ioctl");
		exit(127);
	}

	return data.data;
}

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

/* TODO: on Skylake, also may support  PSys "platform" domain,	*/
/* the whole SoC not just the package.				*/
/* see dcee75b3b7f025cc6765e6c92ba0a4e59a4d25f4			*/

static void do_cpuid(int fd, int level, cpuctl_cpuid_args_t *cpuid_data)
{
	cpuid_data->level = level;
	// Execute cpuid and switch 3,2 so that we get eax,ebx,ecx,edx order
	if (ioctl(fd, CPUCTL_CPUID, cpuid_data) < 0) {
		perror("detect:ioctl");
		exit(1);
	}
}

int detect_cpu(void)
{
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

#define MAX_CPUS	1024
#define MAX_PACKAGES	16

static int total_cores=0,total_packages=0;
static int package_map[MAX_PACKAGES];

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


/*******************************/
/* MSR code                    */
/*******************************/
int rapl_msr(int core, int cpu_model, int delay) {

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

	typedef struct {
		double package_energy;
		double powerplane0;
		double powerplane1;
		double DRAM;
		double PSYS;
	} outputs;

	outputs energy_outputs;

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

		/* PP0 energy */
		/* Not available on Knights* */
		/* Always returns zero on Haswell-EP? */
		if (pp0_avail) {
			result=read_msr(fd,MSR_PP0_ENERGY_STATUS);
			pp0_before[j]=(double)result*cpu_energy_units[j];
		}

		/* PP1 energy */
		/* not available on *Bridge-EP */
		if (pp1_avail) {
	 		result=read_msr(fd,MSR_PP1_ENERGY_STATUS);
			pp1_before[j]=(double)result*cpu_energy_units[j];
		}


		/* Updated documentation (but not the Vol3B) says Haswell and	*/
		/* Broadwell have DRAM support too				*/
		if (dram_avail) {
			result=read_msr(fd,MSR_DRAM_ENERGY_STATUS);
			dram_before[j]=(double)result*dram_energy_units[j];
		}


		/* Skylake and newer for Psys				*/
		if ((cpu_model==CPU_SKYLAKE) ||
			(cpu_model==CPU_SKYLAKE_HS) ||
			(cpu_model==CPU_KABYLAKE) ||
			(cpu_model==CPU_KABYLAKE_MOBILE)) {

			result=read_msr(fd,MSR_PLATFORM_ENERGY_STATUS);
			psys_before[j]=(double)result*cpu_energy_units[j];
		}

		close(fd);
	}

  	printf("\n\tSleeping %d second\n\n", delay);
	sleep(delay);

	for(j=0;j<total_packages;j++) {

		fd=open_msr(package_map[j]);

		printf("\tPackage %d:\n",j);

		result=read_msr(fd,MSR_PKG_ENERGY_STATUS);
		package_after[j]=(double)result*cpu_energy_units[j];
		printf("\t\tPackage energy: %.6fJ\n",
			package_after[j]-package_before[j]);
		energy_outputs.package_energy = package_after[j]-package_before[j];

		result=read_msr(fd,MSR_PP0_ENERGY_STATUS);
		pp0_after[j]=(double)result*cpu_energy_units[j];
		printf("\t\tPowerPlane0 (cores): %.6fJ\n",
			pp0_after[j]-pp0_before[j]);
		energy_outputs.powerplane0 = pp0_after[j]-pp0_before[j];

		/* not available on SandyBridge-EP */
		if (pp1_avail) {
			result=read_msr(fd,MSR_PP1_ENERGY_STATUS);
			pp1_after[j]=(double)result*cpu_energy_units[j];
			printf("\t\tPowerPlane1 (on-core GPU if avail): %.6f J\n",
				pp1_after[j]-pp1_before[j]);
			energy_outputs.powerplane1 = pp1_after[j]-pp1_before[j];
		}

		if (dram_avail) {
			result=read_msr(fd,MSR_DRAM_ENERGY_STATUS);
			dram_after[j]=(double)result*dram_energy_units[j];
			printf("\t\tDRAM: %.6fJ\n",
				dram_after[j]-dram_before[j]);
			energy_outputs.DRAM = dram_after[j]-dram_before[j];
		}

		if (psys_avail) {
			result=read_msr(fd,MSR_PLATFORM_ENERGY_STATUS);
			psys_after[j]=(double)result*cpu_energy_units[j];
			printf("\t\tPSYS: %.6fJ\n",
				psys_after[j]-psys_before[j]);
			energy_outputs.PSYS = psys_after[j]-psys_before[j];
		}

		close(fd);
	}
	printf("\n");
	printf("Note: the energy measurements can overflow in 60s or so\n");
	printf("      so try to sample the counters more often than that.\n\n");

	return 0;
}

int static_output_rapl(int core, int cpu_model) {

	int fd;
	long long result;
	double power_units,time_units;
	double cpu_energy_units[MAX_PACKAGES],dram_energy_units[MAX_PACKAGES];
	double thermal_spec_power,minimum_power,maximum_power,time_window;
	int j;

	int dram_avail=0,pp0_avail=0,pp1_avail=0,psys_avail=0;
	int different_units=0;

	printf("\nTrying /dev/cpuctl interface to gather results\n\n");
	
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
		printf("\tListing parameters for package #%d\n",j);

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
			printf("DRAM: Using %lf instead of %lf\n",
				dram_energy_units[j],cpu_energy_units[j]);
		}
		else {
			dram_energy_units[j]=cpu_energy_units[j];
		}

		printf("\t\tPower units = %.3fW\n",power_units);
		printf("\t\tCPU Energy units = %.8fJ\n",cpu_energy_units[j]);
		printf("\t\tDRAM Energy units = %.8fJ\n",dram_energy_units[j]);
		printf("\t\tTime units = %.8fs\n",time_units);
		printf("\n");

		/* Show package power info */
		result=read_msr(fd,MSR_PKG_POWER_INFO);
		thermal_spec_power=power_units*(double)(result&0x7fff);
		printf("\t\tPackage thermal spec: %.3fW\n",thermal_spec_power);
		minimum_power=power_units*(double)((result>>16)&0x7fff);
		printf("\t\tPackage minimum power: %.3fW\n",minimum_power);
		maximum_power=power_units*(double)((result>>32)&0x7fff);
		printf("\t\tPackage maximum power: %.3fW\n",maximum_power);
		time_window=time_units*(double)((result>>48)&0x7fff);
		printf("\t\tPackage maximum time window: %.6fs\n",time_window);

		/* Show package power limit */
		result=read_msr(fd,MSR_PKG_RAPL_POWER_LIMIT);
		printf("\t\tPackage power limits are %s\n", (result >> 63) ? "locked" : "unlocked");
		double pkg_power_limit_1 = power_units*(double)((result>>0)&0x7FFF);
		double pkg_time_window_1 = time_units*(double)((result>>17)&0x007F);
		printf("\t\tPackage power limit #1: %.3fW for %.6fs (%s, %s)\n",
			pkg_power_limit_1, pkg_time_window_1,
			(result & (1LL<<15)) ? "enabled" : "disabled",
			(result & (1LL<<16)) ? "clamped" : "not_clamped");
		double pkg_power_limit_2 = power_units*(double)((result>>32)&0x7FFF);
		double pkg_time_window_2 = time_units*(double)((result>>49)&0x007F);
		printf("\t\tPackage power limit #2: %.3fW for %.6fs (%s, %s)\n", 
			pkg_power_limit_2, pkg_time_window_2,
			(result & (1LL<<47)) ? "enabled" : "disabled",
			(result & (1LL<<48)) ? "clamped" : "not_clamped");

		/* only available on *Bridge-EP */
		if ((cpu_model==CPU_SANDYBRIDGE_EP) || (cpu_model==CPU_IVYBRIDGE_EP)) {
			result=read_msr(fd,MSR_PKG_PERF_STATUS);
			double acc_pkg_throttled_time=(double)result*time_units;
			printf("\tAccumulated Package Throttled Time : %.6fs\n",
				acc_pkg_throttled_time);
		}

		/* only available on *Bridge-EP */
		if ((cpu_model==CPU_SANDYBRIDGE_EP) || (cpu_model==CPU_IVYBRIDGE_EP)) {
			result=read_msr(fd,MSR_PP0_PERF_STATUS);
			double acc_pp0_throttled_time=(double)result*time_units;
			printf("\tPowerPlane0 (core) Accumulated Throttled Time "
				": %.6fs\n",acc_pp0_throttled_time);

			result=read_msr(fd,MSR_PP0_POLICY);
			int pp0_policy=(int)result&0x001f;
			printf("\tPowerPlane0 (core) for core %d policy: %d\n",core,pp0_policy);

		}


		if (pp1_avail) {
			result=read_msr(fd,MSR_PP1_POLICY);
			int pp1_policy=(int)result&0x001f;
			printf("\tPowerPlane1 (on-core GPU if avail) %d policy: %d\n",
				core,pp1_policy);
		}
		close(fd);

	}
	printf("\n");

	return 0;
}


#define NUM_RAPL_DOMAINS	5

char rapl_domain_names[NUM_RAPL_DOMAINS][30]= {
	"energy-cores",
	"energy-gpu",
	"energy-pkg",
	"energy-ram",
	"energy-psys",
};

int main(int argc, char **argv) {

	int c;
	int core=0;
	int delay=1;
	int result=-1;
	int cpu_model;

	printf("\n");
	printf("RAPL read\n\n");

	opterr=0;

	while ((c = getopt (argc, argv, "c:hmps:")) != -1) {
		switch (c) {
		case 'c':
			core = atoi(optarg);
			break;
		case 'h':
			printf("Usage: %s [-c core] [-h] [-m]\n\n",argv[0]);
			printf("\t-c core : specifies which core to measure\n");
			printf("\t-h      : displays this help\n");
			exit(0);
		case 's':
			delay = atoi(optarg);
			break;
		default:
			fprintf(stderr,"Unknown option %c\n",c);
			exit(-1);
		}
	}

	cpu_model=detect_cpu();
	detect_packages();
	//static_output_rapl(core, cpu_model);

	result=rapl_msr(core,cpu_model,delay);

	if (result<0) {

		printf("Unable to read RAPL counters.\n");
		printf("* Verify you have an Intel Sandybridge or newer processor\n");
		printf("* You may need to run as root or have /proc/sys/kernel/perf_event_paranoid set properly\n");
		printf("* If using raw msr access, make sure msr module is installed\n");
		printf("\n");

		return -1;

	}

	return 0;
}
