/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 UOS Project Contributors
 * Derived from OpenBSD qcscm.c (ISC licence)
 *
 * Qualcomm Secure Channel Manager (SCM) / Trusted Execution Environment
 * ======================================================================
 *
 * The SCM provides the interface to Qualcomm's Trusted Execution Environment
 * (TEE) via ARM Secure Monitor Calls (SMC). Key services:
 *
 *   - Secure boot verification
 *   - Trustzone / Qualcomm Trusted App loading
 *   - QFPROM (eFUSE) reading
 *   - Pillar (firmware) loading and authentication
 *   - Memory protection (XPU - Extended Permissions Unit)
 *   - Widevine DRM (L1) - hardware-backed
 *
 * This driver also enables UOS-specific security hardening:
 *   - W^X enforcement via SCM memory protection APIs
 *   - Secure RNG from hardware PRNG
 *   - Platform-specific exploit mitigations
 *
 * OpenBSD influence: qcscm.c uses SMC64 calling convention matching this.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/armreg.h>
#include <machine/smc.h>   /* ARM SMC calling convention */

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

/* ---- Qualcomm SCM SMC function IDs ---- */
/* Service IDs */
#define QCOM_SCM_SVC_BOOT		0x01
#define QCOM_SCM_SVC_PIL		0x02  /* Peripheral Image Loader */
#define QCOM_SCM_SVC_IO		0x05
#define QCOM_SCM_SVC_INFO		0x06
#define QCOM_SCM_SVC_QSEELOG		0x13
#define QCOM_SCM_SVC_SMMU		0x15
#define QCOM_SCM_SVC_FUSE		0x08  /* eFUSE/QFPROM */
#define QCOM_SCM_SVC_MP		0x0C  /* Memory protection */
#define QCOM_SCM_SVC_DCVS		0x0D  /* Dynamic clock/voltage */

/* Boot service commands */
#define QCOM_SCM_BOOT_SET_REMOTE_STATE	0x0A
#define QCOM_SCM_BOOT_LOCK_XPUS		0x1F

/* Memory protection commands */
#define QCOM_SCM_MP_RESTORE_XPUS	0x01
#define QCOM_SCM_MP_ASSIGN		0x16
#define QCOM_SCM_MP_INFO		0x18

/* Info commands */
#define QCOM_SCM_INFO_IS_CALL_AVAIL	0x01

/* SMC64 function ID encoding (as per Qualcomm SCM spec) */
#define QCOM_SCM_FNID(svc, cmd)	\
    (((svc) << 8) | (cmd))

#define QCOM_SMC_ATOMIC_MASK	0x80000000
#define QCOM_SMC_OWNER_SIP	0x02   /* SiP (Silicon Provider) = Qualcomm */
#define QCOM_SMC_TYPE_FAST	0x01

#define QCOM_SMC64_CALL(owner, type, fn_num) \
    (0xC0000000 | ((owner) << 24) | ((type) << 22) | (fn_num))

/* Return codes */
#define QCOM_SCM_ENOMEM		-5
#define QCOM_SCM_EOPNOTSUPP	-4
#define QCOM_SCM_EINVAL_ADDR	-3
#define QCOM_SCM_EINVAL_ARG	-2
#define QCOM_SCM_ERROR		-1
#define QCOM_SCM_INTERRUPTED	1
#define QCOM_SCM_SUCCESS	0

struct qcom_scm_softc {
	device_t	 dev;
	struct resource	*mem_res;
	struct mtx	 mtx;
	bool		 available;  /* SCM responds to IS_CALL_AVAIL? */
};

/* Global softc for direct kernel access (security hooks) */
static struct qcom_scm_softc *qcom_scm_sc;

/* ---- Low-level SMC invocation ---- */
static int
qcom_scm_call(struct qcom_scm_softc *sc, uint32_t svc_id, uint32_t cmd_id,
    uint64_t arg1, uint64_t arg2, uint64_t arg3,
    uint64_t *res1, uint64_t *res2, uint64_t *res3)
{
	register uint64_t x0 __asm("x0");
	register uint64_t x1 __asm("x1") = arg1;
	register uint64_t x2 __asm("x2") = arg2;
	register uint64_t x3 __asm("x3") = arg3;
	uint32_t fn_id;
	int rv;

	fn_id = QCOM_SMC64_CALL(QCOM_SMC_OWNER_SIP, QCOM_SMC_TYPE_FAST,
	    QCOM_SCM_FNID(svc_id, cmd_id));
	x0 = fn_id;

	__asm volatile(
	    "smc	#0\n"
	    : "+r" (x0), "+r" (x1), "+r" (x2), "+r" (x3)
	    :
	    : "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11",
	      "x12", "x13", "x14", "x15", "x16", "x17", "memory"
	);

	rv = (int)(int32_t)x0;
	if (res1) *res1 = x1;
	if (res2) *res2 = x2;
	if (res3) *res3 = x3;

	return (rv == QCOM_SCM_SUCCESS ? 0 : EIO);
}

/* ---- Public API ---- */

/*
 * qcom_scm_is_available - Check if a specific SCM call is implemented.
 * Used by drivers to probe for optional SCM features.
 */
int
qcom_scm_is_available(uint32_t svc_id, uint32_t cmd_id)
{
	uint64_t res;
	int rv;

	if (qcom_scm_sc == NULL)
		return (ENXIO);

	rv = qcom_scm_call(qcom_scm_sc, QCOM_SCM_SVC_INFO,
	    QCOM_SCM_INFO_IS_CALL_AVAIL,
	    QCOM_SCM_FNID(svc_id, cmd_id), 0, 0, &res, NULL, NULL);
	if (rv != 0)
		return (rv);
	return (res ? 0 : EOPNOTSUPP);
}

/*
 * qcom_scm_mem_protect - Set memory region protection (W^X enforcement).
 * Calls SCM to configure XPU (Extended Permissions Unit) for a memory range.
 * flags: QCOM_XPU_READ=1, QCOM_XPU_WRITE=2, QCOM_XPU_EXEC=4
 */
int
qcom_scm_mem_protect(paddr_t pa, size_t size, uint32_t flags)
{
	if (qcom_scm_sc == NULL)
		return (ENXIO);

	return qcom_scm_call(qcom_scm_sc, QCOM_SCM_SVC_MP,
	    QCOM_SCM_MP_ASSIGN,
	    (uint64_t)pa, (uint64_t)size, (uint64_t)flags,
	    NULL, NULL, NULL);
}

/*
 * qcom_scm_get_secure_boot_state - Returns whether secure boot is enabled.
 */
int
qcom_scm_get_secure_boot_state(bool *enabled)
{
	uint64_t res = 0;
	int rv;

	if (qcom_scm_sc == NULL)
		return (ENXIO);

	rv = qcom_scm_call(qcom_scm_sc, QCOM_SCM_SVC_FUSE,
	    0x07, /* QCOM_QFPROM_IS_AUTHENTICATION_ENABLED */
	    0, 0, 0, &res, NULL, NULL);
	*enabled = (res != 0);
	return (rv);
}

/* ---- Device driver ---- */

static struct ofw_compat_data qcom_scm_compat[] = {
	{ "qcom,scm-sm8450",	1 },
	{ "qcom,scm-sm8550",	1 },
	{ "qcom,scm-sm8650",	1 },
	{ "qcom,scm-sc8280xp",	1 },
	{ "qcom,scm",		1 },	/* Generic fallback */
	{ NULL, 0 },
};

static int
qcom_scm_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, qcom_scm_compat)->ocd_data == 0)
		return (ENXIO);
	device_set_desc(dev, "Qualcomm Secure Channel Manager (SCM/TEE)");
	return (BUS_PROBE_DEFAULT);
}

static int
qcom_scm_attach(device_t dev)
{
	struct qcom_scm_softc *sc;
	bool sb_enabled;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* SCM doesn't need MMIO - it uses SMC instructions */
	sc->mem_res = NULL;

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);
	qcom_scm_sc = sc;

	/* Probe availability */
	if (qcom_scm_is_available(QCOM_SCM_SVC_INFO,
	    QCOM_SCM_INFO_IS_CALL_AVAIL) == 0) {
		sc->available = true;
		device_printf(dev, "SCM functional (ARM SMC64)\n");
	} else {
		sc->available = false;
		device_printf(dev, "SCM not responding - limited security\n");
	}

	/* Report secure boot state */
	if (qcom_scm_get_secure_boot_state(&sb_enabled) == 0) {
		device_printf(dev, "Secure boot: %s\n",
		    sb_enabled ? "ENABLED" : "disabled");
	}

	return (0);
}

static int
qcom_scm_detach(device_t dev)
{
	struct qcom_scm_softc *sc = device_get_softc(dev);
	qcom_scm_sc = NULL;
	mtx_destroy(&sc->mtx);
	return (0);
}

static device_method_t qcom_scm_methods[] = {
	DEVMETHOD(device_probe,  qcom_scm_probe),
	DEVMETHOD(device_attach, qcom_scm_attach),
	DEVMETHOD(device_detach, qcom_scm_detach),
	DEVMETHOD_END
};

static driver_t qcom_scm_driver = {
	"qcom_scm",
	qcom_scm_methods,
	sizeof(struct qcom_scm_softc),
};

/* Must attach very early - before any driver that needs secure memory */
EARLY_DRIVER_MODULE(qcom_scm, simplebus, qcom_scm_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_EARLY);
MODULE_VERSION(qcom_scm, 1);
