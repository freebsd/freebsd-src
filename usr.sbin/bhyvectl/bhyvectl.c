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

#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/nv.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/un.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <libutil.h>
#include <fcntl.h>
#include <getopt.h>
#include <libutil.h>

#include <machine/cpufunc.h>
#include <machine/vmm.h>
#include <machine/vmm_dev.h>
#include <vmmapi.h>

#ifdef BHYVE_SNAPSHOT
#include "snapshot.h"
#endif

#include "bhyvectl.h"

#define	MB	(1UL << 20)
#define	GB	(1UL << 30)

static const char *progname;

static int get_stats, getcap, setcap, capval;
static int force_reset, force_poweroff;
static const char *capname;
static int create, destroy, get_memmap, get_memseg;
static int get_active_cpus, get_debug_cpus, get_suspended_cpus;
static uint64_t memsize;
static int run;
static int get_cpu_topology;
#ifdef BHYVE_SNAPSHOT
static int vm_suspend_opt;
#endif

static int get_all;

enum {
	VMNAME = OPT_START,	/* avoid collision with return values from getopt */
	VCPU,
	SET_MEM,
	SET_CAP,
	CAPNAME,
#ifdef BHYVE_SNAPSHOT
	SET_CHECKPOINT_FILE,
	SET_SUSPEND_FILE,
#endif
	OPT_LAST,
};

_Static_assert(OPT_LAST < OPT_START_MD,
    "OPT_LAST must be less than OPT_START_MD");

static void
print_cpus(const char *banner, const cpuset_t *cpus)
{
	int i, first;

	first = 1;
	printf("%s:\t", banner);
	if (!CPU_EMPTY(cpus)) {
		for (i = 0; i < CPU_SETSIZE; i++) {
			if (CPU_ISSET(i, cpus)) {
				printf("%s%d", first ? " " : ", ", i);
				first = 0;
			}
		}
	} else
		printf(" (none)");
	printf("\n");
}

static struct option *
setup_options(void)
{
	const struct option common_opts[] = {
		{ "vm",		REQ_ARG,	0,	VMNAME },
		{ "cpu",	REQ_ARG,	0,	VCPU },
		{ "set-mem",	REQ_ARG,	0,	SET_MEM },
		{ "capname",	REQ_ARG,	0,	CAPNAME },
		{ "setcap",	REQ_ARG,	0,	SET_CAP },
		{ "getcap",	NO_ARG,		&getcap,	1 },
		{ "get-stats",	NO_ARG,		&get_stats,	1 },
		{ "get-memmap",	NO_ARG,		&get_memmap,	1 },
		{ "get-memseg", NO_ARG,		&get_memseg,	1 },
		{ "get-all",		NO_ARG,	&get_all,		1 },
		{ "run",		NO_ARG,	&run,			1 },
		{ "create",		NO_ARG,	&create,		1 },
		{ "destroy",		NO_ARG,	&destroy,		1 },
		{ "force-reset",	NO_ARG,	&force_reset,		1 },
		{ "force-poweroff", 	NO_ARG,	&force_poweroff, 	1 },
		{ "get-active-cpus", 	NO_ARG,	&get_active_cpus, 	1 },
		{ "get-debug-cpus",	NO_ARG,	&get_debug_cpus,	1 },
		{ "get-suspended-cpus", NO_ARG,	&get_suspended_cpus, 	1 },
		{ "get-cpu-topology",	NO_ARG, &get_cpu_topology,	1 },
#ifdef BHYVE_SNAPSHOT
		{ "checkpoint", 	REQ_ARG, 0,	SET_CHECKPOINT_FILE},
		{ "suspend", 		REQ_ARG, 0,	SET_SUSPEND_FILE},
#endif
	};

	return (bhyvectl_opts(common_opts, nitems(common_opts)));
}

void
usage(const struct option *opts)
{
	static const char *set_desc[] = {
	    [VCPU] = "vcpu_number",
	    [SET_MEM] = "memory in units of MB",
	    [SET_CAP] = "0|1",
	    [CAPNAME] = "capname",
#ifdef BHYVE_SNAPSHOT
	    [SET_CHECKPOINT_FILE] = "filename",
	    [SET_SUSPEND_FILE] = "filename",
#endif
	};
	(void)fprintf(stderr, "Usage: %s --vm=<vmname>\n", progname);
	for (const struct option *o = opts; o->name; o++) {
		if (strcmp(o->name, "vm") == 0)
			continue;
		if (o->has_arg == REQ_ARG) {
			(void)fprintf(stderr, "       [--%s=<%s>]\n", o->name,
			    o->val >= OPT_START_MD ? bhyvectl_opt_desc(o->val) :
			    set_desc[o->val]);
		} else {
			(void)fprintf(stderr, "       [--%s]\n", o->name);
		}
	}
	exit(1);
}

static int
show_memmap(struct vmctx *ctx)
{
	char name[SPECNAMELEN + 1], numbuf[8];
	vm_ooffset_t segoff;
	vm_paddr_t gpa;
	size_t maplen, seglen;
	int error, flags, prot, segid, delim;

	printf("Address     Length      Segment     Offset      ");
	printf("Prot  Flags\n");

	gpa = 0;
	while (1) {
		error = vm_mmap_getnext(ctx, &gpa, &segid, &segoff, &maplen,
		    &prot, &flags);
		if (error)
			return (errno == ENOENT ? 0 : error);

		error = vm_get_memseg(ctx, segid, &seglen, name, sizeof(name));
		if (error)
			return (error);

		printf("%-12lX", gpa);
		humanize_number(numbuf, sizeof(numbuf), maplen, "B",
		    HN_AUTOSCALE, HN_NOSPACE);
		printf("%-12s", numbuf);

		printf("%-12s", name[0] ? name : "sysmem");
		printf("%-12lX", segoff);
		printf("%c%c%c   ", prot & PROT_READ ? 'R' : '-',
		    prot & PROT_WRITE ? 'W' : '-',
		    prot & PROT_EXEC ? 'X' : '-');

		delim = '\0';
		if (flags & VM_MEMMAP_F_WIRED) {
			printf("%cwired", delim);
			delim = '/';
		}
#ifdef __amd64__
		if (flags & VM_MEMMAP_F_IOMMU) {
			printf("%ciommu", delim);
			delim = '/';
		}
#endif
		printf("\n");

		gpa += maplen;
	}
}

static int
show_memseg(struct vmctx *ctx)
{
	char name[SPECNAMELEN + 1], numbuf[8];
	size_t seglen;
	int error, segid;

	printf("ID  Length      Name\n");

	segid = 0;
	while (1) {
		error = vm_get_memseg(ctx, segid, &seglen, name, sizeof(name));
		if (error)
			return (errno == EINVAL ? 0 : error);

		if (seglen) {
			printf("%-4d", segid);
			humanize_number(numbuf, sizeof(numbuf), seglen, "B",
			    HN_AUTOSCALE, HN_NOSPACE);
			printf("%-12s", numbuf);
			printf("%s", name[0] ? name : "sysmem");
			printf("\n");
		}
		segid++;
	}
}

#ifdef BHYVE_SNAPSHOT
static int
send_message(const char *vmname, nvlist_t *nvl)
{
	struct sockaddr_un addr;
	int err = 0, socket_fd;

	socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		perror("Error creating bhyvectl socket");
		err = errno;
		goto done;
	}

	memset(&addr, 0, sizeof(struct sockaddr_un));
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s%s",
	    BHYVE_RUN_DIR, vmname);
	addr.sun_family = AF_UNIX;
	addr.sun_len = SUN_LEN(&addr);

	if (connect(socket_fd, (struct sockaddr *)&addr, addr.sun_len) != 0) {
		perror("connect() failed");
		err = errno;
		goto done;
	}

	if (nvlist_send(socket_fd, nvl) < 0) {
		perror("nvlist_send() failed");
		err = errno;
	}
done:
	nvlist_destroy(nvl);

	if (socket_fd >= 0)
		close(socket_fd);
	return (err);
}

static int
open_directory(const char *file)
{
	char *path;
	int fd;

	if ((path = strdup(file)) == NULL)
		return (-1);

	dirname(path);
	fd = open(path, O_DIRECTORY);
	free(path);

	return (fd);
}

static int
snapshot_request(const char *vmname, char *file, bool suspend)
{
	nvlist_t *nvl;
	int fd;

	if ((fd = open_directory(file)) < 0)
		return (errno);

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "checkpoint");
	nvlist_add_string(nvl, "filename", basename(file));
	nvlist_add_bool(nvl, "suspend", suspend);
	nvlist_move_descriptor(nvl, "fddir", fd);

	return (send_message(vmname, nvl));
}
#endif

int
main(int argc, char *argv[])
{
	char *vmname;
	int error, ch, vcpuid;
	struct vm_run vmrun;
	struct vmctx *ctx;
	struct vcpu *vcpu;
	cpuset_t cpus;
	struct option *opts;
#ifdef BHYVE_SNAPSHOT
	char *checkpoint_file = NULL;
#endif

	opts = setup_options();

	vcpuid = 0;
	vmname = NULL;
	progname = basename(argv[0]);

	while ((ch = getopt_long(argc, argv, "", opts, NULL)) != -1) {
		if (ch >= OPT_START_MD) {
			bhyvectl_handle_opt(opts, ch);
			continue;
		}

		switch (ch) {
		case 0:
			break;
		case VMNAME:
			vmname = optarg;
			break;
		case VCPU:
			vcpuid = atoi(optarg);
			break;
		case SET_MEM:
			memsize = atoi(optarg) * MB;
			memsize = roundup(memsize, 2 * MB);
			break;
		case SET_CAP:
			capval = strtoul(optarg, NULL, 0);
			setcap = 1;
			break;
		case CAPNAME:
			capname = optarg;
			break;
#ifdef BHYVE_SNAPSHOT
		case SET_CHECKPOINT_FILE:
		case SET_SUSPEND_FILE:
			if (checkpoint_file != NULL)
				usage(opts);

			checkpoint_file = optarg;
			vm_suspend_opt = (ch == SET_SUSPEND_FILE);
			break;
#endif
		default:
			usage(opts);
		}
	}
	argc -= optind;
	argv += optind;

	if (vmname == NULL)
		usage(opts);


	ctx = vm_openf(vmname, create ? VMMAPI_OPEN_CREATE : 0);
	if (ctx == NULL) {
		fprintf(stderr,
		    "vm_open: %s could not be opened: %s\n",
		    vmname, strerror(errno));
		exit(1);
	}
	vcpu = vm_vcpu_open(ctx, vcpuid);

	error = 0;
	if (!error && memsize)
		error = vm_setup_memory(ctx, memsize, VM_MMAP_ALL);

	if (!error && (get_memseg || get_all))
		error = show_memseg(ctx);

	if (!error && (get_memmap || get_all))
		error = show_memmap(ctx);

	if (!error)
		bhyvectl_md_main(ctx, vcpu, vcpuid, get_all);

	if (!error && setcap) {
		int captype;

		captype = vm_capability_name2type(capname);
		error = vm_set_capability(vcpu, captype, capval);
		if (error != 0 && errno == ENOENT)
			printf("Capability \"%s\" is not available\n", capname);
	}

	if (!error && (getcap || get_all)) {
		int captype, val, getcaptype;

		if (getcap && capname)
			getcaptype = vm_capability_name2type(capname);
		else
			getcaptype = -1;

		for (captype = 0; captype < VM_CAP_MAX; captype++) {
			if (getcaptype >= 0 && captype != getcaptype)
				continue;
			error = vm_get_capability(vcpu, captype, &val);
			if (error == 0) {
				printf("Capability \"%s\" is %s on vcpu %d\n",
					vm_capability_type2name(captype),
					val ? "set" : "not set", vcpuid);
			} else if (errno == ENOENT) {
				error = 0;
				printf("Capability \"%s\" is not available\n",
					vm_capability_type2name(captype));
			} else {
				break;
			}
		}
	}

	if (!error && (get_active_cpus || get_all)) {
		error = vm_active_cpus(ctx, &cpus);
		if (!error)
			print_cpus("active cpus", &cpus);
	}

	if (!error && (get_debug_cpus || get_all)) {
		error = vm_debug_cpus(ctx, &cpus);
		if (!error)
			print_cpus("debug cpus", &cpus);
	}

	if (!error && (get_suspended_cpus || get_all)) {
		error = vm_suspended_cpus(ctx, &cpus);
		if (!error)
			print_cpus("suspended cpus", &cpus);
	}

	if (!error && (get_stats || get_all)) {
		int i, num_stats;
		uint64_t *stats;
		struct timeval tv;
		const char *desc;

		stats = vm_get_stats(vcpu, &tv, &num_stats);
		if (stats != NULL) {
			printf("vcpu%d stats:\n", vcpuid);
			for (i = 0; i < num_stats; i++) {
				desc = vm_get_stat_desc(ctx, i);
				printf("%-40s\t%ld\n", desc, stats[i]);
			}
		}
	}

	if (!error && (get_cpu_topology || get_all)) {
		uint16_t sockets, cores, threads, maxcpus;

		vm_get_topology(ctx, &sockets, &cores, &threads, &maxcpus);
		printf("cpu_topology:\tsockets=%hu, cores=%hu, threads=%hu, "
		    "maxcpus=%hu\n", sockets, cores, threads, maxcpus);
	}

	if (!error && run) {
		struct vm_exit vmexit;
		cpuset_t cpuset;

		vmrun.vm_exit = &vmexit;
		vmrun.cpuset = &cpuset;
		vmrun.cpusetsize = sizeof(cpuset);
		error = vm_run(vcpu, &vmrun);
		if (error == 0)
			bhyvectl_dump_vm_run_exitcode(&vmexit, vcpuid);
		else
			printf("vm_run error %d\n", error);
	}

	if (!error && force_reset)
		error = vm_suspend(ctx, VM_SUSPEND_RESET);

	if (!error && force_poweroff)
		error = vm_suspend(ctx, VM_SUSPEND_POWEROFF);

	if (error)
		printf("errno = %d\n", errno);

	if (!error && destroy)
		vm_destroy(ctx);

#ifdef BHYVE_SNAPSHOT
	if (!error && checkpoint_file)
		error = snapshot_request(vmname, checkpoint_file, vm_suspend_opt);
#endif

	free(opts);
	exit(error);
}
