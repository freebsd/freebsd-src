/*-
 * Copyright (c) 2014-2015 Sandvine Inc.  All rights reserved.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>

#include <machine/stdarg.h>

#include <sys/nv.h>
#include <sys/iov_schema.h>

#include <dev/pci/schema_private.h>

static const char *pci_iov_schema_valid_types[] = {
	"bool",
	"string",
	"uint8_t",
	"uint16_t",
	"uint32_t",
	"uint64_t",
	"unicast-mac",
};

static void
pci_iov_schema_add_type(nvlist_t *entry, const char *type)
{
	int i, error;

	error = EINVAL;
	for (i = 0; i < nitems(pci_iov_schema_valid_types); i++) {
		if (strcmp(type, pci_iov_schema_valid_types[i]) == 0) {
			error = 0;
			break;
		}
	}

	if (error != 0) {
		nvlist_set_error(entry, error);
		return;
	}

	nvlist_add_string(entry, "type", type);
}

static void
pci_iov_schema_add_required(nvlist_t *entry, uint32_t flags)
{

	if (flags & IOV_SCHEMA_REQUIRED) {
		if (flags & IOV_SCHEMA_HASDEFAULT) {
			nvlist_set_error(entry, EINVAL);
			return;
		}

		nvlist_add_bool(entry, "required", 1);
	}
}

void
pci_iov_schema_add_bool(nvlist_t *schema, const char *name, uint32_t flags,
    int defaultVal)
{
	nvlist_t *entry;

	entry = nvlist_create(NV_FLAG_IGNORE_CASE);
	if (entry == NULL) {
		nvlist_set_error(schema, ENOMEM);
		return;
	}

	pci_iov_schema_add_type(entry, "bool");
	if (flags & IOV_SCHEMA_HASDEFAULT)
		nvlist_add_bool(entry, "default", defaultVal);
	pci_iov_schema_add_required(entry, flags);

	nvlist_move_nvlist(schema, name, entry);
}

void
pci_iov_schema_add_string(nvlist_t *schema, const char *name, uint32_t flags,
    const char *defaultVal)
{
	nvlist_t *entry;

	entry = nvlist_create(NV_FLAG_IGNORE_CASE);
	if (entry == NULL) {
		nvlist_set_error(schema, ENOMEM);
		return;
	}

	pci_iov_schema_add_type(entry, "string");
	if (flags & IOV_SCHEMA_HASDEFAULT)
		nvlist_add_string(entry, "default", defaultVal);
	pci_iov_schema_add_required(entry, flags);

	nvlist_move_nvlist(schema, name, entry);
}

static void
pci_iov_schema_int(nvlist_t *schema, const char *name, const char *type,
    uint32_t flags, uint64_t defaultVal)
{
	nvlist_t *entry;

	entry = nvlist_create(NV_FLAG_IGNORE_CASE);
	if (entry == NULL) {
		nvlist_set_error(schema, ENOMEM);
		return;
	}

	pci_iov_schema_add_type(entry, type);
	if (flags & IOV_SCHEMA_HASDEFAULT)
		nvlist_add_number(entry, "default", defaultVal);
	pci_iov_schema_add_required(entry, flags);

	nvlist_move_nvlist(schema, name, entry);
}

void
pci_iov_schema_add_uint8(nvlist_t *schema, const char *name, uint32_t flags,
    uint8_t defaultVal)
{

	pci_iov_schema_int(schema, name, "uint8_t", flags, defaultVal);
}

void
pci_iov_schema_add_uint16(nvlist_t *schema, const char *name, uint32_t flags,
    uint16_t defaultVal)
{

	pci_iov_schema_int(schema, name, "uint16_t", flags, defaultVal);
}

void
pci_iov_schema_add_uint32(nvlist_t *schema, const char *name, uint32_t flags,
    uint32_t defaultVal)
{

	pci_iov_schema_int(schema, name, "uint32_t", flags, defaultVal);
}

void
pci_iov_schema_add_uint64(nvlist_t *schema, const char *name, uint32_t flags,
    uint64_t defaultVal)
{

	pci_iov_schema_int(schema, name, "uint64_t", flags, defaultVal);
}

void
pci_iov_schema_add_unicast_mac(nvlist_t *schema, const char *name,
    uint32_t flags, const uint8_t * defaultVal)
{
	nvlist_t *entry;

	entry = nvlist_create(NV_FLAG_IGNORE_CASE);
	if (entry == NULL) {
		nvlist_set_error(schema, ENOMEM);
		return;
	}

	pci_iov_schema_add_type(entry, "unicast-mac");
	if (flags & IOV_SCHEMA_HASDEFAULT)
		nvlist_add_binary(entry, "default", defaultVal, ETHER_ADDR_LEN);
	pci_iov_schema_add_required(entry, flags);

	nvlist_move_nvlist(schema, name, entry);
}

/* Allocate a new empty schema node. */
nvlist_t *
pci_iov_schema_alloc_node(void)
{

	return (nvlist_create(NV_FLAG_IGNORE_CASE));
}