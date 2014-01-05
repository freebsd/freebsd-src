/*-
 * Copyright (c) 2013-2014 Robert N. M. Watson
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
#include <string.h>

#include "procstat.h"

/*
 * Various utility functions for processing sampled invocation times.  These
 * are complicated by the fact that zero entries should be disregarded.
 */
static void
sample_trim(u_int *arraylenp, uint64_t **arrayp)
{

	/* Trim leading zeros. */
	while (*arraylenp > 0 && (*arrayp)[0] == 0) {
		(*arrayp)++;
		(*arraylenp)--;
	}

	/* Trim trailing zeros. */
	while (*arraylenp > 0 && (*arrayp)[*arraylenp - 1] == 0)
		(*arraylenp)--;
}

static uint64_t
sample_min(u_int arraylen, uint64_t *array)
{
	u_int i, v;

	sample_trim(&arraylen, &array);
	if (arraylen == 0)
		return (0);
	v = array[0];
	for (i = 1; i < arraylen; i++) {
		if (array[i] < v)
			v = array[i];
	}
	return (v);
}

static uint64_t
sample_max(u_int arraylen, uint64_t *array)
{
	u_int i, v;

	sample_trim(&arraylen, &array);
	if (arraylen == 0)
		return (0);
	v = array[0];
	for (i = 1; i < arraylen; i++) {
		if (array[i] > v)
			v = array[i];
	}
	return (v);
}

static uint64_t
sample_mean(u_int arraylen, uint64_t *array)
{
	uint64_t sum;
	u_int i;

	sample_trim(&arraylen, &array);
	if (arraylen == 0)
		return (0);
	sum = 0;
	for (i = 0; i < arraylen; i++)
		sum += array[i];
	return (sum / arraylen);
}

static int
sample_compare(const void *p1, const void *p2)
{
	uint64_t left = *(const uintptr_t *)p1;
	uint64_t right = *(const uintptr_t *)p2;

	return ((left > right) - (left < right));
}

static uint64_t
sample_median(u_int arraylen, uint64_t *array)
{
	uint64_t copy[arraylen], *arrayp;

	/* Copy into our own array, trim, and sort. */
	memcpy(copy, array, arraylen * sizeof(copy[0]));
	arrayp = copy;
	sample_trim(&arraylen, &arrayp);
	if (arraylen == 0)
		return (0);
	qsort(copy, arraylen, sizeof(copy[0]), sample_compare);

	/* Otherwise, find the middle value. */
	if (arraylen % 2 == 0)		/* Even -- close to the median. */
		return ((arrayp[arraylen/2-1] + arrayp[arraylen/2]) / 2);
	else				/* Odd -- actually the median. */
		return (arrayp[arraylen/2]);
}

/*
 * The actual procstat(1) modes.
 */
void
procstat_sandbox_classes(struct procstat *procstat, struct kinfo_proc *kipp)
{
	struct sandbox_class_stat *scsp, *scsp_free;
	size_t len;
	u_int i;

	if (!hflag) {
		printf("%5s %-10s %-20s %5s %7s\n", "PID", "COMM", "CLASS",
		    "COUNT", "RESET");
	}
	scsp_free = scsp = procstat_getsbclasses(procstat, kipp, &len);
	if (scsp == NULL)
		return;
	for (i = 0; i < (len / sizeof(*scsp)); i++, scsp++) {
		if (scsp->scs_classid == SANDBOX_CLASSID_FREE)
			continue;
		printf("%5d ", kipp->ki_pid);
		printf("%-10s ", kipp->ki_comm);
		printf("%-20s ", scsp->scs_class_name);
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
		printf("%5s %-10s %-20s %-10s %6s %5s %8s %8s", "PID", "COMM",
		    "CLASS", "METHOD", "INVOKE", "FAULT", "LMIN", "LMAX");
		if (Xflag)
			printf(" %8s %8s %8s %8s", "SMIN", "SMAX", "SMEAN",
			    "SMEDIAN");
		printf("\n");
	}
	smsp_free = smsp = procstat_getsbmethods(procstat, kipp, &len);
	if (smsp == NULL)
		return;
	for (i = 0; i < (len / sizeof(*smsp)); i++, smsp++) {
		if (smsp->sms_methodid == SANDBOX_METHODID_FREE)
			continue;
		printf("%5d ", kipp->ki_pid);
		printf("%-10s ", kipp->ki_comm);
		printf("%-20s ", smsp->sms_class_name);
		printf("%-10s ", smsp->sms_method_name);
		printf("%6jd ", (uintmax_t)smsp->sms_stat_invoke);
		printf("%5jd ", (uintmax_t)smsp->sms_stat_fault);
		printf("%8jd ", (uintmax_t)smsp->sms_stat_minrun);
		printf("%8jd", (uintmax_t)smsp->sms_stat_maxrun);
		if (Xflag) {
			printf(" %8jd", sample_min(SANDBOX_METHOD_MAXSAMPVEC,
			    smsp->sms_stat_sampvec));
			printf(" %8jd", sample_max(SANDBOX_METHOD_MAXSAMPVEC,
			    smsp->sms_stat_sampvec));
			printf(" %8jd", sample_mean(SANDBOX_METHOD_MAXSAMPVEC,
			    smsp->sms_stat_sampvec));
			printf(" %8jd", sample_median(
			    SANDBOX_METHOD_MAXSAMPVEC,
			    smsp->sms_stat_sampvec));
		}
		printf("\n");
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
		printf("%11jd ", (uint64_t)name);
		break;

	case SANDBOX_OBJECT_TYPE_POINTER:
		printf("0x%09jx ", (uint64_t)name);
		break;

	default:
		printf("%-11s ", "-");
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
		printf("%5s %-10s %-20s %4s %-11s %6s %8s %8s", "PID", "COMM",
		    "CLASS", "TYPE", "NAME", "INVOKE", "LMIN", "LMAX");
		if (Xflag)
			printf(" %8s %8s %8s %8s", "SMIN", "SMAX", "SMEAN",
			    "SMEDIAN");
		printf("\n");
	}
	sosp_free = sosp = procstat_getsbobjects(procstat, kipp, &len);
	if (sosp == NULL)
		return;
	for (i = 0; i < (len / sizeof(*sosp)); i++, sosp++) {
		if (sosp->sos_objectid == SANDBOX_OBJECTID_FREE)
			continue;
		printf("%5d ", kipp->ki_pid);
		printf("%-10s ", kipp->ki_comm);
		printf("%-20s ", sosp->sos_class_name);
		print_sbobject_type(sosp->sos_object_type);
		print_sbobject_name(sosp->sos_object_type,
		    sosp->sos_object_name);
		printf("%6jd ", (uintmax_t)sosp->sos_stat_invoke);
		printf("%8jd ", (uintmax_t)sosp->sos_stat_minrun);
		printf("%8jd", (uintmax_t)sosp->sos_stat_maxrun);
		if (Xflag) {
			printf(" %8jd", sample_min(SANDBOX_METHOD_MAXSAMPVEC,
			    sosp->sos_stat_sampvec));
			printf(" %8jd", sample_max(SANDBOX_METHOD_MAXSAMPVEC,
			    sosp->sos_stat_sampvec));
			printf(" %8jd", sample_mean(SANDBOX_METHOD_MAXSAMPVEC,
			    sosp->sos_stat_sampvec));
			printf(" %8jd", sample_median(
			    SANDBOX_OBJECT_MAXSAMPVEC,
			    sosp->sos_stat_sampvec));
		}
		printf("\n");
	}
	procstat_freesbobjects(procstat, sosp_free);
}
