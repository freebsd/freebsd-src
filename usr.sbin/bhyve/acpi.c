/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * bhyve ACPI table generator.
 *
 * Create the minimal set of ACPI tables required to boot FreeBSD (and
 * hopefully other o/s's).
 *
 * The tables are placed in the guest's ROM area just below 1MB physical,
 * above the MPTable.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <err.h>
#include <paths.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <machine/vmm.h>
#include <vmmapi.h>

#include "bhyverun.h"
#include "acpi.h"
#include "basl.h"
#include "pci_emul.h"
#include "vmgenc.h"

#define	BHYVE_ASL_TEMPLATE	"bhyve.XXXXXXX"
#define BHYVE_ASL_SUFFIX	".aml"
#define BHYVE_ASL_COMPILER	"/usr/sbin/iasl"

#define BHYVE_ADDRESS_IOAPIC 	0xFEC00000
#define BHYVE_ADDRESS_HPET 	0xFED00000
#define BHYVE_ADDRESS_LAPIC 	0xFEE00000

static int basl_keep_temps;
static int basl_verbose_iasl;
static int basl_ncpu;
static uint32_t hpet_capabilities;

/*
 * Contains the full pathname of the template to be passed
 * to mkstemp/mktemps(3)
 */
static char basl_template[MAXPATHLEN];
static char basl_stemplate[MAXPATHLEN];

/*
 * State for dsdt_line(), dsdt_indent(), and dsdt_unindent().
 */
static FILE *dsdt_fp;
static int dsdt_indent_level;
static int dsdt_error;

static struct basl_table *rsdt;
static struct basl_table *xsdt;

struct basl_fio {
	int	fd;
	FILE	*fp;
	char	f_name[MAXPATHLEN];
};

#define EFPRINTF(...) \
	if (fprintf(__VA_ARGS__) < 0) goto err_exit

#define EFFLUSH(x) \
	if (fflush(x) != 0) goto err_exit

/*
 * A list for additional ACPI devices like a TPM.
 */
struct acpi_device_list_entry {
	SLIST_ENTRY(acpi_device_list_entry) chain;
	const struct acpi_device *dev;
};
static SLIST_HEAD(acpi_device_list,
    acpi_device_list_entry) acpi_devices = SLIST_HEAD_INITIALIZER(acpi_devices);

int
acpi_tables_add_device(const struct acpi_device *const dev)
{
	struct acpi_device_list_entry *const entry = calloc(1, sizeof(*entry));
	if (entry == NULL) {
		return (ENOMEM);
	}

	entry->dev = dev;
	SLIST_INSERT_HEAD(&acpi_devices, entry, chain);

	return (0);
}

/*
 * Helper routines for writing to the DSDT from other modules.
 */
void
dsdt_line(const char *fmt, ...)
{
	va_list ap;

	if (dsdt_error != 0)
		return;

	if (strcmp(fmt, "") != 0) {
		if (dsdt_indent_level != 0)
			EFPRINTF(dsdt_fp, "%*c", dsdt_indent_level * 2, ' ');
		va_start(ap, fmt);
		if (vfprintf(dsdt_fp, fmt, ap) < 0) {
			va_end(ap);
			goto err_exit;
		}
		va_end(ap);
	}
	EFPRINTF(dsdt_fp, "\n");
	return;

err_exit:
	dsdt_error = errno;
}

void
dsdt_indent(int levels)
{

	dsdt_indent_level += levels;
	assert(dsdt_indent_level >= 0);
}

void
dsdt_unindent(int levels)
{

	assert(dsdt_indent_level >= levels);
	dsdt_indent_level -= levels;
}

void
dsdt_fixed_ioport(uint16_t iobase, uint16_t length)
{

	dsdt_line("IO (Decode16,");
	dsdt_line("  0x%04X,             // Range Minimum", iobase);
	dsdt_line("  0x%04X,             // Range Maximum", iobase);
	dsdt_line("  0x01,               // Alignment");
	dsdt_line("  0x%02X,               // Length", length);
	dsdt_line("  )");
}

void
dsdt_fixed_irq(uint8_t irq)
{

	dsdt_line("IRQNoFlags ()");
	dsdt_line("  {%d}", irq);
}

void
dsdt_fixed_mem32(uint32_t base, uint32_t length)
{

	dsdt_line("Memory32Fixed (ReadWrite,");
	dsdt_line("  0x%08X,         // Address Base", base);
	dsdt_line("  0x%08X,         // Address Length", length);
	dsdt_line("  )");
}

static int
basl_fwrite_dsdt(FILE *fp)
{
	dsdt_fp = fp;
	dsdt_error = 0;
	dsdt_indent_level = 0;

	dsdt_line("/*");
	dsdt_line(" * bhyve DSDT template");
	dsdt_line(" */");
	dsdt_line("DefinitionBlock (\"bhyve_dsdt.aml\", \"DSDT\", 2,"
		 "\"BHYVE \", \"BVDSDT  \", 0x00000001)");
	dsdt_line("{");
	dsdt_line("  Name (_S5, Package ()");
	dsdt_line("  {");
	dsdt_line("      0x05,");
	dsdt_line("      Zero,");
	dsdt_line("  })");

	pci_write_dsdt();

	dsdt_line("");
	dsdt_line("  Scope (_SB.PC00)");
	dsdt_line("  {");
	dsdt_line("    Device (HPET)");
	dsdt_line("    {");
	dsdt_line("      Name (_HID, EISAID(\"PNP0103\"))");
	dsdt_line("      Name (_UID, 0)");
	dsdt_line("      Name (_CRS, ResourceTemplate ()");
	dsdt_line("      {");
	dsdt_indent(4);
	dsdt_fixed_mem32(0xFED00000, 0x400);
	dsdt_unindent(4);
	dsdt_line("      })");
	dsdt_line("    }");
	dsdt_line("  }");

	vmgenc_write_dsdt();

	const struct acpi_device_list_entry *entry;
	SLIST_FOREACH(entry, &acpi_devices, chain) {
		BASL_EXEC(acpi_device_write_dsdt(entry->dev));
	}

	dsdt_line("}");

	if (dsdt_error != 0)
		return (dsdt_error);

	EFFLUSH(fp);

	return (0);

err_exit:
	return (errno);
}

static int
basl_open(struct basl_fio *bf, int suffix)
{
	int err;

	err = 0;

	if (suffix) {
		strlcpy(bf->f_name, basl_stemplate, MAXPATHLEN);
		bf->fd = mkstemps(bf->f_name, strlen(BHYVE_ASL_SUFFIX));
	} else {
		strlcpy(bf->f_name, basl_template, MAXPATHLEN);
		bf->fd = mkstemp(bf->f_name);
	}

	if (bf->fd > 0) {
		bf->fp = fdopen(bf->fd, "w+");
		if (bf->fp == NULL) {
			unlink(bf->f_name);
			close(bf->fd);
		}
	} else {
		err = 1;
	}

	return (err);
}

static void
basl_close(struct basl_fio *bf)
{

	if (!basl_keep_temps)
		unlink(bf->f_name);
	fclose(bf->fp);
}

static int
basl_start(struct basl_fio *in, struct basl_fio *out)
{
	int err;

	err = basl_open(in, 0);
	if (!err) {
		err = basl_open(out, 1);
		if (err) {
			basl_close(in);
		}
	}

	return (err);
}

static void
basl_end(struct basl_fio *in, struct basl_fio *out)
{

	basl_close(in);
	basl_close(out);
}

static int
basl_load(struct vmctx *ctx, int fd)
{
	struct stat sb;
	void *addr;

	if (fstat(fd, &sb) < 0)
		return (errno);

	addr = calloc(1, sb.st_size);
	if (addr == NULL)
		return (EFAULT);

	if (read(fd, addr, sb.st_size) < 0)
		return (errno);

	struct basl_table *table;

	uint8_t name[ACPI_NAMESEG_SIZE + 1] = { 0 };
	memcpy(name, addr, sizeof(name) - 1 /* last char is '\0' */);
	BASL_EXEC(basl_table_create(&table, ctx, name, BASL_TABLE_ALIGNMENT));
	BASL_EXEC(basl_table_append_bytes(table, addr, sb.st_size));

	return (0);
}

static int
basl_compile(struct vmctx *ctx, int (*fwrite_section)(FILE *))
{
	struct basl_fio io[2];
	static char iaslbuf[3*MAXPATHLEN + 10];
	const char *fmt;
	int err;

	err = basl_start(&io[0], &io[1]);
	if (!err) {
		err = (*fwrite_section)(io[0].fp);

		if (!err) {
			/*
			 * iasl sends the results of the compilation to
			 * stdout. Shut this down by using the shell to
			 * redirect stdout to /dev/null, unless the user
			 * has requested verbose output for debugging
			 * purposes
			 */
			fmt = basl_verbose_iasl ?
				"%s -p %s %s" :
				"/bin/sh -c \"%s -p %s %s\" 1> /dev/null";

			snprintf(iaslbuf, sizeof(iaslbuf),
				 fmt,
				 BHYVE_ASL_COMPILER,
				 io[1].f_name, io[0].f_name);
			err = system(iaslbuf);

			if (!err) {
				/*
				 * Copy the aml output file into guest
				 * memory at the specified location
				 */
				err = basl_load(ctx, io[1].fd);
			}
		}
		basl_end(&io[0], &io[1]);
	}

	return (err);
}

static int
basl_make_templates(void)
{
	const char *tmpdir;
	int err;
	int len;

	err = 0;

	/*
	 *
	 */
	if ((tmpdir = getenv("BHYVE_TMPDIR")) == NULL || *tmpdir == '\0' ||
	    (tmpdir = getenv("TMPDIR")) == NULL || *tmpdir == '\0') {
		tmpdir = _PATH_TMP;
	}

	len = strlen(tmpdir);

	if ((len + sizeof(BHYVE_ASL_TEMPLATE) + 1) < MAXPATHLEN) {
		strcpy(basl_template, tmpdir);
		while (len > 0 && basl_template[len - 1] == '/')
			len--;
		basl_template[len] = '/';
		strcpy(&basl_template[len + 1], BHYVE_ASL_TEMPLATE);
	} else
		err = E2BIG;

	if (!err) {
		/*
		 * len has been initialized (and maybe adjusted) above
		 */
		if ((len + sizeof(BHYVE_ASL_TEMPLATE) + 1 +
		     sizeof(BHYVE_ASL_SUFFIX)) < MAXPATHLEN) {
			strcpy(basl_stemplate, tmpdir);
			basl_stemplate[len] = '/';
			strcpy(&basl_stemplate[len + 1], BHYVE_ASL_TEMPLATE);
			len = strlen(basl_stemplate);
			strcpy(&basl_stemplate[len], BHYVE_ASL_SUFFIX);
		} else
			err = E2BIG;
	}

	return (err);
}

static int
build_dsdt(struct vmctx *const ctx)
{
	BASL_EXEC(basl_compile(ctx, basl_fwrite_dsdt));

	return (0);
}

static int
build_facs(struct vmctx *const ctx)
{
	ACPI_TABLE_FACS facs;
	struct basl_table *table;

	BASL_EXEC(basl_table_create(&table, ctx, ACPI_SIG_FACS,
	    BASL_TABLE_ALIGNMENT_FACS));

	memset(&facs, 0, sizeof(facs));
	memcpy(facs.Signature, ACPI_SIG_FACS, ACPI_NAMESEG_SIZE);
	facs.Length = sizeof(facs);
	facs.Version = htole32(2);
	BASL_EXEC(basl_table_append_bytes(table, &facs, sizeof(facs)));

	return (0);
}

static int
build_fadt(struct vmctx *const ctx)
{
	ACPI_TABLE_FADT fadt;
	struct basl_table *table;

	BASL_EXEC(basl_table_create(&table, ctx, ACPI_SIG_FADT,
	    BASL_TABLE_ALIGNMENT));

	memset(&fadt, 0, sizeof(fadt));
	BASL_EXEC(basl_table_append_header(table, ACPI_SIG_FADT, 5, 1));
	fadt.Facs = htole32(0); /* patched by basl */
	fadt.Dsdt = htole32(0); /* patched by basl */
	fadt.SciInterrupt = htole16(SCI_INT);
	fadt.SmiCommand = htole32(SMI_CMD);
	fadt.AcpiEnable = BHYVE_ACPI_ENABLE;
	fadt.AcpiDisable = BHYVE_ACPI_DISABLE;
	fadt.Pm1aEventBlock = htole32(PM1A_EVT_ADDR);
	fadt.Pm1aControlBlock = htole32(PM1A_CNT_ADDR);
	fadt.PmTimerBlock = htole32(IO_PMTMR);
	fadt.Gpe0Block = htole32(IO_GPE0_BLK);
	fadt.Pm1EventLength = 4;
	fadt.Pm1ControlLength = 2;
	fadt.PmTimerLength = 4;
	fadt.Gpe0BlockLength = IO_GPE0_LEN;
	fadt.Century = 0x32;
	fadt.BootFlags = htole16(ACPI_FADT_NO_VGA | ACPI_FADT_NO_ASPM);
	fadt.Flags = htole32(ACPI_FADT_WBINVD | ACPI_FADT_C1_SUPPORTED |
	    ACPI_FADT_SLEEP_BUTTON | ACPI_FADT_32BIT_TIMER |
	    ACPI_FADT_RESET_REGISTER | ACPI_FADT_HEADLESS |
	    ACPI_FADT_APIC_PHYSICAL);
	basl_fill_gas(&fadt.ResetRegister, ACPI_ADR_SPACE_SYSTEM_IO, 8, 0,
	    ACPI_GAS_ACCESS_WIDTH_BYTE, 0xCF9);
	fadt.ResetValue = 6;
	fadt.MinorRevision = 1;
	fadt.XFacs = htole64(0); /* patched by basl */
	fadt.XDsdt = htole64(0); /* patched by basl */
	basl_fill_gas(&fadt.XPm1aEventBlock, ACPI_ADR_SPACE_SYSTEM_IO, 0x20, 0,
	    ACPI_GAS_ACCESS_WIDTH_WORD, PM1A_EVT_ADDR);
	basl_fill_gas(&fadt.XPm1bEventBlock, ACPI_ADR_SPACE_SYSTEM_IO, 0, 0,
	    ACPI_GAS_ACCESS_WIDTH_UNDEFINED, 0);
	basl_fill_gas(&fadt.XPm1aControlBlock, ACPI_ADR_SPACE_SYSTEM_IO, 0x10,
	    0, ACPI_GAS_ACCESS_WIDTH_WORD, PM1A_CNT_ADDR);
	basl_fill_gas(&fadt.XPm1bControlBlock, ACPI_ADR_SPACE_SYSTEM_IO, 0, 0,
	    ACPI_GAS_ACCESS_WIDTH_UNDEFINED, 0);
	basl_fill_gas(&fadt.XPm2ControlBlock, ACPI_ADR_SPACE_SYSTEM_IO, 8, 0,
	    ACPI_GAS_ACCESS_WIDTH_UNDEFINED, 0);
	basl_fill_gas(&fadt.XPmTimerBlock, ACPI_ADR_SPACE_SYSTEM_IO, 0x20, 0,
	    ACPI_GAS_ACCESS_WIDTH_DWORD, IO_PMTMR);
	basl_fill_gas(&fadt.XGpe0Block, ACPI_ADR_SPACE_SYSTEM_IO,
	    IO_GPE0_LEN * 8, 0, ACPI_GAS_ACCESS_WIDTH_BYTE, IO_GPE0_BLK);
	basl_fill_gas(&fadt.XGpe1Block, ACPI_ADR_SPACE_SYSTEM_IO, 0, 0,
	    ACPI_GAS_ACCESS_WIDTH_UNDEFINED, 0);
	basl_fill_gas(&fadt.SleepControl, ACPI_ADR_SPACE_SYSTEM_IO, 8, 0,
	    ACPI_GAS_ACCESS_WIDTH_BYTE, 0);
	basl_fill_gas(&fadt.SleepStatus, ACPI_ADR_SPACE_SYSTEM_IO, 8, 0,
	    ACPI_GAS_ACCESS_WIDTH_BYTE, 0);
	BASL_EXEC(basl_table_append_content(table, &fadt, sizeof(fadt)));

	BASL_EXEC(basl_table_add_pointer(table, ACPI_SIG_FACS,
	    offsetof(ACPI_TABLE_FADT, Facs), sizeof(fadt.Facs)));
	BASL_EXEC(basl_table_add_pointer(table, ACPI_SIG_DSDT,
	    offsetof(ACPI_TABLE_FADT, Dsdt), sizeof(fadt.Dsdt)));
	BASL_EXEC(basl_table_add_pointer(table, ACPI_SIG_FACS,
	    offsetof(ACPI_TABLE_FADT, XFacs), sizeof(fadt.XFacs)));
	BASL_EXEC(basl_table_add_pointer(table, ACPI_SIG_DSDT,
	    offsetof(ACPI_TABLE_FADT, XDsdt), sizeof(fadt.XDsdt)));

	BASL_EXEC(basl_table_append_pointer(rsdt, ACPI_SIG_FADT,
	    ACPI_RSDT_ENTRY_SIZE));
	BASL_EXEC(basl_table_append_pointer(xsdt, ACPI_SIG_FADT,
	    ACPI_XSDT_ENTRY_SIZE));

	return (0);
}

static int
build_hpet(struct vmctx *const ctx)
{
	ACPI_TABLE_HPET hpet;
	struct basl_table *table;

	BASL_EXEC(basl_table_create(&table, ctx, ACPI_SIG_HPET,
	    BASL_TABLE_ALIGNMENT));

	memset(&hpet, 0, sizeof(hpet));
	BASL_EXEC(basl_table_append_header(table, ACPI_SIG_HPET, 1, 1));
	hpet.Id = htole32(hpet_capabilities);
	basl_fill_gas(&hpet.Address, ACPI_ADR_SPACE_SYSTEM_MEMORY, 0, 0,
	    ACPI_GAS_ACCESS_WIDTH_LEGACY, BHYVE_ADDRESS_HPET);
	hpet.Flags = ACPI_HPET_PAGE_PROTECT4;
	BASL_EXEC(basl_table_append_content(table, &hpet, sizeof(hpet)));

	BASL_EXEC(basl_table_append_pointer(rsdt, ACPI_SIG_HPET,
	    ACPI_RSDT_ENTRY_SIZE));
	BASL_EXEC(basl_table_append_pointer(xsdt, ACPI_SIG_HPET,
	    ACPI_XSDT_ENTRY_SIZE));

	return (0);
}

static int
build_madt(struct vmctx *const ctx)
{
	ACPI_TABLE_MADT madt;
	ACPI_MADT_LOCAL_APIC madt_lapic;
	ACPI_MADT_IO_APIC madt_ioapic;
	ACPI_MADT_INTERRUPT_OVERRIDE madt_irq_override;
	ACPI_MADT_LOCAL_APIC_NMI madt_lapic_nmi;
	struct basl_table *table;

	BASL_EXEC(basl_table_create(&table, ctx, ACPI_SIG_MADT,
	    BASL_TABLE_ALIGNMENT));

	memset(&madt, 0, sizeof(madt));
	BASL_EXEC(basl_table_append_header(table, ACPI_SIG_MADT, 1, 1));
	madt.Address = htole32(BHYVE_ADDRESS_LAPIC);
	madt.Flags = htole32(ACPI_MADT_PCAT_COMPAT);
	BASL_EXEC(basl_table_append_content(table, &madt, sizeof(madt)));

	/* Local APIC for each CPU */
	for (int i = 0; i < basl_ncpu; ++i) {
		memset(&madt_lapic, 0, sizeof(madt_lapic));
		madt_lapic.Header.Type = ACPI_MADT_TYPE_LOCAL_APIC;
		madt_lapic.Header.Length = sizeof(madt_lapic);
		madt_lapic.ProcessorId = i;
		madt_lapic.Id = i;
		madt_lapic.LapicFlags = htole32(ACPI_MADT_ENABLED);
		BASL_EXEC(basl_table_append_bytes(table, &madt_lapic,
		    sizeof(madt_lapic)));
	}

	/* I/O APIC */
	memset(&madt_ioapic, 0, sizeof(madt_ioapic));
	madt_ioapic.Header.Type = ACPI_MADT_TYPE_IO_APIC;
	madt_ioapic.Header.Length = sizeof(madt_ioapic);
	madt_ioapic.Address = htole32(BHYVE_ADDRESS_IOAPIC);
	BASL_EXEC(
	    basl_table_append_bytes(table, &madt_ioapic, sizeof(madt_ioapic)));

	/* Legacy IRQ0 is connected to pin 2 of the I/O APIC */
	memset(&madt_irq_override, 0, sizeof(madt_irq_override));
	madt_irq_override.Header.Type = ACPI_MADT_TYPE_INTERRUPT_OVERRIDE;
	madt_irq_override.Header.Length = sizeof(madt_irq_override);
	madt_irq_override.GlobalIrq = htole32(2);
	madt_irq_override.IntiFlags = htole16(
	    ACPI_MADT_POLARITY_ACTIVE_HIGH | ACPI_MADT_TRIGGER_EDGE);
	BASL_EXEC(basl_table_append_bytes(table, &madt_irq_override,
	    sizeof(madt_irq_override)));

	memset(&madt_irq_override, 0, sizeof(madt_irq_override));
	madt_irq_override.Header.Type = ACPI_MADT_TYPE_INTERRUPT_OVERRIDE;
	madt_irq_override.Header.Length = sizeof(madt_irq_override);
	madt_irq_override.SourceIrq = SCI_INT;
	madt_irq_override.GlobalIrq = htole32(SCI_INT);
	madt_irq_override.IntiFlags = htole16(
	    ACPI_MADT_POLARITY_ACTIVE_LOW | ACPI_MADT_TRIGGER_LEVEL);
	BASL_EXEC(basl_table_append_bytes(table, &madt_irq_override,
	    sizeof(madt_irq_override)));

	/* Local APIC NMI is conntected to LINT 1 on all CPUs */
	memset(&madt_lapic_nmi, 0, sizeof(madt_lapic_nmi));
	madt_lapic_nmi.Header.Type = ACPI_MADT_TYPE_LOCAL_APIC_NMI;
	madt_lapic_nmi.Header.Length = sizeof(madt_lapic_nmi);
	madt_lapic_nmi.ProcessorId = 0xFF;
	madt_lapic_nmi.IntiFlags = htole16(
	    ACPI_MADT_POLARITY_ACTIVE_HIGH | ACPI_MADT_TRIGGER_EDGE);
	madt_lapic_nmi.Lint = 1;
	BASL_EXEC(basl_table_append_bytes(table, &madt_lapic_nmi,
	    sizeof(madt_lapic_nmi)));

	BASL_EXEC(basl_table_append_pointer(rsdt, ACPI_SIG_MADT,
	    ACPI_RSDT_ENTRY_SIZE));
	BASL_EXEC(basl_table_append_pointer(xsdt, ACPI_SIG_MADT,
	    ACPI_XSDT_ENTRY_SIZE));

	return (0);
}

static int
build_mcfg(struct vmctx *const ctx)
{
	ACPI_TABLE_MCFG mcfg;
	ACPI_MCFG_ALLOCATION mcfg_allocation;
	struct basl_table *table;

	BASL_EXEC(basl_table_create(&table, ctx, ACPI_SIG_MCFG,
	    BASL_TABLE_ALIGNMENT));

	memset(&mcfg, 0, sizeof(mcfg));
	BASL_EXEC(basl_table_append_header(table, ACPI_SIG_MCFG, 1, 1));
	BASL_EXEC(basl_table_append_content(table, &mcfg, sizeof(mcfg)));

	memset(&mcfg_allocation, 0, sizeof(mcfg_allocation));
	mcfg_allocation.Address = htole64(pci_ecfg_base());
	mcfg_allocation.EndBusNumber = 0xFF;
	BASL_EXEC(basl_table_append_bytes(table, &mcfg_allocation,
	    sizeof(mcfg_allocation)));

	BASL_EXEC(basl_table_append_pointer(rsdt, ACPI_SIG_MCFG,
	    ACPI_RSDT_ENTRY_SIZE));
	BASL_EXEC(basl_table_append_pointer(xsdt, ACPI_SIG_MCFG,
	    ACPI_XSDT_ENTRY_SIZE));

	return (0);
}

static int
build_rsdp(struct vmctx *const ctx)
{
	ACPI_TABLE_RSDP rsdp;
	struct basl_table *table;

	BASL_EXEC(basl_table_create(&table, ctx, ACPI_RSDP_NAME,
	    BASL_TABLE_ALIGNMENT));

	memset(&rsdp, 0, sizeof(rsdp));
	memcpy(rsdp.Signature, ACPI_SIG_RSDP, 8);
	rsdp.Checksum = 0; /* patched by basl */
	memcpy(rsdp.OemId, "BHYVE ", ACPI_OEM_ID_SIZE);
	rsdp.Revision = 2;
	rsdp.RsdtPhysicalAddress = htole32(0); /* patched by basl */
	rsdp.Length = htole32(0);	       /* patched by basl */
	rsdp.XsdtPhysicalAddress = htole64(0); /* patched by basl */
	rsdp.ExtendedChecksum = 0;	       /* patched by basl */
	BASL_EXEC(basl_table_append_bytes(table, &rsdp, sizeof(rsdp)));

	BASL_EXEC(basl_table_add_checksum(table,
	    offsetof(ACPI_TABLE_RSDP, Checksum), 0, 20));
	BASL_EXEC(basl_table_add_pointer(table, ACPI_SIG_RSDT,
	    offsetof(ACPI_TABLE_RSDP, RsdtPhysicalAddress),
	    sizeof(rsdp.RsdtPhysicalAddress)));
	BASL_EXEC(basl_table_add_length(table,
	    offsetof(ACPI_TABLE_RSDP, Length), sizeof(rsdp.Length)));
	BASL_EXEC(basl_table_add_pointer(table, ACPI_SIG_XSDT,
	    offsetof(ACPI_TABLE_RSDP, XsdtPhysicalAddress),
	    sizeof(rsdp.XsdtPhysicalAddress)));
	BASL_EXEC(basl_table_add_checksum(table,
	    offsetof(ACPI_TABLE_RSDP, ExtendedChecksum), 0,
	    BASL_TABLE_CHECKSUM_LEN_FULL_TABLE));

	return (0);
}

static int
build_rsdt(struct vmctx *const ctx)
{
	BASL_EXEC(
	    basl_table_create(&rsdt, ctx, ACPI_SIG_RSDT, BASL_TABLE_ALIGNMENT));

	/* Header */
	BASL_EXEC(basl_table_append_header(rsdt, ACPI_SIG_RSDT, 1, 1));
	/* Pointers (added by other build_XXX funcs) */

	return (0);
}

static int
build_spcr(struct vmctx *const ctx)
{
	ACPI_TABLE_SPCR spcr;
	struct basl_table *table;

	BASL_EXEC(basl_table_create(&table, ctx, ACPI_SIG_SPCR,
	    BASL_TABLE_ALIGNMENT));

	memset(&spcr, 0, sizeof(spcr));
	BASL_EXEC(basl_table_append_header(table, ACPI_SIG_SPCR, 1, 1));
	spcr.InterfaceType = ACPI_DBG2_16550_COMPATIBLE;
	basl_fill_gas(&spcr.SerialPort, ACPI_ADR_SPACE_SYSTEM_IO, 8, 0,
	    ACPI_GAS_ACCESS_WIDTH_LEGACY, 0x3F8);
	spcr.InterruptType = ACPI_SPCR_INTERRUPT_TYPE_8259;
	spcr.PcInterrupt = 4;
	spcr.BaudRate = ACPI_SPCR_BAUD_RATE_115200;
	spcr.Parity = ACPI_SPCR_PARITY_NO_PARITY;
	spcr.StopBits = ACPI_SPCR_STOP_BITS_1;
	spcr.FlowControl = 3; /* RTS/CTS | DCD */
	spcr.TerminalType = ACPI_SPCR_TERMINAL_TYPE_VT_UTF8;
	BASL_EXEC(basl_table_append_content(table, &spcr, sizeof(spcr)));

	BASL_EXEC(basl_table_append_pointer(rsdt, ACPI_SIG_SPCR,
	    ACPI_RSDT_ENTRY_SIZE));
	BASL_EXEC(basl_table_append_pointer(xsdt, ACPI_SIG_SPCR,
	    ACPI_XSDT_ENTRY_SIZE));

	return (0);
}

static int
build_xsdt(struct vmctx *const ctx)
{
	BASL_EXEC(
	    basl_table_create(&xsdt, ctx, ACPI_SIG_XSDT, BASL_TABLE_ALIGNMENT));

	/* Header */
	BASL_EXEC(basl_table_append_header(xsdt, ACPI_SIG_XSDT, 1, 1));
	/* Pointers (added by other build_XXX funcs) */

	return (0);
}

int
acpi_build(struct vmctx *ctx, int ncpu)
{
	int err;

	basl_ncpu = ncpu;

	err = vm_get_hpet_capabilities(ctx, &hpet_capabilities);
	if (err != 0)
		return (err);

	/*
	 * For debug, allow the user to have iasl compiler output sent
	 * to stdout rather than /dev/null
	 */
	if (getenv("BHYVE_ACPI_VERBOSE_IASL"))
		basl_verbose_iasl = 1;

	/*
	 * Allow the user to keep the generated ASL files for debugging
	 * instead of deleting them following use
	 */
	if (getenv("BHYVE_ACPI_KEEPTMPS"))
		basl_keep_temps = 1;

	BASL_EXEC(basl_init());

	BASL_EXEC(basl_make_templates());

	/*
	 * Generate ACPI tables and copy them into guest memory.
	 *
	 * According to UEFI Specification v6.3 chapter 5.1 the FADT should be
	 * the first table pointed to by XSDT. For that reason, build it as the
	 * first table after XSDT.
	 */
	BASL_EXEC(build_rsdp(ctx));
	BASL_EXEC(build_rsdt(ctx));
	BASL_EXEC(build_xsdt(ctx));
	BASL_EXEC(build_fadt(ctx));
	BASL_EXEC(build_madt(ctx));
	BASL_EXEC(build_hpet(ctx));
	BASL_EXEC(build_mcfg(ctx));
	BASL_EXEC(build_facs(ctx));
	BASL_EXEC(build_spcr(ctx));

	/* Build ACPI device-specific tables such as a TPM2 table. */
	const struct acpi_device_list_entry *entry;
	SLIST_FOREACH(entry, &acpi_devices, chain) {
		BASL_EXEC(acpi_device_build_table(entry->dev));
	}

	BASL_EXEC(build_dsdt(ctx));

	BASL_EXEC(basl_finish());

	return (0);
}
