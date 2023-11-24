/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#ifndef WITHOUT_CAPSICUM
#include <sys/capsicum.h>
#endif
#include <sys/mman.h>
#ifdef BHYVE_SNAPSHOT
#include <sys/socket.h>
#include <sys/stat.h>
#endif
#include <sys/time.h>
#ifdef BHYVE_SNAPSHOT
#include <sys/un.h>
#endif

#include <machine/atomic.h>

#ifndef WITHOUT_CAPSICUM
#include <capsicum_helpers.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#ifdef BHYVE_SNAPSHOT
#include <fcntl.h>
#endif
#include <libgen.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <pthread_np.h>
#include <sysexits.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef BHYVE_SNAPSHOT
#include <ucl.h>
#include <unistd.h>

#include <libxo/xo.h>
#endif

#include <vmmapi.h>

#include "acpi.h"
#include "bhyverun.h"
#include "bootrom.h"
#include "config.h"
#include "debug.h"
#ifdef BHYVE_GDB
#include "gdb.h"
#endif
#include "mem.h"
#include "mevent.h"
#include "pci_emul.h"
#ifdef __amd64__
#include "amd64/pci_lpc.h"
#endif
#include "qemu_fwcfg.h"
#ifdef BHYVE_SNAPSHOT
#include "snapshot.h"
#endif
#include "tpm_device.h"
#include "vmgenc.h"
#include "vmexit.h"

#define MB		(1024UL * 1024)
#define GB		(1024UL * MB)

int guest_ncpus;
uint16_t cpu_cores, cpu_sockets, cpu_threads;

int raw_stdio = 0;

static char *progname;
static const int BSP = 0;

static cpuset_t cpumask;

static void vm_loop(struct vmctx *ctx, struct vcpu *vcpu);

static struct vcpu_info {
	struct vmctx	*ctx;
	struct vcpu	*vcpu;
	int		vcpuid;
} *vcpu_info;

static cpuset_t **vcpumap;

static void
usage(int code)
{

	fprintf(stderr,
		"Usage: %s [-AaCDeHhPSuWwxY]\n"
		"       %*s [-c [[cpus=]numcpus][,sockets=n][,cores=n][,threads=n]]\n"
		"       %*s [-G port] [-k config_file] [-l lpc] [-m mem] [-o var=value]\n"
		"       %*s [-p vcpu:hostcpu] [-r file] [-s pci] [-U uuid] vmname\n"
		"       -A: create ACPI tables\n"
		"       -a: local apic is in xAPIC mode (deprecated)\n"
		"       -C: include guest memory in core file\n"
		"       -c: number of CPUs and/or topology specification\n"
		"       -D: destroy on power-off\n"
		"       -e: exit on unhandled I/O access\n"
		"       -G: start a debug server\n"
		"       -H: vmexit from the guest on HLT\n"
		"       -h: help\n"
		"       -k: key=value flat config file\n"
		"       -K: PS2 keyboard layout\n"
		"       -l: LPC device configuration\n"
		"       -m: memory size\n"
		"       -o: set config 'var' to 'value'\n"
		"       -P: vmexit from the guest on pause\n"
		"       -p: pin 'vcpu' to 'hostcpu'\n"
#ifdef BHYVE_SNAPSHOT
		"       -r: path to checkpoint file\n"
#endif
		"       -S: guest memory cannot be swapped\n"
		"       -s: <slot,driver,configinfo> PCI slot config\n"
		"       -U: UUID\n"
		"       -u: RTC keeps UTC time\n"
		"       -W: force virtio to use single-vector MSI\n"
		"       -w: ignore unimplemented MSRs\n"
		"       -x: local APIC is in x2APIC mode\n"
		"       -Y: disable MPtable generation\n",
		progname, (int)strlen(progname), "", (int)strlen(progname), "",
		(int)strlen(progname), "");

	exit(code);
}

/*
 * XXX This parser is known to have the following issues:
 * 1.  It accepts null key=value tokens ",," as setting "cpus" to an
 *     empty string.
 *
 * The acceptance of a null specification ('-c ""') is by design to match the
 * manual page syntax specification, this results in a topology of 1 vCPU.
 */
static int
topology_parse(const char *opt)
{
	char *cp, *str, *tofree;

	if (*opt == '\0') {
		set_config_value("sockets", "1");
		set_config_value("cores", "1");
		set_config_value("threads", "1");
		set_config_value("cpus", "1");
		return (0);
	}

	tofree = str = strdup(opt);
	if (str == NULL)
		errx(4, "Failed to allocate memory");

	while ((cp = strsep(&str, ",")) != NULL) {
		if (strncmp(cp, "cpus=", strlen("cpus=")) == 0)
			set_config_value("cpus", cp + strlen("cpus="));
		else if (strncmp(cp, "sockets=", strlen("sockets=")) == 0)
			set_config_value("sockets", cp + strlen("sockets="));
		else if (strncmp(cp, "cores=", strlen("cores=")) == 0)
			set_config_value("cores", cp + strlen("cores="));
		else if (strncmp(cp, "threads=", strlen("threads=")) == 0)
			set_config_value("threads", cp + strlen("threads="));
		else if (strchr(cp, '=') != NULL)
			goto out;
		else
			set_config_value("cpus", cp);
	}
	free(tofree);
	return (0);

out:
	free(tofree);
	return (-1);
}

static int
parse_int_value(const char *key, const char *value, int minval, int maxval)
{
	char *cp;
	long lval;

	errno = 0;
	lval = strtol(value, &cp, 0);
	if (errno != 0 || *cp != '\0' || cp == value || lval < minval ||
	    lval > maxval)
		errx(4, "Invalid value for %s: '%s'", key, value);
	return (lval);
}

/*
 * Set the sockets, cores, threads, and guest_cpus variables based on
 * the configured topology.
 *
 * The limits of UINT16_MAX are due to the types passed to
 * vm_set_topology().  vmm.ko may enforce tighter limits.
 */
static void
calc_topology(void)
{
	const char *value;
	bool explicit_cpus;
	uint64_t ncpus;

	value = get_config_value("cpus");
	if (value != NULL) {
		guest_ncpus = parse_int_value("cpus", value, 1, UINT16_MAX);
		explicit_cpus = true;
	} else {
		guest_ncpus = 1;
		explicit_cpus = false;
	}
	value = get_config_value("cores");
	if (value != NULL)
		cpu_cores = parse_int_value("cores", value, 1, UINT16_MAX);
	else
		cpu_cores = 1;
	value = get_config_value("threads");
	if (value != NULL)
		cpu_threads = parse_int_value("threads", value, 1, UINT16_MAX);
	else
		cpu_threads = 1;
	value = get_config_value("sockets");
	if (value != NULL)
		cpu_sockets = parse_int_value("sockets", value, 1, UINT16_MAX);
	else
		cpu_sockets = guest_ncpus;

	/*
	 * Compute sockets * cores * threads avoiding overflow.  The
	 * range check above insures these are 16 bit values.
	 */
	ncpus = (uint64_t)cpu_sockets * cpu_cores * cpu_threads;
	if (ncpus > UINT16_MAX)
		errx(4, "Computed number of vCPUs too high: %ju",
		    (uintmax_t)ncpus);

	if (explicit_cpus) {
		if (guest_ncpus != (int)ncpus)
			errx(4, "Topology (%d sockets, %d cores, %d threads) "
			    "does not match %d vCPUs",
			    cpu_sockets, cpu_cores, cpu_threads,
			    guest_ncpus);
	} else
		guest_ncpus = ncpus;
}

static int
pincpu_parse(const char *opt)
{
	const char *value;
	char *newval;
	char key[16];
	int vcpu, pcpu;

	if (sscanf(opt, "%d:%d", &vcpu, &pcpu) != 2) {
		fprintf(stderr, "invalid format: %s\n", opt);
		return (-1);
	}

	if (vcpu < 0) {
		fprintf(stderr, "invalid vcpu '%d'\n", vcpu);
		return (-1);
	}

	if (pcpu < 0 || pcpu >= CPU_SETSIZE) {
		fprintf(stderr, "hostcpu '%d' outside valid range from "
		    "0 to %d\n", pcpu, CPU_SETSIZE - 1);
		return (-1);
	}

	snprintf(key, sizeof(key), "vcpu.%d.cpuset", vcpu);
	value = get_config_value(key);

	if (asprintf(&newval, "%s%s%d", value != NULL ? value : "",
	    value != NULL ? "," : "", pcpu) == -1) {
		perror("failed to build new cpuset string");
		return (-1);
	}

	set_config_value(key, newval);
	free(newval);
	return (0);
}

static void
parse_cpuset(int vcpu, const char *list, cpuset_t *set)
{
	char *cp, *token;
	int pcpu, start;

	CPU_ZERO(set);
	start = -1;
	token = __DECONST(char *, list);
	for (;;) {
		pcpu = strtoul(token, &cp, 0);
		if (cp == token)
			errx(4, "invalid cpuset for vcpu %d: '%s'", vcpu, list);
		if (pcpu < 0 || pcpu >= CPU_SETSIZE)
			errx(4, "hostcpu '%d' outside valid range from 0 to %d",
			    pcpu, CPU_SETSIZE - 1);
		switch (*cp) {
		case ',':
		case '\0':
			if (start >= 0) {
				if (start > pcpu)
					errx(4, "Invalid hostcpu range %d-%d",
					    start, pcpu);
				while (start < pcpu) {
					CPU_SET(start, set);
					start++;
				}
				start = -1;
			}
			CPU_SET(pcpu, set);
			break;
		case '-':
			if (start >= 0)
				errx(4, "invalid cpuset for vcpu %d: '%s'",
				    vcpu, list);
			start = pcpu;
			break;
		default:
			errx(4, "invalid cpuset for vcpu %d: '%s'", vcpu, list);
		}
		if (*cp == '\0')
			break;
		token = cp + 1;
	}
}

static void
build_vcpumaps(void)
{
	char key[16];
	const char *value;
	int vcpu;

	vcpumap = calloc(guest_ncpus, sizeof(*vcpumap));
	for (vcpu = 0; vcpu < guest_ncpus; vcpu++) {
		snprintf(key, sizeof(key), "vcpu.%d.cpuset", vcpu);
		value = get_config_value(key);
		if (value == NULL)
			continue;
		vcpumap[vcpu] = malloc(sizeof(cpuset_t));
		if (vcpumap[vcpu] == NULL)
			err(4, "Failed to allocate cpuset for vcpu %d", vcpu);
		parse_cpuset(vcpu, value, vcpumap[vcpu]);
	}
}

void *
paddr_guest2host(struct vmctx *ctx, uintptr_t gaddr, size_t len)
{

	return (vm_map_gpa(ctx, gaddr, len));
}

#ifdef BHYVE_SNAPSHOT
uintptr_t
paddr_host2guest(struct vmctx *ctx, void *addr)
{
	return (vm_rev_map_gpa(ctx, addr));
}
#endif

int
fbsdrun_virtio_msix(void)
{

	return (get_config_bool_default("virtio_msix", true));
}

struct vcpu *
fbsdrun_vcpu(int vcpuid)
{
	return (vcpu_info[vcpuid].vcpu);
}

static void *
fbsdrun_start_thread(void *param)
{
	char tname[MAXCOMLEN + 1];
	struct vcpu_info *vi = param;
	int error;

	snprintf(tname, sizeof(tname), "vcpu %d", vi->vcpuid);
	pthread_set_name_np(pthread_self(), tname);

	if (vcpumap[vi->vcpuid] != NULL) {
		error = pthread_setaffinity_np(pthread_self(),
		    sizeof(cpuset_t), vcpumap[vi->vcpuid]);
		assert(error == 0);
	}

#ifdef BHYVE_SNAPSHOT
	checkpoint_cpu_add(vi->vcpuid);
#endif
#ifdef BHYVE_GDB
	gdb_cpu_add(vi->vcpu);
#endif

	vm_loop(vi->ctx, vi->vcpu);

	/* not reached */
	exit(1);
	return (NULL);
}

void
fbsdrun_addcpu(int vcpuid)
{
	struct vcpu_info *vi;
	pthread_t thr;
	int error;

	vi = &vcpu_info[vcpuid];

	error = vm_activate_cpu(vi->vcpu);
	if (error != 0)
		err(EX_OSERR, "could not activate CPU %d", vi->vcpuid);

	CPU_SET_ATOMIC(vcpuid, &cpumask);

	vm_suspend_cpu(vi->vcpu);

	error = pthread_create(&thr, NULL, fbsdrun_start_thread, vi);
	assert(error == 0);
}

void
fbsdrun_deletecpu(int vcpu)
{
	static pthread_mutex_t resetcpu_mtx = PTHREAD_MUTEX_INITIALIZER;
	static pthread_cond_t resetcpu_cond = PTHREAD_COND_INITIALIZER;

	pthread_mutex_lock(&resetcpu_mtx);
	if (!CPU_ISSET(vcpu, &cpumask)) {
		EPRINTLN("Attempting to delete unknown cpu %d", vcpu);
		exit(4);
	}

	CPU_CLR(vcpu, &cpumask);

	if (vcpu != BSP) {
		pthread_cond_signal(&resetcpu_cond);
		pthread_mutex_unlock(&resetcpu_mtx);
		pthread_exit(NULL);
		/* NOTREACHED */
	}

	while (!CPU_EMPTY(&cpumask)) {
		pthread_cond_wait(&resetcpu_cond, &resetcpu_mtx);
	}
	pthread_mutex_unlock(&resetcpu_mtx);
}

int
fbsdrun_suspendcpu(int vcpuid)
{
	return (vm_suspend_cpu(vcpu_info[vcpuid].vcpu));
}

static void
vm_loop(struct vmctx *ctx, struct vcpu *vcpu)
{
	struct vm_exit vme;
	struct vm_run vmrun;
	int error, rc;
	enum vm_exitcode exitcode;
	cpuset_t active_cpus, dmask;

	error = vm_active_cpus(ctx, &active_cpus);
	assert(CPU_ISSET(vcpu_id(vcpu), &active_cpus));

	vmrun.vm_exit = &vme;
	vmrun.cpuset = &dmask;
	vmrun.cpusetsize = sizeof(dmask);

	while (1) {
		error = vm_run(vcpu, &vmrun);
		if (error != 0)
			break;

		exitcode = vme.exitcode;
		if (exitcode >= VM_EXITCODE_MAX ||
		    vmexit_handlers[exitcode] == NULL) {
			warnx("vm_loop: unexpected exitcode 0x%x", exitcode);
			exit(4);
		}

		rc = (*vmexit_handlers[exitcode])(ctx, vcpu, &vmrun);

		switch (rc) {
		case VMEXIT_CONTINUE:
			break;
		case VMEXIT_ABORT:
			abort();
		default:
			exit(4);
		}
	}
	EPRINTLN("vm_run error %d, errno %d", error, errno);
}

static int
num_vcpus_allowed(struct vmctx *ctx, struct vcpu *vcpu)
{
	uint16_t sockets, cores, threads, maxcpus;
	int tmp, error;

	/*
	 * The guest is allowed to spinup more than one processor only if the
	 * UNRESTRICTED_GUEST capability is available.
	 */
	error = vm_get_capability(vcpu, VM_CAP_UNRESTRICTED_GUEST, &tmp);
	if (error != 0)
		return (1);

	error = vm_get_topology(ctx, &sockets, &cores, &threads, &maxcpus);
	if (error == 0)
		return (maxcpus);
	else
		return (1);
}

static struct vmctx *
do_open(const char *vmname)
{
	struct vmctx *ctx;
	int error;
	bool reinit, romboot;

	reinit = romboot = false;

#ifdef __amd64__
	if (lpc_bootrom())
		romboot = true;
#endif

	error = vm_create(vmname);
	if (error) {
		if (errno == EEXIST) {
			if (romboot) {
				reinit = true;
			} else {
				/*
				 * The virtual machine has been setup by the
				 * userspace bootloader.
				 */
			}
		} else {
			perror("vm_create");
			exit(4);
		}
	} else {
		if (!romboot) {
			/*
			 * If the virtual machine was just created then a
			 * bootrom must be configured to boot it.
			 */
			fprintf(stderr, "virtual machine cannot be booted\n");
			exit(4);
		}
	}

	ctx = vm_open(vmname);
	if (ctx == NULL) {
		perror("vm_open");
		exit(4);
	}

#ifndef WITHOUT_CAPSICUM
	if (vm_limit_rights(ctx) != 0)
		err(EX_OSERR, "vm_limit_rights");
#endif

	if (reinit) {
		error = vm_reinit(ctx);
		if (error) {
			perror("vm_reinit");
			exit(4);
		}
	}
	error = vm_set_topology(ctx, cpu_sockets, cpu_cores, cpu_threads, 0);
	if (error)
		errx(EX_OSERR, "vm_set_topology");
	return (ctx);
}

static bool
parse_config_option(const char *option)
{
	const char *value;
	char *path;

	value = strchr(option, '=');
	if (value == NULL || value[1] == '\0')
		return (false);
	path = strndup(option, value - option);
	if (path == NULL)
		err(4, "Failed to allocate memory");
	set_config_value(path, value + 1);
	return (true);
}

static void
parse_simple_config_file(const char *path)
{
	FILE *fp;
	char *line, *cp;
	size_t linecap;
	unsigned int lineno;

	fp = fopen(path, "r");
	if (fp == NULL)
		err(4, "Failed to open configuration file %s", path);
	line = NULL;
	linecap = 0;
	lineno = 1;
	for (lineno = 1; getline(&line, &linecap, fp) > 0; lineno++) {
		if (*line == '#' || *line == '\n')
			continue;
		cp = strchr(line, '\n');
		if (cp != NULL)
			*cp = '\0';
		if (!parse_config_option(line))
			errx(4, "%s line %u: invalid config option '%s'", path,
			    lineno, line);
	}
	free(line);
	fclose(fp);
}

#ifdef BHYVE_GDB
static void
parse_gdb_options(const char *opt)
{
	const char *sport;
	char *colon;

	if (opt[0] == 'w') {
		set_config_bool("gdb.wait", true);
		opt++;
	}

	colon = strrchr(opt, ':');
	if (colon == NULL) {
		sport = opt;
	} else {
		*colon = '\0';
		colon++;
		sport = colon;
		set_config_value("gdb.address", opt);
	}

	set_config_value("gdb.port", sport);
}
#endif

int
main(int argc, char *argv[])
{
	int c, error;
	int max_vcpus, memflags;
	struct vcpu *bsp;
	struct vmctx *ctx;
	size_t memsize;
	const char *optstr, *value, *vmname;
#ifdef BHYVE_SNAPSHOT
	char *restore_file;
	struct restore_state rstate;

	restore_file = NULL;
#endif

	bhyve_init_config();

	progname = basename(argv[0]);

#ifdef BHYVE_SNAPSHOT
	optstr = "aehuwxACDHIPSWYk:f:o:p:G:c:s:m:l:K:U:r:";
#else
	optstr = "aehuwxACDHIPSWYk:f:o:p:G:c:s:m:l:K:U:";
#endif
	while ((c = getopt(argc, argv, optstr)) != -1) {
		switch (c) {
#ifdef __amd64__
		case 'a':
			set_config_bool("x86.x2apic", false);
			break;
#endif
		case 'A':
			/*
			 * NOP. For backward compatibility. Most systems don't
			 * work properly without sane ACPI tables. Therefore,
			 * we're always generating them.
			 */
			break;
		case 'D':
			set_config_bool("destroy_on_poweroff", true);
			break;
		case 'p':
			if (pincpu_parse(optarg) != 0) {
				errx(EX_USAGE, "invalid vcpu pinning "
				    "configuration '%s'", optarg);
			}
			break;
		case 'c':
			if (topology_parse(optarg) != 0) {
			    errx(EX_USAGE, "invalid cpu topology "
				"'%s'", optarg);
			}
			break;
		case 'C':
			set_config_bool("memory.guest_in_core", true);
			break;
		case 'f':
			if (qemu_fwcfg_parse_cmdline_arg(optarg) != 0) {
			    errx(EX_USAGE, "invalid fwcfg item '%s'", optarg);
			}
			break;
#ifdef BHYVE_GDB
		case 'G':
			parse_gdb_options(optarg);
			break;
#endif
		case 'k':
			parse_simple_config_file(optarg);
			break;
		case 'K':
			set_config_value("keyboard.layout", optarg);
			break;
#ifdef __amd64__
		case 'l':
			if (strncmp(optarg, "help", strlen(optarg)) == 0) {
				lpc_print_supported_devices();
				exit(0);
			} else if (lpc_device_parse(optarg) != 0) {
				errx(EX_USAGE, "invalid lpc device "
				    "configuration '%s'", optarg);
			}
			break;
#endif
#ifdef BHYVE_SNAPSHOT
		case 'r':
			restore_file = optarg;
			break;
#endif
		case 's':
			if (strncmp(optarg, "help", strlen(optarg)) == 0) {
				pci_print_supported_devices();
				exit(0);
			} else if (pci_parse_slot(optarg) != 0)
				exit(4);
			else
				break;
		case 'S':
			set_config_bool("memory.wired", true);
			break;
		case 'm':
			set_config_value("memory.size", optarg);
			break;
		case 'o':
			if (!parse_config_option(optarg))
				errx(EX_USAGE, "invalid configuration option '%s'", optarg);
			break;
#ifdef __amd64__
		case 'H':
			set_config_bool("x86.vmexit_on_hlt", true);
			break;
		case 'I':
			/*
			 * The "-I" option was used to add an ioapic to the
			 * virtual machine.
			 *
			 * An ioapic is now provided unconditionally for each
			 * virtual machine and this option is now deprecated.
			 */
			break;
		case 'P':
			set_config_bool("x86.vmexit_on_pause", true);
			break;
		case 'e':
			set_config_bool("x86.strictio", true);
			break;
		case 'u':
			set_config_bool("rtc.use_localtime", false);
			break;
#endif
		case 'U':
			set_config_value("uuid", optarg);
			break;
#ifdef __amd64__
		case 'w':
			set_config_bool("x86.strictmsr", false);
			break;
#endif
		case 'W':
			set_config_bool("virtio_msix", false);
			break;
#ifdef __amd64__
		case 'x':
			set_config_bool("x86.x2apic", true);
			break;
		case 'Y':
			set_config_bool("x86.mptable", false);
			break;
#endif
		case 'h':
			usage(0);
		default:
			usage(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage(1);

#ifdef BHYVE_SNAPSHOT
	if (restore_file != NULL) {
		error = load_restore_file(restore_file, &rstate);
		if (error) {
			fprintf(stderr, "Failed to read checkpoint info from "
					"file: '%s'.\n", restore_file);
			exit(1);
		}
		vmname = lookup_vmname(&rstate);
		if (vmname != NULL)
			set_config_value("name", vmname);
	}
#endif

	if (argc == 1)
		set_config_value("name", argv[0]);

	vmname = get_config_value("name");
	if (vmname == NULL)
		usage(1);

	if (get_config_bool_default("config.dump", false)) {
		dump_config();
		exit(1);
	}

	calc_topology();
	build_vcpumaps();

	value = get_config_value("memory.size");
	error = vm_parse_memsize(value, &memsize);
	if (error)
		errx(EX_USAGE, "invalid memsize '%s'", value);

	ctx = do_open(vmname);

#ifdef BHYVE_SNAPSHOT
	if (restore_file != NULL) {
		guest_ncpus = lookup_guest_ncpus(&rstate);
		memflags = lookup_memflags(&rstate);
		memsize = lookup_memsize(&rstate);
	}

	if (guest_ncpus < 1) {
		fprintf(stderr, "Invalid guest vCPUs (%d)\n", guest_ncpus);
		exit(1);
	}
#endif

	bsp = vm_vcpu_open(ctx, BSP);
	max_vcpus = num_vcpus_allowed(ctx, bsp);
	if (guest_ncpus > max_vcpus) {
		fprintf(stderr, "%d vCPUs requested but only %d available\n",
			guest_ncpus, max_vcpus);
		exit(4);
	}

	bhyve_init_vcpu(bsp);

	/* Allocate per-VCPU resources. */
	vcpu_info = calloc(guest_ncpus, sizeof(*vcpu_info));
	for (int vcpuid = 0; vcpuid < guest_ncpus; vcpuid++) {
		vcpu_info[vcpuid].ctx = ctx;
		vcpu_info[vcpuid].vcpuid = vcpuid;
		if (vcpuid == BSP)
			vcpu_info[vcpuid].vcpu = bsp;
		else
			vcpu_info[vcpuid].vcpu = vm_vcpu_open(ctx, vcpuid);
	}

	memflags = 0;
	if (get_config_bool_default("memory.wired", false))
		memflags |= VM_MEM_F_WIRED;
	if (get_config_bool_default("memory.guest_in_core", false))
		memflags |= VM_MEM_F_INCORE;
	vm_set_memflags(ctx, memflags);
	error = vm_setup_memory(ctx, memsize, VM_MMAP_ALL);
	if (error) {
		fprintf(stderr, "Unable to setup memory (%d)\n", errno);
		exit(4);
	}

	init_mem(guest_ncpus);
	init_bootrom(ctx);
	if (bhyve_init_platform(ctx, bsp) != 0)
		exit(4);

	if (qemu_fwcfg_init(ctx) != 0) {
		fprintf(stderr, "qemu fwcfg initialization error\n");
		exit(4);
	}

	if (qemu_fwcfg_add_file("opt/bhyve/hw.ncpu", sizeof(guest_ncpus),
	    &guest_ncpus) != 0) {
		fprintf(stderr, "Could not add qemu fwcfg opt/bhyve/hw.ncpu\n");
		exit(4);
	}

	/*
	 * Exit if a device emulation finds an error in its initialization
	 */
	if (init_pci(ctx) != 0) {
		EPRINTLN("Device emulation initialization error: %s",
		    strerror(errno));
		exit(4);
	}
	if (init_tpm(ctx) != 0) {
		EPRINTLN("Failed to init TPM device");
		exit(4);
	}

	/*
	 * Initialize after PCI, to allow a bootrom file to reserve the high
	 * region.
	 */
	if (get_config_bool("acpi_tables"))
		vmgenc_init(ctx);

#ifdef BHYVE_GDB
	init_gdb(ctx);
#endif

	/*
	 * Add all vCPUs.
	 */
	for (int vcpuid = 0; vcpuid < guest_ncpus; vcpuid++)
		bhyve_start_vcpu(vcpu_info[vcpuid].vcpu, vcpuid == BSP);

#ifdef BHYVE_SNAPSHOT
	if (restore_file != NULL) {
		FPRINTLN(stdout, "Pausing pci devs...");
		if (vm_pause_devices() != 0) {
			EPRINTLN("Failed to pause PCI device state.");
			exit(1);
		}

		FPRINTLN(stdout, "Restoring vm mem...");
		if (restore_vm_mem(ctx, &rstate) != 0) {
			EPRINTLN("Failed to restore VM memory.");
			exit(1);
		}

		FPRINTLN(stdout, "Restoring pci devs...");
		if (vm_restore_devices(&rstate) != 0) {
			EPRINTLN("Failed to restore PCI device state.");
			exit(1);
		}

		FPRINTLN(stdout, "Restoring kernel structs...");
		if (vm_restore_kern_structs(ctx, &rstate) != 0) {
			EPRINTLN("Failed to restore kernel structs.");
			exit(1);
		}

		FPRINTLN(stdout, "Resuming pci devs...");
		if (vm_resume_devices() != 0) {
			EPRINTLN("Failed to resume PCI device state.");
			exit(1);
		}
	}
#endif

	if (bhyve_init_platform_late(ctx, bsp) != 0)
		exit(4);

	/*
	 * Change the proc title to include the VM name.
	 */
	setproctitle("%s", vmname);

#ifdef BHYVE_SNAPSHOT
	/*
	 * checkpointing thread for communication with bhyvectl
	 */
	if (init_checkpoint_thread(ctx) != 0)
		errx(EX_OSERR, "Failed to start checkpoint thread");
#endif

#ifndef WITHOUT_CAPSICUM
	caph_cache_catpages();

	if (caph_limit_stdout() == -1 || caph_limit_stderr() == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");

	if (caph_enter() == -1)
		errx(EX_OSERR, "cap_enter() failed");
#endif

#ifdef BHYVE_SNAPSHOT
	if (restore_file != NULL) {
		destroy_restore_state(&rstate);
		if (vm_restore_time(ctx) < 0)
			err(EX_OSERR, "Unable to restore time");

		for (int vcpuid = 0; vcpuid < guest_ncpus; vcpuid++)
			vm_resume_cpu(vcpu_info[vcpuid].vcpu);
	} else
#endif
		vm_resume_cpu(bsp);

	/*
	 * Head off to the main event dispatch loop
	 */
	mevent_dispatch();

	exit(4);
}
