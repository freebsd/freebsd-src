/*
 * CPU Frequency Scaling Driver
 * Implements CPU frequency scaling for power management
 */

#include "cpufreq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX_CPUS 8
#define MAX_FREQS 32
#define SYSFS_CPU "/sys/devices/system/cpu"
#define SYSFS_CPUFREQ SYSFS_CPU "/cpufreq"

static struct cpufreq_policy policies[MAX_CPUS];
static int policy_count = 0;

static const char *governor_names[] = {
    "performance", "powersave", "ondemand", "conservative",
    "schedutil", "userspace", NULL
};

int cf_init(void) {
    policy_count = 0;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++) {
        struct cpufreq_policy *p = &policies[cpu];
        p->cpu_id = cpu;
        p->current_freq = 1800000;
        p->min_freq = 400000;
        p->max_freq = 2500000;
        p->governor = GOV_PERFORMANCE;
        p->online = (cpu == 0);
        p->available_count = 8;
        uint32_t freqs[] = {400000, 600000, 800000, 1000000, 1200000,
                           1400000, 1600000, 1800000, 2000000, 2200000, 2500000};
        memcpy(p->available, freqs, sizeof(freqs));
        p->available_count = 11;
        if (policy_count < MAX_CPUS) policy_count++;
    }
    return 0;
}

int cf_get_count(void) {
    return policy_count;
}

struct cpufreq_policy *cf_get_policy(int cpu) {
    if (cpu < 0 || cpu >= policy_count)
        return NULL;
    return &policies[cpu];
}

int cf_get_freqs(int cpu, uint32_t **freqs, int *count) {
    struct cpufreq_policy *p = cf_get_policy(cpu);
    if (!p || !freqs || !count)
        return -1;
    *freqs = p->available;
    *count = p->available_count;
    return 0;
}

uint32_t cf_get_current(int cpu) {
    struct cpufreq_policy *p = cf_get_policy(cpu);
    return p ? p->current_freq : 0;
}

int cf_set_freq(int cpu, uint32_t freq_khz) {
    struct cpufreq_policy *p = cf_get_policy(cpu);
    if (!p)
        return -1;
    if (freq_khz < p->min_freq || freq_khz > p->max_freq)
        return -1;
    int valid = 0;
    for (int i = 0; i < p->available_count; i++) {
        if (p->available[i] == freq_khz) {
            valid = 1;
            break;
        }
    }
    if (!valid)
        return -1;
    p->current_freq = freq_khz;
    return 0;
}

int cf_set_governor(int cpu, const char *gov_name) {
    struct cpufreq_policy *p = cf_get_policy(cpu);
    if (!p || !gov_name)
        return -1;
    for (int i = 0; governor_names[i]; i++) {
        if (strcmp(governor_names[i], gov_name) == 0) {
            p->governor = (cpufreq_governor_t)i;
            return 0;
        }
    }
    return -1;
}

const char *cf_get_governor_name(int cpu) {
    struct cpufreq_policy *p = cf_get_policy(cpu);
    if (!p)
        return NULL;
    int idx = (int)p->governor;
    if (idx < 0 || !governor_names[idx])
        return "unknown";
    return governor_names[idx];
}

int cf_set_cpu_online(int cpu, int online) {
    if (cpu < 0 || cpu >= MAX_CPUS)
        return -1;
    if (cpu == 0)
        return -1;
    policies[cpu].online = online;
    return 0;
}

int cf_get_cpu_online(int cpu) {
    struct cpufreq_policy *p = cf_get_policy(cpu);
    return p ? p->online : 0;
}

void cf_cleanup(void) {
    policy_count = 0;
    memset(policies, 0, sizeof(policies));
}
