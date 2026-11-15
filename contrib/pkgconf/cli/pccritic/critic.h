/*
 * pccritic/critic.h
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

#ifndef PCCRITIC_CRITIC_H
#define PCCRITIC_CRITIC_H

#include <libpkgconf/libpkgconf.h>

typedef enum {
	PCCRITIC_SEV_CRITICAL = 0,
	PCCRITIC_SEV_MAJOR,
	PCCRITIC_SEV_MINOR,
	PCCRITIC_SEV_INFO,
	PCCRITIC_SEV_COUNT
} pccritic_severity_t;

typedef enum {
	PCCRITIC_CAT_METADATA = 0,
	PCCRITIC_CAT_VERSION,
	PCCRITIC_CAT_RELOCATION,
	PCCRITIC_CAT_CFLAGS,
	PCCRITIC_CAT_LIBS,
	PCCRITIC_CAT_REQUIRES,
	PCCRITIC_CAT_SBOM,
	PCCRITIC_CAT_STYLE,
	PCCRITIC_CAT_COUNT
} pccritic_category_t;

typedef struct {
	pkgconf_node_t iter;

	pccritic_severity_t severity;
	pccritic_category_t category;
	const char *code;
	char *message;
} pccritic_finding_t;

typedef struct {
	pkgconf_list_t findings;

	unsigned int counts[PCCRITIC_SEV_COUNT];

	int score;	/* 0..100 */
	char grade;	/* 'A'..'F' */
} pccritic_report_t;

pccritic_report_t *pccritic_report_new(void);
void pccritic_report_free(pccritic_report_t *report);

/*
 * Analyze a parsed package, appending findings to the report and computing
 * its score and letter grade.  The package must already have been loaded via
 * pkgconf_pkg_new_from_path() or pkgconf_pkg_find().
 */
void pccritic_analyze(pkgconf_client_t *client, pkgconf_pkg_t *pkg, pccritic_report_t *report);

/*
 * Analyze a .pc file that libpkgconf refused to load (typically because it is
 * missing one of the required Name/Description/Version fields).  The raw file
 * is scanned directly so that the most broken files can still be scored.
 */
void pccritic_analyze_unparsable(pccritic_report_t *report, const char *path);

const char *pccritic_severity_name(pccritic_severity_t severity);
const char *pccritic_category_name(pccritic_category_t category);

#endif /* PCCRITIC_CRITIC_H */
