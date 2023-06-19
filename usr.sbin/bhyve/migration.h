/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017-2020 Elena Mihailescu
 * Copyright (c) 2017-2020 Darius Mihai
 * Copyright (c) 2017-2020 Mihai Carabas
 *
 * The migration feature was developed under sponsorships
 * from Matthew Grooms.
 *
 */

#pragma once

#include <sys/param.h>
#include <stdbool.h>

#define DEFAULT_MIGRATION_PORT	24983

struct vmctx;

struct migrate_req {
	char host[MAXHOSTNAMELEN];
	unsigned int port;
};

int receive_vm_migration(struct vmctx *ctx, char *migration_data);
