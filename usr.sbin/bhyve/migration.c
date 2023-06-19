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

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/nv.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <arpa/inet.h>
#ifndef WITHOUT_CAPSICUM
#include <capsicum_helpers.h>
#include <libcasper.h>
#include <casper/cap_net.h>
#include <casper/cap_sysctl.h>
#endif
#include <err.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vmmapi.h>

#include "migration.h"
#include "pci_emul.h"
#include "snapshot.h"


#ifdef BHYVE_DEBUG
#define DPRINTF(FMT, ...)							\
({										\
	fprintf(stderr, "%s: " FMT "\n", __func__, ##__VA_ARGS__);		\
 })
#else
#define DPRINTF(FMT, ...)
#endif

#define EPRINTF(FMT, ...)							\
({										\
	fprintf(stderr, "%s: " FMT "\n", __func__, ##__VA_ARGS__);		\
 })

int
receive_vm_migration(struct vmctx *ctx, char *migration_data)
{
	struct migrate_req req;
	size_t len;
	char *hostname, *pos;
	unsigned int port = DEFAULT_MIGRATION_PORT;
	int rc;

	assert(ctx != NULL);
	assert(migration_data != NULL);

	memset(req.host, 0, MAXHOSTNAMELEN);
	hostname = strdup(migration_data);

	if ((pos = strchr(hostname, ':')) != NULL) {
		*pos = '\0';
		pos = pos + 1;

		rc = sscanf(pos, "%u", &port);

		if (rc <= 0) {
			EPRINTF("Could not parse the port");
			free(hostname);
			return (EINVAL);
		}
	}
	req.port = port;

	len = strlen(hostname);
	if (len > MAXHOSTNAMELEN - 1) {
		EPRINTF("Hostname length %lu bigger than maximum allowed %d",
			len, MAXHOSTNAMELEN - 1);
		free(hostname);
		return (EINVAL);
	}

	strlcpy(req.host, hostname, MAXHOSTNAMELEN);

	// rc = vm_recv_migrate_req(ctx, req);
	rc = EOPNOTSUPP;
	EPRINTF("Migration not implemented yet");

	free(hostname);
	return (rc);
}
