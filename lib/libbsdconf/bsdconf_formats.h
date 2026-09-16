/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _BSDCONF_FORMATS_H_
#define _BSDCONF_FORMATS_H_

#include "bsdconf.h"

/*
 * The built-in format descriptors, one translation unit per format.
 * This header is deliberately not installed.
 *
 * Every format is a self-contained bsdconf_format_<keyword>.c defining a
 * single descriptor; the registry in bsdconf_format.c does nothing but
 * collect them into the table indexed by enum bsdconf_format. Adding a
 * format to the library therefore touches no existing parsing or writing
 * code: drop in the new file, declare its descriptor here, append one
 * enum constant in bsdconf.h and one pointer to the registry table, and
 * list the file in the Makefile.
 *
 * A candidate format whose syntax the existing processing/put flags cannot
 * express (for example, brace-grouped statement blocks) is handled by
 * teaching the shared engine (bsdconf.c and bsdconf_put.c) a new flag that
 * the new descriptor is first to set, so the capability composes with
 * every other format rather than living in a private parser.
 */
extern const struct bsdconf_format_def bsdconf_format_generic_def;
extern const struct bsdconf_format_def bsdconf_format_loader_def;
extern const struct bsdconf_format_def bsdconf_format_make_def;
extern const struct bsdconf_format_def bsdconf_format_src_def;
extern const struct bsdconf_format_def bsdconf_format_sysctl_def;

#endif /* !_BSDCONF_FORMATS_H_ */
