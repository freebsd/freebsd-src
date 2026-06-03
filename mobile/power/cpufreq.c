/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
 * All rights reserved.
 *
 * See cpufreq.h for full license text.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/kobj.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/filio.h>
#include <sys/ioccom.h>

#include <machine/cpufreq.h>
#include "cpufreq.h"
#include "../sensors/sensor.h"

#define CF_LOG(level, fmt, ...)		do {		\
	if (bootverbose)				\
		printf("cpufreq: " fmt "\n", ##__VA_ARGS__);\
} while (0)

FEATURE(module_mobile_cpufreq, "Mobile CPU frequency scaling subsystem");

static enum cf_governor cpu_govs[CF_MAX_CPUS];
static uint32_t cpu_online_map;
static uint32_t cpu_freq_map[CF_MAX_CPUS];

static const struct cf_governor_info governors[CF_GOV_MAX] = {
	[CF_GOV_ONDEMAND]	= { "ondemand",  CF_GOV_ONDEMAND, 0 },
	[CF_GOV_CONSERVATIVE]	= { "conservative", CF_GOV_CONSERVATIVE, 0 },
	[CF_GOV_POWERSAVE]	= { "powersave",  CF_GOV_POWERSAVE, 0 },
	[CF_GOV_PERFORMANCE]	= { "performance", CF_GOV_PERFORMANCE, 0 },
	[CF_GOV_SCHEDUTIL]	= { "schedutil",  CF_GOV_SCHEDUTIL, 0 },
};

static int
cf_read_sysfs_uint(const char *path, uint32_t *val)
{
	char buf[32];
	int fd, len, ret;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (errno);

	len = read(fd, buf, sizeof(buf) - 1);
	ret = close(fd);

	if (len <= 0)
		return (-1);

	buf[len] = '\0';
	*val = (uint32_t)strtoul(buf, NULL, 10);
	return (0);
}

static int
cf_write_sysfs(const char *path, const char *val)
{
	int fd, len, ret;

	fd = open(path, O_WRONLY);
	if (fd < 0)
		return (errno);

	len = (int)strlen(val);
	ret = write(fd, val, len);
	ret = close(fd);

	return (ret == len ? 0 : -1);
}

int
cf_init(void)
{
	uint32_t cpu;
	int error;

	for (cpu = 0; cpu < CF_MAX_CPUS; cpu++) {
		cpu_govs[cpu] = CF_GOV_ONDEMAND;
		cpu_freq_map[cpu] = 0;
	}

	cpu_online_map = 0;

	for (cpu = 0; cpu < mp_ncpus; cpu++) {
		error = cf_get_freqs(cpu, NULL);
		if (error == 0) {
			cpu_online_map |= (1U << cpu);
			CPU_SET(cpu, &all_cpu_setup);
			CF_LOG(LOG_INFO, "CPU %u registered with cpufreq", cpu);
		}
	}

	CF_LOG(LOG_INFO, "cpufreq subsystem initialized, %u CPUs",
	    mp_ncpus);
	return (0);
}

int
cf_get_freqs(uint32_t cpu, struct cf_freqs *freqs)
{
	char path[256];
	uint8_t data[4096];
	uint32_t count;
	int fd, len, i, j;

	snprintf(path, sizeof(path), CF_SYSFS_AVAIL, cpu);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (errno);

	len = read(fd, data, sizeof(data) - 1);
	close(fd);

	if (len <= 0)
		return (EIO);

	data[len] = '\0';

	count = 0;
	for (i = 0; i < len && count < CF_MAX_FREQS; i++) {
		while (i < len && (data[i] == ' ' || data[i] == '\t'))
			i++;
		if (i >= len)
			break;

		j = 0;
		while (i < len && data[i] >= '0' && data[i] <= '9')
			j = j * 10 + (data[i++] - '0');

		if (freqs != NULL)
			freqs->avail[count] = j;

		count++;
	}

	if (freqs != NULL)
		freqs->count = count;

	return (0);
}

int
cf_get_current(uint32_t cpu, struct cf_status *st)
{
	char path[256];
	int error;

	if (st == NULL)
		return (EINVAL);

	st->current_freq = 0;
	st->min_freq = 0;
	st->max_freq = 0;
	st->transition_latency = 0;
	st->online = (cpu_online_map & (1U << cpu)) ? 1 : 0;

	if (!st->online)
		return (ENXIO);

	snprintf(path, sizeof(path), CF_SYSFS_CUR, cpu);
	error = cf_read_sysfs_uint(path, &st->current_freq);
	if (error)
		return (error);

	snprintf(path, sizeof(path), CF_SYSFS_MIN, cpu);
	error = cf_read_sysfs_uint(path, &st->min_freq);
	if (error)
		st->min_freq = st->current_freq;

	snprintf(path, sizeof(path), CF_SYSFS_MAX, cpu);
	error = cf_read_sysfs_uint(path, &st->max_freq);
	if (error)
		st->max_freq = st->current_freq;

	CF_LOG(LOG_DEBUG, "CPU %u freq: %u kHz (min %u, max %u)", cpu,
	    st->current_freq, st->min_freq, st->max_freq);

	return (0);
}

int
cf_set_freq(uint32_t cpu, uint32_t freq_khz)
{
	char path[256];
	char buf[32];
	struct cf_status st;
	int error;

	if (!(cpu_online_map & (1U << cpu)))
		return (ENXIO);

	error = cf_get_current(cpu, &st);
	if (error)
		return (error);

	if (freq_khz < st.min_freq || freq_khz > st.max_freq) {
		CF_LOG(LOG_WARNING, "CPU %u: frequency %u kHz out of range "
		    "[%u, %u]", cpu, freq_khz, st.min_freq, st.max_freq);
		return (EINVAL);
	}

	snprintf(buf, sizeof(buf), "%u", freq_khz);
	snprintf(path, sizeof(path), CF_SYSFS_MIN, cpu);
	error = cf_write_sysfs(path, buf);
	if (error)
		return (error);

	snprintf(path, sizeof(path), CF_SYSFS_MAX, cpu);
	error = cf_write_sysfs(path, buf);
	if (error)
		return (error);

	cpu_freq_map[cpu] = freq_khz;
	CF_LOG(LOG_INFO, "CPU %u frequency set to %u kHz", cpu, freq_khz);

	return (0);
}

int
cf_set_governor(uint32_t cpu, const char *gov_name)
{
	enum cf_governor gov;
	char path[256];
	char govbuf[32];
	int i, error;

	if (gov_name == NULL)
		return (EINVAL);

	for (i = 0; i < CF_GOV_MAX; i++) {
		if (strcmp(governors[i].name, gov_name) == 0) {
			gov = governors[i].gov;
			break;
		}
		if (strncmp(governors[i].name, gov_name, strlen(gov_name)) == 0)
			gov = governors[i].gov;
	}

	if (i >= CF_GOV_MAX)
		return (EINVAL);

	snprintf(path, sizeof(path), CF_SYSFS_GOV, cpu);
	strlcpy(govbuf, gov_name, sizeof(govbuf));
	error = cf_write_sysfs(path, govbuf);
	if (error)
		return (error);

	cpu_govs[cpu] = gov;
	CF_LOG(LOG_INFO, "CPU %u governor set to %s", cpu, gov_name);

	return (0);
}

int
cf_set_governor_enum(uint32_t cpu, enum cf_governor gov)
{
	if (gov >= CF_GOV_MAX)
		return (EINVAL);

	cpu_govs[cpu] = gov;
	return (cf_set_governor(cpu, governors[gov].name));
}

int
cf_cpu_online(uint32_t cpu)
{
	if (cpu >= CF_MAX_CPUS)
		return (EINVAL);

	cpu_online_map |= (1U << cpu);
	return (0);
}

int
cf_cpu_offline(uint32_t cpu)
{
	struct cf_status st;

	if (cpu >= CF_MAX_CPUS)
		return (EINVAL);

	if (cf_get_current(cpu, &st) == 0 && st.online)
		cpu_online_map &= ~(1U << cpu);

	return (0);
}

int
cf_get_policy(uint32_t cpu, struct cf_policy *pol)
{
	int error;

	if (pol == NULL || cpu >= CF_MAX_CPUS)
		return (EINVAL);

	pol->cpu_id = cpu;
	pol->gov = cpu_govs[cpu];

	error = cf_get_current(cpu, &pol->status);
	if (error)
		return (error);

	error = cf_get_freqs(cpu, &pol->freqs);
	if (error)
		return (error);

	pol->min_freq = pol->status.min_freq;
	pol->max_freq = pol->status.max_freq;
	pol->cur_freq = pol->status.current_freq;

	return (0);
}

int
cf_set_policy(uint32_t cpu, const struct cf_policy *pol)
{
	int error;

	if (pol == NULL || cpu >= CF_MAX_CPUS)
		return (EINVAL);

	error = cf_set_governor_enum(cpu, pol->gov);
	if (error)
		return (error);

	if (pol->min_freq > 0 || pol->max_freq > 0) {
		char path[256], buf[32];

		snprintf(buf, sizeof(buf), "%u", pol->min_freq ?
		    pol->min_freq : pol->status.min_freq);
		snprintf(path, sizeof(path), CF_SYSFS_MIN, cpu);
		error = cf_write_sysfs(path, buf);
		if (error)
			return (error);

		snprintf(buf, sizeof(buf), "%u", pol->max_freq ?
		    pol->max_freq : pol->status.max_freq);
		snprintf(path, sizeof(path), CF_SYSFS_MAX, cpu);
		error = cf_write_sysfs(path, buf);
		if (error)
			return (error);
	}

	return (0);
}

const char *
cf_governor_name(enum cf_governor gov)
{
	if (gov >= CF_GOV_MAX)
		return (NULL);
	return (governors[gov].name);
}

enum cf_governor
cf_governor_from_name(const char *name)
{
	int i;

	if (name == NULL)
		return (CF_GOV_MAX);

	for (i = 0; i < CF_GOV_MAX; i++) {
		if (strcmp(governors[i].name, name) == 0)
			return (governors[i].gov);
	}

	return (CF_GOV_MAX);
}
