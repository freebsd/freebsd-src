/* $FreeBSD$ */

/*-
 * Copyright (c) 2011 Hans Petter Selasky. All rights reserved.
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

/*
 * Disclaimer: This utility and format is subject to change and not a
 * comitted interface.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <sysexits.h>
#include <unistd.h>

#include "bus_autoconf.h"
#include "bus_sections.h"
#include "bus_load_file.h"
#include "bus_usb.h"

static void
usage(void)
{
	fprintf(stderr,
	    "bus_autoconf - devd config file generator\n"
	    "	-i <structure_type,module.ko>\n"
	    "	-F <format_file>\n"
	    "	-h show usage\n"
	);
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	const char *params = "i:F:h";
	char *fname;
	char *section;
	char *module;
	char *postfix;
	uint8_t *ptr;
	uint32_t len;
	int c;
	int any_opt = 0;

	while ((c = getopt(argc, argv, params)) != -1) {
		switch (c) {
		case 'i':
			fname = optarg;
			load_file(fname, &ptr, &len);

			module = strchr(fname, ',');
			if (module == NULL) {
				errx(EX_USAGE, "Invalid input "
				    "file name '%s'", fname);
			}
			/* split module and section */
			*module++ = 0;

			/* remove postfix */
			postfix = strchr(module, '.');
			if (postfix)
				*postfix = 0;

			/* get section name */
			section = fname;

			/* check section type */
			if (strncmp(section, "usb_", 4) == 0)
				usb_import_entries(section, module, ptr, len);
			else
				errx(EX_USAGE, "Invalid section '%s'", section);

			free(ptr);

			any_opt = 1;
			break;

		case 'F':
			fname = optarg;
			load_file(fname, &ptr, &len);
			format_parse_entries(ptr, len);
			free(ptr);

			any_opt = 1;
			break;

		default:
			usage();
			break;
		}
	}

	if (any_opt == 0)
		usage();

	usb_dump_entries();

	return (0);
}
