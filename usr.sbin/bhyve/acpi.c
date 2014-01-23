/*-
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
 * hopefully other o/s's) by writing out ASL template files for each of
 * the tables and the compiling them to AML with the Intel iasl compiler.
 * The AML files are then read into guest memory.
 *
 *  The tables are placed in the guest's ROM area just below 1MB physical,
 * above the MPTable.
 *
 *  Layout
 *  ------
 *   RSDP  ->   0xf0400    (36 bytes fixed)
 *     RSDT  ->   0xf0440    (36 bytes + 4*N table addrs, 2 used)
 *     XSDT  ->   0xf0480    (36 bytes + 8*N table addrs, 2 used)
 *       MADT  ->   0xf0500  (depends on #CPUs)
 *       FADT  ->   0xf0600  (268 bytes)
 *       HPET  ->   0xf0740  (56 bytes)
 *         FACS  ->   0xf0780 (64 bytes)
 *         DSDT  ->   0xf0800 (variable - can go up to 0x100000)
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <machine/vmm.h>
#include <vmmapi.h>

#include "bhyverun.h"
#include "acpi.h"

/*
 * Define the base address of the ACPI tables, and the offsets to
 * the individual tables
 */
#define BHYVE_ACPI_BASE		0xf0400
#define RSDT_OFFSET		0x040
#define XSDT_OFFSET		0x080
#define MADT_OFFSET		0x100
#define FADT_OFFSET		0x200
#define	HPET_OFFSET		0x340
#define FACS_OFFSET		0x380
#define DSDT_OFFSET		0x400

#define	BHYVE_ASL_TEMPLATE	"bhyve.XXXXXXX"
#define BHYVE_ASL_SUFFIX	".aml"
#define BHYVE_ASL_COMPILER	"/usr/sbin/iasl"

#define BHYVE_PM_TIMER_ADDR	0x408

static int basl_keep_temps;
static int basl_verbose_iasl;
static int basl_ncpu;
static uint32_t basl_acpi_base = BHYVE_ACPI_BASE;
static uint32_t hpet_capabilities;

/*
 * Contains the full pathname of the template to be passed
 * to mkstemp/mktemps(3)
 */
static char basl_template[MAXPATHLEN];
static char basl_stemplate[MAXPATHLEN];

struct basl_fio {
	int	fd;
	FILE	*fp;
	char	f_name[MAXPATHLEN];
};

#define EFPRINTF(...) \
	err = fprintf(__VA_ARGS__); if (err < 0) goto err_exit;

#define EFFLUSH(x) \
	err = fflush(x); if (err != 0) goto err_exit;

static int
basl_fwrite_rsdp(FILE *fp)
{
	int err;

	err = 0;

	EFPRINTF(fp, "/*\n");
	EFPRINTF(fp, " * bhyve RSDP template\n");
	EFPRINTF(fp, " */\n");
	EFPRINTF(fp, "[0008]\t\tSignature : \"RSD PTR \"\n");
	EFPRINTF(fp, "[0001]\t\tChecksum : 43\n");
	EFPRINTF(fp, "[0006]\t\tOem ID : \"BHYVE \"\n");
	EFPRINTF(fp, "[0001]\t\tRevision : 02\n");
	EFPRINTF(fp, "[0004]\t\tRSDT Address : %08X\n",
	    basl_acpi_base + RSDT_OFFSET);
	EFPRINTF(fp, "[0004]\t\tLength : 00000024\n");
	EFPRINTF(fp, "[0008]\t\tXSDT Address : 00000000%08X\n",
	    basl_acpi_base + XSDT_OFFSET);
	EFPRINTF(fp, "[0001]\t\tExtended Checksum : 00\n");
	EFPRINTF(fp, "[0003]\t\tReserved : 000000\n");

	EFFLUSH(fp);

	return (0);

err_exit:
	return (errno);
}

static int
basl_fwrite_rsdt(FILE *fp)
{
	int err;

	err = 0;

	EFPRINTF(fp, "/*\n");
	EFPRINTF(fp, " * bhyve RSDT template\n");
	EFPRINTF(fp, " */\n");
	EFPRINTF(fp, "[0004]\t\tSignature : \"RSDT\"\n");
	EFPRINTF(fp, "[0004]\t\tTable Length : 00000000\n");
	EFPRINTF(fp, "[0001]\t\tRevision : 01\n");
	EFPRINTF(fp, "[0001]\t\tChecksum : 00\n");
	EFPRINTF(fp, "[0006]\t\tOem ID : \"BHYVE \"\n");
	EFPRINTF(fp, "[0008]\t\tOem Table ID : \"BVRSDT  \"\n");
	EFPRINTF(fp, "[0004]\t\tOem Revision : 00000001\n");
	/* iasl will fill in the compiler ID/revision fields */
	EFPRINTF(fp, "[0004]\t\tAsl Compiler ID : \"xxxx\"\n");
	EFPRINTF(fp, "[0004]\t\tAsl Compiler Revision : 00000000\n");
	EFPRINTF(fp, "\n");

	/* Add in pointers to the MADT, FADT and HPET */
	EFPRINTF(fp, "[0004]\t\tACPI Table Address 0 : %08X\n",
	    basl_acpi_base + MADT_OFFSET);
	EFPRINTF(fp, "[0004]\t\tACPI Table Address 1 : %08X\n",
	    basl_acpi_base + FADT_OFFSET);
	EFPRINTF(fp, "[0004]\t\tACPI Table Address 2 : %08X\n",
	    basl_acpi_base + HPET_OFFSET);

	EFFLUSH(fp);

	return (0);

err_exit:
	return (errno);
}

static int
basl_fwrite_xsdt(FILE *fp)
{
	int err;

	err = 0;

	EFPRINTF(fp, "/*\n");
	EFPRINTF(fp, " * bhyve XSDT template\n");
	EFPRINTF(fp, " */\n");
	EFPRINTF(fp, "[0004]\t\tSignature : \"XSDT\"\n");
	EFPRINTF(fp, "[0004]\t\tTable Length : 00000000\n");
	EFPRINTF(fp, "[0001]\t\tRevision : 01\n");
	EFPRINTF(fp, "[0001]\t\tChecksum : 00\n");
	EFPRINTF(fp, "[0006]\t\tOem ID : \"BHYVE \"\n");
	EFPRINTF(fp, "[0008]\t\tOem Table ID : \"BVXSDT  \"\n");
	EFPRINTF(fp, "[0004]\t\tOem Revision : 00000001\n");
	/* iasl will fill in the compiler ID/revision fields */
	EFPRINTF(fp, "[0004]\t\tAsl Compiler ID : \"xxxx\"\n");
	EFPRINTF(fp, "[0004]\t\tAsl Compiler Revision : 00000000\n");
	EFPRINTF(fp, "\n");

	/* Add in pointers to the MADT, FADT and HPET */
	EFPRINTF(fp, "[0004]\t\tACPI Table Address 0 : 00000000%08X\n",
	    basl_acpi_base + MADT_OFFSET);
	EFPRINTF(fp, "[0004]\t\tACPI Table Address 1 : 00000000%08X\n",
	    basl_acpi_base + FADT_OFFSET);
	EFPRINTF(fp, "[0004]\t\tACPI Table Address 2 : 00000000%08X\n",
	    basl_acpi_base + HPET_OFFSET);

	EFFLUSH(fp);

	return (0);

err_exit:
	return (errno);
}

static int
basl_fwrite_madt(FILE *fp)
{
	int err;
	int i;

	err = 0;

	EFPRINTF(fp, "/*\n");
	EFPRINTF(fp, " * bhyve MADT template\n");
	EFPRINTF(fp, " */\n");
	EFPRINTF(fp, "[0004]\t\tSignature : \"APIC\"\n");
	EFPRINTF(fp, "[0004]\t\tTable Length : 00000000\n");
	EFPRINTF(fp, "[0001]\t\tRevision : 01\n");
	EFPRINTF(fp, "[0001]\t\tChecksum : 00\n");
	EFPRINTF(fp, "[0006]\t\tOem ID : \"BHYVE \"\n");
	EFPRINTF(fp, "[0008]\t\tOem Table ID : \"BVMADT  \"\n");
	EFPRINTF(fp, "[0004]\t\tOem Revision : 00000001\n");

	/* iasl will fill in the compiler ID/revision fields */
	EFPRINTF(fp, "[0004]\t\tAsl Compiler ID : \"xxxx\"\n");
	EFPRINTF(fp, "[0004]\t\tAsl Compiler Revision : 00000000\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp, "[0004]\t\tLocal Apic Address : FEE00000\n");
	EFPRINTF(fp, "[0004]\t\tFlags (decoded below) : 00000001\n");
	EFPRINTF(fp, "\t\t\tPC-AT Compatibility : 1\n");
	EFPRINTF(fp, "\n");

	/* Add a Processor Local APIC entry for each CPU */
	for (i = 0; i < basl_ncpu; i++) {
		EFPRINTF(fp, "[0001]\t\tSubtable Type : 00\n");
		EFPRINTF(fp, "[0001]\t\tLength : 08\n");
		/* iasl expects hex values for the proc and apic id's */
		EFPRINTF(fp, "[0001]\t\tProcessor ID : %02x\n", i);
		EFPRINTF(fp, "[0001]\t\tLocal Apic ID : %02x\n", i);
		EFPRINTF(fp, "[0004]\t\tFlags (decoded below) : 00000001\n");
		EFPRINTF(fp, "\t\t\tProcessor Enabled : 1\n");
		EFPRINTF(fp, "\n");
	}

	/* Always a single IOAPIC entry, with ID 0 */
	EFPRINTF(fp, "[0001]\t\tSubtable Type : 01\n");
	EFPRINTF(fp, "[0001]\t\tLength : 0C\n");
	/* iasl expects a hex value for the i/o apic id */
	EFPRINTF(fp, "[0001]\t\tI/O Apic ID : %02x\n", 0);
	EFPRINTF(fp, "[0001]\t\tReserved : 00\n");
	EFPRINTF(fp, "[0004]\t\tAddress : fec00000\n");
	EFPRINTF(fp, "[0004]\t\tInterrupt : 00000000\n");
	EFPRINTF(fp, "\n");

	/* Legacy IRQ0 is connected to pin 2 of the IOAPIC */
	EFPRINTF(fp, "[0001]\t\tSubtable Type : 02\n");
	EFPRINTF(fp, "[0001]\t\tLength : 0A\n");
	EFPRINTF(fp, "[0001]\t\tBus : 00\n");
	EFPRINTF(fp, "[0001]\t\tSource : 00\n");
	EFPRINTF(fp, "[0004]\t\tInterrupt : 00000002\n");
	EFPRINTF(fp, "[0002]\t\tFlags (decoded below) : 0005\n");
	EFPRINTF(fp, "\t\t\tPolarity : 1\n");
	EFPRINTF(fp, "\t\t\tTrigger Mode : 1\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp, "[0001]\t\tSubtable Type : 02\n");
	EFPRINTF(fp, "[0001]\t\tLength : 0A\n");
	EFPRINTF(fp, "[0001]\t\tBus : 00\n");
	EFPRINTF(fp, "[0001]\t\tSource : 09\n");
	EFPRINTF(fp, "[0004]\t\tInterrupt : 00000009\n");
	EFPRINTF(fp, "[0002]\t\tFlags (decoded below) : 0000\n");
	EFPRINTF(fp, "\t\t\tPolarity : 0\n");
	EFPRINTF(fp, "\t\t\tTrigger Mode : 0\n");
	EFPRINTF(fp, "\n");

	EFFLUSH(fp);

	return (0);

err_exit:
	return (errno);
}

static int
basl_fwrite_fadt(FILE *fp)
{
	int err;

	err = 0;

	EFPRINTF(fp, "/*\n");
	EFPRINTF(fp, " * bhyve FADT template\n");
	EFPRINTF(fp, " */\n");
	EFPRINTF(fp, "[0004]\t\tSignature : \"FACP\"\n");
	EFPRINTF(fp, "[0004]\t\tTable Length : 0000010C\n");
	EFPRINTF(fp, "[0001]\t\tRevision : 05\n");
	EFPRINTF(fp, "[0001]\t\tChecksum : 00\n");
	EFPRINTF(fp, "[0006]\t\tOem ID : \"BHYVE \"\n");
	EFPRINTF(fp, "[0008]\t\tOem Table ID : \"BVFACP  \"\n");
	EFPRINTF(fp, "[0004]\t\tOem Revision : 00000001\n");
	/* iasl will fill in the compiler ID/revision fields */
	EFPRINTF(fp, "[0004]\t\tAsl Compiler ID : \"xxxx\"\n");
	EFPRINTF(fp, "[0004]\t\tAsl Compiler Revision : 00000000\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp, "[0004]\t\tFACS Address : %08X\n",
	    basl_acpi_base + FACS_OFFSET);
	EFPRINTF(fp, "[0004]\t\tDSDT Address : %08X\n",
	    basl_acpi_base + DSDT_OFFSET);
	EFPRINTF(fp, "[0001]\t\tModel : 00\n");
	EFPRINTF(fp, "[0001]\t\tPM Profile : 00 [Unspecified]\n");
	EFPRINTF(fp, "[0002]\t\tSCI Interrupt : 0009\n");
	EFPRINTF(fp, "[0004]\t\tSMI Command Port : 00000000\n");
	EFPRINTF(fp, "[0001]\t\tACPI Enable Value : 00\n");
	EFPRINTF(fp, "[0001]\t\tACPI Disable Value : 00\n");
	EFPRINTF(fp, "[0001]\t\tS4BIOS Command : 00\n");
	EFPRINTF(fp, "[0001]\t\tP-State Control : 00\n");
	EFPRINTF(fp, "[0004]\t\tPM1A Event Block Address : 00000000\n");
	EFPRINTF(fp, "[0004]\t\tPM1B Event Block Address : 00000000\n");
	EFPRINTF(fp, "[0004]\t\tPM1A Control Block Address : 00000000\n");
	EFPRINTF(fp, "[0004]\t\tPM1B Control Block Address : 00000000\n");
	EFPRINTF(fp, "[0004]\t\tPM2 Control Block Address : 00000000\n");
	EFPRINTF(fp, "[0004]\t\tPM Timer Block Address : %08X\n",
		 BHYVE_PM_TIMER_ADDR);
	EFPRINTF(fp, "[0004]\t\tGPE0 Block Address : 00000000\n");
	EFPRINTF(fp, "[0004]\t\tGPE1 Block Address : 00000000\n");
	EFPRINTF(fp, "[0001]\t\tPM1 Event Block Length : 04\n");
	EFPRINTF(fp, "[0001]\t\tPM1 Control Block Length : 02\n");
	EFPRINTF(fp, "[0001]\t\tPM2 Control Block Length : 00\n");
	EFPRINTF(fp, "[0001]\t\tPM Timer Block Length : 04\n");
	EFPRINTF(fp, "[0001]\t\tGPE0 Block Length : 00\n");
	EFPRINTF(fp, "[0001]\t\tGPE1 Block Length : 00\n");
	EFPRINTF(fp, "[0001]\t\tGPE1 Base Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\t_CST Support : 00\n");
	EFPRINTF(fp, "[0002]\t\tC2 Latency : 0000\n");
	EFPRINTF(fp, "[0002]\t\tC3 Latency : 0000\n");
	EFPRINTF(fp, "[0002]\t\tCPU Cache Size : 0000\n");
	EFPRINTF(fp, "[0002]\t\tCache Flush Stride : 0000\n");
	EFPRINTF(fp, "[0001]\t\tDuty Cycle Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\tDuty Cycle Width : 00\n");
	EFPRINTF(fp, "[0001]\t\tRTC Day Alarm Index : 00\n");
	EFPRINTF(fp, "[0001]\t\tRTC Month Alarm Index : 00\n");
	EFPRINTF(fp, "[0001]\t\tRTC Century Index : 00\n");
	EFPRINTF(fp, "[0002]\t\tBoot Flags (decoded below) : 0000\n");
	EFPRINTF(fp, "\t\t\tLegacy Devices Supported (V2) : 0\n");
	EFPRINTF(fp, "\t\t\t8042 Present on ports 60/64 (V2) : 0\n");
	EFPRINTF(fp, "\t\t\tVGA Not Present (V4) : 1\n");
	EFPRINTF(fp, "\t\t\tMSI Not Supported (V4) : 0\n");
	EFPRINTF(fp, "\t\t\tPCIe ASPM Not Supported (V4) : 1\n");
	EFPRINTF(fp, "\t\t\tCMOS RTC Not Present (V5) : 0\n");
	EFPRINTF(fp, "[0001]\t\tReserved : 00\n");
	EFPRINTF(fp, "[0004]\t\tFlags (decoded below) : 00000000\n");
	EFPRINTF(fp, "\t\t\tWBINVD instruction is operational (V1) : 1\n");
	EFPRINTF(fp, "\t\t\tWBINVD flushes all caches (V1) : 0\n");
	EFPRINTF(fp, "\t\t\tAll CPUs support C1 (V1) : 0\n");
	EFPRINTF(fp, "\t\t\tC2 works on MP system (V1) : 0\n");
	EFPRINTF(fp, "\t\t\tControl Method Power Button (V1) : 1\n");
	EFPRINTF(fp, "\t\t\tControl Method Sleep Button (V1) : 1\n");
	EFPRINTF(fp, "\t\t\tRTC wake not in fixed reg space (V1) : 0\n");
	EFPRINTF(fp, "\t\t\tRTC can wake system from S4 (V1) : 0\n");
	EFPRINTF(fp, "\t\t\t32-bit PM Timer (V1) : 1\n");
	EFPRINTF(fp, "\t\t\tDocking Supported (V1) : 0\n");
	EFPRINTF(fp, "\t\t\tReset Register Supported (V2) : 0\n");
	EFPRINTF(fp, "\t\t\tSealed Case (V3) : 0\n");
	EFPRINTF(fp, "\t\t\tHeadless - No Video (V3) : 1\n");
	EFPRINTF(fp, "\t\t\tUse native instr after SLP_TYPx (V3) : 0\n");
	EFPRINTF(fp, "\t\t\tPCIEXP_WAK Bits Supported (V4) : 0\n");
	EFPRINTF(fp, "\t\t\tUse Platform Timer (V4) : 0\n");
	EFPRINTF(fp, "\t\t\tRTC_STS valid on S4 wake (V4) : 0\n");
	EFPRINTF(fp, "\t\t\tRemote Power-on capable (V4) : 0\n");
	EFPRINTF(fp, "\t\t\tUse APIC Cluster Model (V4) : 0\n");
	EFPRINTF(fp, "\t\t\tUse APIC Physical Destination Mode (V4) : 1\n");
	EFPRINTF(fp, "\t\t\tHardware Reduced (V5) : 0\n");
	EFPRINTF(fp, "\t\t\tLow Power S0 Idle (V5) : 0\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp,
	    "[0012]\t\tReset Register : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 08\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\tEncoded Access Width : 01 [Byte Access:8]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000001\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp, "[0001]\t\tValue to cause reset : 00\n");
	EFPRINTF(fp, "[0003]\t\tReserved : 000000\n");
	EFPRINTF(fp, "[0008]\t\tFACS Address : 00000000%08X\n",
	    basl_acpi_base + FACS_OFFSET);
	EFPRINTF(fp, "[0008]\t\tDSDT Address : 00000000%08X\n",
	    basl_acpi_base + DSDT_OFFSET);
	EFPRINTF(fp,
	    "[0012]\t\tPM1A Event Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 20\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\tEncoded Access Width : 02 [Word Access:16]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000001\n");
	EFPRINTF(fp, "\n");
	
	EFPRINTF(fp,
	    "[0012]\t\tPM1B Event Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 00\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp,
	    "[0001]\t\tEncoded Access Width : 00 [Undefined/Legacy]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000000\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp,
	    "[0012]\t\tPM1A Control Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 10\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\tEncoded Access Width : 02 [Word Access:16]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000001\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp,
	    "[0012]\t\tPM1B Control Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 00\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp,
	    "[0001]\t\tEncoded Access Width : 00 [Undefined/Legacy]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000000\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp,
	    "[0012]\t\tPM2 Control Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 08\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp,
	    "[0001]\t\tEncoded Access Width : 00 [Undefined/Legacy]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000000\n");
	EFPRINTF(fp, "\n");

	/* Valid for bhyve */
	EFPRINTF(fp,
	    "[0012]\t\tPM Timer Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 32\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp,
	    "[0001]\t\tEncoded Access Width : 03 [DWord Access:32]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 00000000%08X\n",
	    BHYVE_PM_TIMER_ADDR);
	EFPRINTF(fp, "\n");

	EFPRINTF(fp, "[0012]\t\tGPE0 Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 80\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\tEncoded Access Width : 01 [Byte Access:8]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000000\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp, "[0012]\t\tGPE1 Block : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 00\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp,
	    "[0001]\t\tEncoded Access Width : 00 [Undefined/Legacy]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000000\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp,
	   "[0012]\t\tSleep Control Register : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 08\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\tEncoded Access Width : 01 [Byte Access:8]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000000\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp,
	    "[0012]\t\tSleep Status Register : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 01 [SystemIO]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 08\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp, "[0001]\t\tEncoded Access Width : 01 [Byte Access:8]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 0000000000000000\n");

	EFFLUSH(fp);

	return (0);

err_exit:
	return (errno);
}

static int
basl_fwrite_hpet(FILE *fp)
{
	int err;

	err = 0;

	EFPRINTF(fp, "/*\n");
	EFPRINTF(fp, " * bhyve HPET template\n");
	EFPRINTF(fp, " */\n");
	EFPRINTF(fp, "[0004]\t\tSignature : \"HPET\"\n");
	EFPRINTF(fp, "[0004]\t\tTable Length : 00000000\n");
	EFPRINTF(fp, "[0001]\t\tRevision : 01\n");
	EFPRINTF(fp, "[0001]\t\tChecksum : 00\n");
	EFPRINTF(fp, "[0006]\t\tOem ID : \"BHYVE \"\n");
	EFPRINTF(fp, "[0008]\t\tOem Table ID : \"BVHPET  \"\n");
	EFPRINTF(fp, "[0004]\t\tOem Revision : 00000001\n");

	/* iasl will fill in the compiler ID/revision fields */
	EFPRINTF(fp, "[0004]\t\tAsl Compiler ID : \"xxxx\"\n");
	EFPRINTF(fp, "[0004]\t\tAsl Compiler Revision : 00000000\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp, "[0004]\t\tTimer Block ID : %08X\n", hpet_capabilities);
	EFPRINTF(fp,
	    "[0012]\t\tTimer Block Register : [Generic Address Structure]\n");
	EFPRINTF(fp, "[0001]\t\tSpace ID : 00 [SystemMemory]\n");
	EFPRINTF(fp, "[0001]\t\tBit Width : 00\n");
	EFPRINTF(fp, "[0001]\t\tBit Offset : 00\n");
	EFPRINTF(fp,
		 "[0001]\t\tEncoded Access Width : 00 [Undefined/Legacy]\n");
	EFPRINTF(fp, "[0008]\t\tAddress : 00000000FED00000\n");
	EFPRINTF(fp, "\n");

	EFPRINTF(fp, "[0001]\t\tHPET Number : 00\n");
	EFPRINTF(fp, "[0002]\t\tMinimum Clock Ticks : 0000\n");
	EFPRINTF(fp, "[0004]\t\tFlags (decoded below) : 00000001\n");
	EFPRINTF(fp, "\t\t\t4K Page Protect : 1\n");
	EFPRINTF(fp, "\t\t\t64K Page Protect : 0\n");
	EFPRINTF(fp, "\n");

	EFFLUSH(fp);

	return (0);

err_exit:
	return (errno);
}

static int
basl_fwrite_facs(FILE *fp)
{
	int err;

	err = 0;

	EFPRINTF(fp, "/*\n");
	EFPRINTF(fp, " * bhyve FACS template\n");
	EFPRINTF(fp, " */\n");
	EFPRINTF(fp, "[0004]\t\tSignature : \"FACS\"\n");
	EFPRINTF(fp, "[0004]\t\tLength : 00000040\n");
	EFPRINTF(fp, "[0004]\t\tHardware Signature : 00000000\n");
	EFPRINTF(fp, "[0004]\t\t32 Firmware Waking Vector : 00000000\n");
	EFPRINTF(fp, "[0004]\t\tGlobal Lock : 00000000\n");
	EFPRINTF(fp, "[0004]\t\tFlags (decoded below) : 00000000\n");
	EFPRINTF(fp, "\t\t\tS4BIOS Support Present : 0\n");
	EFPRINTF(fp, "\t\t\t64-bit Wake Supported (V2) : 0\n");
	EFPRINTF(fp,
	    "[0008]\t\t64 Firmware Waking Vector : 0000000000000000\n");
	EFPRINTF(fp, "[0001]\t\tVersion : 02\n");
	EFPRINTF(fp, "[0003]\t\tReserved : 000000\n");
	EFPRINTF(fp, "[0004]\t\tOspmFlags (decoded below) : 00000000\n");
	EFPRINTF(fp, "\t\t\t64-bit Wake Env Required (V2) : 0\n");

	EFFLUSH(fp);

	return (0);
	
err_exit:
	return (errno);
}

static int
basl_fwrite_dsdt(FILE *fp)
{
	int err;

	err = 0;

	EFPRINTF(fp, "/*\n");
	EFPRINTF(fp, " * bhyve DSDT template\n");
	EFPRINTF(fp, " */\n");
	EFPRINTF(fp, "DefinitionBlock (\"bhyve_dsdt.aml\", \"DSDT\", 2,"
		 "\"BHYVE \", \"BVDSDT  \", 0x00000001)\n");
	EFPRINTF(fp, "{\n");
	EFPRINTF(fp, "  Scope (_SB)\n");
	EFPRINTF(fp, "  {\n");
	EFPRINTF(fp, "    Device (PCI0)\n");
	EFPRINTF(fp, "    {\n");
	EFPRINTF(fp, "      Name (_HID, EisaId (\"PNP0A03\"))\n");
	EFPRINTF(fp, "      Name (_ADR, Zero)\n");
	EFPRINTF(fp, "      Name (_UID, One)\n");
	EFPRINTF(fp, "      Name (_CRS, ResourceTemplate ()\n");
	EFPRINTF(fp, "      {\n");
	EFPRINTF(fp, "        WordBusNumber (ResourceProducer, MinFixed,"
		 "MaxFixed, PosDecode,\n");
	EFPRINTF(fp, "            0x0000,             // Granularity\n");
	EFPRINTF(fp, "            0x0000,             // Range Minimum\n");
	EFPRINTF(fp, "            0x00FF,             // Range Maximum\n");
	EFPRINTF(fp, "            0x0000,             // Transl Offset\n");
	EFPRINTF(fp, "            0x0100,             // Length\n");
	EFPRINTF(fp, "            ,, )\n");
	EFPRINTF(fp, "         IO (Decode16,\n");
	EFPRINTF(fp, "            0x0CF8,             // Range Minimum\n");
	EFPRINTF(fp, "            0x0CF8,             // Range Maximum\n");
	EFPRINTF(fp, "            0x01,               // Alignment\n");
	EFPRINTF(fp, "            0x08,               // Length\n");
	EFPRINTF(fp, "            )\n");
	EFPRINTF(fp, "         WordIO (ResourceProducer, MinFixed, MaxFixed,"
		 "PosDecode, EntireRange,\n");
	EFPRINTF(fp, "            0x0000,             // Granularity\n");
	EFPRINTF(fp, "            0x0000,             // Range Minimum\n");
	EFPRINTF(fp, "            0x0CF7,             // Range Maximum\n");
	EFPRINTF(fp, "            0x0000,             // Transl Offset\n");
	EFPRINTF(fp, "            0x0CF8,             // Length\n");
	EFPRINTF(fp, "            ,, , TypeStatic)\n");
	EFPRINTF(fp, "         WordIO (ResourceProducer, MinFixed, MaxFixed,"
		 "PosDecode, EntireRange,\n");
	EFPRINTF(fp, "            0x0000,             // Granularity\n");
	EFPRINTF(fp, "            0x0D00,             // Range Minimum\n");
	EFPRINTF(fp, "            0xFFFF,             // Range Maximum\n");
	EFPRINTF(fp, "            0x0000,             // Transl Offset\n");
	EFPRINTF(fp, "            0xF300,             // Length\n");
	EFPRINTF(fp, "             ,, , TypeStatic)\n");
	EFPRINTF(fp, "          })\n");
	EFPRINTF(fp, "     }\n");
	EFPRINTF(fp, "  }\n");
	EFPRINTF(fp, "\n");
	EFPRINTF(fp, "  Scope (_SB.PCI0)\n");
	EFPRINTF(fp, "  {\n");
	EFPRINTF(fp, "    Device (ISA)\n");
	EFPRINTF(fp, "    {\n");
	EFPRINTF(fp, "      Name (_ADR, 0x00010000)\n");
	EFPRINTF(fp, "      OperationRegion (P40C, PCI_Config, 0x60, 0x04)\n");
	EFPRINTF(fp, "    }\n");

	EFPRINTF(fp, "    Device (HPET)\n");
	EFPRINTF(fp, "    {\n");
	EFPRINTF(fp, "      Name (_HID, EISAID(\"PNP0103\"))\n");
	EFPRINTF(fp, "      Name (_UID, 0)\n");
	EFPRINTF(fp, "      Name (_CRS, ResourceTemplate ()\n");
	EFPRINTF(fp, "      {\n");
	EFPRINTF(fp, "        DWordMemory (ResourceConsumer, PosDecode, "
		 "MinFixed, MaxFixed, NonCacheable, ReadWrite,\n");
	EFPRINTF(fp, "            0x00000000,\n");
	EFPRINTF(fp, "            0xFED00000,\n");
	EFPRINTF(fp, "            0xFED003FF,\n");
	EFPRINTF(fp, "            0x00000000,\n");
	EFPRINTF(fp, "            0x00000400\n");
	EFPRINTF(fp, "            )\n");
	EFPRINTF(fp, "      })\n");
	EFPRINTF(fp, "    }\n");

	EFPRINTF(fp, "  }\n");
	EFPRINTF(fp, "\n");
	EFPRINTF(fp, "  Scope (_SB.PCI0.ISA)\n");
        EFPRINTF(fp, "  {\n");
	EFPRINTF(fp, "    Device (RTC)\n");
        EFPRINTF(fp, "    {\n");
	EFPRINTF(fp, "      Name (_HID, EisaId (\"PNP0B00\"))\n");
	EFPRINTF(fp, "      Name (_CRS, ResourceTemplate ()\n");
	EFPRINTF(fp, "      {\n");
	EFPRINTF(fp, "        IO (Decode16,\n");
	EFPRINTF(fp, "            0x0070,             // Range Minimum\n");
	EFPRINTF(fp, "            0x0070,             // Range Maximum\n");
	EFPRINTF(fp, "            0x10,               // Alignment\n");
	EFPRINTF(fp, "            0x02,               // Length\n");
	EFPRINTF(fp, "            )\n");
	EFPRINTF(fp, "        IRQNoFlags ()\n");
	EFPRINTF(fp, "            {8}\n");
	EFPRINTF(fp, "        IO (Decode16,\n");
	EFPRINTF(fp, "            0x0072,             // Range Minimum\n");
	EFPRINTF(fp, "            0x0072,             // Range Maximum\n");
	EFPRINTF(fp, "            0x02,               // Alignment\n");
	EFPRINTF(fp, "            0x06,               // Length\n");
	EFPRINTF(fp, "            )\n");
	EFPRINTF(fp, "      })\n");
        EFPRINTF(fp, "    }\n");
	EFPRINTF(fp, "  }\n");
	EFPRINTF(fp, "}\n");

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
		strncpy(bf->f_name, basl_stemplate, MAXPATHLEN);
		bf->fd = mkstemps(bf->f_name, strlen(BHYVE_ASL_SUFFIX));
	} else {
		strncpy(bf->f_name, basl_template, MAXPATHLEN);
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
basl_load(struct vmctx *ctx, int fd, uint64_t off)
{
	struct stat sb;
	void *gaddr;

	if (fstat(fd, &sb) < 0)
		return (errno);
		
	gaddr = paddr_guest2host(ctx, basl_acpi_base + off, sb.st_size);
	if (gaddr == NULL)
		return (EFAULT);

	if (read(fd, gaddr, sb.st_size) < 0)
		return (errno);

	return (0);
}

static int
basl_compile(struct vmctx *ctx, int (*fwrite_section)(FILE *), uint64_t offset)
{
	struct basl_fio io[2];
	static char iaslbuf[3*MAXPATHLEN + 10];
	char *fmt;
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
				err = basl_load(ctx, io[1].fd, offset);
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
		 * len has been intialized (and maybe adjusted) above
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

static struct {
	int	(*wsect)(FILE *fp);
	uint64_t  offset;
} basl_ftables[] =
{
	{ basl_fwrite_rsdp, 0},
	{ basl_fwrite_rsdt, RSDT_OFFSET },
	{ basl_fwrite_xsdt, XSDT_OFFSET },
	{ basl_fwrite_madt, MADT_OFFSET },
	{ basl_fwrite_fadt, FADT_OFFSET },
	{ basl_fwrite_hpet, HPET_OFFSET },
	{ basl_fwrite_facs, FACS_OFFSET },
	{ basl_fwrite_dsdt, DSDT_OFFSET },
	{ NULL }
};

int
acpi_build(struct vmctx *ctx, int ncpu)
{
	int err;
	int i;

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

	i = 0;
	err = basl_make_templates();

	/*
	 * Run through all the ASL files, compiling them and
	 * copying them into guest memory
	 */
	while (!err && basl_ftables[i].wsect != NULL) {
		err = basl_compile(ctx, basl_ftables[i].wsect,
				   basl_ftables[i].offset);
		i++;
	}

	return (err);
}
