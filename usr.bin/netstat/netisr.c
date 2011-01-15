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

#include <sys/types.h>
#include <sys/sysctl.h>

#include <net/netisr.h>

#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
netisr_load_config(void)
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
netisr_load_proto(void)
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
netisr_load_workstream(void)
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
netisr_load_work(void)
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
netisr_stats(void)
{
	struct sysctl_netisr_workstream *snwsp;
	struct sysctl_netisr_proto *snpp;
	u_int i;

	netisr_load_config();
	netisr_load_proto();
	netisr_load_workstream();
	netisr_load_work();

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
