/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "bsdconf_formats.h"

/*
 * BSDCONF_FORMAT_SRC: /usr/src build audience
 *
 * Make(1) syntax, matching make.conf(5) / src.conf(5).  The FreeBSD source
 * tree consults three configuration files in order (see src.sys.env.mk,
 * sys.mk, and src.sys.mk): src-env.conf, make.conf, then src.conf.  New
 * directives are written to src.conf (def->path); later files override
 * earlier ones for `=' and accumulate for `+='.  Environment overrides
 * (SRC_ENV_CONF, __MAKE_CONF, SRCCONF) are applied by sysconf(8).
 *
 * sys.mk / local.sys.mk and `.if' / control flow are intentionally out of
 * scope: conf files may contain them, but this format does not evaluate
 * them (see LIMITATIONS in sysconf(8)).
 */
static const struct bsdconf_source bsdconf_src_sources[] = {
	{ BSDCONF_SOURCE_FILE, "/etc/src-env.conf" },
	{ BSDCONF_SOURCE_FILE, "/etc/make.conf" },
	{ BSDCONF_SOURCE_FILE, "/etc/src.conf" },
	{ BSDCONF_SOURCE_FILE, NULL },
};

const struct bsdconf_format_def bsdconf_format_src_def = {
	.keyword	= "src",
	.path		= "/etc/src.conf",
	.sources	= bsdconf_src_sources,
	.processing	= BSDCONF_BREAK_ON_EQUALS | BSDCONF_CASE_SENSITIVE |
			  BSDCONF_OPERATOR_EQUALS,
	.put		= BSDCONF_PUT_UNQUOTED | BSDCONF_PUT_ALLOW_EMPTY,
};
