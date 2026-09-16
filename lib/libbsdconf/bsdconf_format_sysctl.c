/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "bsdconf_formats.h"

/*
 * BSDCONF_FORMAT_SYSCTL: sysctl.conf(5)
 *
 * Assignments in the format of the sysctl(8) command, whose file parser
 * trims whitespace around the `=' and strips one pair of quotes around the
 * value, so values are written unquoted unless quoting is required to
 * round-trip (embedded whitespace or a comment character).
 *
 * Sourced at boot in the order below: the first two by rc.d/sysctl (and
 * again by rc.d/sysctl_lastload), the module-specific drop-in by
 * rc.subr(8)'s load_kld when the named kernel module loads.
 */
static const struct bsdconf_source bsdconf_sysctl_sources[] = {
	{ BSDCONF_SOURCE_FILE,		"/etc/sysctl.conf" },
	{ BSDCONF_SOURCE_FILE,		"/etc/sysctl.conf.local" },
	{ BSDCONF_SOURCE_MODDIR,	"/etc/sysctl.kld.d" },
	{ BSDCONF_SOURCE_FILE,		NULL },
};

const struct bsdconf_format_def bsdconf_format_sysctl_def = {
	.keyword	= "sysctl",
	.path		= "/etc/sysctl.conf",
	.sources	= bsdconf_sysctl_sources,
	.processing	= BSDCONF_BREAK_ON_EQUALS | BSDCONF_CASE_SENSITIVE |
			  BSDCONF_REQUIRE_EQUALS,
	.put		= BSDCONF_PUT_ALLOW_EMPTY,
};
