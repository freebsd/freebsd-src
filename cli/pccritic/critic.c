/*
 * pccritic/critic.c
 * pc(5) quality scoring engine
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2026 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include "libpkgconf/config.h"
#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>
#include "critic.h"

/*
 * Each severity carries a fixed point penalty.  The score begins at 100 and
 * each finding subtracts its severity's penalty; the result is clamped to the
 * range 0..100.  Keeping the weights here makes the scoring model auditable in
 * one place.
 */
static const int severity_penalty[PCCRITIC_SEV_COUNT] = {
	[PCCRITIC_SEV_CRITICAL]	= 25,
	[PCCRITIC_SEV_MAJOR]	= 10,
	[PCCRITIC_SEV_MINOR]	= 4,
	[PCCRITIC_SEV_INFO]	= 1,
};

static const char *severity_names[PCCRITIC_SEV_COUNT] = {
	[PCCRITIC_SEV_CRITICAL]	= "critical",
	[PCCRITIC_SEV_MAJOR]	= "major",
	[PCCRITIC_SEV_MINOR]	= "minor",
	[PCCRITIC_SEV_INFO]	= "info",
};

static const char *category_names[PCCRITIC_CAT_COUNT] = {
	[PCCRITIC_CAT_METADATA]		= "metadata",
	[PCCRITIC_CAT_VERSION]		= "version",
	[PCCRITIC_CAT_RELOCATION]	= "relocation",
	[PCCRITIC_CAT_CFLAGS]		= "cflags",
	[PCCRITIC_CAT_LIBS]		= "libs",
	[PCCRITIC_CAT_REQUIRES]		= "requires",
	[PCCRITIC_CAT_SBOM]		= "sbom",
	[PCCRITIC_CAT_STYLE]		= "style",
};

const char *
pccritic_severity_name(pccritic_severity_t severity)
{
	if (severity >= PCCRITIC_SEV_COUNT)
		return "unknown";

	return severity_names[severity];
}

const char *
pccritic_category_name(pccritic_category_t category)
{
	if (category >= PCCRITIC_CAT_COUNT)
		return "unknown";

	return category_names[category];
}

pccritic_report_t *
pccritic_report_new(void)
{
	pccritic_report_t *report = calloc(1, sizeof(*report));

	if (report == NULL)
		return NULL;

	report->score = 100;
	report->grade = 'A';

	return report;
}

void
pccritic_report_free(pccritic_report_t *report)
{
	pkgconf_node_t *node, *tmp;

	if (report == NULL)
		return;

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(report->findings.head, tmp, node)
	{
		pccritic_finding_t *finding = node->data;

		free(finding->message);
		free(finding);
	}

	free(report);
}

PRINTFLIKE(5, 6) static void
add_finding(pccritic_report_t *report, pccritic_severity_t severity, pccritic_category_t category,
	const char *code, const char *fmt, ...)
{
	pccritic_finding_t *finding;
	va_list va;
	char buf[1024];

	finding = calloc(1, sizeof(*finding));
	if (finding == NULL)
		return;

	va_start(va, fmt);
	vsnprintf(buf, sizeof buf, fmt, va);
	va_end(va);

	finding->severity = severity;
	finding->category = category;
	finding->code = code;
	finding->message = strdup(buf);

	if (finding->message == NULL)
	{
		free(finding);
		return;
	}

	report->counts[severity]++;
	pkgconf_node_insert_tail(&finding->iter, finding, &report->findings);
}

static bool
str_empty(const char *s)
{
	return s == NULL || *s == '\0';
}

/* ------------------------------------------------------------------------ */

static void
check_metadata(pkgconf_pkg_t *pkg, pccritic_report_t *report)
{
	if (str_empty(pkg->realname))
		add_finding(report, PCCRITIC_SEV_CRITICAL, PCCRITIC_CAT_METADATA, "PC001",
			"missing required 'Name:' field");

	if (str_empty(pkg->version))
		add_finding(report, PCCRITIC_SEV_CRITICAL, PCCRITIC_CAT_METADATA, "PC002",
			"missing required 'Version:' field");

	if (str_empty(pkg->description))
		add_finding(report, PCCRITIC_SEV_MAJOR, PCCRITIC_CAT_METADATA, "PC003",
			"missing required 'Description:' field");
	else if (strlen(pkg->description) < 12)
		add_finding(report, PCCRITIC_SEV_MINOR, PCCRITIC_CAT_METADATA, "PC004",
			"'Description:' is very terse (\"%s\")", pkg->description);

	if (str_empty(pkg->url))
		add_finding(report, PCCRITIC_SEV_MINOR, PCCRITIC_CAT_METADATA, "PC005",
			"missing recommended 'URL:' field");
}

static void
check_version(pkgconf_pkg_t *pkg, pccritic_report_t *report)
{
	const char *p;
	bool has_digit = false, has_space = false;

	if (str_empty(pkg->version))
		return;	/* already reported as PC002 */

	for (p = pkg->version; *p != '\0'; p++)
	{
		if (*p >= '0' && *p <= '9')
			has_digit = true;
		if (*p == ' ' || *p == '\t')
			has_space = true;
	}

	if (has_space)
		add_finding(report, PCCRITIC_SEV_MINOR, PCCRITIC_CAT_VERSION, "PC011",
			"'Version:' contains whitespace (\"%s\")", pkg->version);

	if (!has_digit)
		add_finding(report, PCCRITIC_SEV_MAJOR, PCCRITIC_CAT_VERSION, "PC010",
			"'Version:' has no numeric component (\"%s\")", pkg->version);
}

static bool
var_relocatable(pkgconf_pkg_t *pkg, const char *name)
{
	pkgconf_variable_t *v = pkgconf_variable_find(&pkg->vars, name);

	if (v == NULL)
		return true;	/* not present: not our concern here */

	return pkgconf_bytecode_references_var(&v->bcbuf, "prefix") ||
	       pkgconf_bytecode_references_var(&v->bcbuf, "exec_prefix");
}

static void
check_relocation(pkgconf_pkg_t *pkg, pccritic_report_t *report)
{
	bool has_prefix = pkgconf_variable_find(&pkg->vars, "prefix") != NULL;

	if (!has_prefix)
		add_finding(report, PCCRITIC_SEV_INFO, PCCRITIC_CAT_RELOCATION, "PC020",
			"no 'prefix' variable is defined; the package cannot be relocated");

	if (pkgconf_variable_find(&pkg->vars, "libdir") != NULL && !var_relocatable(pkg, "libdir"))
		add_finding(report, PCCRITIC_SEV_MAJOR, PCCRITIC_CAT_RELOCATION, "PC021",
			"'libdir' is hardcoded; derive it from ${prefix} or ${exec_prefix} for relocatability");

	if (pkgconf_variable_find(&pkg->vars, "includedir") != NULL && !var_relocatable(pkg, "includedir"))
		add_finding(report, PCCRITIC_SEV_MAJOR, PCCRITIC_CAT_RELOCATION, "PC022",
			"'includedir' is hardcoded; derive it from ${prefix} for relocatability");
}

/* Reconstruct a fragment's textual flag form for diagnostics. */
static const char *
fragment_text(const pkgconf_fragment_t *frag, char *buf, size_t buflen)
{
	if (frag->type != 0)
		snprintf(buf, buflen, "-%c%s", frag->type, frag->data != NULL ? frag->data : "");
	else
		snprintf(buf, buflen, "%s", frag->data != NULL ? frag->data : "");

	return buf;
}

static bool
prefix_match(const char *s, const char *prefix)
{
	size_t n = strlen(prefix);
	return strncmp(s, prefix, n) == 0;
}

/* Well-known directories already on the compiler/linker default search path. */
static bool
is_system_includedir(const char *path)
{
#ifdef SYSTEM_INCLUDEDIR
	if (!strcmp(path, SYSTEM_INCLUDEDIR))
		return true;
#endif
	return !strcmp(path, "/usr/include") || !strcmp(path, "/usr/local/include");
}

static bool
is_system_libdir(const char *path)
{
#ifdef SYSTEM_LIBDIR
	if (!strcmp(path, SYSTEM_LIBDIR))
		return true;
#endif
	return !strcmp(path, "/usr/lib") || !strcmp(path, "/usr/lib64") ||
	       !strcmp(path, "/lib") || !strcmp(path, "/lib64") ||
	       !strcmp(path, "/usr/local/lib");
}

static void
check_cflags(pkgconf_pkg_t *pkg, pccritic_report_t *report)
{
	pkgconf_node_t *node;
	char buf[PKGCONF_BUFSIZE];

	PKGCONF_FOREACH_LIST_ENTRY(pkg->cflags.head, node)
	{
		pkgconf_fragment_t *frag = node->data;

		switch (frag->type)
		{
		case 'I':
			if (frag->data != NULL && is_system_includedir(frag->data))
				add_finding(report, PCCRITIC_SEV_INFO, PCCRITIC_CAT_CFLAGS, "PC031",
					"Cflags adds an include path already on the default search path (pkgconf strips it): -I%s", frag->data);
			break;
		case 'D':
		case 'U':
		case 'F':	/* -F: framework search dir (Darwin) */
			break;
		case 0:
			/* raw / unmergeable flag: classify by content */
			if (frag->data == NULL)
				break;
			if (!strcmp(frag->data, "-pthread") ||
			    prefix_match(frag->data, "-isystem") ||
			    prefix_match(frag->data, "-idirafter") ||
			    prefix_match(frag->data, "-iquote") ||
			    prefix_match(frag->data, "-imacros") ||
			    prefix_match(frag->data, "-include") ||
			    prefix_match(frag->data, "-framework"))
				break;
			if (prefix_match(frag->data, "-std=") || prefix_match(frag->data, "-stdlib="))
				add_finding(report, PCCRITIC_SEV_INFO, PCCRITIC_CAT_CFLAGS, "PC033",
					"Cflags imposes a language standard on consumers: %s", frag->data);
			else if (prefix_match(frag->data, "-Wl,"))
				add_finding(report, PCCRITIC_SEV_MINOR, PCCRITIC_CAT_CFLAGS, "PC032",
					"Cflags contains a linker flag, which belongs in Libs: %s", frag->data);
			else
				add_finding(report, PCCRITIC_SEV_MAJOR, PCCRITIC_CAT_CFLAGS, "PC030",
					"Cflags contains a build-tuning flag that pollutes consumer builds: %s", frag->data);
			break;
		case 'l':
		case 'L':
			add_finding(report, PCCRITIC_SEV_MINOR, PCCRITIC_CAT_CFLAGS, "PC034",
				"Cflags contains a linker flag, which belongs in Libs: %s",
				fragment_text(frag, buf, sizeof buf));
			break;
		default:
			add_finding(report, PCCRITIC_SEV_MAJOR, PCCRITIC_CAT_CFLAGS, "PC030",
				"Cflags contains a build-tuning flag that pollutes consumer builds: %s",
				fragment_text(frag, buf, sizeof buf));
			break;
		}
	}
}

static void
check_libs_list(pkgconf_list_t *libs, const char *which, pccritic_report_t *report)
{
	pkgconf_node_t *node;
	char buf[PKGCONF_BUFSIZE];

	PKGCONF_FOREACH_LIST_ENTRY(libs->head, node)
	{
		pkgconf_fragment_t *frag = node->data;

		switch (frag->type)
		{
		case 'l':
			break;
		case 'L':
			if (frag->data != NULL && is_system_libdir(frag->data))
				add_finding(report, PCCRITIC_SEV_INFO, PCCRITIC_CAT_LIBS, "PC040",
					"%s adds a library path already on the default search path (pkgconf strips it): -L%s", which, frag->data);
			break;
		case 'F':	/* -F framework dir / -framework arrives as type 0 */
			break;
		case 0:
			if (frag->data == NULL)
				break;
			if (strstr(frag->data, "-rpath") != NULL || strstr(frag->data, "rpath=") != NULL)
				add_finding(report, PCCRITIC_SEV_MAJOR, PCCRITIC_CAT_LIBS, "PC042",
					"%s hardcodes a runtime library path: %s", which, frag->data);
			else if (!strcmp(frag->data, "-pthread") ||
				 prefix_match(frag->data, "-framework") ||
				 prefix_match(frag->data, "-Wl,") ||
				 prefix_match(frag->data, "-l"))
				break;
			else
				add_finding(report, PCCRITIC_SEV_MINOR, PCCRITIC_CAT_LIBS, "PC041",
					"%s contains an unexpected flag: %s", which, frag->data);
			break;
		default:
			add_finding(report, PCCRITIC_SEV_MINOR, PCCRITIC_CAT_LIBS, "PC041",
				"%s contains an unexpected flag: %s", which, fragment_text(frag, buf, sizeof buf));
			break;
		}
	}
}

static void
check_libs(pkgconf_pkg_t *pkg, pccritic_report_t *report)
{
	check_libs_list(&pkg->libs, "Libs", report);
	check_libs_list(&pkg->libs_private, "Libs.private", report);
}

static bool
dep_in_list(pkgconf_list_t *list, const char *package)
{
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(list->head, node)
	{
		pkgconf_dependency_t *dep = node->data;

		if (dep->package != NULL && !strcmp(dep->package, package))
			return true;
	}

	return false;
}

static void
check_requires(pkgconf_pkg_t *pkg, pccritic_report_t *report)
{
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(pkg->required.head, node)
	{
		pkgconf_dependency_t *dep = node->data;

		if (dep->package != NULL && dep_in_list(&pkg->requires_private, dep->package))
			add_finding(report, PCCRITIC_SEV_MINOR, PCCRITIC_CAT_REQUIRES, "PC051",
				"'%s' appears in both Requires and Requires.private", dep->package);
	}
}

/*
 * The NTIA "minimum elements" for an SBOM are: supplier name, component name,
 * component version, other unique identifiers, dependency relationships,
 * author of the SBOM data, and a timestamp.  Component name and version are
 * the required pc(5) fields (PC001/PC002) and dependency relationships are the
 * Requires fields, so they are covered elsewhere; author and timestamp are
 * supplied by the SBOM generator (bomtool/spdxtool) rather than the .pc file.
 * This leaves the supplier and a provenance identifier, both of which map to
 * optional pkgconf fields that the SBOM tooling consumes.
 */
static void
check_sbom(pkgconf_pkg_t *pkg, pccritic_report_t *report)
{
	if (str_empty(pkg->maintainer))
		add_finding(report, PCCRITIC_SEV_INFO, PCCRITIC_CAT_SBOM, "PC070",
			"missing 'Maintainer:' field; the NTIA SBOM supplier name cannot be determined");

	if (str_empty(pkg->source))
		add_finding(report, PCCRITIC_SEV_INFO, PCCRITIC_CAT_SBOM, "PC071",
			"missing 'Source:' field; an SBOM has no download location for this package");

	if (pkg->license.head == NULL)
		add_finding(report, PCCRITIC_SEV_MINOR, PCCRITIC_CAT_SBOM, "PC072",
			"missing 'License:' field; an SBOM cannot record a declared license");
}

static void
check_style(const char *path, pccritic_report_t *report)
{
	FILE *f;
	int c, prev = '\n';
	bool saw_trailing_ws = false, saw_bom = false;
	long pos = 0;

	if (path == NULL)
		return;

	f = fopen(path, "rb");
	if (f == NULL)
		return;

	while ((c = fgetc(f)) != EOF)
	{
		if (pos < 3)
		{
			static const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
			if ((unsigned char) c == bom[pos])
			{
				if (pos == 2)
					saw_bom = true;
			}
			else
				pos = 3;	/* not a BOM; stop checking */
		}

		if (c == '\n' && (prev == ' ' || prev == '\t'))
			saw_trailing_ws = true;

		prev = c;
		pos++;
	}

	fclose(f);

	if (saw_bom)
		add_finding(report, PCCRITIC_SEV_MINOR, PCCRITIC_CAT_STYLE, "PC060",
			"file begins with a UTF-8 byte-order mark");

	if (saw_trailing_ws)
		add_finding(report, PCCRITIC_SEV_INFO, PCCRITIC_CAT_STYLE, "PC062",
			"file has trailing whitespace on one or more lines");

	if (pos > 0 && prev != '\n')
		add_finding(report, PCCRITIC_SEV_INFO, PCCRITIC_CAT_STYLE, "PC063",
			"file does not end with a newline");
}

static void
finalize_score(pccritic_report_t *report)
{
	int penalty = 0;
	pccritic_severity_t sev;

	for (sev = 0; sev < PCCRITIC_SEV_COUNT; sev++)
		penalty += (int) report->counts[sev] * severity_penalty[sev];

	report->score = 100 - penalty;
	if (report->score < 0)
		report->score = 0;

	if (report->score >= 90)
		report->grade = 'A';
	else if (report->score >= 80)
		report->grade = 'B';
	else if (report->score >= 70)
		report->grade = 'C';
	else if (report->score >= 60)
		report->grade = 'D';
	else
		report->grade = 'F';
}

void
pccritic_analyze(pkgconf_client_t *client, pkgconf_pkg_t *pkg, pccritic_report_t *report)
{
	(void) client;

	check_metadata(pkg, report);
	check_version(pkg, report);
	check_relocation(pkg, report);
	check_cflags(pkg, report);
	check_libs(pkg, report);
	check_requires(pkg, report);
	check_sbom(pkg, report);
	check_style(pkg->filename, report);

	finalize_score(report);
}

/* Does the file declare `key:' at the start of any logical line? */
static bool
file_declares_field(FILE *f, const char *key)
{
	char line[PKGCONF_BUFSIZE];
	size_t keylen = strlen(key);

	rewind(f);

	while (fgets(line, sizeof line, f) != NULL)
	{
		const char *p = line;

		while (*p == ' ' || *p == '\t')
			p++;

		if (!strncmp(p, key, keylen))
		{
			p += keylen;
			while (*p == ' ' || *p == '\t')
				p++;
			if (*p == ':')
				return true;
		}
	}

	return false;
}

void
pccritic_analyze_unparsable(pccritic_report_t *report, const char *path)
{
	static const struct {
		const char *field;
		const char *code;
	} required[] = {
		{ "Name", "PC001" },
		{ "Version", "PC002" },
		{ "Description", "PC003" },
	};
	FILE *f = fopen(path, "rb");
	size_t i;

	if (f == NULL)
	{
		add_finding(report, PCCRITIC_SEV_CRITICAL, PCCRITIC_CAT_METADATA, "PC000",
			"file could not be read");
		finalize_score(report);
		return;
	}

	for (i = 0; i < PKGCONF_ARRAY_SIZE(required); i++)
		if (!file_declares_field(f, required[i].field))
			add_finding(report, PCCRITIC_SEV_CRITICAL, PCCRITIC_CAT_METADATA, required[i].code,
				"missing required '%s:' field", required[i].field);

	fclose(f);

	if (report->findings.head == NULL)
		add_finding(report, PCCRITIC_SEV_CRITICAL, PCCRITIC_CAT_METADATA, "PC000",
			"file is not a valid pc(5) file");

	check_style(path, report);
	finalize_score(report);
}
