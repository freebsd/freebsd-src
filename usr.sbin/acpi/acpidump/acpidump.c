/*-
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *
 *	$Id: acpidump.c,v 1.3 2000/08/08 14:12:21 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>

#include "acpidump.h"

static void
asl_dump_from_file(char *file)
{
	u_int8_t	*dp;
	u_int8_t	*end;
	struct	ACPIsdt *dsdt;

	acpi_load_dsdt(file, &dp, &end);
	acpi_dump_dsdt(dp, end);
}

static void
asl_dump_from_devmem()
{
	struct	ACPIrsdp *rp;
	struct	ACPIsdt *rsdp;

	rp = acpi_find_rsd_ptr();
	if (!rp)
		errx(1, "Can't find ACPI information\n");

	acpi_print_rsd_ptr(rp);
	rsdp = (struct ACPIsdt *) acpi_map_sdt(rp->addr);
	if (memcmp(rsdp->signature, "RSDT", 4) ||
	    acpi_checksum(rsdp, rsdp->len))
		errx(1, "RSDT is corrupted\n");

	acpi_handle_rsdt(rsdp);
}

static void
usage(const char *progname)
{

	printf("usage:\t%s [-r] [-o dsdt_file_for_output]\n", progname);
	printf("\t%s [-r] [-f dsdt_file_for_input]\n", progname);
	printf("\t%s [-h]\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char	c, *progname;

	progname = argv[0];
	while ((c = getopt(argc, argv, "f:o:hr")) != -1) {
		switch (c) {
		case 'f':
			asl_dump_from_file(optarg);
			return (0);
		case 'o':
			aml_dumpfile = optarg;
			break;
		case 'h':
			usage(progname);
			break;
		case 'r':
			rflag++;
			break;
		default:
			argc -= optind;
			argv += optind;
		}
	}

	asl_dump_from_devmem();
	return (0);
}
