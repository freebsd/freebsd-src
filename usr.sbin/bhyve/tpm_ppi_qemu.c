/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/endian.h>
#include <sys/linker_set.h>

#include <machine/vmm.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <vmmapi.h>

#include "acpi.h"
#include "acpi_device.h"
#include "config.h"
#include "mem.h"
#include "qemu_fwcfg.h"
#include "tpm_ppi.h"

#define TPM_PPI_ADDRESS 0xFED45000
#define TPM_PPI_SIZE 0x1000

#define TPM_PPI_FWCFG_FILE "etc/tpm/config"

#define TPM_PPI_QEMU_NAME "qemu"

struct tpm_ppi_qemu {
	uint8_t func[256];	    // FUNC
	uint8_t in;		    // PPIN
	uint32_t ip;		    // PPIP
	uint32_t response;	    // PPRP
	uint32_t request;	    // PPRQ
	uint32_t request_parameter; // PPRM
	uint32_t last_request;	    // LPPR
	uint32_t func_ret;	    // FRET
	uint8_t _reserved1[0x40];   // RES1
	uint8_t next_step;	    // next_step
} __packed;
static_assert(sizeof(struct tpm_ppi_qemu) <= TPM_PPI_SIZE,
    "Wrong size of tpm_ppi_qemu");

struct tpm_ppi_fwcfg {
	uint32_t ppi_address;
	uint8_t tpm_version;
	uint8_t ppi_version;
} __packed;

static int
tpm_ppi_mem_handler(struct vcpu *const vcpu __unused, const int dir,
    const uint64_t addr, const int size, uint64_t *const val, void *const arg1,
    const long arg2 __unused)
{
	struct tpm_ppi_qemu *ppi;
	uint8_t *ptr;
	uint64_t off;

	if ((addr & (size - 1)) != 0) {
		warnx("%s: unaligned %s access @ %16lx [size = %x]", __func__,
		    (dir == MEM_F_READ) ? "read" : "write", addr, size);
	}

	ppi = arg1;

	off = addr - TPM_PPI_ADDRESS;
	ptr = (uint8_t *)ppi + off;

	if (off > TPM_PPI_SIZE || off + size > TPM_PPI_SIZE) {
		return (EINVAL);
	}

	assert(size == 1 || size == 2 || size == 4 || size == 8);
	if (dir == MEM_F_READ) {
		memcpy(val, ptr, size);
	} else {
		memcpy(ptr, val, size);
	}

	return (0);
}

static struct mem_range ppi_mmio = {
	.name = "ppi-mmio",
	.base = TPM_PPI_ADDRESS,
	.size = TPM_PPI_SIZE,
	.flags = MEM_F_RW,
	.handler = tpm_ppi_mem_handler,
};

static int
tpm_ppi_init(void **sc)
{
	struct tpm_ppi_qemu *ppi = NULL;
	struct tpm_ppi_fwcfg *fwcfg = NULL;
	int error;

	ppi = calloc(1, sizeof(*ppi));
	if (ppi == NULL) {
		warnx("%s: failed to allocate acpi region for ppi", __func__);
		error = ENOMEM;
		goto err_out;
	}

	fwcfg = calloc(1, sizeof(struct tpm_ppi_fwcfg));
	if (fwcfg == NULL) {
		warnx("%s: failed to allocate fwcfg item", __func__);
		error = ENOMEM;
		goto err_out;
	}

	fwcfg->ppi_address = htole32(TPM_PPI_ADDRESS);
	fwcfg->tpm_version = 2;
	fwcfg->ppi_version = 1;

	error = qemu_fwcfg_add_file(TPM_PPI_FWCFG_FILE,
	    sizeof(struct tpm_ppi_fwcfg), fwcfg);
	if (error) {
		warnx("%s: failed to add fwcfg file", __func__);
		goto err_out;
	}

	/*
	 * We would just need to create some guest memory for the PPI region.
	 * Sadly, bhyve has a strange memory interface. We can't just add more
	 * memory to the VM. So, create a trap instead which reads and writes to
	 * the ppi region. It's very slow but ppi shouldn't be used frequently.
	 */
	ppi_mmio.arg1 = ppi;
	error = register_mem(&ppi_mmio);
	if (error) {
		warnx("%s: failed to create trap for ppi accesses", __func__);
		goto err_out;
	}

	*sc = ppi;

	return (0);

err_out:
	free(fwcfg);
	free(ppi);

	return (error);
}

static void
tpm_ppi_deinit(void *sc)
{
	struct tpm_ppi_qemu *ppi;
	int error;

	if (sc == NULL)
		return;

	ppi = sc;

	error = unregister_mem(&ppi_mmio);
	assert(error = 0);

	free(ppi);
}

static int
tpm_ppi_write_dsdt_regions(void *sc __unused)
{
	/*
	 * struct tpm_ppi_qemu
	 */
	/*
	 * According to qemu the Windows ACPI parser has a bug that DerefOf is
	 * broken for SYSTEM_MEMORY. Due to that bug, qemu uses a dynamic
	 * operation region inside a method.
	 */
	dsdt_line("Method(TPFN, 1, Serialized)");
	dsdt_line("{");
	dsdt_line("  If(LGreaterEqual(Arg0, 0x100))");
	dsdt_line("  {");
	dsdt_line("    Return(Zero)");
	dsdt_line("  }");
	dsdt_line(
	    "  OperationRegion(TPP1, SystemMemory, Add(0x%8x, Arg0), One)",
	    TPM_PPI_ADDRESS);
	dsdt_line("  Field(TPP1, ByteAcc, NoLock, Preserve)");
	dsdt_line("  {");
	dsdt_line("    TPPF, 8,");
	dsdt_line("  }");
	dsdt_line("  Return(TPPF)");
	dsdt_line("}");
	dsdt_line("OperationRegion(TPP2, SystemMemory, 0x%8x, 0x%x)",
	    TPM_PPI_ADDRESS + 0x100, 0x5A);
	dsdt_line("Field(TPP2, AnyAcc, NoLock, Preserve)");
	dsdt_line("{");
	dsdt_line("  PPIN, 8,");
	dsdt_line("  PPIP, 32,");
	dsdt_line("  PPRP, 32,");
	dsdt_line("  PPRQ, 32,");
	dsdt_line("  PPRM, 32,");
	dsdt_line("  LPPR, 32,");
	dsdt_line("}");
	/*
	 * Used for TCG Platform Reset Attack Mitigation
	 */
	dsdt_line("OperationRegion(TPP3, SystemMemory, 0x%8x, 1)",
	    TPM_PPI_ADDRESS + sizeof(struct tpm_ppi_qemu));
	dsdt_line("Field(TPP3, ByteAcc, NoLock, Preserve)");
	dsdt_line("{");
	dsdt_line("  MOVV, 8,");
	dsdt_line("}");

	return (0);
}

static int
tpm_ppi_write_dsdt_dsm(void *sc __unused)
{
	/*
	 * Physical Presence Interface
	 */
	dsdt_line(
	    "If(LEqual(Arg0, ToUUID(\"3DDDFAA6-361B-4EB4-A424-8D10089D1653\"))) /* UUID */");
	dsdt_line("{");
	/*
	 * Function 0 - _DSM Query Function
	 * Arguments:
	 *   Empty Package
	 * Return:
	 *   Buffer - Index field of supported functions
	 */
	dsdt_line("  If(LEqual(Arg2, 0)) /* Function */");
	dsdt_line("  {");
	dsdt_line("    Return(Buffer(0x02)");
	dsdt_line("    {");
	dsdt_line("      0xFF, 0x01");
	dsdt_line("    })");
	dsdt_line("  }");
	/*
	 * Function 1 - Get Physical Presence Interface Version
	 * Arguments:
	 *   Empty Package
	 * Return:
	 *   String - Supported Physical Presence Interface revision
	 */
	dsdt_line("  If(LEqual(Arg2, 1)) /* Function */");
	dsdt_line("  {");
	dsdt_line("    Return(\"1.3\")");
	dsdt_line("  }");
	/*
	 * Function 2 - Submit TPM Operation Request to Pre-OS Environment
	 * !!!DEPRECATED BUT MANDATORY!!!
	 * Arguments:
	 *   Integer - Operation Value of the Request
	 * Return:
	 *   Integer - Function Return Code
	 *     0 - Success
	 *     1 - Operation Value of the Request Not Supported
	 *     2 - General Failure
	 */
	dsdt_line("  If(LEqual(Arg2, 2)) /* Function */");
	dsdt_line("  {");
	dsdt_line("    Store(DerefOf(Index(Arg3, 0)), Local0)");
	dsdt_line("    Store(TPFN(Local0), Local1)");
	dsdt_line("    If (LEqual(And(Local1, 7), 0))");
	dsdt_line("    {");
	dsdt_line("      Return(1)");
	dsdt_line("    }");
	dsdt_line("    Store(Local0, PPRQ)");
	dsdt_line("    Store(0, PPRM)");
	dsdt_line("    Return(0)");
	dsdt_line("  }");
	/*
	 * Function 3 - Get Pending TPM Operation Request By the OS
	 * Arguments:
	 *   Empty Package
	 * Return:
	 *   Package
	 *     Integer 1 - Function Return Code
	 *       0 - Success
	 *       1 - General Failure
	 *     Integer 2 - Pending operation requested by the OS
	 *       0 - None
	 *      >0 - Operation Value of the Pending Request
	 *     Integer 3 - Optional argument to pending operation requested by
	 *                 the OS
	 *       0 - None
	 *      >0 - Argument of the Pending Request
	 */
	dsdt_line("  If(LEqual(Arg2, 3)) /* Function */");
	dsdt_line("  {");
	dsdt_line("    If(LEqual(Arg1, 1)) /* Revision */");
	dsdt_line("    {");
	dsdt_line("      Store(PPRQ, Index(TPM2, 1))");
	dsdt_line("      Return(TPM2)");
	dsdt_line("    }");
	dsdt_line("    If(LEqual(Arg1, 2)) /* Revision */");
	dsdt_line("    {");
	dsdt_line("      Store(PPRQ, Index(TPM3, 1))");
	dsdt_line("      Store(PPRM, Index(TPM3, 2))");
	dsdt_line("      Return(TPM3)");
	dsdt_line("    }");
	dsdt_line("  }");
	/*
	 * Function 4 - Get Platform-Specific Action to Transition to Pre-OS
	 *              Environment
	 * Arguments:
	 *   Empty Package
	 * Return:
	 *   Integer - Action that the OS should take to transition to the
	 *             pre-OS environment for execution of a requested operation
	 *     0 - None
	 *     1 - Shutdown
	 *     2 - Reboot
	 *     3 - OS Vendor-specific
	 */
	dsdt_line("  If(LEqual(Arg2, 4)) /* Function */");
	dsdt_line("  {");
	dsdt_line("    Return(2)");
	dsdt_line("  }");
	/*
	 * Function 5 - Return TPM Operation Response to OS Environment
	 * Arguments:
	 *   Empty Package
	 * Return:
	 *   Package
	 *     Integer 1 - Function Return Code
	 *       0 - Success
	 *       1 - General Failure
	 *     Integer 2 - Most recent operation request
	 *       0 - None
	 *      >0 - Operation value of the most recent request
	 *     Integer 3 - Response to the most recent operation request
	 *       0 - Success
	 *       0x00000001..0x000000FF - Corresponding TPM error code
	 *       0xFFFFFFF0 - User Abort or timeout of dialog
	 *       0xFFFFFFF1 - firmware failure
	 */
	dsdt_line("  If(LEqual(Arg2, 5)) /* Function */");
	dsdt_line("  {");
	dsdt_line("    Store(LPPR, Index(TPM3, 1))");
	dsdt_line("    Store(PPRP, Index(TPM3, 2))");
	dsdt_line("    Return(TPM3)");
	dsdt_line("  }");
	/*
	 * Function 6 - Submit preferred user language
	 * !!!DEPRECATED BUT MANDATORY!!!
	 * Arguments:
	 *   Package
	 *     String - Preferred language code
	 * Return:
	 *   Integer
	 *     3 - Not implemented
	 */
	dsdt_line("  If(LEqual(Arg2, 6)) /* Function */");
	dsdt_line("  {");
	dsdt_line("    Return(3)");
	dsdt_line("  }");
	/*
	 * Function 7 - Submit TPM Operation Request to Pre-OS Environment 2
	 * Arguments:
	 *   Package
	 *     Integer 1 - Operation Value of the Request
	 *     Integer 2 - Argument for Operation
	 * Return:
	 *   Integer - Function Return Code
	 *     0 - Success
	 *     1 - Not Implemented
	 *     2 - General Failure
	 *     3 - Operation blocked by current firmware settings
	 */
	dsdt_line("  If(LEqual(Arg2, 7)) /* Function */");
	dsdt_line("  {");
	dsdt_line("    Store(DerefOf(Index(Arg3, 0)), Local0)");
	dsdt_line("    Store(TPFN(Local0), Local1)");
	dsdt_line("    If (LEqual(And(Local1, 7), 0)) /* Not Implemented */");
	dsdt_line("    {");
	dsdt_line("      Return(1)");
	dsdt_line("    }");
	dsdt_line("    If (LEqual(And(Local1, 7), 2)) /* Blocked */ ");
	dsdt_line("    {");
	dsdt_line("      Return(3)");
	dsdt_line("    }");
	dsdt_line("    If(LEqual(Arg1, 1)) /* Revision */");
	dsdt_line("    {");
	dsdt_line("      Store(Local0, PPRQ)");
	dsdt_line("      Store(0, PPRM)");
	dsdt_line("    }");
	dsdt_line("    If(LEqual(Arg1, 2)) /* Revision */");
	dsdt_line("    {");
	dsdt_line("      Store(Local0, PPRQ)");
	dsdt_line("      Store(DerefOf(Index(Arg3, 1)), PPRM)");
	dsdt_line("    }");
	dsdt_line("    Return(0)");
	dsdt_line("  }");
	/*
	 * Function 8 - Get User Confirmation Status for Operation
	 * Arguments:
	 *   Package
	 *     Integer - Operation Value that may need user confirmation
	 * Return:
	 *   Integer - Function Return Code
	 *     0 - Not implemented
	 *     1 - Firmware only
	 *     2 - Blocked for OS by firmware configuration
	 *     3 - Allowed and physically present user required
	 *     4 - Allowed and physically present user not required
	 */
	dsdt_line("    If(LEqual(Arg2, 8)) /* Function */");
	dsdt_line("    {");
	dsdt_line("      Store(DerefOf(Index(Arg3, 0)), Local0)");
	dsdt_line("      Store(TPFN(Local0), Local1)");
	dsdt_line("      Return(And(Local1, 7))");
	dsdt_line("    }");
	/*
	 * Unknown function
	 */
	dsdt_line("  Return(Buffer(1)");
	dsdt_line("  {");
	dsdt_line("    0x00");
	dsdt_line("  })");
	dsdt_line("}");

	/*
	 * TCG Platform Reset Attack Mitigation
	 */
	dsdt_line(
	    "If(LEqual(Arg0, ToUUID(\"376054ED-CC13-4675-901C-4756D7F2D45D\"))) /* UUID */");
	dsdt_line("{");
	/*
	 * Function 0 - _DSM Query Function
	 * Arguments:
	 *   Empty Package
	 * Return:
	 *   Buffer - Index field of supported functions
	 */
	dsdt_line("  If(LEqual(Arg2, 0)) /* Function */");
	dsdt_line("  {");
	dsdt_line("    Return(Buffer(1)");
	dsdt_line("    {");
	dsdt_line("      0x03");
	dsdt_line("    })");
	dsdt_line("  }");
	/*
	 * Function 1 - Memory Clear
	 * Arguments:
	 *   Package
	 *     Integer - Operation Value of the Request
	 * Return:
	 *   Integer - Function Return Code
	 *     0 - Success
	 *     1 - General Failure
	 */
	dsdt_line("  If(LEqual(Arg2, 1)) /* Function */");
	dsdt_line("  {");
	dsdt_line("    Store(DerefOf(Index(Arg3, 0)), Local0)");
	dsdt_line("    Store(Local0, MOVV)");
	dsdt_line("    Return(0)");
	dsdt_line("  }");
	dsdt_line("}");

	return (0);
}

static struct tpm_ppi tpm_ppi_qemu = {
	.name = TPM_PPI_QEMU_NAME,
	.init = tpm_ppi_init,
	.deinit = tpm_ppi_deinit,
	.write_dsdt_regions = tpm_ppi_write_dsdt_regions,
	.write_dsdt_dsm = tpm_ppi_write_dsdt_dsm,
};
TPM_PPI_SET(tpm_ppi_qemu);
