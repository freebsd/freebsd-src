/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005 Nate Lawson
 * Copyright (c) 2004 Colin Percival
 * Copyright (c) 2004-2005 Bruno Durcot
 * Copyright (c) 2004 FUKUDA Nobuhiko
 * Copyright (c) 2009 Michael Reifenberger
 * Copyright (c) 2009 Norikatsu Shigemura
 * Copyright (c) 2008-2009 Gen Otsuji
 * Copyright (c) 2025 ShengYi Hung
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * Portions of this software were developed by Olivier Certner
 * <olce@FreeBSD.org> at Kumacom SARL under sponsorship from the FreeBSD
 * Foundation.
 *
 * This code is depending on kern_cpu.c, est.c, powernow.c, p4tcc.c, smist.c
 * in various parts. The authors of these files are Nate Lawson,
 * Colin Percival, Bruno Durcot, and FUKUDA Nobuhiko.
 * This code contains patches by Michael Reifenberger and Norikatsu Shigemura.
 * Thank you.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * For more info:
 * BIOS and Kernel Developer's Guide(BKDG) for AMD Family 10h Processors
 * 31116 Rev 3.20  February 04, 2009
 * BIOS and Kernel Developer's Guide(BKDG) for AMD Family 11h Processors
 * 41256 Rev 3.00 - July 07, 2008
 * Processor Programming Reference (PPR) for AMD Family 1Ah Model 02h,
 * Revision C1 Processors Volume 1 of 7 - Sep 29, 2024
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/sched.h>
#include <sys/smp.h>

#include <machine/_inttypes.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>

#include <x86/cpufreq/hwpstate_common.h>

#include "acpi_if.h"
#include "cpufreq_if.h"


#define	MSR_AMD_10H_11H_LIMIT	0xc0010061
#define	MSR_AMD_10H_11H_CONTROL	0xc0010062
#define	MSR_AMD_10H_11H_STATUS	0xc0010063
#define	MSR_AMD_10H_11H_CONFIG	0xc0010064

#define	MSR_AMD_CPPC_CAPS_1	0xc00102b0
#define	MSR_AMD_CPPC_ENABLE	0xc00102b1
#define	MSR_AMD_CPPC_CAPS_2	0xc00102b2
#define	MSR_AMD_CPPC_REQUEST	0xc00102b3
#define	MSR_AMD_CPPC_STATUS	0xc00102b4

#define	MSR_AMD_CPPC_CAPS_1_NAME	"CPPC_CAPABILITY_1"
#define	MSR_AMD_CPPC_ENABLE_NAME	"CPPC_ENABLE"
#define	MSR_AMD_CPPC_REQUEST_NAME	"CPPC_REQUEST"

#define	MSR_AMD_PWR_ACC		0xc001007a
#define	MSR_AMD_PWR_ACC_MX	0xc001007b

#define	AMD_10H_11H_MAX_STATES	16

/* for MSR_AMD_10H_11H_LIMIT C001_0061 */
#define	AMD_10H_11H_GET_PSTATE_MAX_VAL(msr)	(((msr) >> 4) & 0x7)
#define	AMD_10H_11H_GET_PSTATE_LIMIT(msr)	(((msr)) & 0x7)
/* for MSR_AMD_10H_11H_CONFIG 10h:C001_0064:68 / 11h:C001_0064:6B */
#define	AMD_10H_11H_CUR_VID(msr)		(((msr) >> 9) & 0x7F)
#define	AMD_10H_11H_CUR_DID(msr)		(((msr) >> 6) & 0x07)
#define	AMD_10H_11H_CUR_FID(msr)		((msr) & 0x3F)

#define	AMD_17H_CUR_IDIV(msr)			(((msr) >> 30) & 0x03)
#define	AMD_17H_CUR_IDD(msr)			(((msr) >> 22) & 0xFF)
#define	AMD_17H_CUR_VID(msr)			(((msr) >> 14) & 0xFF)
#define	AMD_17H_CUR_DID(msr)			(((msr) >> 8) & 0x3F)
#define	AMD_17H_CUR_FID(msr)			((msr) & 0xFF)

#define	AMD_1AH_CUR_FID(msr)			((msr) & 0xFFF)

#define	AMD_CPPC_CAPS_1_HIGHEST_PERF_BITS	0xff000000
#define	AMD_CPPC_CAPS_1_NOMINAL_PERF_BITS	0x00ff0000
#define	AMD_CPPC_CAPS_1_EFFICIENT_PERF_BITS	0x0000ff00
#define	AMD_CPPC_CAPS_1_LOWEST_PERF_BITS	0x000000ff

#define	AMD_CPPC_REQUEST_EPP_BITS		0xff000000
#define	AMD_CPPC_REQUEST_DES_PERF_BITS		0x00ff0000
#define	AMD_CPPC_REQUEST_MIN_PERF_BITS		0x0000ff00
#define	AMD_CPPC_REQUEST_MAX_PERF_BITS		0x000000ff

#define	HWP_AMD_CLASSNAME			"hwpstate_amd"

#define	BITS_VALUE(bits, val)						\
	(((val) & (bits)) >> (ffsll((bits)) - 1))
#define	BITS_WITH_VALUE(bits, val)					\
	(((uintmax_t)(val) << (ffsll((bits)) - 1)) & (bits))
#define	SET_BITS_VALUE(var, bits, val)					\
	((var) = ((var) & ~(bits)) | BITS_WITH_VALUE((bits), (val)))

#define	HWPSTATE_DEBUG(dev, msg...)			\
	do {						\
		if (hwpstate_verbose)			\
			device_printf(dev, msg);	\
	} while (0)

struct hwpstate_setting {
	int	freq;		/* CPU clock in Mhz or 100ths of a percent. */
	int	volts;		/* Voltage in mV. */
	int	power;		/* Power consumed in mW. */
	int	lat;		/* Transition latency in us. */
	int	pstate_id;	/* P-State id */
};

#define HWPFL_USE_CPPC			(1 << 0)
#define HWPFL_CPPC_REQUEST_NOT_READ	(1 << 1)

/*
 * Atomicity is achieved by only modifying a given softc on its associated CPU
 * and with interrupts disabled.
 *
 * XXX - Only the CPPC support complies at the moment.
 */
struct hwpstate_softc {
	device_t	dev;
	u_int		flags;
	union {
		struct {
			struct hwpstate_setting
			hwpstate_settings[AMD_10H_11H_MAX_STATES];
			int cfnum;
		};
		struct {
			uint64_t request;
		} cppc;
	};
};

static void	hwpstate_identify(driver_t *driver, device_t parent);
static int	hwpstate_probe(device_t dev);
static int	hwpstate_attach(device_t dev);
static int	hwpstate_detach(device_t dev);
static int	hwpstate_set(device_t dev, const struct cf_setting *cf);
static int	hwpstate_get(device_t dev, struct cf_setting *cf);
static int	hwpstate_settings(device_t dev, struct cf_setting *sets, int *count);
static int	hwpstate_type(device_t dev, int *type);
static int	hwpstate_shutdown(device_t dev);
static int	hwpstate_features(driver_t *driver, u_int *features);
static int	hwpstate_get_info_from_acpi_perf(device_t dev, device_t perf_dev);
static int	hwpstate_get_info_from_msr(device_t dev);
static int	hwpstate_goto_pstate(device_t dev, int pstate_id);

static int	hwpstate_verify;
SYSCTL_INT(_debug, OID_AUTO, hwpstate_verify, CTLFLAG_RWTUN,
    &hwpstate_verify, 0, "Verify P-state after setting");

static bool	hwpstate_pstate_limit;
SYSCTL_BOOL(_debug, OID_AUTO, hwpstate_pstate_limit, CTLFLAG_RWTUN,
    &hwpstate_pstate_limit, 0,
    "If enabled (1), limit administrative control of P-states to the value in "
    "CurPstateLimit");

static bool	hwpstate_amd_cppc_enable = true;
SYSCTL_BOOL(_machdep, OID_AUTO, hwpstate_amd_cppc_enable, CTLFLAG_RDTUN,
    &hwpstate_amd_cppc_enable, 0,
    "Set 1 (default) to enable AMD CPPC, 0 to disable");

static device_method_t hwpstate_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	hwpstate_identify),
	DEVMETHOD(device_probe,		hwpstate_probe),
	DEVMETHOD(device_attach,	hwpstate_attach),
	DEVMETHOD(device_detach,	hwpstate_detach),
	DEVMETHOD(device_shutdown,	hwpstate_shutdown),

	/* cpufreq interface */
	DEVMETHOD(cpufreq_drv_set,	hwpstate_set),
	DEVMETHOD(cpufreq_drv_get,	hwpstate_get),
	DEVMETHOD(cpufreq_drv_settings,	hwpstate_settings),
	DEVMETHOD(cpufreq_drv_type,	hwpstate_type),

	/* ACPI interface */
	DEVMETHOD(acpi_get_features,	hwpstate_features),
	{0, 0}
};

static inline void
check_cppc_in_use(const struct hwpstate_softc *const sc, const char *const func)
{
	KASSERT((sc->flags & HWPFL_USE_CPPC) != 0, (HWP_AMD_CLASSNAME
	    ": %s() called but HWPFL_USE_CPPC not set", func));
}

static void
print_msr_bits(struct sbuf *const sb, const char *const legend,
    const uint64_t bits, const uint64_t msr_value)
{
	sbuf_printf(sb, "\t%s: %" PRIu64 "\n", legend,
	    BITS_VALUE(bits, msr_value));
}

static void
print_cppc_caps_1(struct sbuf *const sb, const uint64_t caps)
{
	sbuf_printf(sb, MSR_AMD_CPPC_CAPS_1_NAME ": %#016" PRIx64 "\n", caps);
	print_msr_bits(sb, "Highest Performance",
	    AMD_CPPC_CAPS_1_HIGHEST_PERF_BITS, caps);
	print_msr_bits(sb, "Guaranteed Performance",
	    AMD_CPPC_CAPS_1_NOMINAL_PERF_BITS, caps);
	print_msr_bits(sb, "Efficient Performance",
	    AMD_CPPC_CAPS_1_EFFICIENT_PERF_BITS, caps);
	print_msr_bits(sb, "Lowest Performance",
	    AMD_CPPC_CAPS_1_LOWEST_PERF_BITS, caps);
}

#define MSR_NOT_READ_MSG	"Not read (fault or previous errors)"

static void
print_cppc_no_caps_1(struct sbuf *const sb)
{
	sbuf_printf(sb, MSR_AMD_CPPC_CAPS_1_NAME ": " MSR_NOT_READ_MSG "\n");
}

static void
print_cppc_request(struct sbuf *const sb, const uint64_t request)
{
	sbuf_printf(sb, MSR_AMD_CPPC_REQUEST_NAME ": %#016" PRIx64 "\n",
	    request);
	print_msr_bits(sb, "Efficiency / Energy Preference",
	    AMD_CPPC_REQUEST_EPP_BITS, request);
	print_msr_bits(sb, "Desired Performance",
	    AMD_CPPC_REQUEST_DES_PERF_BITS, request);
	print_msr_bits(sb, "Minimum Performance",
	    AMD_CPPC_REQUEST_MIN_PERF_BITS, request);
	print_msr_bits(sb, "Maximum Performance",
	    AMD_CPPC_REQUEST_MAX_PERF_BITS, request);
}

static void
print_cppc_no_request(struct sbuf *const sb)
{
	sbuf_printf(sb, MSR_AMD_CPPC_REQUEST_NAME ": " MSR_NOT_READ_MSG "\n");
}

/*
 * Internal errors conveyed by code executing on another CPU.
 */
#define HWP_ERROR_CPPC_ENABLE		(1 << 0)
#define HWP_ERROR_CPPC_CAPS		(1 << 1)
#define HWP_ERROR_CPPC_REQUEST		(1 << 2)
#define HWP_ERROR_CPPC_REQUEST_WRITE	(1 << 3)

static inline bool
hwp_has_error(u_int res, u_int err)
{
	return ((res & err) != 0);
}

struct get_cppc_regs_data {
	uint64_t enable;
	uint64_t caps;
	uint64_t req;
	/* HWP_ERROR_CPPC_* except HWP_ERROR_*_WRITE */
	u_int res;
};

static void
get_cppc_regs_cb(void *args)
{
	struct get_cppc_regs_data *data = args;
	int error;

	data->res = 0;

	error = rdmsr_safe(MSR_AMD_CPPC_ENABLE, &data->enable);
	if (error != 0)
		data->res |= HWP_ERROR_CPPC_ENABLE;

	error = rdmsr_safe(MSR_AMD_CPPC_CAPS_1, &data->caps);
	if (error != 0)
		data->res |= HWP_ERROR_CPPC_CAPS;

	error = rdmsr_safe(MSR_AMD_CPPC_REQUEST, &data->req);
	if (error != 0)
		data->res |= HWP_ERROR_CPPC_REQUEST;
}

/*
 * Debug: Read all MSRs (bypassing the softc) and dump them.
 */
static int
sysctl_cppc_dump_handler(SYSCTL_HANDLER_ARGS)
{
	const struct hwpstate_softc *const sc = arg1;
	const device_t dev = sc->dev;
	const u_int cpuid = cpu_get_pcpu(dev)->pc_cpuid;
	struct sbuf *sb;
	struct sbuf sbs;
	struct get_cppc_regs_data data;
	int error;

	/* Sysctl knob does not exist if HWPFL_USE_CPPC is not set. */
	check_cppc_in_use(sc, __func__);

	sb = sbuf_new_for_sysctl(&sbs, NULL, 0, req);

	smp_rendezvous_cpu(cpuid, smp_no_rendezvous_barrier, get_cppc_regs_cb,
	    smp_no_rendezvous_barrier, &data);

	if (hwp_has_error(data.res, HWP_ERROR_CPPC_ENABLE))
		sbuf_printf(sb, "CPU%u: " MSR_AMD_CPPC_ENABLE_NAME ": "
		    MSR_NOT_READ_MSG "\n", cpuid);
	else
		sbuf_printf(sb, "CPU%u: HWP %sabled (" MSR_AMD_CPPC_REQUEST_NAME
		    ": %#" PRIx64 ")\n", cpuid, data.enable & 1 ? "En" : "Dis",
		    data.enable);

	if (hwp_has_error(data.res, HWP_ERROR_CPPC_CAPS))
		print_cppc_no_caps_1(sb);
	else
		print_cppc_caps_1(sb, data.caps);

	if (hwp_has_error(data.res, HWP_ERROR_CPPC_REQUEST))
		print_cppc_no_request(sb);
	else
		print_cppc_request(sb, data.req);

	error = sbuf_finish(sb);
	sbuf_delete(sb);

	return (error);
}

/*
 * Read CPPC_REQUEST's value in the softc, if not already present.
 */
static int
get_cppc_request(struct hwpstate_softc *const sc)
{
	uint64_t val;
	int error;

	check_cppc_in_use(sc, __func__);

	if ((sc->flags & HWPFL_CPPC_REQUEST_NOT_READ) != 0) {
		error = rdmsr_safe(MSR_AMD_CPPC_REQUEST, &val);
		if (error != 0)
			return (EIO);
		sc->flags &= ~HWPFL_CPPC_REQUEST_NOT_READ;
		sc->cppc.request = val;
	}

	return (0);
}

struct set_cppc_request_cb {
	struct hwpstate_softc	*sc;
	uint64_t		 request;
	uint64_t		 mask;
	int			 res; /* 0 or HWP_ERROR_CPPC_REQUEST* */
};

static void
set_cppc_request_cb(void *args)
{
	struct set_cppc_request_cb *const data = args;
	uint64_t *const sc_req = &data->sc->cppc.request;
	uint64_t new_req;
	int error;

	/* We proceed sequentially, so we'll clear out errors on progress. */
	data->res = HWP_ERROR_CPPC_REQUEST | HWP_ERROR_CPPC_REQUEST_WRITE;

	error = get_cppc_request(data->sc);
	if (error != 0)
		return;
	data->res &= ~HWP_ERROR_CPPC_REQUEST;

	new_req = (*sc_req & ~data->mask) | (data->request & data->mask);

	error = wrmsr_safe(MSR_AMD_CPPC_REQUEST, new_req);
	if (error != 0)
		return;
	data->res &= ~HWP_ERROR_CPPC_REQUEST_WRITE;
	*sc_req = new_req;
}

static inline void
set_cppc_request_send_one(struct set_cppc_request_cb *const data, device_t dev)
{
	const u_int cpuid = cpu_get_pcpu(dev)->pc_cpuid;

	data->sc = device_get_softc(dev);
	smp_rendezvous_cpu(cpuid, smp_no_rendezvous_barrier,
	    set_cppc_request_cb, smp_no_rendezvous_barrier, data);
}

static inline void
set_cppc_request_update_error(const struct set_cppc_request_cb *const data,
    int *const error)
{
	/* A read error has precedence on a write error. */
	if (hwp_has_error(data->res, HWP_ERROR_CPPC_REQUEST))
		*error = EIO;
	else if (hwp_has_error(data->res, HWP_ERROR_CPPC_REQUEST_WRITE) &&
	    *error != EIO)
		*error = EOPNOTSUPP;
	else if (data->res != 0)
		/* Fallback case (normally not needed; defensive). */
		*error = EFAULT;
}

static int
set_cppc_request(device_t hwp_dev, uint64_t request, uint64_t mask)
{
	struct set_cppc_request_cb data = {
		.request = request,
		.mask = mask,
		/* 'sc' filled by set_cppc_request_send_one(). */
	};
	int error = 0;

	if (hwpstate_pkg_ctrl_enable) {
		const devclass_t dc = devclass_find(HWP_AMD_CLASSNAME);
		const int units = devclass_get_maxunit(dc);

		for (int i = 0; i < units; ++i) {
			const device_t dev = devclass_get_device(dc, i);

			set_cppc_request_send_one(&data, dev);
			/* Note errors, but always continue. */
			set_cppc_request_update_error(&data, &error);
		}
	} else {
		set_cppc_request_send_one(&data, hwp_dev);
		set_cppc_request_update_error(&data, &error);
	}

	return (error);
}

static void
get_cppc_request_cb(void *args)
{
	struct hwpstate_softc *const sc = args;

	(void)get_cppc_request(sc);
}

static int
sysctl_cppc_request_field_handler(SYSCTL_HANDLER_ARGS)
{
	const u_int max = BITS_VALUE(arg2, (uint64_t)-1);
	const device_t dev = arg1;
	struct hwpstate_softc *const sc = device_get_softc(dev);
	u_int val;
	int error;

	/* Sysctl knob does not exist if HWPFL_USE_CPPC is not set. */
	check_cppc_in_use(sc, __func__);

	if ((sc->flags & HWPFL_CPPC_REQUEST_NOT_READ) != 0) {
		const u_int cpuid = cpu_get_pcpu(dev)->pc_cpuid;

		smp_rendezvous_cpu(cpuid, smp_no_rendezvous_barrier,
		    get_cppc_request_cb, smp_no_rendezvous_barrier, sc);

		if ((sc->flags & HWPFL_CPPC_REQUEST_NOT_READ) != 0)
			return (EIO);
	}

	val = BITS_VALUE(arg2, sc->cppc.request);

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (val > max)
		return (EINVAL);
	error = set_cppc_request(dev, BITS_WITH_VALUE(arg2, val),
	    BITS_WITH_VALUE(arg2, -1));
	return (error);
}

static driver_t hwpstate_driver = {
	HWP_AMD_CLASSNAME,
	hwpstate_methods,
	sizeof(struct hwpstate_softc),
};

DRIVER_MODULE(hwpstate, cpu, hwpstate_driver, 0, 0);

static int
hwpstate_amd_iscale(int val, int div)
{
	switch (div) {
	case 3: /* divide by 1000 */
		val /= 10;
	case 2: /* divide by 100 */
		val /= 10;
	case 1: /* divide by 10 */
		val /= 10;
	case 0: /* divide by 1 */
	    ;
	}

	return (val);
}

/*
 * Go to Px-state on all cpus, considering the limit register (if so
 * configured).
 */
static int
hwpstate_goto_pstate(device_t dev, int id)
{
	sbintime_t sbt;
	uint64_t msr;
	int cpu, i, j, limit;

	if (hwpstate_pstate_limit) {
		/* get the current pstate limit */
		msr = rdmsr(MSR_AMD_10H_11H_LIMIT);
		limit = AMD_10H_11H_GET_PSTATE_LIMIT(msr);
		if (limit > id) {
			HWPSTATE_DEBUG(dev, "Restricting requested P%d to P%d "
			    "due to HW limit\n", id, limit);
			id = limit;
		}
	}

	cpu = curcpu;
	HWPSTATE_DEBUG(dev, "setting P%d-state on cpu%d\n", id, cpu);
	/* Go To Px-state */
	wrmsr(MSR_AMD_10H_11H_CONTROL, id);

	/*
	 * We are going to the same Px-state on all cpus.
	 * Probably should take _PSD into account.
	 */
	CPU_FOREACH(i) {
		if (i == cpu)
			continue;

		/* Bind to each cpu. */
		thread_lock(curthread);
		sched_bind(curthread, i);
		thread_unlock(curthread);
		HWPSTATE_DEBUG(dev, "setting P%d-state on cpu%d\n", id, i);
		/* Go To Px-state */
		wrmsr(MSR_AMD_10H_11H_CONTROL, id);
	}

	/*
	 * Verify whether each core is in the requested P-state.
	 */
	if (hwpstate_verify) {
		CPU_FOREACH(i) {
			thread_lock(curthread);
			sched_bind(curthread, i);
			thread_unlock(curthread);
			/* wait loop (100*100 usec is enough ?) */
			for (j = 0; j < 100; j++) {
				/* get the result. not assure msr=id */
				msr = rdmsr(MSR_AMD_10H_11H_STATUS);
				if (msr == id)
					break;
				sbt = SBT_1MS / 10;
				tsleep_sbt(dev, PZERO, "pstate_goto", sbt,
				    sbt >> tc_precexp, 0);
			}
			HWPSTATE_DEBUG(dev, "result: P%d-state on cpu%d\n",
			    (int)msr, i);
			if (msr != id) {
				HWPSTATE_DEBUG(dev,
				    "error: loop is not enough.\n");
				return (ENXIO);
			}
		}
	}

	return (0);
}

static int
hwpstate_set(device_t dev, const struct cf_setting *cf)
{
	struct hwpstate_softc *sc;
	struct hwpstate_setting *set;
	int i;

	if (cf == NULL)
		return (EINVAL);
	sc = device_get_softc(dev);
	if ((sc->flags & HWPFL_USE_CPPC) != 0)
		return (EOPNOTSUPP);
	set = sc->hwpstate_settings;
	for (i = 0; i < sc->cfnum; i++)
		if (CPUFREQ_CMP(cf->freq, set[i].freq))
			break;
	if (i == sc->cfnum)
		return (EINVAL);

	return (hwpstate_goto_pstate(dev, set[i].pstate_id));
}

static int
hwpstate_get(device_t dev, struct cf_setting *cf)
{
	struct hwpstate_softc *sc;
	struct hwpstate_setting set;
	struct pcpu *pc;
	uint64_t msr;
	uint64_t rate;
	int ret;

	sc = device_get_softc(dev);
	if (cf == NULL)
		return (EINVAL);

	if ((sc->flags & HWPFL_USE_CPPC) != 0) {
		pc = cpu_get_pcpu(dev);
		if (pc == NULL)
			return (ENXIO);

		memset(cf, CPUFREQ_VAL_UNKNOWN, sizeof(*cf));
		cf->dev = dev;
		if ((ret = cpu_est_clockrate(pc->pc_cpuid, &rate)))
			return (ret);
		cf->freq = rate / 1000000;
	} else {
		msr = rdmsr(MSR_AMD_10H_11H_STATUS);
		if (msr >= sc->cfnum)
			return (EINVAL);
		set = sc->hwpstate_settings[msr];

		cf->freq = set.freq;
		cf->volts = set.volts;
		cf->power = set.power;
		cf->lat = set.lat;
		cf->dev = dev;
	}

	return (0);
}

static int
hwpstate_settings(device_t dev, struct cf_setting *sets, int *count)
{
	struct hwpstate_softc *sc;
	struct hwpstate_setting set;
	int i;

	if (sets == NULL || count == NULL)
		return (EINVAL);
	sc = device_get_softc(dev);
	if ((sc->flags & HWPFL_USE_CPPC) != 0)
		return (EOPNOTSUPP);

	if (*count < sc->cfnum)
		return (E2BIG);
	for (i = 0; i < sc->cfnum; i++, sets++) {
		set = sc->hwpstate_settings[i];
		sets->freq = set.freq;
		sets->volts = set.volts;
		sets->power = set.power;
		sets->lat = set.lat;
		sets->dev = dev;
	}
	*count = sc->cfnum;

	return (0);
}

static int
hwpstate_type(device_t dev, int *type)
{
	struct hwpstate_softc *sc;

	if (type == NULL)
		return (EINVAL);
	sc = device_get_softc(dev);

	*type = CPUFREQ_TYPE_ABSOLUTE;
	*type |= (sc->flags & HWPFL_USE_CPPC) != 0 ?
	    CPUFREQ_FLAG_INFO_ONLY | CPUFREQ_FLAG_UNCACHED :
	    0;
	return (0);
}

static void
hwpstate_identify(driver_t *driver, device_t parent)
{
	if (device_find_child(parent, HWP_AMD_CLASSNAME, DEVICE_UNIT_ANY) !=
	    NULL)
		return;

	if ((cpu_vendor_id != CPU_VENDOR_AMD || CPUID_TO_FAMILY(cpu_id) < 0x10) &&
	    cpu_vendor_id != CPU_VENDOR_HYGON)
		return;

	/*
	 * Check if hardware pstate enable bit is set.
	 */
	if ((amd_pminfo & AMDPM_HW_PSTATE) == 0) {
		HWPSTATE_DEBUG(parent, "hwpstate enable bit is not set.\n");
		return;
	}

	if (resource_disabled(HWP_AMD_CLASSNAME, 0))
		return;

	if (BUS_ADD_CHILD(parent, 10, HWP_AMD_CLASSNAME,
		device_get_unit(parent)) == NULL)
		device_printf(parent, "hwpstate: add child failed\n");
}

struct set_autonomous_hwp_data {
	/* Inputs */
	struct hwpstate_softc *sc;
	/* Outputs */
	/* HWP_ERROR_CPPC_* */
	u_int res;
	/* Below fields filled depending on 'res'. */
	uint64_t caps;
	uint64_t init_request;
	uint64_t request;
};

static void
enable_cppc_cb(void *args)
{
	struct set_autonomous_hwp_data *const data = args;
	struct hwpstate_softc *const sc = data->sc;
	uint64_t lowest_perf, highest_perf;
	int error;

	/* We proceed sequentially, so we'll clear out errors on progress. */
	data->res = HWP_ERROR_CPPC_ENABLE | HWP_ERROR_CPPC_CAPS |
	    HWP_ERROR_CPPC_REQUEST | HWP_ERROR_CPPC_REQUEST_WRITE;

	sc->flags |= HWPFL_CPPC_REQUEST_NOT_READ;

	error = wrmsr_safe(MSR_AMD_CPPC_ENABLE, 1);
	if (error != 0)
		return;
	data->res &= ~HWP_ERROR_CPPC_ENABLE;

	error = rdmsr_safe(MSR_AMD_CPPC_CAPS_1, &data->caps);
	if (error != 0)
		return;
	data->res &= ~HWP_ERROR_CPPC_CAPS;

	error = get_cppc_request(sc);
	if (error != 0)
		return;
	data->res &= ~HWP_ERROR_CPPC_REQUEST;
	data->init_request = sc->cppc.request;

	data->request = sc->cppc.request;
	/*
	 * In Intel's reference manual, the default value of EPP is 0x80u which
	 * is the balanced mode. For consistency, we set the same value in AMD's
	 * CPPC driver.
	 */
	SET_BITS_VALUE(data->request, AMD_CPPC_REQUEST_EPP_BITS, 0x80);
	/* Enable autonomous mode by setting desired performance to 0. */
	SET_BITS_VALUE(data->request, AMD_CPPC_REQUEST_DES_PERF_BITS, 0);
	/*
	 * When MSR_AMD_CPPC_CAPS_1 stays at its reset value (0) before CPPC
	 * activation (not supposed to happen, but happens in the field), we use
	 * reasonable default values that are explicitly described by the ACPI
	 * spec (all 0s for the minimum value, all 1s for the maximum one).
	 * Going further, we actually do the same as long as the minimum and
	 * maximum performance levels are not sorted or are equal (in which case
	 * CPPC is not supposed to make sense at all), which covers the reset
	 * value case.
	 */
	lowest_perf = BITS_VALUE(AMD_CPPC_CAPS_1_LOWEST_PERF_BITS, data->caps);
	highest_perf = BITS_VALUE(AMD_CPPC_CAPS_1_HIGHEST_PERF_BITS, data->caps);
	if (lowest_perf >= highest_perf) {
		lowest_perf = 0;
		highest_perf = -1;
	}
	SET_BITS_VALUE(data->request, AMD_CPPC_REQUEST_MIN_PERF_BITS,
	    lowest_perf);
	SET_BITS_VALUE(data->request, AMD_CPPC_REQUEST_MAX_PERF_BITS,
	    highest_perf);

	error = wrmsr_safe(MSR_AMD_CPPC_REQUEST, data->request);
	if (error != 0)
		return;
	data->res &= ~HWP_ERROR_CPPC_REQUEST_WRITE;
	sc->cppc.request = data->request;
}

static int
enable_cppc(struct hwpstate_softc *sc)
{
	const device_t dev = sc->dev;
	const u_int cpuid = cpu_get_pcpu(dev)->pc_cpuid;
	struct set_autonomous_hwp_data data;
	struct sbuf sbs;
	struct sbuf *sb;

	data.sc = sc;
	smp_rendezvous_cpu(cpuid, smp_no_rendezvous_barrier,
	    enable_cppc_cb, smp_no_rendezvous_barrier, &data);

	if (hwp_has_error(data.res, HWP_ERROR_CPPC_ENABLE)) {
		device_printf(dev, "CPU%u: Failed to enable CPPC!\n", cpuid);
		return (ENXIO);
	}
	device_printf(dev, "CPU%u: CPPC enabled.\n", cpuid);

	/*
	 * Now that we have enabled CPPC, we can't go back (hardware does not
	 * support doing so), so we'll attach even in case of further
	 * malfunction, allowing the user to retry retrieving/setting MSRs via
	 * the sysctl knobs.
	 */

	sb = sbuf_new(&sbs, NULL, 0, SBUF_AUTOEXTEND);

	if (hwpstate_verbose)
		sbuf_printf(sb,
		    "CPU%u: Initial MSR values after CPPC enable:\n", cpuid);
	if (hwp_has_error(data.res, HWP_ERROR_CPPC_CAPS))
		print_cppc_no_caps_1(sb);
	else if (hwpstate_verbose)
		print_cppc_caps_1(sb, data.caps);
	if (hwp_has_error(data.res, HWP_ERROR_CPPC_REQUEST))
		print_cppc_no_request(sb);
	else if (hwpstate_verbose)
		print_cppc_request(sb, data.init_request);
	if (hwp_has_error(data.res, HWP_ERROR_CPPC_REQUEST_WRITE)) {
		const bool request_read = !hwp_has_error(data.res,
		    HWP_ERROR_CPPC_REQUEST);

		/* This is printed first, as it is not printed into 'sb'. */
		device_printf(dev, "CPU%u: %s not write into "
		    MSR_AMD_CPPC_REQUEST_NAME "!\n", cpuid,
		    request_read ? "Could" : "Did");
		if (request_read) {
			sbuf_printf(sb, "CPU%u: Failed when trying to set:",
			    cpuid);
			print_cppc_request(sb, data.request);
		}
	} else if (hwpstate_verbose) {
		sbuf_printf(sb, "CPU%u: Tweaked MSR values:\n", cpuid);
		print_cppc_request(sb, data.request);
	}

	sbuf_finish(sb);
	sbuf_putbuf(sb);
	sbuf_delete(sb);

	return (0);
}

static int
hwpstate_probe(device_t dev)
{
	struct hwpstate_softc *sc;
	device_t perf_dev;
	uint64_t msr;
	int error, type;

	sc = device_get_softc(dev);

	if (hwpstate_amd_cppc_enable &&
	   (amd_extended_feature_extensions & AMDFEID_CPPC)) {
		sc->flags |= HWPFL_USE_CPPC;
		device_set_desc(dev,
		    "AMD Collaborative Processor Performance Control (CPPC)");
	} else {
		/*
		 * No CPPC support.  Only keep hwpstate0, it goes well with
		 * acpi_throttle.
		 */
		if (device_get_unit(dev) != 0)
			return (ENXIO);
		device_set_desc(dev, "Cool`n'Quiet 2.0");
	}

	sc->dev = dev;
	if ((sc->flags & HWPFL_USE_CPPC) != 0)
		return (0);

	/*
	 * Check if acpi_perf has INFO only flag.
	 */
	perf_dev = device_find_child(device_get_parent(dev), "acpi_perf",
	    DEVICE_UNIT_ANY);
	error = TRUE;
	if (perf_dev && device_is_attached(perf_dev)) {
		error = CPUFREQ_DRV_TYPE(perf_dev, &type);
		if (error == 0) {
			if ((type & CPUFREQ_FLAG_INFO_ONLY) == 0) {
				/*
				 * If acpi_perf doesn't have INFO_ONLY flag,
				 * it will take care of pstate transitions.
				 */
				HWPSTATE_DEBUG(dev, "acpi_perf will take care of pstate transitions.\n");
				return (ENXIO);
			} else {
				/*
				 * If acpi_perf has INFO_ONLY flag, (_PCT has FFixedHW)
				 * we can get _PSS info from acpi_perf
				 * without going into ACPI.
				 */
				HWPSTATE_DEBUG(dev, "going to fetch info from acpi_perf\n");
				error = hwpstate_get_info_from_acpi_perf(dev, perf_dev);
			}
		}
	}

	if (error == 0) {
		/*
		 * Now we get _PSS info from acpi_perf without error.
		 * Let's check it.
		 */
		msr = rdmsr(MSR_AMD_10H_11H_LIMIT);
		if (sc->cfnum != 1 + AMD_10H_11H_GET_PSTATE_MAX_VAL(msr)) {
			HWPSTATE_DEBUG(dev, "MSR (%jd) and ACPI _PSS (%d)"
			    " count mismatch\n", (intmax_t)msr, sc->cfnum);
			error = TRUE;
		}
	}

	/*
	 * If we cannot get info from acpi_perf,
	 * Let's get info from MSRs.
	 */
	if (error)
		error = hwpstate_get_info_from_msr(dev);
	if (error)
		return (error);

	return (0);
}

static int
hwpstate_attach(device_t dev)
{
	struct hwpstate_softc *sc;
	int res;

	sc = device_get_softc(dev);
	if ((sc->flags & HWPFL_USE_CPPC) != 0) {
		if ((res = enable_cppc(sc)) != 0)
			return (res);
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_STATIC_CHILDREN(_debug), OID_AUTO,
		    device_get_nameunit(dev),
		    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_SKIP | CTLFLAG_MPSAFE,
		    sc, 0, sysctl_cppc_dump_handler, "A", "");

		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    "epp", CTLTYPE_UINT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    dev, AMD_CPPC_REQUEST_EPP_BITS,
		    sysctl_cppc_request_field_handler, "IU",
		    "Efficiency/Performance Preference (from 0, "
		    "most performant, to 255, most efficient)");

		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    "minimum_performance",
		    CTLTYPE_UINT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    dev, AMD_CPPC_REQUEST_MIN_PERF_BITS,
		    sysctl_cppc_request_field_handler, "IU",
		    "Minimum allowed performance level (from 0 to 255; "
		    "should be smaller than 'maximum_performance'; "
		    "effective range limited by CPU)");

		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    "maximum_performance",
		    CTLTYPE_UINT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    dev, AMD_CPPC_REQUEST_MAX_PERF_BITS,
		    sysctl_cppc_request_field_handler, "IU",
		    "Maximum allowed performance level (from 0 to 255; "
		    "should be larger than 'minimum_performance'; "
		    "effective range limited by CPU)");

		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    "desired_performance",
		    CTLTYPE_UINT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    dev, AMD_CPPC_REQUEST_DES_PERF_BITS,
		    sysctl_cppc_request_field_handler, "IU",
		    "Desired performance level (from 0 to 255, "
		    "0 enables autonomous mode, otherwise value should be "
		    "between 'minimum_performance' and 'maximum_performance' "
		    "inclusive)");
	}
	return (cpufreq_register(dev));
}

static int
hwpstate_get_info_from_msr(device_t dev)
{
	struct hwpstate_softc *sc;
	struct hwpstate_setting *hwpstate_set;
	uint64_t msr;
	int family, i, fid, did;

	family = CPUID_TO_FAMILY(cpu_id);
	sc = device_get_softc(dev);
	/* Get pstate count */
	msr = rdmsr(MSR_AMD_10H_11H_LIMIT);
	sc->cfnum = 1 + AMD_10H_11H_GET_PSTATE_MAX_VAL(msr);
	hwpstate_set = sc->hwpstate_settings;
	for (i = 0; i < sc->cfnum; i++) {
		msr = rdmsr(MSR_AMD_10H_11H_CONFIG + i);
		if ((msr & ((uint64_t)1 << 63)) == 0) {
			HWPSTATE_DEBUG(dev, "msr is not valid.\n");
			return (ENXIO);
		}
		did = AMD_10H_11H_CUR_DID(msr);
		fid = AMD_10H_11H_CUR_FID(msr);

		hwpstate_set[i].volts = CPUFREQ_VAL_UNKNOWN;
		hwpstate_set[i].power = CPUFREQ_VAL_UNKNOWN;
		hwpstate_set[i].lat = CPUFREQ_VAL_UNKNOWN;
		/* Convert fid/did to frequency. */
		switch (family) {
		case 0x11:
			hwpstate_set[i].freq = (100 * (fid + 0x08)) >> did;
			break;
		case 0x10:
		case 0x12:
		case 0x15:
		case 0x16:
			hwpstate_set[i].freq = (100 * (fid + 0x10)) >> did;
			break;
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1A:
			/* calculate freq */
			if (family == 0x1A) {
				fid = AMD_1AH_CUR_FID(msr);
				/* 1Ah CPU don't use a divisor */
				hwpstate_set[i].freq = fid;
				if (fid > 0x0f)
					hwpstate_set[i].freq *= 5;
				else {
					HWPSTATE_DEBUG(dev,
					    "unexpected fid: %d\n", fid);
					return (ENXIO);
				}
			} else {
				did = AMD_17H_CUR_DID(msr);
				if (did == 0) {
					HWPSTATE_DEBUG(dev,
					    "unexpected did: 0\n");
					did = 1;
				}
				fid = AMD_17H_CUR_FID(msr);
				hwpstate_set[i].freq = (200 * fid) / did;
			}

			/* Vid step is 6.25mV, so scale by 100. */
			hwpstate_set[i].volts =
			    (155000 - (625 * AMD_17H_CUR_VID(msr))) / 100;
			/*
			 * Calculate current first.
			 * This equation is mentioned in
			 * "BKDG for AMD Family 15h Models 70h-7fh Processors",
			 * section 2.5.2.1.6.
			 */
			hwpstate_set[i].power = AMD_17H_CUR_IDD(msr) * 1000;
			hwpstate_set[i].power = hwpstate_amd_iscale(
			    hwpstate_set[i].power, AMD_17H_CUR_IDIV(msr));
			hwpstate_set[i].power *= hwpstate_set[i].volts;
			/* Milli amps * milli volts to milli watts. */
			hwpstate_set[i].power /= 1000;
			break;
		default:
			HWPSTATE_DEBUG(dev, "get_info_from_msr: %s family"
			    " 0x%02x CPUs are not supported yet\n",
			    cpu_vendor_id == CPU_VENDOR_HYGON ? "Hygon" : "AMD",
			    family);
			return (ENXIO);
		}
		hwpstate_set[i].pstate_id = i;
	}
	return (0);
}

static int
hwpstate_get_info_from_acpi_perf(device_t dev, device_t perf_dev)
{
	struct hwpstate_softc *sc;
	struct cf_setting *perf_set;
	struct hwpstate_setting *hwpstate_set;
	int count, error, i;

	perf_set = malloc(MAX_SETTINGS * sizeof(*perf_set), M_TEMP, M_NOWAIT);
	if (perf_set == NULL) {
		HWPSTATE_DEBUG(dev, "nomem\n");
		return (ENOMEM);
	}
	/*
	 * Fetch settings from acpi_perf.
	 * Now it is attached, and has info only flag.
	 */
	count = MAX_SETTINGS;
	error = CPUFREQ_DRV_SETTINGS(perf_dev, perf_set, &count);
	if (error) {
		HWPSTATE_DEBUG(dev, "error: CPUFREQ_DRV_SETTINGS.\n");
		goto out;
	}
	sc = device_get_softc(dev);
	sc->cfnum = count;
	hwpstate_set = sc->hwpstate_settings;
	for (i = 0; i < count; i++) {
		if (i == perf_set[i].spec[0]) {
			hwpstate_set[i].pstate_id = i;
			hwpstate_set[i].freq = perf_set[i].freq;
			hwpstate_set[i].volts = perf_set[i].volts;
			hwpstate_set[i].power = perf_set[i].power;
			hwpstate_set[i].lat = perf_set[i].lat;
		} else {
			HWPSTATE_DEBUG(dev, "ACPI _PSS object mismatch.\n");
			error = ENXIO;
			goto out;
		}
	}
out:
	if (perf_set)
		free(perf_set, M_TEMP);
	return (error);
}

static int
hwpstate_detach(device_t dev)
{
	struct hwpstate_softc *sc;

	sc = device_get_softc(dev);
	if ((sc->flags & HWPFL_USE_CPPC) == 0)
		hwpstate_goto_pstate(dev, 0);
	return (cpufreq_unregister(dev));
}

static int
hwpstate_shutdown(device_t dev)
{

	/* hwpstate_goto_pstate(dev, 0); */
	return (0);
}

static int
hwpstate_features(driver_t *driver, u_int *features)
{

	/* Notify the ACPI CPU that we support direct access to MSRs */
	*features = ACPI_CAP_PERF_MSRS;
	return (0);
}
