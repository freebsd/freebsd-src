/*-
 * Copyright (c) 2013 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/user.h>

#include <inttypes.h>
#include <libprocstat.h>
#include <sandbox_stat.h>
#include <stdio.h>
#include <stdlib.h>

#include "procstat.h"

void
procstat_sandbox_classes(struct procstat *procstat, struct kinfo_proc *kipp)
{
	struct sandbox_class_stat *scsp, *scsp_free;
	size_t len;
	u_int i;

	if (!hflag) {
		printf("%5s %-12s %-20s %3s %5s %7s\n", "PID", "COMM", "CLASS",
		    "CID", "COUNT", "RESET");
	}
	scsp_free = scsp = procstat_getsbclasses(procstat, kipp, &len);
	if (scsp == NULL)
		return;
	for (i = 0; i < (len / sizeof(*scsp)); i++, scsp++) {
		if (scsp->scs_classid == SANDBOX_CLASSID_FREE)
			continue;
		printf("%5d ", kipp->ki_pid);
		printf("%-12s ", kipp->ki_comm);
		printf("%-20s ", scsp->scs_class_name);
		printf("%3jd ", (uintmax_t)scsp->scs_classid);
		printf("%5jd ", (uintmax_t)(scsp->scs_stat_alloc -
		    scsp->scs_stat_free));
		printf("%7jd\n", (uintmax_t)scsp->scs_stat_reset);
	}
	procstat_freesbclasses(procstat, scsp_free);
}

void
procstat_sandbox_methods(struct procstat *procstat, struct kinfo_proc *kipp)
{
	struct sandbox_method_stat *smsp, *smsp_free;
	size_t len;
	u_int i;

	if (!hflag) {
		printf("%5s %-12s %-20s %3s %-12s %3s %7s %7s\n", "PID",
		    "COMM", "CLASS", "CID", "METHOD", "MID", "INVOKE",
		    "FAULT");
	}
	smsp_free = smsp = procstat_getsbmethods(procstat, kipp, &len);
	if (smsp == NULL)
		return;
	for (i = 0; i < (len / sizeof(*smsp)); i++, smsp++) {
		if (smsp->sms_methodid == SANDBOX_METHODID_FREE)
			continue;
		printf("%5d ", kipp->ki_pid);
		printf("%-12s ", kipp->ki_comm);
		printf("%-20s ", smsp->sms_class_name);
		printf("%3jd ", (uintmax_t)smsp->sms_classid);
		printf("%-12s ", smsp->sms_method_name);
		printf("%3jd ", (uintmax_t)smsp->sms_methodid);
		printf("%7jd ", (uintmax_t)smsp->sms_stat_invoke);
		printf("%7jd\n", (uintmax_t)smsp->sms_stat_fault);
	}
	procstat_freesbmethods(procstat, smsp_free);
}

static void
print_sbobject_type(uint64_t type)
{
	const char *str;

	switch (type) {
	case SANDBOX_OBJECT_TYPE_PID:
		str = "PID";
		break;

	case SANDBOX_OBJECT_TYPE_POINTER:
		str = "PTR";
		break;

	default:
		str = "-";
		break;
	}
	printf("%4s ", str);
}

static void
print_sbobject_name(uint64_t type, uint64_t name)
{

	switch (type) {
	case SANDBOX_OBJECT_TYPE_PID:
		printf("%18jd ", (uint64_t)name);
		break;

	case SANDBOX_OBJECT_TYPE_POINTER:
		printf("0x%016jx ", (uint64_t)name);
		break;

	default:
		printf("%-18s ", "-");
		break;
	}
}

void
procstat_sandbox_objects(struct procstat *procstat, struct kinfo_proc *kipp)
{
	struct sandbox_object_stat *sosp, *sosp_free;
	size_t len;
	u_int i;

	if (!hflag) {
		printf("%5s %-12s %-20s %3s %4s %-18s %7s\n", "PID", "COMM",
		    "CLASS", "CID", "TYPE", "NAME", "INVOKE");
	}
	sosp_free = sosp = procstat_getsbobjects(procstat, kipp, &len);
	if (sosp == NULL)
		return;
	for (i = 0; i < (len / sizeof(*sosp)); i++, sosp++) {
		if (sosp->sos_objectid == SANDBOX_OBJECTID_FREE)
			continue;
		printf("%5d ", kipp->ki_pid);
		printf("%-12s ", kipp->ki_comm);
		printf("%-20s ", sosp->sos_class_name);
		printf("%3jd ", (uintmax_t)sosp->sos_classid);
		print_sbobject_type(sosp->sos_object_type);
		print_sbobject_name(sosp->sos_object_type,
		    sosp->sos_object_name);
		printf("%7jd", (uintmax_t)sosp->sos_stat_invoke);
		printf("\n");
	}
	procstat_freesbobjects(procstat, sosp_free);
}
