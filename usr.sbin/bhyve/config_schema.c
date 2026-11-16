/*
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/tree.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "config_schema.h"

SET_DECLARE(config_option_set, struct config_option_set);

struct config_schema_option {
	RB_ENTRY(config_schema_option) entry;
	char *name;
	enum config_option_type type;
};

RB_HEAD(config_schema, config_schema_option);

static int
config_schema_option_compare(struct config_schema_option *a,
    struct config_schema_option *b)
{

	return (strcmp(a->name, b->name));
}

RB_GENERATE_STATIC(config_schema, config_schema_option, entry,
    config_schema_option_compare);

static const struct config_option config_options[] = {
	{ "name", CONFIG_OPTION_STRING },
	{ "cpus", CONFIG_OPTION_INTEGER },
	{ "cores", CONFIG_OPTION_INTEGER },
	{ "threads", CONFIG_OPTION_INTEGER },
	{ "sockets", CONFIG_OPTION_INTEGER },
	{ "memory.guest_in_core", CONFIG_OPTION_BOOL },
	{ "memory.size", CONFIG_OPTION_STRING },
	{ "memory.wired", CONFIG_OPTION_BOOL },
	{ "bootrom", CONFIG_OPTION_STRING },
	{ "destroy_on_poweroff", CONFIG_OPTION_BOOL },
	{ "pci.enable_bars", CONFIG_OPTION_BOOL },
	{ "uuid", CONFIG_OPTION_STRING },
	{ "virtio.msix", CONFIG_OPTION_BOOL },
	{ "config.dump", CONFIG_OPTION_BOOL },
	{ "rundir", CONFIG_OPTION_STRING },
	{ "monitor.enabled", CONFIG_OPTION_BOOL },
	{ "monitor.no_reboot", CONFIG_OPTION_BOOL },

	{ "vcpu.N.cpuset", CONFIG_OPTION_STRING },
	{ "domains.N.size", CONFIG_OPTION_STRING },
	{ "domains.N.domain_policy", CONFIG_OPTION_STRING },
	{ "domains.N.cpus", CONFIG_OPTION_STRING },

	{ "pci.B.S.F.device", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.vendor", CONFIG_OPTION_INTEGER },
	{ "pci.B.S.F.devid", CONFIG_OPTION_INTEGER },
	{ "pci.B.S.F.pcireg.*", CONFIG_OPTION_INTEGER },
	{ "pci.B.S.F.pcireg.vendor", CONFIG_OPTION_INTEGER },
	{ "pci.B.S.F.pcireg.device", CONFIG_OPTION_INTEGER },
	{ "pci.B.S.F.port.N.type", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.port.N.path", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.port.N.nocache", CONFIG_OPTION_BOOL },
	{ "pci.B.S.F.port.N.nodelete", CONFIG_OPTION_BOOL },
	{ "pci.B.S.F.port.N.sync", CONFIG_OPTION_BOOL },
	{ "pci.B.S.F.port.N.direct", CONFIG_OPTION_BOOL },
	{ "pci.B.S.F.port.N.ro", CONFIG_OPTION_BOOL },
	{ "pci.B.S.F.port.N.sectorsize", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.port.N.nmrr", CONFIG_OPTION_INTEGER },
	{ "pci.B.S.F.port.N.ser", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.port.N.rev", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.port.N.model", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.port.N.bootindex", CONFIG_OPTION_INTEGER },

	{ "pci.B.S.F.backend", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.type", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.peerhook", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.socket", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.hook", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.hostfwd", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.mac", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.mtu", CONFIG_OPTION_INTEGER },
	{ "pci.B.S.F.play", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.rec", CONFIG_OPTION_STRING },

	{ "pci.B.S.F.serial", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.sharename", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.port.N.name", CONFIG_OPTION_STRING },

	{ "pci.B.S.F.ctl_ringsz", CONFIG_OPTION_INTEGER },
	{ "pci.B.S.F.evt_ringsz", CONFIG_OPTION_INTEGER },
	{ "pci.B.S.F.req_ringsz", CONFIG_OPTION_INTEGER },
	{ "pci.B.S.F.num_queues", CONFIG_OPTION_INTEGER },
	{ "pci.B.S.F.seg_max", CONFIG_OPTION_INTEGER },
	{ "pci.B.S.F.thr_per_q", CONFIG_OPTION_INTEGER },
	{ "pci.B.S.F.target.N", CONFIG_OPTION_STRING },
	{ "pci.B.S.F.iid", CONFIG_OPTION_INTEGER },
	{ "pci.B.S.F.slot.N.device", CONFIG_OPTION_STRING },
};

static const char *
config_option_type_name(enum config_option_type type)
{

	switch (type) {
	case CONFIG_OPTION_BOOL:
		return ("bool");
	case CONFIG_OPTION_INTEGER:
		return ("integer");
	case CONFIG_OPTION_STRING:
		return ("string");
	}
	errx(EX_SOFTWARE, "invalid configuration option type");
}

void
config_schema_merge_options(struct config_schema *schema, const char *prefix,
    const struct config_option *options, size_t count)
{
	struct config_schema_option *existing, *option;
	char *name;
	size_t i;

	if (prefix == NULL)
		prefix = "";
	for (i = 0; i < count; i++) {
		if (asprintf(&name, "%s%s", prefix, options[i].name) == -1)
			err(EX_OSERR, "asprintf");
		option = malloc(sizeof(*option));
		if (option == NULL)
			err(EX_OSERR, "malloc");
		option->name = name;
		option->type = options[i].type;

		existing = RB_INSERT(config_schema, schema, option);
		if (existing == NULL)
			continue;
		if (existing->type != option->type) {
			errx(EX_SOFTWARE,
			    "conflicting types for configuration option "
			    "'%s': %s and %s", option->name,
			    config_option_type_name(existing->type),
			    config_option_type_name(option->type));
		}
		free(option->name);
		free(option);
	}
}

static void
config_schema_merge_option_set(struct config_schema *schema,
    const struct config_option_set *set, const char *inherited_prefix)
{
	const char *prefix;

	prefix = set->prefix != NULL ? set->prefix : inherited_prefix;
	if (set->base != NULL)
		config_schema_merge_option_set(schema, set->base, prefix);
	config_schema_merge_options(schema, prefix, set->options, set->count);
}

static void
config_schema_destroy(struct config_schema *schema)
{
	struct config_schema_option *option, *tmp;

	RB_FOREACH_SAFE(option, config_schema, schema, tmp) {
		RB_REMOVE(config_schema, schema, option);
		free(option->name);
		free(option);
	}
}

void
print_config_options(void)
{
	struct config_schema_option *option;
	struct config_option_set **setp;
	struct config_schema schema;

	RB_INIT(&schema);
	config_schema_merge_options(&schema, NULL, config_options,
	    nitems(config_options));
	SET_FOREACH(setp, config_option_set) {
		config_schema_merge_option_set(&schema, *setp, NULL);
	}
	config_schema_merge_options_machdep(&schema);

	RB_FOREACH(option, config_schema, &schema) {
		printf("%s\n", option->name);
	}
	config_schema_destroy(&schema);
}
