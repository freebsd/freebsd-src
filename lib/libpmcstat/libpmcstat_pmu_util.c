/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018, Matthew Macy
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
 *
 * $FreeBSD$
 *
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <pmc.h>
#include <pmclog.h>
#include <libpmcstat.h>
#include "pmu-events/pmu-events.h"

#if defined(__amd64__)
struct pmu_event_desc {
	uint32_t ped_umask;
	uint32_t ped_event;
	uint64_t ped_period;
};

static const struct pmu_events_map *
pmu_events_map_get(void)
{
	size_t s;
	char buf[64];
	const struct pmu_events_map *pme;

	if (sysctlbyname("kern.hwpmc.cpuid", (void *)NULL, &s,
					 (void *)NULL, 0) == -1)
		return (NULL);
	if (sysctlbyname("kern.hwpmc.cpuid", buf, &s,
					 (void *)NULL, 0) == -1)
		return (NULL);
	for (pme = pmu_events_map; pme->cpuid != NULL; pme++)
		if (strcmp(buf, pme->cpuid) == 0)
			return (pme);
	return (NULL);
}

static const struct pmu_event *
pmu_event_get(const char *event_name)
{
	const struct pmu_events_map *pme;
	const struct pmu_event *pe;

	if ((pme = pmu_events_map_get()) == NULL)
		return (NULL);
	for (pe = pme->table; pe->name != NULL; pe++)
		if (strcmp(pe->name, event_name) == 0)
			return (pe);
	return (NULL);
}

static int
pmu_parse_event(struct pmu_event_desc *ped, const char *eventin)
{
	char *event;
	char *kvp, *key, *value;

	if ((event = strdup(eventin)) == NULL)
		return (ENOMEM);
	bzero(ped, sizeof(*ped));
	while ((kvp = strsep(&event, ",")) != NULL) {
		key = strsep(&kvp, "=");
		if (key == NULL)
			abort();
		value = kvp;
		if (strcmp(key, "umask") == 0)
			ped->ped_umask = strtol(value, NULL, 16);
		if (strcmp(key, "event") == 0)
			ped->ped_event = strtol(value, NULL, 16);
		if (strcmp(key, "period") == 0)
			ped->ped_umask = strtol(value, NULL, 10);
	}
	free(event);
	return (0);
}

uint64_t
pmcstat_pmu_sample_rate_get(const char *event_name)
{
	const struct pmu_event *pe;
	struct pmu_event_desc ped;

	if ((pe = pmu_event_get(event_name)) == NULL)
		return (DEFAULT_SAMPLE_COUNT);
	if (pe->alias && (pe = pmu_event_get(pe->alias)) == NULL)
		return (DEFAULT_SAMPLE_COUNT);
	if (pe->event == NULL)
		return (DEFAULT_SAMPLE_COUNT);
	if (pmu_parse_event(&ped, pe->event))
		return (DEFAULT_SAMPLE_COUNT);
	return (ped.ped_period);
}

#else
uint64_t pmcstat_pmu_sample_rate_get(void) { return (DEFAULT_SAMPLE_COUNT); }
#endif
