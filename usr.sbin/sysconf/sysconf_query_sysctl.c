/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Running-kernel sysctl OID descriptions and writability checks.
 */

#include "sysconf_priv.h"

#if defined(__FreeBSD__)

/*
 * Fetch the running kernel's description of the sysctl OID `name' into
 * `buf' via CTL_SYSCTL_OIDDESCR (the same information sysctl(8) prints
 * with its own -d; an OID with no description yields the empty string).
 * Returns zero on success; -1 when the OID does not resolve.
 */
int
sysctl_descr(const char *name, char *buf, size_t bufsize)
{
	int mib[CTL_MAXNAME];
	int qoid[CTL_MAXNAME + 2];
	size_t len;
	size_t size;

	buf[0] = '\0';

	len = CTL_MAXNAME;
	if (sysctlnametomib(name, mib, &len) != 0)
		return (-1);

	qoid[0] = CTL_SYSCTL;
	qoid[1] = CTL_SYSCTL_OIDDESCR;
	memcpy(qoid + 2, mib, len * sizeof(int));
	size = bufsize - 1;
	if (sysctl(qoid, (u_int)len + 2, buf, &size, 0, 0) != 0)
		buf[0] = '\0';
	buf[bufsize - 1] = '\0';

	return (0);
}

/*
 * Process `-d' with explicit names for the sysctl target: report each
 * requested OID's description from the running kernel. Returns
 * EXIT_SUCCESS or EXIT_FAILURE.
 */
int
describe_sysctl(void)
{
	int rv = EXIT_SUCCESS;
	unsigned int n;
	char buf[BUFSIZ];

	for (n = 0; n < nreqs; n++) {
		if (sysctl_descr(reqs[n].name, buf, sizeof(buf)) != 0) {
			if (ignore_unknown)
				continue;
			if (!quiet)
				warnx("unknown oid '%s'", reqs[n].name);
			rv = EXIT_FAILURE;
			continue;
		}
		if (name_only)
			puts(reqs[n].name);
		else if (value_only)
			puts(buf);
		else
			printf("%s: %s\n", reqs[n].name, buf);
	}

	return (rv);
}

/*
 * Return a short name for a CTLTYPE_* value (for warnings).
 */
static const char *
sysctl_typename(u_int kind)
{

	switch (kind & CTLTYPE) {
	case CTLTYPE_INT:	return ("integer");
	case CTLTYPE_UINT:	return ("unsigned integer");
	case CTLTYPE_LONG:	return ("long integer");
	case CTLTYPE_ULONG:	return ("unsigned long");
	case CTLTYPE_S8:	return ("int8_t");
	case CTLTYPE_S16:	return ("int16_t");
	case CTLTYPE_S32:	return ("int32_t");
	case CTLTYPE_S64:	return ("int64_t");
	case CTLTYPE_U8:	return ("uint8_t");
	case CTLTYPE_U16:	return ("uint16_t");
	case CTLTYPE_U32:	return ("uint32_t");
	case CTLTYPE_U64:	return ("uint64_t");
	case CTLTYPE_STRING:	return ("string");
	case CTLTYPE_OPAQUE:	return ("opaque");
	default:		return ("unknown");
	}
}

/*
 * True if `value' can be represented as a signed integer of the given
 * inclusive [min, max] range. Requires a full-string numeric parse.
 */
static int
sysctl_signed_ok(const char *value, intmax_t min, intmax_t max)
{
	char *end;
	intmax_t v;

	errno = 0;
	v = strtoimax(value, &end, 0);
	if (errno != 0 || end == value || *end != '\0')
		return (0);
	return (v >= min && v <= max);
}

/*
 * True if `value' can be represented as an unsigned integer <= max.
 * Leading whitespace is not accepted (sysctl.conf values are trimmed by
 * the caller before we see them); a leading minus is rejected so that
 * strtoumax() cannot wrap a negative into a large positive.
 */
static int
sysctl_unsigned_ok(const char *value, uintmax_t max)
{
	char *end;
	uintmax_t v;

	if (value[0] == '\0' || value[0] == '-')
		return (0);
	errno = 0;
	v = strtoumax(value, &end, 0);
	if (errno != 0 || end == value || *end != '\0')
		return (0);
	return (v <= max);
}

/*
 * Validate that `value' fits the numeric CTLTYPE described by `kind'.
 * `fmt' is the OIDFMT format string (may start with "IK" for Kelvin
 * temperatures); those are left to sysctl(8) at apply time. String and
 * opaque OIDs are accepted as-is. Returns zero if acceptable; -1 if not.
 */
static int
sysctl_value_ok(u_int kind, const char *fmt, const char *value)
{

	if (value == NULL)
		return (0);

	switch (kind & CTLTYPE) {
	case CTLTYPE_STRING:
	case CTLTYPE_OPAQUE:
		return (0);
	case CTLTYPE_INT:
		/*
		 * Temperature OIDs use an "IK" / "IK<digit>" display format
		 * with a specialized parser in sysctl(8); do not apply a
		 * plain integer range here.
		 */
		if (fmt != NULL && strncmp(fmt, "IK", 2) == 0)
			return (0);
		if (sysctl_signed_ok(value, INT_MIN, INT_MAX))
			return (0);
		break;
	case CTLTYPE_UINT:
		if (sysctl_unsigned_ok(value, UINT_MAX))
			return (0);
		break;
	case CTLTYPE_LONG:
		if (sysctl_signed_ok(value, LONG_MIN, LONG_MAX))
			return (0);
		break;
	case CTLTYPE_ULONG:
		if (sysctl_unsigned_ok(value, ULONG_MAX))
			return (0);
		break;
	case CTLTYPE_S8:
		if (sysctl_signed_ok(value, INT8_MIN, INT8_MAX))
			return (0);
		break;
	case CTLTYPE_S16:
		if (sysctl_signed_ok(value, INT16_MIN, INT16_MAX))
			return (0);
		break;
	case CTLTYPE_S32:
		if (sysctl_signed_ok(value, INT32_MIN, INT32_MAX))
			return (0);
		break;
	case CTLTYPE_S64:
		if (sysctl_signed_ok(value, INT64_MIN, INT64_MAX))
			return (0);
		break;
	case CTLTYPE_U8:
		if (sysctl_unsigned_ok(value, UINT8_MAX))
			return (0);
		break;
	case CTLTYPE_U16:
		if (sysctl_unsigned_ok(value, UINT16_MAX))
			return (0);
		break;
	case CTLTYPE_U32:
		if (sysctl_unsigned_ok(value, UINT32_MAX))
			return (0);
		break;
	case CTLTYPE_U64:
		if (sysctl_unsigned_ok(value, UINT64_MAX))
			return (0);
		break;
	default:
		/* NODE already rejected; unknown types: do not block */
		return (0);
	}
	return (-1);
}

/*
 * Determine whether the sysctl OID `name' can take effect when set from
 * sysctl.conf(5) to `value': if the OID resolves it must be a leaf and
 * writable at run time, and `value' must fit the OID's CTLTYPE (so an
 * overflowing assignment cannot land in the conf file and surprise
 * sysctl(8) at boot). An OID that is only tunable from the boot loader is
 * reported as such (pointing at the loader target). An OID that does not
 * resolve is still written -- sysctl.conf(5) commonly names OIDs from
 * modules that are not yet loaded, and init(8) only warns -- so we warn
 * (unless `quiet_unknown') and allow the assignment through; flag and
 * range checks are only possible against a resolvable OID. Returns zero
 * when the value should be written; -1 (after warning) otherwise.
 */
int
sysctl_writable(const char *name, const char *value, int quiet_unknown)
{
	int mib[CTL_MAXNAME];
	int qoid[CTL_MAXNAME + 2];
	size_t len;
	size_t size;
	u_int kind;
	u_char buf[BUFSIZ];
	const char *fmt;

	len = CTL_MAXNAME;
	if (sysctlnametomib(name, mib, &len) != 0) {
		if (!quiet_unknown)
			warnx("WARNING: unknown oid '%s'", name);
		return (0);
	}

	qoid[0] = CTL_SYSCTL;
	qoid[1] = CTL_SYSCTL_OIDFMT;
	memcpy(qoid + 2, mib, len * sizeof(int));
	size = sizeof(buf);
	if (sysctl(qoid, (u_int)len + 2, buf, &size, 0, 0) != 0) {
		warnx("couldn't find format of oid '%s'", name);
		return (-1);
	}
	kind = *(u_int *)(void *)buf;
	fmt = (const char *)(buf + sizeof(u_int));

	if ((kind & CTLTYPE) == CTLTYPE_NODE) {
		warnx("oid '%s' isn't a leaf node", name);
		return (-1);
	}
	if ((kind & CTLFLAG_WR) == 0) {
		if ((kind & CTLFLAG_TUN) != 0) {
			warnx("oid '%s' is a read only tunable", name);
			warnx("use: %s loader %s=<value>", pgm, name);
		} else
			warnx("oid '%s' is read only", name);
		return (-1);
	}
	if (sysctl_value_ok(kind, fmt, value) != 0) {
		warnx("oid '%s' value '%s' is invalid or out of range for %s",
		    name, value, sysctl_typename(kind));
		return (-1);
	}

	return (0);
}

#endif /* __FreeBSD__ */
