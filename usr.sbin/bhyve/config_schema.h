/*
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef __CONFIG_SCHEMA_H__
#define __CONFIG_SCHEMA_H__

#include <sys/linker_set.h>
#include <sys/types.h>

enum config_option_type {
	CONFIG_OPTION_BOOL,
	CONFIG_OPTION_INTEGER,
	CONFIG_OPTION_STRING,
};

struct config_option {
	const char *name;
	enum config_option_type type;
};

struct config_option_set {
	const char *prefix;
	const struct config_option_set *base;
	const struct config_option *options;
	size_t count;
};

struct config_schema;

#define	CONFIG_OPTION_SET(x)	DATA_SET(config_option_set, x)

void	config_schema_merge_options(struct config_schema *, const char *,
	    const struct config_option *, size_t);
void	config_schema_merge_options_machdep(struct config_schema *);
void	print_config_options(void);

#endif /* !__CONFIG_SCHEMA_H__ */
