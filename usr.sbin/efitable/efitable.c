/*-
 * Copyright (c) 2021 3mdeb Embedded Systems Consulting <contact@3mdeb.com>
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

#include <sys/types.h>
#include <sys/efi.h>
#include <sys/efiio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <uuid.h>
#include <libxo/xo.h>

#define TABLE_MAX_LEN 30
#define EFITABLE_XO_VERSION "1"

static void efi_table_print_esrt(const void *data);
static void efi_table_print_prop(const void *data);
static void usage(void) __dead2;

struct efi_table_op {
	char name[TABLE_MAX_LEN];
	void (*parse) (const void *);
	struct uuid uuid;
};

static const struct efi_table_op efi_table_ops[] = {
	{ .name = "esrt", .parse = efi_table_print_esrt,
	    .uuid = EFI_TABLE_ESRT },
	{ .name = "prop", .parse = efi_table_print_prop,
	    .uuid = EFI_PROPERTIES_TABLE }
};

int
main(int argc, char **argv)
{
	struct efi_get_table_ioc table = {
		.buf = NULL,
		.buf_len = 0,
		.table_len = 0
	};
	int efi_fd, ch, rc = 1, efi_idx = -1;
	bool got_table = false;
	bool table_set = false;
	bool uuid_set = false;
	struct option longopts[] = {
		{ "uuid",  required_argument, NULL, 'u' },
		{ "table", required_argument, NULL, 't' },
		{ NULL,    0,                 NULL,  0  }
	};

	argc = xo_parse_args(argc, argv);
	if (argc < 0)
		exit(EXIT_FAILURE);

	while ((ch = getopt_long(argc, argv, "u:t:", longopts, NULL)) != -1) {
		switch (ch) {
		case 'u':
		{
			char *uuid_str = optarg;
			struct uuid uuid;
			uint32_t status;

			uuid_set = 1;

			uuid_from_string(uuid_str, &uuid, &status);
			if (status != uuid_s_ok)
				xo_errx(EX_DATAERR, "invalid UUID");

			for (size_t n = 0; n < nitems(efi_table_ops); n++) {
				if (!memcmp(&uuid, &efi_table_ops[n].uuid,
				    sizeof(uuid))) {
					efi_idx = n;
					got_table = true;
					break;
				}
			}
			break;
		}
		case 't':
		{
			char *table_name = optarg;

			table_set = true;

			for (size_t n = 0; n < nitems(efi_table_ops); n++) {
				if (!strcmp(table_name,
				    efi_table_ops[n].name)) {
					efi_idx = n;
					got_table = true;
					break;
				}
			}

			if (!got_table)
				xo_errx(EX_DATAERR, "unsupported efi table");

			break;
		}
		default:
			usage();
		}
	}

	if (!table_set && !uuid_set)
		xo_errx(EX_USAGE, "table is not set");

	if (!got_table)
		xo_errx(EX_DATAERR, "unsupported table");

	efi_fd = open("/dev/efi", O_RDWR);
	if (efi_fd < 0)
		xo_err(EX_OSFILE, "/dev/efi");

	table.uuid = efi_table_ops[efi_idx].uuid;
	if (ioctl(efi_fd, EFIIOC_GET_TABLE, &table) == -1)
		xo_err(EX_OSERR, "EFIIOC_GET_TABLE (len == 0)");

	table.buf = malloc(table.table_len);
	table.buf_len = table.table_len;

	if (ioctl(efi_fd, EFIIOC_GET_TABLE, &table) == -1)
		xo_err(EX_OSERR, "EFIIOC_GET_TABLE");

	efi_table_ops[efi_idx].parse(table.buf);
	close(efi_fd);

	return (rc);
}

static void
efi_table_print_esrt(const void *data)
{
	const struct efi_esrt_entry_v1 *entries_v1;
	const struct efi_esrt_table *esrt;

	esrt = (const struct efi_esrt_table *)data;

	xo_set_version(EFITABLE_XO_VERSION);
	xo_open_container("esrt");
	xo_emit("{Lwc:FwResourceCount}{:fw_resource_count/%u}\n",
	    esrt->fw_resource_count);
	xo_emit("{Lwc:FwResourceCountMax}{:fw_resource_count_max/%u}\n",
	    esrt->fw_resource_count_max);
	xo_emit("{Lwc:FwResourceVersion}{:fw_resource_version/%u}\n",
	    esrt->fw_resource_version);
	xo_open_list("entries");
	xo_emit("\nEntries:\n");

	entries_v1 = (const void *) esrt->entries;
	for (uint32_t i = 0; i < esrt->fw_resource_count; i++) {
		const struct efi_esrt_entry_v1 *e = &entries_v1[i];
		uint32_t status;
		char *uuid;

		uuid_to_string(&e->fw_class, &uuid, &status);
		if (status != uuid_s_ok) {
			xo_errx(EX_DATAERR, "uuid_to_string error");
		}

		xo_open_instance("entries");
		xo_emit("\n");
		xo_emit("{P:  }{Lwc:FwClass}{:fw_class/%s}\n", uuid);
		xo_emit("{P:  }{Lwc:FwType}{:fw_type/%u}\n", e->fw_type);
		xo_emit("{P:  }{Lwc:FwVersion}{:fw_version/%u}\n",
		    e->fw_version);
		xo_emit("{P:  }{Lwc:LowestSupportedFwVersion}"
		    "{:lowest_supported_fw_version/%u}\n",
		    e->lowest_supported_fw_version);
		xo_emit("{P:  }{Lwc:CapsuleFlags}{:capsule_flags/%#x}\n",
		    e->capsule_flags);
		xo_emit("{P:  }{Lwc:LastAttemptVersion"
		    "}{:last_attempt_version/%u}\n", e->last_attempt_version);
		xo_emit("{P:  }{Lwc:LastAttemptStatus"
		    "}{:last_attempt_status/%u}\n", e->last_attempt_status);

		xo_close_instance("entries");
	}

	xo_close_list("entries");
	xo_close_container("esrt");
	if (xo_finish() < 0)
		xo_err(EX_IOERR, "stdout");
}

static void
efi_table_print_prop(const void *data)
{
	const struct efi_prop_table *prop;

	prop = (const struct efi_prop_table *)data;

	xo_set_version(EFITABLE_XO_VERSION);
	xo_open_container("prop");
	xo_emit("{Lwc:Version}{:version/%#x}\n", prop->version);
	xo_emit("{Lwc:Length}{:length/%u}\n", prop->length);
	xo_emit("{Lwc:MemoryProtectionAttribute}"
	    "{:memory_protection_attribute/%#lx}\n",
	    prop->memory_protection_attribute);
	xo_close_container("prop");
	if (xo_finish() < 0)
		xo_err(EX_IOERR, "stdout");
}

static void usage(void)
{
	xo_error("usage: efitable [-d uuid | -t name] [--libxo]\n");
	exit(EX_USAGE);
}
