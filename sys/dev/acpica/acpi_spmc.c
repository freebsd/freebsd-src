/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024-2025 The FreeBSD Foundation
 *
 * This software was developed by Aymeric Wibo <obiwac@freebsd.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/uuid.h>
#include <sys/kdb.h>

#include <machine/_inttypes.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>

/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT	ACPI_SPMC
ACPI_MODULE_NAME("SPMC")

static SYSCTL_NODE(_debug_acpi, OID_AUTO, spmc, CTLFLAG_RD | CTLFLAG_MPSAFE,
    NULL, "SPMC debugging");

static char *spmc_ids[] = {
	"PNP0D80",
	NULL
};

enum intel_dsm_index {
	DSM_ENUM_FUNCTIONS		= 0,
	DSM_GET_DEVICE_CONSTRAINTS	= 1,
	DSM_GET_CRASH_DUMP_DEVICE	= 2,
	DSM_DISPLAY_OFF_NOTIF		= 3,
	DSM_DISPLAY_ON_NOTIF		= 4,
	DSM_ENTRY_NOTIF			= 5,
	DSM_EXIT_NOTIF			= 6,
	/* Only for Microsoft DSM set. */
	DSM_MODERN_ENTRY_NOTIF		= 7,
	DSM_MODERN_EXIT_NOTIF		= 8,
};

enum amd_dsm_index {
	AMD_DSM_ENUM_FUNCTIONS		= 0,
	AMD_DSM_GET_DEVICE_CONSTRAINTS	= 1,
	AMD_DSM_ENTRY_NOTIF		= 2,
	AMD_DSM_EXIT_NOTIF		= 3,
	AMD_DSM_DISPLAY_OFF_NOTIF	= 4,
	AMD_DSM_DISPLAY_ON_NOTIF	= 5,
};

enum dsm_set_flags {
	DSM_SET_INTEL	= 1 << 0,
	DSM_SET_MS	= 1 << 1,
	DSM_SET_AMD	= 1 << 2,
};

struct dsm_set {
	enum dsm_set_flags	flag;
	const char		*name;
	int			revision;
	struct uuid		uuid;
	uint64_t		dsms_expected;
};

static struct dsm_set intel_dsm_set = {
	.flag = DSM_SET_INTEL,
	.name = "Intel",
	/*
	 * XXX Linux uses 1 for the revision on Intel DSMs, but doesn't explain
	 * why.  The commit that introduces this links to a document mentioning
	 * revision 0, so default this to 0.
	 *
	 * The debug.acpi.spmc.intel_dsm_revision sysctl may be used to configure
	 * this just in case.
	 */
	.revision = 0,
	.uuid = { /* c4eb40a0-6cd2-11e2-bcfd-0800200c9a66 */
		0xc4eb40a0, 0x6cd2, 0x11e2, 0xbc, 0xfd,
		{0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66},
	},
	.dsms_expected = DSM_GET_DEVICE_CONSTRAINTS | DSM_DISPLAY_OFF_NOTIF |
	    DSM_DISPLAY_ON_NOTIF | DSM_ENTRY_NOTIF | DSM_EXIT_NOTIF,
};

SYSCTL_INT(_debug_acpi_spmc, OID_AUTO, intel_dsm_revision, CTLFLAG_RW,
    &intel_dsm_set.revision, 0,
    "Revision to use when evaluating Intel SPMC DSMs");

static struct dsm_set ms_dsm_set = {
	.flag = DSM_SET_MS,
	.name = "Microsoft",
	.revision = 0,
	.uuid = { /* 11e00d56-ce64-47ce-837b-1f898f9aa461 */
		0x11e00d56, 0xce64, 0x47ce, 0x83, 0x7b,
		{0x1f, 0x89, 0x8f, 0x9a, 0xa4, 0x61},
	},
	.dsms_expected = DSM_DISPLAY_OFF_NOTIF | DSM_DISPLAY_ON_NOTIF |
	    DSM_ENTRY_NOTIF | DSM_EXIT_NOTIF | DSM_MODERN_ENTRY_NOTIF |
	    DSM_MODERN_EXIT_NOTIF,
};

static struct dsm_set amd_dsm_set = {
	.flag = DSM_SET_AMD,
	.name = "AMD",
	/*
	 * XXX Linux uses 0 for the revision on AMD DSMs, but at least on the
	 * Framework 13 AMD 7040 series, the enum functions DSM only returns a
	 * function mask that covers all the DSMs we need to call when called
	 * with revision 2.
	 *
	 * The debug.acpi.spmc.amd_dsm_revision sysctl may be used to configure
	 * this just in case.
	 */
	.revision = 2,
	.uuid = { /* e3f32452-febc-43ce-9039-932122d37721 */
		0xe3f32452, 0xfebc, 0x43ce, 0x90, 0x39,
		{0x93, 0x21, 0x22, 0xd3, 0x77, 0x21},
	},
	.dsms_expected = AMD_DSM_GET_DEVICE_CONSTRAINTS | AMD_DSM_ENTRY_NOTIF |
	    AMD_DSM_EXIT_NOTIF | AMD_DSM_DISPLAY_OFF_NOTIF |
	    AMD_DSM_DISPLAY_ON_NOTIF,
};

SYSCTL_INT(_debug_acpi_spmc, OID_AUTO, amd_dsm_revision, CTLFLAG_RW,
    &amd_dsm_set.revision, 0, "Revision to use when evaluating AMD SPMC DSMs");

union dsm_index {
	int			i;
	enum intel_dsm_index	regular;
	enum amd_dsm_index	amd;
};

struct acpi_spmc_constraint {
	bool		enabled;
	char		*name;
	int		min_d_state;
	ACPI_HANDLE	handle;

	/* Unused, spec only. */
	uint64_t	lpi_uid;
	uint64_t	min_dev_specific_state;

	/* Unused, AMD only. */
	uint64_t	function_states;
};

struct acpi_spmc_softc {
	device_t		dev;
	ACPI_HANDLE		handle;
	ACPI_OBJECT		*obj;
	enum dsm_set_flags	dsm_sets;

	bool				constraints_populated;
	size_t				constraint_count;
	struct acpi_spmc_constraint	*constraints;
};

static void	acpi_spmc_check_dsm_set(struct acpi_spmc_softc *sc,
		    ACPI_HANDLE handle, struct dsm_set *dsm_set);
static int	acpi_spmc_get_constraints(device_t dev);
static void	acpi_spmc_free_constraints(struct acpi_spmc_softc *sc);

static int
acpi_spmc_probe(device_t dev)
{
	char			*name;
	ACPI_HANDLE		handle;
	struct acpi_spmc_softc	*sc;

	/* Check that this is an enabled device. */
	if (acpi_get_type(dev) != ACPI_TYPE_DEVICE || acpi_disabled("spmc"))
		return (ENXIO);

	if (ACPI_ID_PROBE(device_get_parent(dev), dev, spmc_ids, &name) > 0)
		return (ENXIO);

	handle = acpi_get_handle(dev);
	if (handle == NULL)
		return (ENXIO);

	sc = device_get_softc(dev);

	/* Check which sets of DSM's are supported. */
	sc->dsm_sets = 0;

	acpi_spmc_check_dsm_set(sc, handle, &intel_dsm_set);
	acpi_spmc_check_dsm_set(sc, handle, &ms_dsm_set);
	acpi_spmc_check_dsm_set(sc, handle, &amd_dsm_set);

	if (sc->dsm_sets == 0)
		return (ENXIO);

	device_set_descf(dev, "Low Power S0 Idle (DSM sets 0x%x)",
	    sc->dsm_sets);

	return (0);
}

static int
acpi_spmc_attach(device_t dev)
{
	struct acpi_spmc_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->handle = acpi_get_handle(dev);
	if (sc->handle == NULL)
		return (ENXIO);

	sc->constraints_populated = false;
	sc->constraint_count = 0;
	sc->constraints = NULL;

	/* Get device constraints. We can only call this once so do this now. */
	acpi_spmc_get_constraints(sc->dev);

	return (0);
}

static int
acpi_spmc_detach(device_t dev)
{
	acpi_spmc_free_constraints(device_get_softc(dev));
	return (0);
}

static void
acpi_spmc_check_dsm_set(struct acpi_spmc_softc *sc, ACPI_HANDLE handle,
    struct dsm_set *dsm_set)
{
	const uint64_t dsms_supported = acpi_DSMQuery(handle,
	    (uint8_t *)&dsm_set->uuid, dsm_set->revision);

	/*
	 * Check if DSM set supported at all.  We do this by checking the
	 * existence of "enum functions".
	 */
	if ((dsms_supported & 1) == 0)
		return;
	if ((dsms_supported & dsm_set->dsms_expected)
	    != dsm_set->dsms_expected) {
		device_printf(sc->dev, "DSM set %s does not support expected "
		    "DSMs (%#" PRIx64 " vs %#" PRIx64 "). "
		    "Some methods may fail.\n",
		    dsm_set->name, dsms_supported, dsm_set->dsms_expected);
	}
	sc->dsm_sets |= dsm_set->flag;
}

static void
acpi_spmc_free_constraints(struct acpi_spmc_softc *sc)
{
	if (sc->constraints == NULL)
		return;

	for (size_t i = 0; i < sc->constraint_count; i++) {
		if (sc->constraints[i].name != NULL)
			free(sc->constraints[i].name, M_TEMP);
	}

	free(sc->constraints, M_TEMP);
	sc->constraints = NULL;
}

static int
acpi_spmc_get_constraints_spec(struct acpi_spmc_softc *sc, ACPI_OBJECT *object)
{
	struct acpi_spmc_constraint *constraint;
	int		revision;
	ACPI_OBJECT	*constraint_obj;
	ACPI_OBJECT	*name_obj;
	ACPI_OBJECT	*detail;
	ACPI_OBJECT	*constraint_package;

	KASSERT(sc->constraints_populated == false,
	    ("constraints already populated"));

	sc->constraint_count = object->Package.Count;
	sc->constraints = malloc(sc->constraint_count * sizeof *sc->constraints,
	    M_TEMP, M_WAITOK | M_ZERO);

	/*
	 * The value of sc->constraint_count can change during the loop, so
	 * iterate until object->Package.Count so we actually go over all
	 * elements in the package.
	 */
	for (size_t i = 0; i < object->Package.Count; i++) {
		constraint_obj = &object->Package.Elements[i];
		constraint = &sc->constraints[i];

		constraint->enabled =
		    constraint_obj->Package.Elements[1].Integer.Value;

		name_obj = &constraint_obj->Package.Elements[0];
		constraint->name = strdup(name_obj->String.Pointer, M_TEMP);
		if (constraint->name == NULL) {
			acpi_spmc_free_constraints(sc);
			return (ENOMEM);
		}

		/*
		 * The first element in the device constraint detail package is
		 * the revision, and should always be zero.
		 */
		revision = constraint_obj->Package.Elements[0].Integer.Value;
		if (revision != 0) {
			device_printf(sc->dev, "Unknown revision %d for "
			    "device constraint detail package\n", revision);
			sc->constraint_count--;
			continue;
		}

		detail = &constraint_obj->Package.Elements[2];
		constraint_package = &detail->Package.Elements[1];

		constraint->lpi_uid =
		    constraint_package->Package.Elements[0].Integer.Value;
		constraint->min_d_state =
		    constraint_package->Package.Elements[1].Integer.Value;
		constraint->min_dev_specific_state =
		    constraint_package->Package.Elements[2].Integer.Value;
	}

	sc->constraints_populated = true;
	return (0);
}

static int
acpi_spmc_get_constraints_amd(struct acpi_spmc_softc *sc, ACPI_OBJECT *object)
{
	size_t		constraint_count;
	ACPI_OBJECT	*constraint_obj;
	ACPI_OBJECT	*constraints;
	struct acpi_spmc_constraint *constraint;
	ACPI_OBJECT	*name_obj;

	KASSERT(sc->constraints_populated == false,
	    ("constraints already populated"));

	/*
	 * First element in the package is unknown.
	 * Second element is the number of device constraints.
	 * Third element is the list of device constraints itself.
	 */
	constraint_count = object->Package.Elements[1].Integer.Value;
	constraints = &object->Package.Elements[2];

	if (constraints->Package.Count != constraint_count) {
		device_printf(sc->dev, "constraint count mismatch (%d to %zu)\n",
		    constraints->Package.Count, constraint_count);
		return (ENXIO);
	}

	sc->constraint_count = constraint_count;
	sc->constraints = malloc(constraint_count * sizeof *sc->constraints,
	    M_TEMP, M_WAITOK | M_ZERO);

	for (size_t i = 0; i < constraint_count; i++) {
		/* Parse the constraint package. */
		constraint_obj = &constraints->Package.Elements[i];
		if (constraint_obj->Package.Count != 4) {
			device_printf(sc->dev, "constraint %zu has %d elements\n",
			    i, constraint_obj->Package.Count);
			acpi_spmc_free_constraints(sc);
			return (ENXIO);
		}

		constraint = &sc->constraints[i];
		constraint->enabled =
		    constraint_obj->Package.Elements[0].Integer.Value;

		name_obj = &constraint_obj->Package.Elements[1];
		constraint->name = strdup(name_obj->String.Pointer, M_TEMP);
		if (constraint->name == NULL) {
			acpi_spmc_free_constraints(sc);
			return (ENOMEM);
		}

		constraint->function_states =
		    constraint_obj->Package.Elements[2].Integer.Value;
		constraint->min_d_state =
		    constraint_obj->Package.Elements[3].Integer.Value;
	}

	sc->constraints_populated = true;
	return (0);
}

static int
acpi_spmc_get_constraints(device_t dev)
{
	struct acpi_spmc_softc	*sc;
	union dsm_index		dsm_index;
	struct dsm_set		*dsm_set;
	ACPI_STATUS		status;
	ACPI_BUFFER		result;
	ACPI_OBJECT		*object;
	bool			is_amd;
	int			rv;
	struct acpi_spmc_constraint *constraint;

	sc = device_get_softc(dev);
	if (sc->constraints_populated)
		return (0);

	/* The Microsoft DSM set doesn't have this DSM. */
	is_amd = (sc->dsm_sets & DSM_SET_AMD) != 0;
	if (is_amd) {
		dsm_set = &amd_dsm_set;
		dsm_index.amd = AMD_DSM_GET_DEVICE_CONSTRAINTS;
	} else {
		dsm_set = &intel_dsm_set;
		dsm_index.regular = DSM_GET_DEVICE_CONSTRAINTS;
	}

	/* XXX It seems like this DSM fails if called more than once. */
	status = acpi_EvaluateDSMTyped(sc->handle, (uint8_t *)&dsm_set->uuid,
	    dsm_set->revision, dsm_index.i, NULL, &result,
	    ACPI_TYPE_PACKAGE);
	if (ACPI_FAILURE(status)) {
		device_printf(dev, "%s failed to call %s DSM %d (rev %d)\n",
		    __func__, dsm_set->name, dsm_index.i, dsm_set->revision);
		return (ENXIO);
	}

	object = (ACPI_OBJECT *)result.Pointer;
	if (is_amd)
		rv = acpi_spmc_get_constraints_amd(sc, object);
	else
		rv = acpi_spmc_get_constraints_spec(sc, object);
	AcpiOsFree(object);
	if (rv != 0)
		return (rv);

	/* Get handles for each constraint device. */
	for (size_t i = 0; i < sc->constraint_count; i++) {
		constraint = &sc->constraints[i];

		status = acpi_GetHandleInScope(sc->handle,
		    __DECONST(char *, constraint->name), &constraint->handle);
		if (ACPI_FAILURE(status)) {
			device_printf(dev, "failed to get handle for %s\n",
			    constraint->name);
			constraint->handle = NULL;
		}
	}
	return (0);
}

static void
acpi_spmc_check_constraints(struct acpi_spmc_softc *sc)
{
	bool violation = false;

	KASSERT(sc->constraints_populated, ("constraints not populated"));
	for (size_t i = 0; i < sc->constraint_count; i++) {
		struct acpi_spmc_constraint *constraint = &sc->constraints[i];

		if (!constraint->enabled)
			continue;
		if (constraint->handle == NULL)
			continue;

		ACPI_STATUS status = acpi_GetHandleInScope(sc->handle,
		    __DECONST(char *, constraint->name), &constraint->handle);
		if (ACPI_FAILURE(status)) {
			device_printf(sc->dev, "failed to get handle for %s\n",
			    constraint->name);
			constraint->handle = NULL;
		}
		if (constraint->handle == NULL)
			continue;

#ifdef notyet
		int d_state;
		if (ACPI_FAILURE(acpi_pwr_get_state(constraint->handle, &d_state)))
			continue;
		if (d_state < constraint->min_d_state) {
			device_printf(sc->dev, "constraint for device %s"
			    " violated (minimum D-state required was %s, actual"
			    " D-state is %s), might fail to enter LPI state\n",
			    constraint->name,
			    acpi_d_state_to_str(constraint->min_d_state),
			    acpi_d_state_to_str(d_state));
			violation = true;
		}
#endif
	}
	if (!violation)
		device_printf(sc->dev,
		    "all device power constraints respected!\n");
}

static void
acpi_spmc_run_dsm(device_t dev, struct dsm_set *dsm_set, int index)
{
	struct acpi_spmc_softc	*sc;
	ACPI_STATUS		status;
	ACPI_BUFFER		result;

	sc = device_get_softc(dev);

	status = acpi_EvaluateDSMTyped(sc->handle, (uint8_t *)&dsm_set->uuid,
	    dsm_set->revision, index, NULL, &result, ACPI_TYPE_ANY);

	if (ACPI_FAILURE(status)) {
		device_printf(dev, "%s failed to call %s DSM %d (rev %d)\n",
		    __func__, dsm_set->name, index, dsm_set->revision);
		return;
	}

	AcpiOsFree(result.Pointer);
}

/*
 * Try running the DSMs from all the DSM sets we have, as them failing costs us
 * nothing, and it seems like on AMD platforms, both the AMD entry and Microsoft
 * "modern" DSM's are required for it to enter modern standby.
 *
 * This is what Linux does too.
 */
static void
acpi_spmc_display_off_notif(device_t dev)
{
	struct acpi_spmc_softc *sc = device_get_softc(dev);

	if ((sc->dsm_sets & DSM_SET_INTEL) != 0)
		acpi_spmc_run_dsm(dev, &intel_dsm_set, DSM_DISPLAY_OFF_NOTIF);
	if ((sc->dsm_sets & DSM_SET_MS) != 0)
		acpi_spmc_run_dsm(dev, &ms_dsm_set, DSM_DISPLAY_OFF_NOTIF);
	if ((sc->dsm_sets & DSM_SET_AMD) != 0)
		acpi_spmc_run_dsm(dev, &amd_dsm_set, AMD_DSM_DISPLAY_OFF_NOTIF);
}

static void
acpi_spmc_display_on_notif(device_t dev)
{
	struct acpi_spmc_softc *sc = device_get_softc(dev);

	if ((sc->dsm_sets & DSM_SET_INTEL) != 0)
		acpi_spmc_run_dsm(dev, &intel_dsm_set, DSM_DISPLAY_ON_NOTIF);
	if ((sc->dsm_sets & DSM_SET_MS) != 0)
		acpi_spmc_run_dsm(dev, &ms_dsm_set, DSM_DISPLAY_ON_NOTIF);
	if ((sc->dsm_sets & DSM_SET_AMD) != 0)
		acpi_spmc_run_dsm(dev, &amd_dsm_set, AMD_DSM_DISPLAY_ON_NOTIF);
}

static void
acpi_spmc_entry_notif(device_t dev)
{
	struct acpi_spmc_softc *sc = device_get_softc(dev);

	acpi_spmc_check_constraints(sc);

	if ((sc->dsm_sets & DSM_SET_AMD) != 0)
		acpi_spmc_run_dsm(dev, &amd_dsm_set, AMD_DSM_ENTRY_NOTIF);
	if ((sc->dsm_sets & DSM_SET_MS) != 0) {
		acpi_spmc_run_dsm(dev, &ms_dsm_set, DSM_MODERN_ENTRY_NOTIF);
		acpi_spmc_run_dsm(dev, &ms_dsm_set, DSM_ENTRY_NOTIF);
	}
	if ((sc->dsm_sets & DSM_SET_INTEL) != 0)
		acpi_spmc_run_dsm(dev, &intel_dsm_set, DSM_ENTRY_NOTIF);
}

static void
acpi_spmc_exit_notif(device_t dev)
{
	struct acpi_spmc_softc *sc = device_get_softc(dev);

	if ((sc->dsm_sets & DSM_SET_INTEL) != 0)
		acpi_spmc_run_dsm(dev, &intel_dsm_set, DSM_EXIT_NOTIF);
	if ((sc->dsm_sets & DSM_SET_AMD) != 0)
		acpi_spmc_run_dsm(dev, &amd_dsm_set, AMD_DSM_EXIT_NOTIF);
	if ((sc->dsm_sets & DSM_SET_MS) != 0) {
		acpi_spmc_run_dsm(dev, &ms_dsm_set, DSM_EXIT_NOTIF);
		acpi_spmc_run_dsm(dev, &ms_dsm_set, DSM_MODERN_EXIT_NOTIF);
	}
}

static int
acpi_spmc_suspend(device_t dev)
{
	acpi_spmc_display_off_notif(dev);
	acpi_spmc_entry_notif(dev);

	return (0);
}

static int
acpi_spmc_resume(device_t dev)
{
	acpi_spmc_exit_notif(dev);
	acpi_spmc_display_on_notif(dev);

	return (0);
}

static device_method_t acpi_spmc_methods[] = {
	DEVMETHOD(device_probe,		acpi_spmc_probe),
	DEVMETHOD(device_attach,	acpi_spmc_attach),
	DEVMETHOD(device_detach,	acpi_spmc_detach),
	DEVMETHOD_END
};

static driver_t acpi_spmc_driver = {
	"acpi_spmc",
	acpi_spmc_methods,
	sizeof(struct acpi_spmc_softc),
};

DRIVER_MODULE_ORDERED(acpi_spmc, acpi, acpi_spmc_driver, NULL, NULL, SI_ORDER_ANY);
MODULE_DEPEND(acpi_spmc, acpi, 1, 1, 1);
