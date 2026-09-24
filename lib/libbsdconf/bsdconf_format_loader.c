/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "bsdconf_formats.h"

/*
 * BSDCONF_FORMAT_LOADER: loader.conf(5)
 *
 * Assignments with no whitespace around the `=' -- the Forth reader in
 * support.4th rejects anything else, so BSDCONF_STRICT_EQUALS is enforced.
 * Quoted or unquoted input values are accepted when parsing but values are
 * always written back quoted (BSDCONF_PUT_QUOTE_ALWAYS), the form every
 * loader accepts for every value.
 *
 * The loader itself hardcodes only /boot/defaults/loader.conf and discovers
 * every other file from the loader_conf_files, loader_conf_dirs, and
 * local_loader_conf_files directives encountered along the way -- any file
 * so discovered may revise those lists in turn. The `defaults' member
 * quintet below hands that same discovery to bsdconf_format_files(); the
 * static source list is only the fallback for systems whose defaults file
 * is missing. The defaults file itself is deliberately excluded from the
 * resolved list, mirroring how sysrc(8) excludes /etc/defaults/rc.conf.
 */
static const struct bsdconf_source bsdconf_loader_sources[] = {
	{ BSDCONF_SOURCE_FILE,	"/boot/device.hints" },
	{ BSDCONF_SOURCE_FILE,	"/boot/loader.conf" },
	{ BSDCONF_SOURCE_DIR,	"/boot/loader.conf.d" },
	{ BSDCONF_SOURCE_FILE,	"/boot/loader.conf.local" },
	{ BSDCONF_SOURCE_FILE,	NULL },
};

const struct bsdconf_format_def bsdconf_format_loader_def = {
	.keyword		= "loader",
	.path			= "/boot/loader.conf",
	.sources		= bsdconf_loader_sources,
	.defaults		= "/boot/defaults/loader.conf",
	.defaults_env		= "LOADER_DEFAULTS",
	.files_directive	= "loader_conf_files",
	.dirs_directive		= "loader_conf_dirs",
	.local_directive	= "local_loader_conf_files",
	.processing		= BSDCONF_BREAK_ON_EQUALS |
				  BSDCONF_CASE_SENSITIVE |
				  BSDCONF_REQUIRE_EQUALS |
				  BSDCONF_STRICT_EQUALS,
	.put			= BSDCONF_PUT_QUOTE_ALWAYS |
				  BSDCONF_PUT_ALLOW_EMPTY,
};
