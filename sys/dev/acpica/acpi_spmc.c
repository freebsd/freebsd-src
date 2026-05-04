/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024-2026 The FreeBSD Foundation
 *
 * This software was developed by Aymeric Wibo <obiwac@freebsd.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/uuid.h>

#include <machine/_inttypes.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>


/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT	ACPI_SPMC
ACPI_MODULE_NAME("SPMC")

static char *spmc_ids[] = {
	"PNP0D80",
	NULL
};

/* Conversion of an index to a mask. */
#define IDX_TO_BIT(idx)		(1ull << (idx))

/* List of supported DSMs. */
#define DSM_INTEL			0
#define DSM_MS				1
#define DSM_AMD				2

/* List of DSM function indices. */
#define DSM_ENUM_FUNCTIONS		0	/* Common to all DSMs */
#define DSM_GET_DEVICE_CONSTRAINTS	1	/* AMD and Intel, MS N/A */

#define DSM_GET_CRASH_DUMP_DEVICE	2	/* Intel, MS N/A */
#define DSM_INTEL_MS_DISPLAY_OFF_NOTIF	3
#define DSM_INTEL_MS_DISPLAY_ON_NOTIF	4
#define DSM_INTEL_MS_LPI_ENTRY_NOTIF	5
#define DSM_INTEL_MS_LPI_EXIT_NOTIF	6

#define DSM_MS_SLEEP_ENTRY_NOTIF	7
#define DSM_MS_SLEEP_EXIT_NOTIF		8
#define DSM_MS_TURN_ON_DISPLAY		9

#define DSM_AMD_LPI_ENTRY_NOTIF		2
#define DSM_AMD_LPI_EXIT_NOTIF		3
#define DSM_AMD_DISPLAY_OFF_NOTIF	4
#define DSM_AMD_DISPLAY_ON_NOTIF	5


/* Descriptors for the DSMs we support. */

struct dsm_desc {
	const char		*name;
	/* Index in the dsms[] array below. */
	int			 index;
	/*
	 * Revisions are zero or a positive number.  Strictly speaking, next
	 * field should be a 'uint64_t' as per the ACPI spec, but our ACPI DSM
	 * interface takes an 'int' and anyway actual revision numbers never
	 * even exceed the limits of a 'uint8_t'.
	 */
	int			 revision;
	struct uuid		 uuid;
	uint64_t		 supported_functions;
	uint64_t		 expected_functions;
	uint64_t		 extra_functions;
	/* Human-friendly names of known functions. */
	const char *const	*function_names;
	int			 function_names_nb;
};

static const char *const dsm_intel_function_names[] = {
	[DSM_GET_DEVICE_CONSTRAINTS] = "DEVICE_CONSTRAINTS",
	[DSM_GET_CRASH_DUMP_DEVICE] = "CRASH_DUMP_DEVICE",
	[DSM_INTEL_MS_DISPLAY_OFF_NOTIF] = "DISPLAY_OFF",
	[DSM_INTEL_MS_DISPLAY_ON_NOTIF] = "DISPLAY_ON",
	[DSM_INTEL_MS_LPI_ENTRY_NOTIF] = "LPI_ENTRY",
	[DSM_INTEL_MS_LPI_EXIT_NOTIF] = "LPI_EXIT",
};

static struct dsm_desc dsm_intel = {
	.index = DSM_INTEL,
	.name = "Intel",
	.uuid = { /* c4eb40a0-6cd2-11e2-bcfd-0800200c9a66 */
		0xc4eb40a0, 0x6cd2, 0x11e2, 0xbc, 0xfd,
		{0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66}
	},
	/*
	 * XXX Linux uses 1 for the revision on Intel DSMs, but doesn't explain
	 * why.  The commit that introduces this links to a document mentioning
	 * revision 0, so default this to 0.
	 *
	 * The debug.acpi.spmc.intel_dsm_revision sysctl may be used to configure
	 * this just in case.
	 */
	.revision = 0,
	.expected_functions =
	    IDX_TO_BIT(DSM_GET_DEVICE_CONSTRAINTS) |
	    IDX_TO_BIT(DSM_INTEL_MS_DISPLAY_OFF_NOTIF) |
	    IDX_TO_BIT(DSM_INTEL_MS_DISPLAY_ON_NOTIF) |
	    IDX_TO_BIT(DSM_INTEL_MS_LPI_ENTRY_NOTIF) |
	    IDX_TO_BIT(DSM_INTEL_MS_LPI_EXIT_NOTIF),
	.extra_functions =
	    IDX_TO_BIT(DSM_GET_CRASH_DUMP_DEVICE), /* Not used. */
	.function_names = dsm_intel_function_names,
	.function_names_nb = nitems(dsm_intel_function_names),
};

static const char *const dsm_ms_function_names[] = {
	[DSM_INTEL_MS_DISPLAY_OFF_NOTIF] = "DISPLAY_OFF",
	[DSM_INTEL_MS_DISPLAY_ON_NOTIF] = "DISPLAY_ON",
	[DSM_INTEL_MS_LPI_ENTRY_NOTIF] = "LPI_ENTRY",
	[DSM_INTEL_MS_LPI_EXIT_NOTIF] = "LPI_EXIT",
	[DSM_MS_SLEEP_ENTRY_NOTIF] = "SLEEP_ENTRY",
	[DSM_MS_SLEEP_EXIT_NOTIF] = "SLEEP_EXIT",
	[DSM_MS_TURN_ON_DISPLAY] = "TURN_ON",
};

static struct dsm_desc dsm_ms = {
	.index = DSM_MS,
	.name = "Microsoft",
	.uuid = { /* 11e00d56-ce64-47ce-837b-1f898f9aa461 */
		0x11e00d56, 0xce64, 0x47ce, 0x83, 0x7b,
		{0x1f, 0x89, 0x8f, 0x9a, 0xa4, 0x61}
	},
	.revision = 0,
	.expected_functions =
	    IDX_TO_BIT(DSM_INTEL_MS_DISPLAY_OFF_NOTIF) |
	    IDX_TO_BIT(DSM_INTEL_MS_DISPLAY_ON_NOTIF) |
	    IDX_TO_BIT(DSM_INTEL_MS_LPI_ENTRY_NOTIF) |
	    IDX_TO_BIT(DSM_INTEL_MS_LPI_EXIT_NOTIF) |
	    IDX_TO_BIT(DSM_MS_SLEEP_ENTRY_NOTIF) |
	    IDX_TO_BIT(DSM_MS_SLEEP_EXIT_NOTIF),
	.extra_functions =
	    IDX_TO_BIT(DSM_MS_TURN_ON_DISPLAY),
	.function_names = dsm_ms_function_names,
	.function_names_nb = nitems(dsm_ms_function_names),
};

static const char *const dsm_amd_function_names[] = {
	[DSM_GET_DEVICE_CONSTRAINTS] = "DEVICE_CONSTRAINTS",
	[DSM_AMD_DISPLAY_OFF_NOTIF] = "DISPLAY_OFF",
	[DSM_AMD_DISPLAY_ON_NOTIF] = "DISPLAY_ON",
	[DSM_AMD_LPI_ENTRY_NOTIF] = "LPI_ENTRY",
	[DSM_AMD_LPI_EXIT_NOTIF] = "LPI_EXIT",
};

static struct dsm_desc dsm_amd = {
	.index = DSM_AMD,
	.name = "AMD",
	.uuid = { /* e3f32452-febc-43ce-9039-932122d37721 */
		0xe3f32452, 0xfebc, 0x43ce, 0x90, 0x39,
		{0x93, 0x21, 0x22, 0xd3, 0x77, 0x21}
	},
	/*
	 * XXX Linux uses 0 for the revision on AMD DSMs, but at least on the
	 * Framework 13 AMD 7040 series, the "enumerate functions" DSM function
	 * needs to be called with revision 2 to return a mask that covers all
	 * the functions we would like to call.
	 *
	 * The debug.acpi.spmc.amd_dsm_revision sysctl may be used to configure
	 * this just in case.
	 */
	.revision = 2,
	.expected_functions =
	    IDX_TO_BIT(DSM_GET_DEVICE_CONSTRAINTS) |
	    IDX_TO_BIT(DSM_AMD_DISPLAY_OFF_NOTIF) |
	    IDX_TO_BIT(DSM_AMD_DISPLAY_ON_NOTIF) |
	    IDX_TO_BIT(DSM_AMD_LPI_ENTRY_NOTIF) |
	    IDX_TO_BIT(DSM_AMD_LPI_EXIT_NOTIF),
	.function_names = dsm_amd_function_names,
	.function_names_nb = nitems(dsm_amd_function_names),
};

static struct dsm_desc *const dsms[] = {
	[DSM_INTEL] = &dsm_intel,
	[DSM_MS] = &dsm_ms,
	[DSM_AMD] = &dsm_amd,
};

static SYSCTL_NODE(_debug_acpi, OID_AUTO, spmc, CTLFLAG_RD | CTLFLAG_MPSAFE,
    NULL, "SPMC debugging");

SYSCTL_INT(_debug_acpi_spmc, OID_AUTO, intel_dsm_revision, CTLFLAG_RW,
    &dsm_intel.revision, 0,
    "Revision to use when evaluating Intel SPMC DSMs");

SYSCTL_INT(_debug_acpi_spmc, OID_AUTO, amd_dsm_revision, CTLFLAG_RW,
    &dsm_amd.revision, 0, "Revision to use when evaluating AMD SPMC DSMs");

struct acpi_spmc_constraint {
	bool		enabled;
	char		*name;
	int		min_d_state;
	ACPI_HANDLE	handle;

	/* Intel only.  Currently filled but unused. */
	uint64_t	lpi_uid;
	uint64_t	min_dev_specific_state;

	/* AMD only.  Currently filled but unused. */
	uint64_t	function_states;
};

struct acpi_spmc_softc {
	device_t		dev;
	ACPI_HANDLE		handle;
	int			dsms;

	struct eventhandler_entry	*eh_suspend;
	struct eventhandler_entry	*eh_resume;

	bool				constraints_populated;
	size_t				constraint_count;
	struct acpi_spmc_constraint	*constraints;
};


static const struct dsm_desc *
resolve_dsm(int dsm_index)
{
	MPASS(0 <= dsm_index && dsm_index < nitems(dsms));
	return (dsms[dsm_index]);
}

static bool
supports_function(const struct dsm_desc *const dsm, const int function_index)
{
	return ((dsm->supported_functions & IDX_TO_BIT(function_index)) != 0);
}

static bool
has_dsm(const struct acpi_spmc_softc *const sc, const int dsm_index)
{
	return ((sc->dsms & IDX_TO_BIT(dsm_index)) != 0);
}

typedef const char *pbf_get_name_t(const int, const void *const);

static const char *
pbf_dsm_name(const int dsm_index, const void *const opaque __unused)
{
	return (resolve_dsm(dsm_index)->name);
}

static const char *
dsm_function_name(const struct dsm_desc *const dsm, const int function_index)
{
	MPASS(function_index >= 0);
	if (function_index >= dsm->function_names_nb)
		return (NULL);
	/* May be NULL. */
	return (dsm->function_names[function_index]);
}

static const char *
pbf_function_name(const int function_index, const void *const opaque)
{
	return (dsm_function_name(opaque, function_index));
}

static int
print_bit_field(char *const buf, const size_t buf_size,
    const uint64_t bit_field, const char *const fallback_prefix,
    pbf_get_name_t get_name, const void *const opaque)
{
	uint64_t bf = bit_field;
	char *const buf_end = buf + buf_size;
	char *p = buf;
	int ret = 0;
	bool one_set = false;

#define PBF_PRINT(...)							\
	do {								\
		const __ptrdiff_t rem = MAX(buf_end - p, 0);		\
		const int lret = snprintf(p, rem, __VA_ARGS__);		\
									\
		MPASS(lret >= 0);					\
		p += MIN(lret, rem);					\
		ret += lret;						\
	} while (0)

	if (bf == 0) {
		PBF_PRINT("");
		return (ret);
	}

	do {
		const int b_idx = ffsll(bf) - 1;
		const char *const name = get_name(b_idx, opaque);

		PBF_PRINT(one_set ? "," : "<");
		one_set = true;
		if (name != NULL)
			PBF_PRINT("%s", name);
		else
			PBF_PRINT("%s_%d", fallback_prefix, b_idx);

		bf &= ~IDX_TO_BIT(b_idx);
	} while (bf != 0);
	PBF_PRINT(">");
#undef PBF_PRINT

	return (ret);
}

static void
failed_to_call_dsm(const struct acpi_spmc_softc *const sc,
    const struct dsm_desc *const dsm, const int function_index)
{
	(void)device_printf(sc->dev,
	    "Failed to call DSM %s (rev %u) function %s\n",
	    dsm->name, dsm->revision, dsm_function_name(dsm, function_index));
}

static void	acpi_spmc_check_dsm(struct acpi_spmc_softc *sc,
		    ACPI_HANDLE handle, struct dsm_desc *const dsm);
static void	acpi_spmc_dsm_print_functions(
		    const struct acpi_spmc_softc *const sc,
		    const struct dsm_desc *const dsm);
static int	acpi_spmc_get_constraints(device_t dev);
static void	acpi_spmc_free_constraints(struct acpi_spmc_softc *const sc);

static void	acpi_spmc_suspend(device_t dev, enum power_stype stype);
static void	acpi_spmc_resume(device_t dev, enum power_stype stype);

static int
acpi_spmc_probe(device_t dev)
{
	char			*name;
	ACPI_HANDLE		 handle;
	struct acpi_spmc_softc	*sc;
	char			 buf[32];

	/* Check that this is an enabled device. */
	if (acpi_get_type(dev) != ACPI_TYPE_DEVICE || acpi_disabled("spmc"))
		return (ENXIO);

	if (ACPI_ID_PROBE(device_get_parent(dev), dev, spmc_ids, &name) > 0)
		return (ENXIO);

	if (device_get_unit(dev) > 0) {
		device_printf(dev, "shouldn't have more than one SPMC");
		return (ENXIO);
	}

	handle = acpi_get_handle(dev);
	/* ACPI_ID_PROBE() above cannot succeed without a handle. */
	MPASS(handle != NULL);

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Check which DSMs are supported. */
	sc->dsms = 0;

	for (int i = 0; i < nitems(dsms); ++i)
		acpi_spmc_check_dsm(sc, handle, dsms[i]);

	if (sc->dsms == 0)
		return (ENXIO);

	print_bit_field(buf, sizeof(buf), sc->dsms, "DSM", pbf_dsm_name, NULL);
	device_set_descf(dev, "System Power Management Controller (DSMs %s)",
	    buf);

	return (0);
}

static int
acpi_spmc_attach(device_t dev)
{
	struct acpi_spmc_softc *sc = device_get_softc(dev);

	sc->handle = acpi_get_handle(dev);
	if (sc->handle == NULL)
		return (ENXIO);

	sc->constraints_populated = false;
	sc->constraint_count = 0;
	sc->constraints = NULL;

	/* Get device constraints. We can only call this once so do this now. */
	acpi_spmc_get_constraints(dev);

	sc->eh_suspend = EVENTHANDLER_REGISTER(acpi_post_dev_suspend,
	    acpi_spmc_suspend, dev, 0);
	sc->eh_resume = EVENTHANDLER_REGISTER(acpi_pre_dev_resume,
	    acpi_spmc_resume, dev, 0);

	return (0);
}

static int
acpi_spmc_detach(device_t dev)
{
	struct acpi_spmc_softc *sc = device_get_softc(dev);

	EVENTHANDLER_DEREGISTER(acpi_post_dev_suspend, sc->eh_suspend);
	EVENTHANDLER_DEREGISTER(acpi_pre_dev_resume, sc->eh_resume);

	acpi_spmc_free_constraints(device_get_softc(dev));
	return (0);
}

static void
acpi_spmc_dsm_print_functions(const struct acpi_spmc_softc *const sc,
    const struct dsm_desc *const dsm)
{
	const uint64_t missing = dsm->expected_functions &
	    ~dsm->supported_functions;
	const uint64_t unknown = dsm->supported_functions &
	    ~(dsm->expected_functions | dsm->extra_functions);
	char buf[128];

	print_bit_field(buf, sizeof(buf), dsm->supported_functions,
	    "FUNC", pbf_function_name, dsm);
	device_printf(sc->dev, "DSM %s: Supported functions: %#" PRIx64 "%s\n",
	    dsm->name, dsm->supported_functions, buf);

	if (missing != 0) {
		print_bit_field(buf, sizeof(buf), missing, "FUNC",
		    pbf_function_name, dsm);
		device_printf(sc->dev, "DSM %s: Does not enumerate expected "
		    "functions %#" PRIx64 "%s.  Calls to them may fail.\n",
		    dsm->name, missing, buf);
	}

	if (unknown != 0) {
		print_bit_field(buf, sizeof(buf), unknown, "FUNC",
		    pbf_function_name, dsm);
		device_printf(sc->dev, "DSM %s: Supports more functions than "
		    "used (%#" PRIx64 "%s), driver might need an upgrade.\n",
		    dsm->name, unknown, buf);
	}
}

static void
acpi_spmc_check_dsm(struct acpi_spmc_softc *sc, ACPI_HANDLE handle,
    struct dsm_desc *const dsm)
{
	const uint64_t supported_functions = acpi_DSMQuery(handle,
	    (uint8_t *)&dsm->uuid, dsm->revision);

	/*
	 * DSM is supported if bit 0 is set.
	 */
	if ((supported_functions & 1) == 0)
		return;
	dsm->supported_functions = supported_functions & ~1;
	sc->dsms |= IDX_TO_BIT(dsm->index);
	acpi_spmc_dsm_print_functions(sc, dsm);
}

static void
acpi_spmc_free_constraints(struct acpi_spmc_softc *const sc)
{
	for (size_t i = 0; i < sc->constraint_count; i++)
		free(sc->constraints[i].name, M_TEMP);
	sc->constraint_count = 0;

	free(sc->constraints, M_TEMP);
	sc->constraints = NULL;
}

static int
acpi_spmc_parse_constraints_intel(struct acpi_spmc_softc *sc, ACPI_OBJECT *object)
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

		detail = &constraint_obj->Package.Elements[2];
		/*
		 * The first element in the device constraint detail package is
		 * the revision, and should always be zero.
		 */
		revision = detail->Package.Elements[0].Integer.Value;
		if (revision != 0) {
			device_printf(sc->dev, "Unknown revision %d for "
			    "device constraint detail package\n", revision);
			sc->constraint_count--;
			continue;
		}

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
acpi_spmc_parse_constraints_amd(struct acpi_spmc_softc *sc, ACPI_OBJECT *object)
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
	struct dsm_desc		*dsm;
	ACPI_STATUS		status;
	ACPI_BUFFER		result;
	ACPI_OBJECT		*object;
	bool			is_amd;
	int			rv;
	struct acpi_spmc_constraint *constraint;

	sc = device_get_softc(dev);
	if (sc->constraints_populated)
		return (0);

	/* The Microsoft DSM doesn't have this function. */
	is_amd = has_dsm(sc, DSM_AMD);
	dsm = is_amd ? &dsm_amd : &dsm_intel;

	/* It seems like this DSM can fail if called more than once. */
	status = acpi_EvaluateDSMTyped(sc->handle, (uint8_t *)&dsm->uuid,
	    dsm->revision, DSM_GET_DEVICE_CONSTRAINTS, NULL, &result,
	    ACPI_TYPE_PACKAGE);
	if (ACPI_FAILURE(status)) {
		failed_to_call_dsm(sc, dsm, DSM_GET_DEVICE_CONSTRAINTS);
		return (ENXIO);
	}

	object = (ACPI_OBJECT *)result.Pointer;
	if (is_amd)
		rv = acpi_spmc_parse_constraints_amd(sc, object);
	else
		rv = acpi_spmc_parse_constraints_intel(sc, object);
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
acpi_spmc_run_dsm(device_t dev, const struct dsm_desc *dsm, int function_index)
{
	struct acpi_spmc_softc	*sc;
	ACPI_STATUS		status;
	ACPI_BUFFER		result;

	sc = device_get_softc(dev);

	status = acpi_EvaluateDSMTyped(sc->handle, (const uint8_t *)&dsm->uuid,
	    dsm->revision, function_index, NULL, &result, ACPI_TYPE_ANY);

	if (ACPI_FAILURE(status))
		failed_to_call_dsm(sc, dsm, function_index);
	else
		AcpiOsFree(result.Pointer);
}

/*
 * Try running the functions from all the DSMs we have, as them failing costs us
 * nothing, and it seems like on AMD platforms, both the AMD entry and Microsoft
 * "modern" functions are required for it to enter modern standby.
 *
 * This is what Linux does too.
 */
static void
acpi_spmc_display_off_notif(device_t dev)
{
	struct acpi_spmc_softc *sc = device_get_softc(dev);

	if (has_dsm(sc, DSM_INTEL))
		acpi_spmc_run_dsm(dev, &dsm_intel,
		    DSM_INTEL_MS_DISPLAY_OFF_NOTIF);
	if (has_dsm(sc, DSM_MS))
		acpi_spmc_run_dsm(dev, &dsm_ms,
		    DSM_INTEL_MS_DISPLAY_OFF_NOTIF);
	if (has_dsm(sc, DSM_AMD))
		acpi_spmc_run_dsm(dev, &dsm_amd, DSM_AMD_DISPLAY_OFF_NOTIF);
}

static void
acpi_spmc_display_on_notif(device_t dev)
{
	struct acpi_spmc_softc *sc = device_get_softc(dev);

	if (has_dsm(sc, DSM_INTEL))
		acpi_spmc_run_dsm(dev, &dsm_intel,
		    DSM_INTEL_MS_DISPLAY_ON_NOTIF);
	if (has_dsm(sc, DSM_MS))
		acpi_spmc_run_dsm(dev, &dsm_ms,
		    DSM_INTEL_MS_DISPLAY_ON_NOTIF);
	if (has_dsm(sc, DSM_AMD))
		acpi_spmc_run_dsm(dev, &dsm_amd, DSM_AMD_DISPLAY_ON_NOTIF);
}

static void
acpi_spmc_entry_notif(device_t dev)
{
	struct acpi_spmc_softc *sc = device_get_softc(dev);

	acpi_spmc_check_constraints(sc);

	if (has_dsm(sc, DSM_AMD))
		acpi_spmc_run_dsm(dev, &dsm_amd, DSM_AMD_LPI_ENTRY_NOTIF);
	if (has_dsm(sc, DSM_MS)) {
		acpi_spmc_run_dsm(dev, &dsm_ms, DSM_MS_SLEEP_ENTRY_NOTIF);
		acpi_spmc_run_dsm(dev, &dsm_ms, DSM_INTEL_MS_LPI_ENTRY_NOTIF);
	}
	if (has_dsm(sc, DSM_INTEL))
		acpi_spmc_run_dsm(dev, &dsm_intel,
		    DSM_INTEL_MS_LPI_ENTRY_NOTIF);
}

static void
acpi_spmc_exit_notif(device_t dev)
{
	struct acpi_spmc_softc *sc = device_get_softc(dev);

	if (has_dsm(sc, DSM_INTEL))
		acpi_spmc_run_dsm(dev, &dsm_intel, DSM_INTEL_MS_LPI_EXIT_NOTIF);
	if (has_dsm(sc, DSM_AMD))
		acpi_spmc_run_dsm(dev, &dsm_amd, DSM_AMD_LPI_EXIT_NOTIF);
	if (has_dsm(sc, DSM_MS)) {
		acpi_spmc_run_dsm(dev, &dsm_ms, DSM_INTEL_MS_LPI_EXIT_NOTIF);
		if (supports_function(&dsm_ms, DSM_MS_TURN_ON_DISPLAY))
			acpi_spmc_run_dsm(dev, &dsm_ms,
			    DSM_MS_TURN_ON_DISPLAY);
		acpi_spmc_run_dsm(dev, &dsm_ms, DSM_MS_SLEEP_EXIT_NOTIF);
	}
}

static void
acpi_spmc_suspend(device_t dev, enum power_stype stype)
{
	if (stype != POWER_STYPE_SUSPEND_TO_IDLE)
		return;

	acpi_spmc_display_off_notif(dev);
	acpi_spmc_entry_notif(dev);
}

static void
acpi_spmc_resume(device_t dev, enum power_stype stype)
{
	if (stype != POWER_STYPE_SUSPEND_TO_IDLE)
		return;

	acpi_spmc_exit_notif(dev);
	acpi_spmc_display_on_notif(dev);
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
