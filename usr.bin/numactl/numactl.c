/*
 * Copyright (c) 2015 Adrian Chadd <adrian@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/numa.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static struct option longopts[] = {
	{ "tid",	required_argument,	NULL,	't' },
	{ "pid",	required_argument,	NULL,	'p' },
	{ "memdomain",	required_argument,	NULL,	'm' },
	{ "cpudomain",	required_argument,	NULL,	'c' },
	{ "mempolicy",	required_argument, 	NULL,	'l' },
	{ "set",	no_argument,	NULL,	's' },
	{ "get",	no_argument,	NULL,	'g' },
	{ NULL, 0, NULL, 0 }
};

static const char *
policy_to_str(vm_domain_policy_type_t vt)
{

	switch (vt) {
	case VM_POLICY_NONE:
		return ("none");
	case VM_POLICY_ROUND_ROBIN:
		return ("rr");
	case VM_POLICY_FIXED_DOMAIN:
		return ("fixed-domain");
	case VM_POLICY_FIXED_DOMAIN_ROUND_ROBIN:
		return ("fixed-domain-rr");
	case VM_POLICY_FIRST_TOUCH:
		return ("first-touch");
	case VM_POLICY_FIRST_TOUCH_ROUND_ROBIN:
		return ("first-touch-rr");
	default:
		return ("unknown");
	}
}

static int
parse_policy(struct vm_domain_policy_entry *vd, const char *str)
{

	if (strcmp(str, "rr") == 0) {
		vd->policy = VM_POLICY_ROUND_ROBIN;
		vd->domain = -1;
		return (0);
	}

	if (strcmp(str, "first-touch-rr") == 0) {
		vd->policy = VM_POLICY_FIRST_TOUCH_ROUND_ROBIN;
		vd->domain = -1;
		return (0);
	}

	if (strcmp(str, "first-touch") == 0) {
		vd->policy = VM_POLICY_FIRST_TOUCH;
		vd->domain = -1;
		return (0);
	}

	if (strcmp(str, "fixed-domain") == 0) {
		vd->policy = VM_POLICY_FIXED_DOMAIN;
		vd->domain = 0;
		return (0);
	}

	if (strcmp(str, "fixed-domain-rr") == 0) {
		vd->policy = VM_POLICY_FIXED_DOMAIN_ROUND_ROBIN;
		vd->domain = 0;
		return (0);
	}

	return (-1);
}

static void
usage(void)
{

	printf("usage: numactl --get [--tid/-t <tid>] [--pid/-p <pid>]\n");
	printf("       numactl --set [--tid=<tid>] [--pid/-p<pid>]\n");
	printf("                     [--mempolicy/-l <policy>] [--memdomain/"
	    "-m <domain>]\n");
	printf("                     [--cpudomain/-c <domain>]\n");
	printf("       numactl [--mempolicy/-l <policy>] [--memdomain/-m "
	    "<domain>]\n");
	printf("               [--cpudomain/-c <domain>] <cmd> ...\n");

	exit(EX_USAGE);
}

static int
set_numa_domain_cpuaffinity(int cpu_domain, cpuwhich_t which, id_t id)
{
	cpuset_t set;
	int error;

	error = cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_DOMAIN,
	    cpu_domain, sizeof(set), &set);
	if (error != 0)
		err(1, "cpuset_getaffinity");
	error = cpuset_setaffinity(CPU_LEVEL_WHICH, which, id, sizeof(set),
	    &set);
	if (error != 0)
		err(1, "cpuset_setaffinity");

	return (0);
}

int
main(int argc, char *argv[])
{
	struct vm_domain_policy_entry vd;
	lwpid_t tid;
	pid_t pid;
	cpuwhich_t which;
	id_t id;
	int error;
	int is_set, is_get;
	int mem_policy_set;
	int ch;
	int cpu_domain;

	id = -1;
	which = -1;
	is_set = 0;
	is_get = 0;
	mem_policy_set = 0;
	tid = -1;
	pid = -1;
	cpu_domain = -1;

	while ((ch = getopt_long(argc, argv, "c:gl:m:p:st:", longopts,
	    NULL)) != -1) {
		switch (ch) {
		case 'c':
			cpu_domain = atoi(optarg);
			break;
		case 'g':
			is_get = 1;
			break;
		case 'l':
			if (parse_policy(&vd, optarg) != 0) {
				fprintf(stderr,
				    "Could not parse policy: '%s'\n", optarg);
				exit(1);
			}
			mem_policy_set = 1;
			break;
		case 'm':
			if (mem_policy_set == 0) {
				fprintf(stderr,
				    "Error: set policy first before domain\n");
				exit(1);
			}
			vd.domain = atoi(optarg);
			break;
		case 'p':
			pid = atoi(optarg);
			break;
		case 's':
			is_set = 1;
			break;
		case 't':
			tid = atoi(optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Handle the user wishing to run a command */
	if (argc) {
		/* Ensure that a policy was set */
		if (mem_policy_set == 0) {
			fprintf(stderr, "Error: no policy given\n");
			usage();
		}

		/* Set current memory process policy, will be inherited */
		if (numa_setaffinity(CPU_WHICH_PID, -1, &vd) != 0)
			err(1, "numa_setaffinity");

		/* If a CPU domain policy was given, include that too */
		if (cpu_domain != -1)
			(void) set_numa_domain_cpuaffinity(cpu_domain,
			    CPU_WHICH_PID, -1);

		errno = 0;
		execvp(*argv, argv);
		err(errno == ENOENT ? 127 : 126, "%s", *argv);
	}

	/* Figure out which */
	if (tid != -1) {
		which = CPU_WHICH_TID;
		id = tid;
	} else if (pid != -1) {
		which = CPU_WHICH_PID;
		id = pid;
	} else {
		fprintf(stderr, "Error: one of tid or pid must be given\n");
		usage();
	}

	/* Sanity checks */
	if (is_set && is_get) {
		fprintf(stderr, "Error: can't set both 'set' and 'get'\n");
		usage();
	}

	if (is_set && ! mem_policy_set) {
		fprintf(stderr, "Error: --set given, but no policy\n");
		usage();
	}

	/* If it's get, then get the policy and return */
	if (is_get) {
		error = numa_getaffinity(which, id, &vd);
		if (error != 0)
			err(1, "numa_getaffinity");
		printf("  Policy: %s; domain: %d\n",
		    policy_to_str(vd.policy),
		    vd.domain);
		exit(0);
	}

	/* Assume it's set */

	/* Syscall */
	error = numa_setaffinity(which, id, &vd);
	if (error != 0)
		err(1, "numa_setaffinity");

	/* If a CPU domain policy was given, include that too */
	if (cpu_domain != -1)
		(void) set_numa_domain_cpuaffinity(cpu_domain, which, id);

	exit(0);
}
