/*	$OpenBSD: p4tcc.c,v 1.1 2003/12/20 18:23:18 tedu Exp $ */
/*-
 * Copyright (c) 2003 Ted Unangst
 * Copyright (c) 2004 Maxim Sobolev <sobomax@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Restrict power consumption by using thermal control circuit.
 * This operates independently of speedstep.
 * Found on Pentium 4 and later models (feature TM).
 *
 * References:
 * Intel Developer's manual v.3 #245472-012
 *
 * On some models, the cpu can hang if it's running at a slow speed.
 * Workarounds included below.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_cpu.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/power.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <machine/md_var.h>
#include <machine/specialreg.h>

static u_int			p4tcc_percentage;
static u_int			p4tcc_economy;
static u_int			p4tcc_performance;
static struct sysctl_ctx_list	p4tcc_sysctl_ctx;
static struct sysctl_oid	*p4tcc_sysctl_tree;

static struct {
	u_short level;
	u_short rlevel;
	u_short reg;
} tcc[] = {
	{ 88, 100, 0 },
	{ 75, 88,  7 },
	{ 63, 75,  6 },
	{ 50, 63,  5 },
	{ 38, 50,  4 },
	{ 25, 38,  3 },
	{ 13, 25,  2 },
	{ 0,  13,  1 }
};

#define TCC_LEVELS	sizeof(tcc) / sizeof(tcc[0])

static u_short
p4tcc_getperf(void)
{
	u_int64_t msreg;
	int i;

	msreg = rdmsr(MSR_THERM_CONTROL);
	msreg = (msreg >> 1) & 0x07;
	for (i = 0; i < TCC_LEVELS; i++) {
		if (msreg == tcc[i].reg)
			break;
	}

	return (tcc[i].rlevel);
}

static void
p4tcc_setperf(u_int percentage)
{
	int i;
	u_int64_t msreg;

	if (percentage > tcc[0].rlevel)
		percentage = tcc[0].rlevel;
	for (i = 0; i < TCC_LEVELS - 1; i++) {
		if (percentage > tcc[i].level)
			break;
	}

	msreg = rdmsr(MSR_THERM_CONTROL);
	msreg &= ~0x1e;	/* bit 0 reserved */
	if (tcc[i].reg != 0)
		msreg |= tcc[i].reg << 1 | 1 << 4;
	wrmsr(MSR_THERM_CONTROL, msreg);
}

static int
p4tcc_perf_sysctl(SYSCTL_HANDLER_ARGS)
{
	u_int percentage;
	int error;

	p4tcc_percentage = p4tcc_getperf();
	percentage = p4tcc_percentage;
	error = sysctl_handle_int(oidp, &percentage, 0, req);
	if (error || !req->newptr) {
		return (error);
	}
	if (p4tcc_percentage != percentage) {
		p4tcc_setperf(percentage);
	}

	return (error);
}

static void
p4tcc_power_profile(void *arg)
{
	u_int new;

	switch (power_profile_get_state()) {
	case POWER_PROFILE_PERFORMANCE:
		new = p4tcc_performance;
		break;
	case POWER_PROFILE_ECONOMY:
		new = p4tcc_economy;
		break;
	default:
		return;
	}

	if (p4tcc_getperf() != new) {
		p4tcc_setperf(new);
	}
}

static int
p4tcc_profile_sysctl(SYSCTL_HANDLER_ARGS)
{
	u_int32_t *argp;
	u_int32_t arg;
	int error;

	argp = (u_int32_t *)oidp->oid_arg1;
	arg = *argp;
	error = sysctl_handle_int(oidp, &arg, 0, req);

	/* error or no new value */
	if ((error != 0) || (req->newptr == NULL))
		return (error);

	/* range check */
	if (arg > tcc[0].rlevel)
		arg = tcc[0].rlevel;

	/* set new value and possibly switch */
	*argp = arg;

	p4tcc_power_profile(NULL);

	*argp = p4tcc_getperf();

	return (0);
}

static void
setup_p4tcc(void *dummy __unused)
{
	int nsteps, i;
	static char p4tcc_levels[(3 * TCC_LEVELS) + 1];
	char buf[4 + 1];

	if ((cpu_feature & (CPUID_ACPI | CPUID_TM)) !=
	    (CPUID_ACPI | CPUID_TM))
		return;

	nsteps = TCC_LEVELS;
	switch (cpu_id & 0xf) {
	case 0x22:	/* errata O50 P44 and Z21 */
	case 0x24:
	case 0x25:
	case 0x27:
	case 0x29:
		/* hang with 12.5 */
		tcc[TCC_LEVELS - 1] = tcc[TCC_LEVELS - 2];
		nsteps -= 1;
		break;
	case 0x07:	/* errata N44 and P18 */
	case 0x0a:
	case 0x12:
	case 0x13:
		/* hang at 12.5 and 25 */
		tcc[TCC_LEVELS - 1] = tcc[TCC_LEVELS - 2] = tcc[TCC_LEVELS - 3];
		nsteps -= 2;
		break;
	default:
		break;
	}

	p4tcc_levels[0] = '\0';
	for (i = nsteps; i > 0; i--) {
	    sprintf(buf, "%d%s", tcc[i - 1].rlevel, (i != 1) ? " " : "");
	    strcat(p4tcc_levels, buf);
	}

	p4tcc_economy = tcc[TCC_LEVELS - 1].rlevel;
	p4tcc_performance = tcc[0].rlevel;

	/*
	 * Since after the reboot the TCC is usually in the Automatic
	 * mode, in which reading current performance level is likely to
	 * produce bogus results make sure to switch it to the On-Demand
	 * mode and set to some known performance level. Unfortunately
	 * there is no reliable way to check that TCC is in the Automatic
	 * mode, reading bit 4 of ACPI Thermal Monitor Control Register
	 * produces 0 regardless of the current mode.
	 */
	p4tcc_setperf(p4tcc_performance);

	p4tcc_percentage = p4tcc_getperf();
	printf("Pentium 4 TCC support enabled, %d steps from 100%% to %d%%, "
	    "current performance %u%%\n", nsteps, p4tcc_economy,
	    p4tcc_percentage);

	sysctl_ctx_init(&p4tcc_sysctl_ctx);
	p4tcc_sysctl_tree = SYSCTL_ADD_NODE(&p4tcc_sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO, "p4tcc", CTLFLAG_RD, 0,
	    "Pentium 4 Thermal Control Circuitry support");
	SYSCTL_ADD_PROC(&p4tcc_sysctl_ctx,
	    SYSCTL_CHILDREN(p4tcc_sysctl_tree), OID_AUTO,
	    "cpuperf", CTLTYPE_INT | CTLFLAG_RW,
	    &p4tcc_percentage, 0, p4tcc_perf_sysctl, "I",
	    "CPU performance in % of maximum");
	SYSCTL_ADD_PROC(&p4tcc_sysctl_ctx,
	    SYSCTL_CHILDREN(p4tcc_sysctl_tree), OID_AUTO,
	    "cpuperf_performance", CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_RW,
	    &p4tcc_performance, 0, p4tcc_profile_sysctl, "I",
	    "CPU performance in % of maximum in Performance mode");
	SYSCTL_ADD_PROC(&p4tcc_sysctl_ctx,
	    SYSCTL_CHILDREN(p4tcc_sysctl_tree), OID_AUTO,
	    "cpuperf_economy", CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_RW,
	    &p4tcc_economy, 0, p4tcc_profile_sysctl, "I",
	    "CPU performance in % of maximum in Economy mode");
	SYSCTL_ADD_STRING(&p4tcc_sysctl_ctx,
	    SYSCTL_CHILDREN(p4tcc_sysctl_tree), OID_AUTO,
	    "cpuperf_levels", CTLFLAG_RD, p4tcc_levels, 0,
	    "Perormance levels supported by the Pentium 4 Thermal Control "
	    "Circuitry");

	/* register performance profile change handler */
	EVENTHANDLER_REGISTER(power_profile_change, p4tcc_power_profile, NULL, 0);
}
SYSINIT(setup_p4tcc, SI_SUB_CPU, SI_ORDER_ANY, setup_p4tcc, NULL);
