/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Netflix, Inc.
 *
 * This software was developed by Ali Mashtizadeh under the sponsorship from
 * Netflix, Inc.
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
 */

#include <sys/param.h>
#include <sys/cdefs.h>
#include <sys/cpuset.h>
#include <sys/event.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/ttycom.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <assert.h>
#include <curses.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <kvm.h>
#include <libgen.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <pmc.h>
#include <pmclog.h>
#include <regex.h>
#include <signal.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <machine/cpufunc.h>

#include <libpmcstat.h>
#include "cmd_pmc.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "display.hh"
#include "headers.hh"

#define DEFAULT_RATE 65536

class pmc_config
{
public:
	std::string	event;
	uint64_t	count;
	cpuset_t	cpumask;
	uint32_t	caps;
	pmc_config() : event(), count(0), ids()
	{
		CPU_ZERO(&cpumask);
	}
	pmc_config(const std::string &event, uint64_t count, cpuset_t cpumask)
	    : event(event), count(count), ids()
	{
		CPU_COPY(&cpumask, &this->cpumask);
	}
	/*
	 * Read the capabilities and fills in the cpumask for a given event 
	 * counter.  The way that the hwpmc module currently works we do not 
	 * know which pmc class will back our counter until we look it up.  The 
	 * easiest way is to program the counter into the first CPU present in 
	 * the mask and then retrieve the capabilities.
	 */
	void getcaps() {
		int status;
		int testcpu;
		pmc_id_t id;

		testcpu = CPU_FFS(&cpumask) - 1;

		status = pmc_allocate(event.c_str(), PMC_MODE_SS, PMC_F_CALLCHAIN,
		    testcpu, &id, count);
		if (status < 0)
			err(EX_OSERR, "ERROR: Cannot allocate event '%s'", event.c_str());

		status = pmc_capabilities(id, &caps);
		if (status < 0)
			err(EX_OSERR, "ERROR: Cannot get pmc capabilities");

		pmc_release(id);

		if (caps & PMC_CAP_SYSWIDE) {
			CPU_ZERO(&cpumask);
			CPU_SET(0, &cpumask);
		}
		if (caps & PMC_CAP_DOMWIDE) {
			int domains;
			size_t len;

			CPU_ZERO(&cpumask);

			len = sizeof(domains);
			if (sysctlbyname("vm.ndomains", &domains, &len, NULL, 0) == -1)
				err(EX_OSERR, "ERROR: Cannot get number of domains");

			for (int i = 1; i < domains; i++) {
				cpuset_t dmask;

				CPU_ZERO(&dmask);
				status = cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_DOMAIN, i,
				    sizeof(dmask), &dmask);
				if (status < 0)
					err(EX_OSERR, "ERROR: Cannot get domain mask");
				CPU_SET(CPU_FFS(&dmask) - 1, &cpumask);
			}
		}
	}
	/*
	 * Allocate the PMC across all CPUs.
	 */
	void allocate() {
		int cpu;
		int status;
		pmc_id_t id;

		for (cpu = 0; cpu < CPU_SETSIZE; cpu++) {
			if (!CPU_ISSET(cpu, &cpumask))
				continue;

			status = pmc_allocate(event.c_str(), PMC_MODE_SS, PMC_F_CALLCHAIN,
			    cpu, &id, count);
			if (status < 0)
				err(EX_OSERR, "ERROR: Cannot allocate event '%s'",
				    event.c_str());

			status = pmc_set(id, count);
			if (status < 0)
				err(EX_OSERR, "ERROR: Cannot set sampling count for event '%s'",
				    event.c_str());

			ids.push_back(id);
		}
	}
	/*
	 * Release all PMC instances (one per CPU).
	 */
	void release() {
		for (auto i : ids) {
			if (pmc_release(i) < 0) {
				perror("pmc_start");
			}
		}
	}
	/*
	 * Start all PMC instances (one per CPU).
	 */
	void start() {
		for (auto i : ids) {
			if (pmc_start(i) < 0) {
				perror("pmc_start");
			}
		}
	}
	/*
	 * Stop all PMC instances (one per CPU).
	 */
	void stop() {
		for (auto i : ids) {
			if (pmc_stop(i) < 0) {
				perror("pmc_start");
			}
		}
	}
private:
	std::vector<pmc_id_t> ids;
};

static std::vector<pmc_config> pmcs = std::vector<pmc_config>();
static struct option longopts[] = {
	{ "rate",	required_argument,	NULL,	'n' },
	{ "counter",	required_argument,	NULL,	'c' },
	{ "time",	required_argument,	NULL,	't' },
	{ "study",	required_argument,	NULL,	's' },
	{ NULL,		0,			NULL,	0 }
};
static struct timespec start;
static int timelimit = 0;

static void
usage(void)
{
	printf("Usage: pmc record [options] [output.pmc]\n\n");
	printf("Record a study\n\n");
	printf("Options:\n");
	printf("\t-c              Sampling counter\n");
	printf("\t-r              Sample rate (default: %d)\n", DEFAULT_RATE);
	printf("\t-s              Specify a study\n");
	printf("\t-t              Run the study for specified amount of time\n");
	printf("\nStudies:\n");
#if defined(__i386__) || defined(__amd64__)
	printf("\tbranches        Study branch misprediction (Requires AMD IBS)\n");
	printf("\tc2c             Study cache to cache communications (Requires AMD IBS)\n");
#endif
	printf("\tdefault         Instruction sampling for general analysis\n");
	printf("\tflamegraph      Instruction sampling for general analysis\n");
#if defined(__i386__) || defined(__amd64__)
	printf("\tfrontend        Study frontend stalls (Requires AMD IBS)\n");
	printf("\tmemory          Study memory operations (Requires AMD IBS)\n");
#endif
}

int
writelog(int logfd, const void *buf, size_t len)
{
	int status;
	const char *cur = (const char *)buf;

	while (len != 0) {
		status = write(logfd, cur, len);
		if (status < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			else
				return status;
		}
		if (status == 0)
			return status;

		cur += status;
		len -= status;
	}

	return 0;
}

/*
 * Write the PMC header.  Each of the header payloads have their own header 
 * containing the payloads size.  Thus the pmc tools can just skip over regions 
 * that they do not know how to handle.
 */
int
write_header(int logfd)
{
	int status;
	pmchdr_header hdr;

	hdr.magic = PMC_HEADER_MAGIC;
	hdr.version = PMC_HEADER_VERSION;
#if defined(__amd64__)
	hdr.arch = PMC_ARCH_AMD64;
#elif defined(__aarch64__)
	hdr.arch = PMC_ARCH_ARM64;
#elif defined(__powerpc64__)
	hdr.arch = PMC_ARCH_PPC64;
#elif defined(__riscv)
	hdr.arch = PMC_ARCH_RISCV64;
#elif defined(__arm__)
	hdr.arch = PMC_ARCH_ARM;
#else
	hdr.arch = 0;
#endif

	status = writelog(logfd, &hdr, sizeof(hdr));
	if (status < 0) {
		perror("writelog");
	}

	return status;
}

/*
 * Write out the sysinfo header.
 */
int
write_sysinfo(int logfd)
{
	int status;
	pmchdr_infohdr hdr;
	pmchdr_sysinfo sys;
	char val[64];
	size_t valsz;

	valsz = sizeof(val);
	status = sysctlbyname("hw.model", &val, &valsz, NULL, 0);
	if (status < 0) {
		perror("sysctlbyname");
		return status;
	}
	strlcpy(sys.cpumodel, val, sizeof(sys.cpumodel));

	valsz = sizeof(val);
	status = sysctlbyname("kern.osrelease", &val, &valsz, NULL, 0);
	if (status < 0) {
		perror("sysctlbyname");
		return status;
	}
	strlcpy(sys.osrelease, val, sizeof(sys.osrelease));

	valsz = sizeof(val);
	status = sysctlbyname("kern.build_id", &val, &valsz, NULL, 0);
	if (status < 0) {
		perror("sysctlbyname");
		return status;
	}
	strlcpy(sys.buildid, val, sizeof(sys.buildid));

	hdr.type = INFOHDR_TYPE_SYSINFO;
	hdr.length = sizeof(sys);
	status = writelog(logfd, &hdr, sizeof(hdr));
	if (status < 0) {
		err(EX_IOERR, "writelog");
	}

	status = writelog(logfd, &sys, sizeof(sys));
	if (status < 0) {
		err(EX_IOERR, "writelog");
	}

	return status;
};

#if defined(__i386__) || defined(__amd64__)
#define CPUID_ROOT_BASE		0x00000000
#define CPUID_ROOT_VM		0x40000000
#define CPUID_ROOT_EXT		0x80000000

/*
 * Write out the CPU Info block.  After the standard header that declares the 
 * size of the payload, we write out all the CPUID root and leafs for each of 
 * the three major roots: base, VM, and extended artribute space.  On all 
 * modern processors the root contains the maximum leaf as the first value 
 * allowing us to decode how many leafs there are.
 */
int
write_cpuinfo(int logfd)
{
	int status;
	u_int tmp[4];
	uint32_t base_max, vm_max, ext_max;
	uint32_t len;
	pmchdr_infohdr *hdr;
	uint32_t *buf, *off;

	/*
	 * Find the length of the main, VM, and extended CPUID leafs
	 */
	do_cpuid(CPUID_ROOT_BASE, tmp);
	base_max = tmp[0];
	len = base_max + 1;

	do_cpuid(CPUID_ROOT_VM, tmp);
	vm_max = tmp[0];
	if (vm_max != 0)
		len += vm_max - CPUID_ROOT_VM + 1;

	do_cpuid(CPUID_ROOT_EXT, tmp);
	ext_max = tmp[0];
	if (ext_max != 0)
		len += ext_max - CPUID_ROOT_EXT + 1;

	// 4 Registers per Leaf x 4 Bytes per Register
	len *= 4;

	buf = (uint32_t *)new uint32_t[len + sizeof(pmchdr_infohdr) / 4];
	hdr = (pmchdr_infohdr *)buf;
	hdr->type = INFOHDR_TYPE_CPUID;
	hdr->length = 4 * len;
	off = (uint32_t *)(hdr + 1);

	for (uint32_t i = 0; i <= base_max; i++) {
		do_cpuid(i, off);
		off += 4;
	}

	if (vm_max) {
		for (uint32_t i = CPUID_ROOT_VM; i <= vm_max; i++) {
			do_cpuid(i, off);
			off += 4;
		}
	}
	if (ext_max) {
		for (uint32_t i = CPUID_ROOT_EXT; i <= ext_max; i++) {
			do_cpuid(i, off);
			off += 4;
		}
	}

	status = writelog(logfd, buf, 4 * len + sizeof(*hdr));
	if (status < 0) {
		delete[] buf;
		err(EX_IOERR, "writelog");
	}

	delete[] buf;

	return 0;
}
#elif defined(__aarch64__)
int
write_cpuinfo(__unused int logfd)
{
	return 0;
}
#elif defined(__powerpc64__)
int
write_cpuinfo(__unused int logfd)
{
	return 0;
}
#else
int
write_cpuinfo(__unused int logfd)
{
	return 0;
}
#endif

int
write_footer(int logfd)
{
	int status;
	pmchdr_infohdr hdr;

	hdr.type = INFOHDR_TYPE_DONE;
	hdr.length = 0;

	status = writelog(logfd, &hdr, sizeof(hdr));
	if (status < 0) {
		perror("writelog");
	}

	return status;
}

#if defined(__i386__) || defined(__amd64__)
int
setup_study(const std::string &study, uint64_t rate, cpuset_t mask)
{
	u_int tmp[4];
	alignas(4) char vendor[16];
	std::string event;
	uint32_t ext_max;
	uint32_t ibs_features;
	bool is_amd, is_intel;

	do_cpuid(CPUID_ROOT_BASE, tmp);

	/* Find the brand */
	is_intel = false;
	is_amd = false;

	/* i386 complains without explicit alignment */
	((u_int *)&vendor)[0] = tmp[1];
	((u_int *)&vendor)[1] = tmp[3];
	((u_int *)&vendor)[2] = tmp[2];
	vendor[12] = 0;
	if (strncmp(vendor, INTEL_VENDOR_ID, 12) == 0)
		is_intel = true;
	if (strncmp(vendor, AMD_VENDOR_ID, 12) == 0)
		is_amd = true;
	if (strncmp(vendor, HYGON_VENDOR_ID, 12) == 0)
		is_amd = true;

	ibs_features = 0;
	do_cpuid(CPUID_ROOT_EXT, tmp);
	ext_max = tmp[0];
	if (is_amd && ext_max >= CPUID_IBSID) {
		do_cpuid(CPUID_IBSID, tmp);
		ibs_features = tmp[0];
	}

	if (study == "default" || study == "flamegraph") {
		pmcs.push_back(pmc_config("unhalted-cycles", rate, mask));
		return 0;
	}

	if (is_intel) {
		err(EX_SOFTWARE, "ERROR: Study is unsupported on Intel");
	}

	if (study == "frontend") {
		event = "ibs-fetch,randomize";
	} else if (study == "branches" || study == "memory") {
		event = "ibs-op";
		if (ibs_features & CPUID_IBSID_OPCNT)
			event += ",opcount";
	} else if (study == "c2c") {
		event = "ibs-op";
		if (ibs_features & CPUID_IBSID_IBSLOADLATENCYFILT)
			event += ",l3miss";
		if (ibs_features & CPUID_IBSID_OPCNT)
			event += ",opcount";
	} else {
		err(EX_USAGE, "ERROR: Study '%s' unknown", study.c_str());
	}

	pmcs.push_back(pmc_config(event, rate, mask));

	return 0;
}
#else
int
setup_study(const std::string &study, uint64_t rate, cpuset_t mask)
{
	if (study == "default" || study == "flamegraph") {
		pmcs.push_back(pmc_config("unhalted-cycles", rate, mask));
		return 0;
	}

	err(EX_SOFTWARE, "ERROR: Study is unsupported on this architecture");
}
#endif

/*
 * Compute the current runtime and print it to the screen, if it exceeds 
 * timelimit return 1 otherwise 0.
 */
int
update_status(int logfd)
{
	int status;
	long sec, nsec;
	struct timespec end;
	struct stat sb;
	std::string filesz;

	status = clock_gettime(CLOCK_MONOTONIC, &end);
	if (status < 0) {
		err(EX_OSERR, "Could not read current time");
	}

	sec = end.tv_sec - start.tv_sec;
	nsec = end.tv_nsec - start.tv_nsec;
	if (nsec < 0) {
		sec--;
		nsec += 1000000000L;
	}

	// Do this another way for sockets
	status = fstat(logfd, &sb);
	if (status < 0) {
		err(EX_OSERR, "Failed to fstat log file");
	}

	filesz = format_binprefix(sb.st_size);

	/*
	 * Use %-10s to ensure we print spaces after in case the size string 
	 * shrinks.
	 */
	printf("Elapsed: %ld.%03ld seconds, Log file size: %-10s\r",
	    sec, nsec / 1000000,
	    filesz.c_str());
	fflush(stdout);

	if (timelimit != 0 && sec >= timelimit) {
		printf("\nElapsed time completed\n");
		return 1;
	}

	return 0;
}

int
cmd_pmc_record(int argc, char **argv)
{
	struct kevent kev;
	cpuset_t mask;
	uint64_t rate = DEFAULT_RATE;
	const char *logfile = "default.log";
	int status;
	int option, logfd, kq;

	CPU_ZERO(&mask);
	if (cpuset_getaffinity(CPU_LEVEL_ROOT, CPU_WHICH_PID, -1,
	    sizeof(mask), &mask) == -1)
		err(EX_OSERR, "ERROR: Cannot determine the available CPUs");

	if (pmc_init() < 0)
		err(EX_SOFTWARE, "ERROR: Failed to initialize libpmc");

	while ((option = getopt_long(argc, argv, "c:r:s:t:", longopts, NULL)) != -1) {
		switch (option) {
		case 'c':
			pmcs.push_back(pmc_config(optarg, rate, mask));
			break;
		case 'r':
			rate = strtoull(optarg, NULL, 0);
			break;
		case 's':
			setup_study(optarg, rate, mask);
			break;
		case 't':
			timelimit = atoi(optarg);
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0 && argc != 1) {
		usage();
		exit(EX_USAGE);
	}
	if (argc == 1)
		logfile = argv[0];

	if (pmcs.size() == 0) {
		errx(EX_NOINPUT, "ERROR: Please select one or more counters or performance studies.");
	}

	if ((logfd = open(logfile, O_CREAT|O_EXCL|O_WRONLY,
			S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
		errx(EX_OSERR, "ERROR: Cannot open \"%s\" for writing: %s.", logfile,
		    strerror(errno));
	}

	setup_screen();

	for (auto &p : pmcs) {
		p.getcaps();
	}

	write_header(logfd);
	write_sysinfo(logfd);
	write_cpuinfo(logfd);
	write_footer(logfd);

	// Record pmclog
	kq = kqueue();
	if (kq < 0)
		err(EX_OSERR, "ERROR: kqueue creation failed");

	EV_SET(&kev, fileno(stdin), EVFILT_READ, EV_ADD, 0, 0, NULL);
	if (kevent(kq, &kev, 1, NULL, 0, NULL) < 0)
		err(EX_OSERR, "ERROR: kevent failed to register stdin");

	EV_SET(&kev, SIGINT, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	if (kevent(kq, &kev, 1, NULL, 0, NULL) < 0)
		err(EX_OSERR, "ERROR: kevent failed to register SIGINT");
	EV_SET(&kev, SIGIO, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	if (kevent(kq, &kev, 1, NULL, 0, NULL) < 0)
		err(EX_OSERR, "ERROR: kevent failed to register SIGIO");

	EV_SET(&kev, 0, EVFILT_TIMER, EV_ADD, 0, 100, NULL);
	if (kevent(kq, &kev, 1, NULL, 0, NULL) < 0)
		err(EX_OSERR, "ERROR: Cannot register kevent for timer");

	pmc_configure_logfile(logfd);

	for (auto &p : pmcs) {
		p.allocate();
	}

	for (auto &p : pmcs) {
		p.start();
	}

	if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
		err(EX_OSERR, "ERROR: Could not get the current time");

	printf("Recording performance trace press Ctrl-C to stop\n");

	/*
	 * loop till either the target process (if any) exits, or we
	 * are killed by a SIGINT or we reached the time duration.
	 */
	while (1) {
		status = kevent(kq, NULL, 0, &kev, 1, NULL);
		if (status <= 0) {
			if (errno != EINTR)
				err(EX_OSERR, "ERROR: kevent failed");
			else
				continue;
		}

		if (kev.flags & EV_ERROR)
			errc(EX_OSERR, kev.data, "ERROR: kevent failed");

		switch (kev.filter) {
		case EVFILT_READ:  /* log file data is present */
			if (kev.ident == (unsigned)fileno(stdin)) {
				// Check for exit key
			}
			break;
		case EVFILT_SIGNAL:
			if (kev.ident == SIGIO) {
				fprintf(stderr, "ERROR: IO error");
				status = EX_OSERR;
				goto done;
			} else if (kev.ident == SIGINT) {
				status = EX_OK;
				goto done;
			} else {
				err(EX_OSERR, "Unknown signal recieved");
			}
			break;
		case EVFILT_TIMER:
			if (update_status(logfd) == 1)
				goto done;
			break;
		}
	}

done:
	for (auto &p : pmcs) {
		p.stop();
		p.release();
	}
	close(logfd);

	return (status);
}

