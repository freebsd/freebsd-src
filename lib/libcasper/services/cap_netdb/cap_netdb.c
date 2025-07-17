/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Ryan Moeller <freqlabs@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/dnv.h>
#include <sys/nv.h>
#include <netinet/in.h>

#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libcasper.h>
#include <libcasper_service.h>

#include "cap_netdb.h"

static struct protoent *
protoent_unpack(nvlist_t *nvl)
{
	struct protoent *pp;
	char **aliases;
	size_t n;

	pp = malloc(sizeof(*pp));
	if (pp == NULL) {
		nvlist_destroy(nvl);
		return (NULL);
	}

	pp->p_name = nvlist_take_string(nvl, "name");

	aliases = nvlist_take_string_array(nvl, "aliases", &n);
	pp->p_aliases = realloc(aliases, sizeof(char *) * (n + 1));
	if (pp->p_aliases == NULL) {
		while (n-- > 0)
			free(aliases[n]);
		free(aliases);
		free(pp->p_name);
		free(pp);
		nvlist_destroy(nvl);
		return (NULL);
	}
	pp->p_aliases[n] = NULL;

	pp->p_proto = (int)nvlist_take_number(nvl, "proto");

	nvlist_destroy(nvl);
	return (pp);
}

struct protoent *
cap_getprotobyname(cap_channel_t *chan, const char *name)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "getprotobyname");
	nvlist_add_string(nvl, "name", name);
	nvl = cap_xfer_nvlist(chan, nvl);
	if (nvl == NULL)
		return (NULL);
	if (dnvlist_get_number(nvl, "error", 0) != 0) {
		nvlist_destroy(nvl);
		return (NULL);
	}
	return (protoent_unpack(nvl));
}

static void
protoent_pack(const struct protoent *pp, nvlist_t *nvl)
{
	int n = 0;

	nvlist_add_string(nvl, "name", pp->p_name);

	while (pp->p_aliases[n] != NULL)
		++n;
	nvlist_add_string_array(nvl, "aliases",
	    (const char * const *)pp->p_aliases, n);

	nvlist_add_number(nvl, "proto", (uint64_t)pp->p_proto);
}

static int
netdb_getprotobyname(const nvlist_t *limits __unused, const nvlist_t *nvlin,
    nvlist_t *nvlout)
{
	const char *name;
	struct protoent *pp;

	name = dnvlist_get_string(nvlin, "name", NULL);
	if (name == NULL)
		return (EDOOFUS);

	pp = getprotobyname(name);
	if (pp == NULL)
		return (EINVAL);

	protoent_pack(pp, nvlout);
	return (0);
}

static int
netdb_limit(const nvlist_t *oldlimits __unused,
    const nvlist_t *newlimits __unused)
{

	return (0);
}

static int
netdb_command(const char *cmd, const nvlist_t *limits, nvlist_t *nvlin,
    nvlist_t *nvlout)
{
	int error;

	if (strcmp(cmd, "getprotobyname") == 0)
		error = netdb_getprotobyname(limits, nvlin, nvlout);
	else
		error = NO_RECOVERY;

	return (error);
}

CREATE_SERVICE("system.netdb", netdb_limit, netdb_command, 0);
