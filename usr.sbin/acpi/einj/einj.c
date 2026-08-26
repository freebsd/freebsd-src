/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Netflix, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/actbl1.h>
#pragma GCC diagnostic pop

#include <dev/acpica/acpiio.h>
#include <dev/pci/pcireg.h>

static int fd;

static struct error_type_info {
	const char *name;
	uint32_t bit;
	const char *description;
} error_types[] = {
	{ "cpu.cor", ACPI_EINJ_PROCESSOR_CORRECTABLE, "Processor Correctable" },
	{ "cpu.uc", ACPI_EINJ_PROCESSOR_UNCORRECTABLE,
	  "Processor Uncorrectable Non-Fatal" },
	{ "cpu.fatal", ACPI_EINJ_PROCESSOR_FATAL,
	  "Processor Uncorrectable Fatal" },
	{ "mem.cor", ACPI_EINJ_MEMORY_CORRECTABLE, "Memory Correctable" },
	{ "mem.uc", ACPI_EINJ_MEMORY_UNCORRECTABLE,
	  "Memory Uncorrectable Non-Fatal" },
	{ "mem.fatal", ACPI_EINJ_MEMORY_FATAL, "Memory Uncorrectable Fatal" },
	{ "pcie.cor", ACPI_EINJ_PCIX_CORRECTABLE, "PCI Express Correctable" },
	{ "pcie.uc", ACPI_EINJ_PCIX_UNCORRECTABLE,
	  "PCI Express Uncorrectable Non-Fatal" },
	{ "pcie.fatal", ACPI_EINJ_PCIX_FATAL,
	  "PCI Express Uncorrectable Fatal" },
	{ "platform.cor", ACPI_EINJ_PLATFORM_CORRECTABLE,
	  "Platform Correctable" },
	{ "platform.uc", ACPI_EINJ_PLATFORM_UNCORRECTABLE,
	  "Platform Uncorrectable Non-Fatal" },
	{ "platform.fatal", ACPI_EINJ_PLATFORM_FATAL,
	  "Platform Uncorrectable Fatal" },
	{ "cxl.cache.cor", ACPI_EINJ_CXL_CACHE_CORRECTABLE,
	  "CXL.cache Correctable" },
	{ "cxl.cache.uc", ACPI_EINJ_CXL_CACHE_UNCORRECTABLE,
	  "CXL.cache Uncorrectable Non-Fatal" },
	{ "cxl.cache.fatal", ACPI_EINJ_CXL_CACHE_FATAL,
	  "CXL.cache Uncorrectable Fatal" },
	{ "cxl.mem.cor", ACPI_EINJ_CXL_MEM_CORRECTABLE, "CXL.mem Correctable" },
	{ "cxl.mem.uc", ACPI_EINJ_CXL_MEM_UNCORRECTABLE,
	  "CXL.mem Uncorrectable Non-Fatal" },
	{ "cxl.mem.fatal", ACPI_EINJ_CXL_MEM_FATAL,
	  "CXL.mem Uncorrectable Fatal" },
	{ "vendor", ACPI_EINJ_VENDOR_DEFINED, "Vendor Defined" },
};

static void
usage(void)
{
	fprintf(stderr, "usage:\n"
	    "\teinj list\n"
	    "\teinj inject -a <address> -C <cpu> -P <dbsf> <type>\n");
	exit(1);
}

static void
print_vendor_struct(uint32_t vendor_length)
{
	struct acpi_einj_vendor_info vi;
	ACPI_EINJ_VENDOR *vendor;
	uint32_t dbsf;

	vi.buf = malloc(vendor_length);
	vi.len = vendor_length;
	if (ioctl(fd, ACPIIO_EINJ_GET_VENDOR, &vi) == -1) {
		warn("ACPIIO_EINJ_GET_VENDOR");
		free(vi.buf);
		return;
	}

	vendor = vi.buf;
	dbsf = vendor->PcieId;

	printf("Vendor Extension:\n");
	printf("\tLength:\t%u\n", vendor->Length);
	printf("\tPCIe Id:\t%u:%u:%u:%u\n", dbsf >> 24,
	    (dbsf >> 16) & 0xff, (dbsf >> 11) & 0x1f, (dbsf >> 8) & 7);
	printf("\tVendor Id:\t%04x\n", vendor->VendorId);
	printf("\tDevice Id:\t%04x\n", vendor->DeviceId);
	printf("\tRevision:\t%02x\n", vendor->RevisionId);
	free(vi.buf);
}

static void
list_error_types(void)
{
	struct acpi_einj_info info;

	if (ioctl(fd, ACPIIO_EINJ_GET_INFO, &info) == -1)
		err(1, "ACPIIO_EINJ_GET_INFO");

	printf("Supported errors <%#x>:\n", info.error_type);
	for (u_int i = 0; i < nitems(error_types); i++) {
		struct error_type_info *eti = &error_types[i];

		if ((info.error_type & eti->bit) == 0)
			continue;
		printf("\t%s (%s)\n", eti->name, eti->description);
	}

	if ((info.error_type & ACPI_EINJ_VENDOR_DEFINED) != 0 &&
	    info.vendor_length != 0) {
		printf("\n");
		print_vendor_struct(info.vendor_length);
	}
}

static uint32_t
parse_dbsf(const char *arg)
{
	char *cp;
	unsigned long val[4];
	u_int i;

	/*
	 * Permit the same format as pciconf.  Eventually this should
	 * accept PCI device names, too.
	 */
	if (strncmp(arg, "pci", 3) == 0)
		arg += 3;

	/* Read up to 4 colon-separated values. */
	for (i = 0;; i++) {
		val[i] = strtoul(arg, &cp, 10);
		if (cp == arg || val[i] > 255)
			goto error;
		if (*cp == ':') {
			if (i == nitems(val) - 1)
				goto error;
			arg = cp + 1;
			continue;
		}
		if (*cp == '\0') {
			if (i < 3)
				goto error;
			break;
		}
		goto error;
	}

	/* If no domain is given, assume a domain of 0. */
	if (i == 3) {
		val[3] = val[2];
		val[2] = val[1];
		val[1] = val[0];
		val[0] = 0;
	}

	/*
	 * To handle ARI, only limit function numbers if the slot is
	 * non-zero.
	 */
	if (val[2] != 0 && val[3] > PCI_FUNCMAX)
		goto error;

	return (val[0] << 24 | val[1] << 16 | val[2] << 3 | val[3]);

error:
	errx(1, "invalid PCIe ID");
}

static void
inject_error(int argc, char *argv[])
{
	struct acpi_einj_error einj;
	char *cp;
	uintmax_t value;
	int ch;

	memset(&einj, 0, sizeof(einj));
	while ((ch = getopt(argc, argv, "a:C:P:")) != -1) {
		switch (ch) {
		case 'a':
			einj.memory_address = strtoumax(optarg, &cp, 0);
			if (*cp != '\0')
				errx(1, "invalid address");
			einj.address_flags |= ACPI_EINJ_MEMADDRESS_VALID;
			break;
		case 'C':
			value = strtoumax(optarg, &cp, 0);
			if (*cp != '\0' || value > UINT32_MAX)
				errx(1, "invalid APIC ID");
			einj.apic_id = value;
			einj.address_flags |= ACPI_EINJ_APICID_VALID;
			break;
		case 'P':
			einj.pcie_id = parse_dbsf(optarg);
			einj.address_flags |= ACPI_EINJ_PCIE_VALID;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	if (strcmp(argv[0], "vendor") == 0)
		errx(1, "vendor errors are not supported");

	for (u_int i = 0; i < nitems(error_types); i++) {
		struct error_type_info *eti = &error_types[i];

		if (strcmp(argv[0], eti->name) == 0) {
			einj.error_type = eti->bit;
			break;
		}
	}
	if (einj.error_type == 0)
		errx(1, "unknown error type");

	if (ioctl(fd, ACPIIO_EINJ_SET_ERROR, &einj) == -1)
		err(1, "ACPIIO_EINJ_SET_ERROR");
}

int
main(int argc, char *argv[])
{
	if (argc < 2)
		usage();

	fd = open("/dev/acpi", O_RDWR);
	if (fd == -1)
		err(1, "/dev/acpi");

	if (strcasecmp(argv[1], "list") == 0)
		list_error_types();
	else if (strcasecmp(argv[1], "inject") == 0)
		inject_error(argc - 1, argv + 1);
	else
		usage();

	close(fd);
	return (0);
}
