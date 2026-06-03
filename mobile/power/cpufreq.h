/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions of binary form must reproduce the above copyright
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

#ifndef _MOBILE_POWER_CPUFREQ_H_
#define _MOBILE_POWER_CPUFREQ_H_

#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>

#define CF_MAX_CPUS		256
#define CF_MAX_FREQS		64
#define CF_MAX_GOVNAMES		16

struct cf_freqs {
	uint32_t	count;
	uint32_t	avail[CF_MAX_FREQS];
};

struct cf_status {
	uint32_t	current_freq;
	uint32_t	min_freq;
	uint32_t	max_freq;
	uint32_t	transition_latency;
	uint8_t		online;
};

enum cf_governor {
	CF_GOV_ONDEMAND,
	CF_GOV_CONSERVATIVE,
	CF_GOV_POWERSAVE,
	CF_GOV_PERFORMANCE,
	CF_GOV_SCHEDUTIL,
	CF_GOV_MAX
};

struct cf_governor_info {
	char		name[CF_MAX_GOVNAMES];
	enum cf_governor	 gov;
	uint32_t	flags;
};

struct cf_policy {
	uint32_t	cpu_id;
	uint32_t	min_freq;
	uint32_t	max_freq;
	uint32_t	cur_freq;
	enum cf_governor	 gov;
	struct cf_freqs	freqs;
	struct cf_status	status;
};

#define CPUFREQ_IOCTL_GETFREQS		_IOWR('C', 0x01, struct cf_freqs)
#define CPUFREQ_IOCTL_GETSTATUS		_IOWR('C', 0x02, struct cf_status)
#define CPUFREQ_IOCTL_SETFREQ		_IOW('C',  0x03, uint32_t)
#define CPUFREQ_IOCTL_GETGOV		_IOWR('C', 0x04, enum cf_governor)
#define CPUFREQ_IOCTL_SETGOV		_IOW('C',  0x05, enum cf_governor)
#define CPUFREQ_IOCTL_ONLINE		_IOW('C',  0x06, uint32_t)
#define CPUFREQ_IOCTL_OFFLINE		_IOW('C',  0x07, uint32_t)
#define CPUFREQ_IOCTL_GETPOLICY		_IOWR('C', 0x08, struct cf_policy)
#define CPUFREQ_IOCTL_SETPOLICY		_IOW('C',  0x09, struct cf_policy)

#define CF_SYSFS_BASE		"/sys/devices/system/cpu/cpufreq"
#define CF_SYSFS_AVAIL		CF_SYSFS_BASE "/policy%d/scaling_available_frequencies"
#define CF_SYSFS_CUR		CF_SYSFS_BASE "/policy%d/scaling_cur_freq"
#define CF_SYSFS_MIN		CF_SYSFS_BASE "/policy%d/scaling_min_freq"
#define CF_SYSFS_MAX		CF_SYSFS_BASE "/policy%d/scaling_max_freq"
#define CF_SYSFS_GOV		CF_SYSFS_BASE "/policy%d/scaling_governor"
#define CF_SYSFS_GOVS		CF_SYSFS_BASE "/policy%d/scaling_available_governors"

int cf_init(void);
int cf_get_freqs(uint32_t cpu, struct cf_freqs *freqs);
int cf_get_current(uint32_t cpu, struct cf_status *st);
int cf_set_freq(uint32_t cpu, uint32_t freq_khz);
int cf_set_governor(uint32_t cpu, const char *gov_name);
int cf_set_governor_enum(uint32_t cpu, enum cf_governor gov);
int cf_cpu_online(uint32_t cpu);
int cf_cpu_offline(uint32_t cpu);
int cf_get_policy(uint32_t cpu, struct cf_policy *pol);
int cf_set_policy(uint32_t cpu, const struct cf_policy *pol);
const char *cf_governor_name(enum cf_governor gov);
enum cf_governor cf_governor_from_name(const char *name);

#endif
