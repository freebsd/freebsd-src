/*-
 * Copyright (c) 2010 Juniper Networks, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert N. M. Watson under contract
 * to Juniper Networks, Inc.
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
#include <sys/sysctl.h>

#include <sys/_lock.h>
#include <sys/_mutex.h>

#define	_WANT_NETISR_INTERNAL
#include <net/netisr.h>
#include <net/netisr_internal.h>

#include <err.h>
#include <kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netstat.h"

/*
 * Print statistics for the kernel netisr subsystem.
 */
static u_int				 bindthreads;
static u_int				 maxthreads;
static u_int				 numthreads;

static u_int				 defaultqlimit;
static u_int				 maxqlimit;

static u_int				 direct;
static u_int				 direct_force;

static struct sysctl_netisr_proto	*proto_array;
static u_int				 proto_array_len;

static struct sysctl_netisr_workstream	*workstream_array;
static u_int				 workstream_array_len;

static struct sysctl_netisr_work	*work_array;
static u_int				 work_array_len;

static u_int				*nws_array;

static u_int				 maxprot;

static void
netisr_load_kvm_uint(kvm_t *kd, char *name, u_int *p)
{
	struct nlist nl[] = {
		{ .n_name = name },
		{ .n_name = NULL },
	};
	int ret;

	ret = kvm_nlist(kd, nl);
	if (ret < 0)
		errx(-1, "%s: kvm_nlist(%s): %s", __func__, name,
		    kvm_geterr(kd));
	if (ret != 0)
		errx(-1, "%s: kvm_nlist(%s): unresolved symbol", __func__,
		    name);
	if (kvm_read(kd, nl[0].n_value, p, sizeof(*p)) != sizeof(*p))
		errx(-1, "%s: kvm_read(%s): %s", __func__, name,
		    kvm_geterr(kd));
}

/*
 * Load a nul-terminated string from KVM up to 'limit', guarantee that the
 * string in local memory is nul-terminated.
 */
static void
netisr_load_kvm_string(kvm_t *kd, uintptr_t addr, char *dest, u_int limit)
{
	u_int i;

	for (i = 0; i < limit; i++) {
		if (kvm_read(kd, addr + i, &dest[i], sizeof(dest[i])) !=
		    sizeof(dest[i]))
			err(-1, "%s: kvm_read: %s", __func__,
			    kvm_geterr(kd));
		if (dest[i] == '\0')
			break;
	}
	dest[limit - 1] = '\0';
}

static const char *
netisr_proto2name(u_int proto)
{
	u_int i;

	for (i = 0; i < proto_array_len; i++) {
		if (proto_array[i].snp_proto == proto)
			return (proto_array[i].snp_name);
	}
	return ("unknown");
}

static int
netisr_protoispresent(u_int proto)
{
	u_int i;

	for (i = 0; i < proto_array_len; i++) {
		if (proto_array[i].snp_proto == proto)
			return (1);
	}
	return (0);
}

static void
netisr_load_kvm_config(kvm_t *kd)
{

	netisr_load_kvm_uint(kd, "_netisr_bindthreads", &bindthreads);
	netisr_load_kvm_uint(kd, "_netisr_maxthreads", &maxthreads);
	netisr_load_kvm_uint(kd, "_nws_count", &numthreads);

	netisr_load_kvm_uint(kd, "_netisr_defaultqlimit", &defaultqlimit);
	netisr_load_kvm_uint(kd, "_netisr_maxqlimit", &maxqlimit);

	netisr_load_kvm_uint(kd, "_netisr_direct", &direct);
	netisr_load_kvm_uint(kd, "_netisr_direct_force", &direct_force);
}

static void
netisr_load_sysctl_uint(const char *name, u_int *p)
{
	size_t retlen;

	retlen = sizeof(u_int);
	if (sysctlbyname(name, p, &retlen, NULL, 0) < 0)
		err(-1, "%s", name);
	if (retlen != sizeof(u_int))
		errx(-1, "%s: invalid len %ju", name, (uintmax_t)retlen);
}

static void
netisr_load_sysctl_config(void)
{

	netisr_load_sysctl_uint("net.isr.bindthreads", &bindthreads);
	netisr_load_sysctl_uint("net.isr.maxthreads", &maxthreads);
	netisr_load_sysctl_uint("net.isr.numthreads", &numthreads);

	netisr_load_sysctl_uint("net.isr.defaultqlimit", &defaultqlimit);
	netisr_load_sysctl_uint("net.isr.maxqlimit", &maxqlimit);

	netisr_load_sysctl_uint("net.isr.direct", &direct);
	netisr_load_sysctl_uint("net.isr.direct_force", &direct_force);
}

static void
netisr_load_kvm_proto(kvm_t *kd)
{
	struct nlist nl[] = {
#define	NLIST_NETISR_PROTO	0
		{ .n_name = "_netisr_proto" },
		{ .n_name = NULL },
	};
	struct netisr_proto *np_array, *npp;
	u_int i, protocount;
	struct sysctl_netisr_proto *snpp;
	size_t len;
	int ret;

	/*
	 * Kernel compile-time and user compile-time definitions of
	 * NETISR_MAXPROT must match, as we use that to size work arrays.
	 */
	netisr_load_kvm_uint(kd, "_netisr_maxprot", &maxprot);
	if (maxprot != NETISR_MAXPROT)
		errx(-1, "%s: NETISR_MAXPROT mismatch", __func__);
	len = maxprot * sizeof(*np_array);
	np_array = malloc(len);
	if (np_array == NULL)
		err(-1, "%s: malloc", __func__);
	ret = kvm_nlist(kd, nl);
	if (ret < 0)
		errx(-1, "%s: kvm_nlist(_netisr_proto): %s", __func__,
		    kvm_geterr(kd));
	if (ret != 0)
		errx(-1, "%s: kvm_nlist(_netisr_proto): unresolved symbol",
		    __func__);
	if (kvm_read(kd, nl[NLIST_NETISR_PROTO].n_value, np_array, len) !=
	    (ssize_t)len)
		errx(-1, "%s: kvm_read(_netisr_proto): %s", __func__,
		    kvm_geterr(kd));

	/*
	 * Size and allocate memory to hold only live protocols.
	 */
	protocount = 0;
	for (i = 0; i < maxprot; i++) {
		if (np_array[i].np_name == NULL)
			continue;
		protocount++;
	}
	proto_array = calloc(protocount, sizeof(*proto_array));
	if (proto_array == NULL)
		err(-1, "malloc");
	protocount = 0;
	for (i = 0; i < maxprot; i++) {
		npp = &np_array[i];
		if (npp->np_name == NULL)
			continue;
		snpp = &proto_array[protocount];
		snpp->snp_version = sizeof(*snpp);
		netisr_load_kvm_string(kd, (uintptr_t)npp->np_name,
		    snpp->snp_name, sizeof(snpp->snp_name));
		snpp->snp_proto = i;
		snpp->snp_qlimit = npp->np_qlimit;
		snpp->snp_policy = npp->np_policy;
		if (npp->np_m2flow != NULL)
			snpp->snp_flags |= NETISR_SNP_FLAGS_M2FLOW;
		if (npp->np_m2cpuid != NULL)
			snpp->snp_flags |= NETISR_SNP_FLAGS_M2CPUID;
		if (npp->np_drainedcpu != NULL)
			snpp->snp_flags |= NETISR_SNP_FLAGS_DRAINEDCPU;
		protocount++;
	}
	proto_array_len = protocount;
	free(np_array);
}

static void
netisr_load_sysctl_proto(void)
{
	size_t len;

	if (sysctlbyname("net.isr.proto", NULL, &len, NULL, 0) < 0)
		err(-1, "net.isr.proto: query len");
	if (len % sizeof(*proto_array) != 0)
		errx(-1, "net.isr.proto: invalid len");
	proto_array = malloc(len);
	if (proto_array == NULL)
		err(-1, "malloc");
	if (sysctlbyname("net.isr.proto", proto_array, &len, NULL, 0) < 0)
		err(-1, "net.isr.proto: query data");
	if (len % sizeof(*proto_array) != 0)
		errx(-1, "net.isr.proto: invalid len");
	proto_array_len = len / sizeof(*proto_array);
	if (proto_array_len < 1)
		errx(-1, "net.isr.proto: no data");
	if (proto_array[0].snp_version != sizeof(proto_array[0]))
		errx(-1, "net.isr.proto: invalid version");
}

static void
netisr_load_kvm_workstream(kvm_t *kd)
{
	struct nlist nl[] = {
#define	NLIST_NWS_ARRAY		0
		{ .n_name = "_nws_array" },
		{ .n_name = NULL },
	};
	struct netisr_workstream nws;
	struct sysctl_netisr_workstream *snwsp;
	struct sysctl_netisr_work *snwp;
	struct netisr_work *nwp;
	struct nlist nl_nws[2];
	u_int counter, cpuid, proto, wsid;
	size_t len;
	int ret;

	len = numthreads * sizeof(*nws_array);
	nws_array = malloc(len);
	if (nws_array == NULL)
		err(-1, "malloc");
	ret = kvm_nlist(kd, nl);
	if (ret < 0)
		errx(-1, "%s: kvm_nlist: %s", __func__, kvm_geterr(kd));
	if (ret != 0)
		errx(-1, "%s: kvm_nlist: unresolved symbol", __func__);
	if (kvm_read(kd, nl[NLIST_NWS_ARRAY].n_value, nws_array, len) !=
	    (ssize_t)len)
		errx(-1, "%s: kvm_read(_nws_array): %s", __func__,
		    kvm_geterr(kd));
	workstream_array = calloc(numthreads, sizeof(*workstream_array));
	if (workstream_array == NULL)
		err(-1, "calloc");
	workstream_array_len = numthreads;
	work_array = calloc(numthreads * proto_array_len, sizeof(*work_array));
	if (work_array == NULL)
		err(-1, "calloc");
	counter = 0;
	for (wsid = 0; wsid < numthreads; wsid++) {
		cpuid = nws_array[wsid];
		if (kvm_dpcpu_setcpu(kd, cpuid) < 0)
			errx(-1, "%s: kvm_dpcpu_setcpu(%u): %s", __func__,
			    cpuid, kvm_geterr(kd));
		bzero(nl_nws, sizeof(nl_nws));
		nl_nws[0].n_name = "_nws";
		ret = kvm_nlist(kd, nl_nws);
		if (ret < 0)
			errx(-1, "%s: kvm_nlist looking up nws on CPU %u: %s",
			    __func__, cpuid, kvm_geterr(kd));
		if (ret != 0)
			errx(-1, "%s: kvm_nlist(nws): unresolved symbol on "
			    "CPU %u", __func__, cpuid);
		if (kvm_read(kd, nl_nws[0].n_value, &nws, sizeof(nws)) !=
		    sizeof(nws))
			errx(-1, "%s: kvm_read(nw): %s", __func__,
			    kvm_geterr(kd));
		snwsp = &workstream_array[wsid];
		snwsp->snws_version = sizeof(*snwsp);
		snwsp->snws_wsid = cpuid;
		snwsp->snws_cpu = cpuid;
		if (nws.nws_intr_event != NULL)
			snwsp->snws_flags |= NETISR_SNWS_FLAGS_INTR;

		/*
		 * Extract the CPU's per-protocol work information.
		 */
		printf("counting to maxprot: %u\n", maxprot);
		for (proto = 0; proto < maxprot; proto++) {
			if (!netisr_protoispresent(proto))
				continue;
			nwp = &nws.nws_work[proto];
			snwp = &work_array[counter];
			snwp->snw_version = sizeof(*snwp);
			snwp->snw_wsid = cpuid;
			snwp->snw_proto = proto;
			snwp->snw_len = nwp->nw_len;
			snwp->snw_watermark = nwp->nw_watermark;
			snwp->snw_dispatched = nwp->nw_dispatched;
			snwp->snw_hybrid_dispatched =
			    nwp->nw_hybrid_dispatched;
			snwp->snw_qdrops = nwp->nw_qdrops;
			snwp->snw_queued = nwp->nw_queued;
			snwp->snw_handled = nwp->nw_handled;
			counter++;
		}
	}
	work_array_len = counter;
}

static void
netisr_load_sysctl_workstream(void)
{
	size_t len;

	if (sysctlbyname("net.isr.workstream", NULL, &len, NULL, 0) < 0)
		err(-1, "net.isr.workstream: query len");
	if (len % sizeof(*workstream_array) != 0)
		errx(-1, "net.isr.workstream: invalid len");
	workstream_array = malloc(len);
	if (workstream_array == NULL)
		err(-1, "malloc");
	if (sysctlbyname("net.isr.workstream", workstream_array, &len, NULL,
	    0) < 0)
		err(-1, "net.isr.workstream: query data");
	if (len % sizeof(*workstream_array) != 0)
		errx(-1, "net.isr.workstream: invalid len");
	workstream_array_len = len / sizeof(*workstream_array);
	if (workstream_array_len < 1)
		errx(-1, "net.isr.workstream: no data");
	if (workstream_array[0].snws_version != sizeof(workstream_array[0]))
		errx(-1, "net.isr.workstream: invalid version");
}

static void
netisr_load_sysctl_work(void)
{
	size_t len;

	if (sysctlbyname("net.isr.work", NULL, &len, NULL, 0) < 0)
		err(-1, "net.isr.work: query len");
	if (len % sizeof(*work_array) != 0)
		errx(-1, "net.isr.work: invalid len");
	work_array = malloc(len);
	if (work_array == NULL)
		err(-1, "malloc");
	if (sysctlbyname("net.isr.work", work_array, &len, NULL, 0) < 0)
		err(-1, "net.isr.work: query data");
	if (len % sizeof(*work_array) != 0)
		errx(-1, "net.isr.work: invalid len");
	work_array_len = len / sizeof(*work_array);
	if (work_array_len < 1)
		errx(-1, "net.isr.work: no data");
	if (work_array[0].snw_version != sizeof(work_array[0]))
		errx(-1, "net.isr.work: invalid version");
}

static void
netisr_print_proto(struct sysctl_netisr_proto *snpp)
{

	printf("%-6s", snpp->snp_name);
	printf(" %5u", snpp->snp_proto);
	printf(" %6u", snpp->snp_qlimit);
	printf(" %6s",
	    (snpp->snp_policy == NETISR_POLICY_SOURCE) ?  "source" :
	    (snpp->snp_policy == NETISR_POLICY_FLOW) ? "flow" :
	    (snpp->snp_policy == NETISR_POLICY_CPU) ? "cpu" : "-");
	printf("   %s%s%s\n",
	    (snpp->snp_flags & NETISR_SNP_FLAGS_M2CPUID) ?  "C" : "-",
	    (snpp->snp_flags & NETISR_SNP_FLAGS_DRAINEDCPU) ?  "D" : "-",
	    (snpp->snp_flags & NETISR_SNP_FLAGS_M2FLOW) ? "F" : "-");
}

static void
netisr_print_workstream(struct sysctl_netisr_workstream *snwsp)
{
	struct sysctl_netisr_work *snwp;
	int first;
	u_int i;

	first = 1;
	for (i = 0; i < work_array_len; i++) {
		snwp = &work_array[i];
		if (snwp->snw_wsid != snwsp->snws_wsid)
			continue;
		if (first) {
			printf("%4u", snwsp->snws_wsid);
			printf(" %3u", snwsp->snws_cpu);
			first = 0;
		} else
			printf("%4s %3s", "", "");
		printf("%2s", "");
		printf("%-6s", netisr_proto2name(snwp->snw_proto));
		printf(" %5u", snwp->snw_len);
		printf(" %5u", snwp->snw_watermark);
		printf(" %8ju", snwp->snw_dispatched);
		printf(" %8ju", snwp->snw_hybrid_dispatched);
		printf(" %8ju", snwp->snw_qdrops);
		printf(" %8ju", snwp->snw_queued);
		printf(" %8ju", snwp->snw_handled);
		printf("\n");
	}
}

void
netisr_stats(void *kvmd)
{
	struct sysctl_netisr_workstream *snwsp;
	struct sysctl_netisr_proto *snpp;
	kvm_t *kd = kvmd;
	u_int i;

	if (live) {
		netisr_load_sysctl_config();
		netisr_load_sysctl_proto();
		netisr_load_sysctl_workstream();
		netisr_load_sysctl_work();
	} else {
		if (kd == NULL)
			errx(-1, "netisr_stats: !live but !kd");
		netisr_load_kvm_config(kd);
		netisr_load_kvm_proto(kd);
		netisr_load_kvm_workstream(kd);		/* Also does work. */
	}

	printf("Configuration:\n");
	printf("%-25s %12s %12s\n", "Setting", "Value", "Maximum");
	printf("%-25s %12u %12u\n", "Thread count", numthreads, maxthreads);
	printf("%-25s %12u %12u\n", "Default queue limit", defaultqlimit,
	    maxqlimit);
	printf("%-25s %12s %12s\n", "Direct dispatch", 
	    direct ? "enabled" : "disabled", "n/a");
	printf("%-25s %12s %12s\n", "Forced direct dispatch",
	    direct_force ? "enabled" : "disabled", "n/a");
	printf("%-25s %12s %12s\n", "Threads bound to CPUs",
	    bindthreads ? "enabled" : "disabled", "n/a");
	printf("\n");

	printf("Protocols:\n");
	printf("%-6s %5s %6s %-6s %-5s\n", "Name", "Proto", "QLimit",
	    "Policy", "Flags");
	for (i = 0; i < proto_array_len; i++) {
		snpp = &proto_array[i];
		netisr_print_proto(snpp);
	}
	printf("\n");

	printf("Workstreams:\n");
	printf("%4s %3s ", "WSID", "CPU");
	printf("%2s", "");
	printf("%-6s %5s %5s %8s %8s %8s %8s %8s\n", "Name", "Len", "WMark",
	    "Disp'd", "HDisp'd", "QDrops", "Queued", "Handled");
	for (i = 0; i < workstream_array_len; i++) {
		snwsp = &workstream_array[i];
		netisr_print_workstream(snwsp);
	}
}
