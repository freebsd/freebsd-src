/******************************************************************************
 *
 * Module Name: tbfadt   - FADT table utilities
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#define __TBFADT_C__

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/actables.h>

#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbfadt")

/* Local prototypes */

static void
AcpiTbInitGenericAddress (
    ACPI_GENERIC_ADDRESS    *GenericAddress,
    UINT8                   SpaceId,
    UINT8                   ByteWidth,
    UINT64                  Address,
    char                    *RegisterName);

static void
AcpiTbConvertFadt (
    void);

static void
AcpiTbValidateFadt (
    void);

static void
AcpiTbSetupFadtRegisters (
    void);


/* Table for conversion of FADT to common internal format and FADT validation */

typedef struct acpi_fadt_info
{
    char                    *Name;
    UINT16                  Address64;
    UINT16                  Address32;
    UINT16                  Length;
    UINT8                   DefaultLength;
    UINT8                   Type;

} ACPI_FADT_INFO;

#define ACPI_FADT_OPTIONAL          0
#define ACPI_FADT_REQUIRED          1
#define ACPI_FADT_SEPARATE_LENGTH   2

static ACPI_FADT_INFO     FadtInfoTable[] =
{
    {"Pm1aEventBlock",
        ACPI_FADT_OFFSET (XPm1aEventBlock),
        ACPI_FADT_OFFSET (Pm1aEventBlock),
        ACPI_FADT_OFFSET (Pm1EventLength),
        ACPI_PM1_REGISTER_WIDTH * 2,        /* Enable + Status register */
        ACPI_FADT_REQUIRED},

    {"Pm1bEventBlock",
        ACPI_FADT_OFFSET (XPm1bEventBlock),
        ACPI_FADT_OFFSET (Pm1bEventBlock),
        ACPI_FADT_OFFSET (Pm1EventLength),
        ACPI_PM1_REGISTER_WIDTH * 2,        /* Enable + Status register */
        ACPI_FADT_OPTIONAL},

    {"Pm1aControlBlock",
        ACPI_FADT_OFFSET (XPm1aControlBlock),
        ACPI_FADT_OFFSET (Pm1aControlBlock),
        ACPI_FADT_OFFSET (Pm1ControlLength),
        ACPI_PM1_REGISTER_WIDTH,
        ACPI_FADT_REQUIRED},

    {"Pm1bControlBlock",
        ACPI_FADT_OFFSET (XPm1bControlBlock),
        ACPI_FADT_OFFSET (Pm1bControlBlock),
        ACPI_FADT_OFFSET (Pm1ControlLength),
        ACPI_PM1_REGISTER_WIDTH,
        ACPI_FADT_OPTIONAL},

    {"Pm2ControlBlock",
        ACPI_FADT_OFFSET (XPm2ControlBlock),
        ACPI_FADT_OFFSET (Pm2ControlBlock),
        ACPI_FADT_OFFSET (Pm2ControlLength),
        ACPI_PM2_REGISTER_WIDTH,
        ACPI_FADT_SEPARATE_LENGTH},

    {"PmTimerBlock",
        ACPI_FADT_OFFSET (XPmTimerBlock),
        ACPI_FADT_OFFSET (PmTimerBlock),
        ACPI_FADT_OFFSET (PmTimerLength),
        ACPI_PM_TIMER_WIDTH,
        ACPI_FADT_REQUIRED},

    {"Gpe0Block",
        ACPI_FADT_OFFSET (XGpe0Block),
        ACPI_FADT_OFFSET (Gpe0Block),
        ACPI_FADT_OFFSET (Gpe0BlockLength),
        0,
        ACPI_FADT_SEPARATE_LENGTH},

    {"Gpe1Block",
        ACPI_FADT_OFFSET (XGpe1Block),
        ACPI_FADT_OFFSET (Gpe1Block),
        ACPI_FADT_OFFSET (Gpe1BlockLength),
        0,
        ACPI_FADT_SEPARATE_LENGTH}
};

#define ACPI_FADT_INFO_ENTRIES \
            (sizeof (FadtInfoTable) / sizeof (ACPI_FADT_INFO))


/* Table used to split Event Blocks into separate status/enable registers */

typedef struct acpi_fadt_pm_info
{
    ACPI_GENERIC_ADDRESS    *Target;
    UINT16                  Source;
    UINT8                   RegisterNum;

} ACPI_FADT_PM_INFO;

static ACPI_FADT_PM_INFO    FadtPmInfoTable[] =
{
    {&AcpiGbl_XPm1aStatus,
        ACPI_FADT_OFFSET (XPm1aEventBlock),
        0},

    {&AcpiGbl_XPm1aEnable,
        ACPI_FADT_OFFSET (XPm1aEventBlock),
        1},

    {&AcpiGbl_XPm1bStatus,
        ACPI_FADT_OFFSET (XPm1bEventBlock),
        0},

    {&AcpiGbl_XPm1bEnable,
        ACPI_FADT_OFFSET (XPm1bEventBlock),
        1}
};

#define ACPI_FADT_PM_INFO_ENTRIES \
            (sizeof (FadtPmInfoTable) / sizeof (ACPI_FADT_PM_INFO))


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbInitGenericAddress
 *
 * PARAMETERS:  GenericAddress      - GAS struct to be initialized
 *              SpaceId             - ACPI Space ID for this register
 *              ByteWidth           - Width of this register
 *              Address             - Address of the register
 *
 * RETURN:      None
 *
 * DESCRIPTION: Initialize a Generic Address Structure (GAS)
 *              See the ACPI specification for a full description and
 *              definition of this structure.
 *
 ******************************************************************************/

static void
AcpiTbInitGenericAddress (
    ACPI_GENERIC_ADDRESS    *GenericAddress,
    UINT8                   SpaceId,
    UINT8                   ByteWidth,
    UINT64                  Address,
    char                    *RegisterName)
{
    UINT8                   BitWidth;


    /* Bit width field in the GAS is only one byte long, 255 max */

    BitWidth = (UINT8) (ByteWidth * 8);

    if (ByteWidth > 31) /* (31*8)=248 */
    {
        ACPI_ERROR ((AE_INFO,
            "%s - 32-bit FADT register is too long (%u bytes, %u bits) "
            "to convert to GAS struct - 255 bits max, truncating",
            RegisterName, ByteWidth, (ByteWidth * 8)));

        BitWidth = 255;
    }

    /*
     * The 64-bit Address field is non-aligned in the byte packed
     * GAS struct.
     */
    ACPI_MOVE_64_TO_64 (&GenericAddress->Address, &Address);

    /* All other fields are byte-wide */

    GenericAddress->SpaceId = SpaceId;
    GenericAddress->BitWidth = BitWidth;
    GenericAddress->BitOffset = 0;
    GenericAddress->AccessWidth = 0; /* Access width ANY */
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbParseFadt
 *
 * PARAMETERS:  TableIndex          - Index for the FADT
 *
 * RETURN:      None
 *
 * DESCRIPTION: Initialize the FADT, DSDT and FACS tables
 *              (FADT contains the addresses of the DSDT and FACS)
 *
 ******************************************************************************/

void
AcpiTbParseFadt (
    UINT32                  TableIndex)
{
    UINT32                  Length;
    ACPI_TABLE_HEADER       *Table;


    /*
     * The FADT has multiple versions with different lengths,
     * and it contains pointers to both the DSDT and FACS tables.
     *
     * Get a local copy of the FADT and convert it to a common format
     * Map entire FADT, assumed to be smaller than one page.
     */
    Length = AcpiGbl_RootTableList.Tables[TableIndex].Length;

    Table = AcpiOsMapMemory (
                AcpiGbl_RootTableList.Tables[TableIndex].Address, Length);
    if (!Table)
    {
        return;
    }

    /*
     * Validate the FADT checksum before we copy the table. Ignore
     * checksum error as we want to try to get the DSDT and FACS.
     */
    (void) AcpiTbVerifyChecksum (Table, Length);

    /* Create a local copy of the FADT in common ACPI 2.0+ format */

    AcpiTbCreateLocalFadt (Table, Length);

    /* All done with the real FADT, unmap it */

    AcpiOsUnmapMemory (Table, Length);

    /* Obtain the DSDT and FACS tables via their addresses within the FADT */

    AcpiTbInstallTable ((ACPI_PHYSICAL_ADDRESS) AcpiGbl_FADT.XDsdt,
        ACPI_SIG_DSDT, ACPI_TABLE_INDEX_DSDT);

    /* If Hardware Reduced flag is set, there is no FACS */

    if (!AcpiGbl_ReducedHardware)
    {
        AcpiTbInstallTable ((ACPI_PHYSICAL_ADDRESS) AcpiGbl_FADT.XFacs,
            ACPI_SIG_FACS, ACPI_TABLE_INDEX_FACS);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbCreateLocalFadt
 *
 * PARAMETERS:  Table               - Pointer to BIOS FADT
 *              Length              - Length of the table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Get a local copy of the FADT and convert it to a common format.
 *              Performs validation on some important FADT fields.
 *
 * NOTE:        We create a local copy of the FADT regardless of the version.
 *
 ******************************************************************************/

void
AcpiTbCreateLocalFadt (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Length)
{

    /*
     * Check if the FADT is larger than the largest table that we expect
     * (the ACPI 5.0 version). If so, truncate the table, and issue
     * a warning.
     */
    if (Length > sizeof (ACPI_TABLE_FADT))
    {
        ACPI_BIOS_WARNING ((AE_INFO,
            "FADT (revision %u) is longer than ACPI 5.0 version, "
            "truncating length %u to %u",
            Table->Revision, Length, (UINT32) sizeof (ACPI_TABLE_FADT)));
    }

    /* Clear the entire local FADT */

    ACPI_MEMSET (&AcpiGbl_FADT, 0, sizeof (ACPI_TABLE_FADT));

    /* Copy the original FADT, up to sizeof (ACPI_TABLE_FADT) */

    ACPI_MEMCPY (&AcpiGbl_FADT, Table,
        ACPI_MIN (Length, sizeof (ACPI_TABLE_FADT)));

    /* Take a copy of the Hardware Reduced flag */

    AcpiGbl_ReducedHardware = FALSE;
    if (AcpiGbl_FADT.Flags & ACPI_FADT_HW_REDUCED)
    {
        AcpiGbl_ReducedHardware = TRUE;
    }

    /* Convert the local copy of the FADT to the common internal format */

    AcpiTbConvertFadt ();

    /* Validate FADT values now, before we make any changes */

    AcpiTbValidateFadt ();

    /* Initialize the global ACPI register structures */

    AcpiTbSetupFadtRegisters ();
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbConvertFadt
 *
 * PARAMETERS:  None, uses AcpiGbl_FADT
 *
 * RETURN:      None
 *
 * DESCRIPTION: Converts all versions of the FADT to a common internal format.
 *              Expand 32-bit addresses to 64-bit as necessary.
 *
 * NOTE:        AcpiGbl_FADT must be of size (ACPI_TABLE_FADT),
 *              and must contain a copy of the actual FADT.
 *
 * Notes on 64-bit register addresses:
 *
 * After this FADT conversion, later ACPICA code will only use the 64-bit "X"
 * fields of the FADT for all ACPI register addresses.
 *
 * The 64-bit "X" fields are optional extensions to the original 32-bit FADT
 * V1.0 fields. Even if they are present in the FADT, they are optional and
 * are unused if the BIOS sets them to zero. Therefore, we must copy/expand
 * 32-bit V1.0 fields if the corresponding X field is zero.
 *
 * For ACPI 1.0 FADTs, all 32-bit address fields are expanded to the
 * corresponding "X" fields in the internal FADT.
 *
 * For ACPI 2.0+ FADTs, all valid (non-zero) 32-bit address fields are expanded
 * to the corresponding 64-bit X fields. For compatibility with other ACPI
 * implementations, we ignore the 64-bit field if the 32-bit field is valid,
 * regardless of whether the host OS is 32-bit or 64-bit.
 *
 ******************************************************************************/

static void
AcpiTbConvertFadt (
    void)
{
    ACPI_GENERIC_ADDRESS    *Address64;
    UINT32                  Address32;
    UINT32                  i;


    /*
     * Expand the 32-bit FACS and DSDT addresses to 64-bit as necessary.
     * Later code will always use the X 64-bit field.
     */
    if (!AcpiGbl_FADT.XFacs)
    {
        AcpiGbl_FADT.XFacs = (UINT64) AcpiGbl_FADT.Facs;
    }
    if (!AcpiGbl_FADT.XDsdt)
    {
        AcpiGbl_FADT.XDsdt = (UINT64) AcpiGbl_FADT.Dsdt;
    }

    /*
     * For ACPI 1.0 FADTs (revision 1 or 2), ensure that reserved fields which
     * should be zero are indeed zero. This will workaround BIOSs that
     * inadvertently place values in these fields.
     *
     * The ACPI 1.0 reserved fields that will be zeroed are the bytes located
     * at offset 45, 55, 95, and the word located at offset 109, 110.
     *
     * Note: The FADT revision value is unreliable. Only the length can be
     * trusted.
     */
    if (AcpiGbl_FADT.Header.Length <= ACPI_FADT_V2_SIZE)
    {
        AcpiGbl_FADT.PreferredProfile = 0;
        AcpiGbl_FADT.PstateControl = 0;
        AcpiGbl_FADT.CstControl = 0;
        AcpiGbl_FADT.BootFlags = 0;
    }

    /*
     * Now we can update the local FADT length to the length of the
     * current FADT version as defined by the ACPI specification.
     * Thus, we will have a common FADT internally.
     */
    AcpiGbl_FADT.Header.Length = sizeof (ACPI_TABLE_FADT);

    /*
     * Expand the ACPI 1.0 32-bit addresses to the ACPI 2.0 64-bit "X"
     * generic address structures as necessary. Later code will always use
     * the 64-bit address structures.
     *
     * March 2009:
     * We now always use the 32-bit address if it is valid (non-null). This
     * is not in accordance with the ACPI specification which states that
     * the 64-bit address supersedes the 32-bit version, but we do this for
     * compatibility with other ACPI implementations. Most notably, in the
     * case where both the 32 and 64 versions are non-null, we use the 32-bit
     * version. This is the only address that is guaranteed to have been
     * tested by the BIOS manufacturer.
     */
    for (i = 0; i < ACPI_FADT_INFO_ENTRIES; i++)
    {
        Address32 = *ACPI_ADD_PTR (UINT32,
            &AcpiGbl_FADT, FadtInfoTable[i].Address32);

        Address64 = ACPI_ADD_PTR (ACPI_GENERIC_ADDRESS,
            &AcpiGbl_FADT, FadtInfoTable[i].Address64);

        /*
         * If both 32- and 64-bit addresses are valid (non-zero),
         * they must match.
         */
        if (Address64->Address && Address32 &&
           (Address64->Address != (UINT64) Address32))
        {
            ACPI_BIOS_ERROR ((AE_INFO,
                "32/64X address mismatch in FADT/%s: "
                "0x%8.8X/0x%8.8X%8.8X, using 32",
                FadtInfoTable[i].Name, Address32,
                ACPI_FORMAT_UINT64 (Address64->Address)));
        }

        /* Always use 32-bit address if it is valid (non-null) */

        if (Address32)
        {
            /*
             * Copy the 32-bit address to the 64-bit GAS structure. The
             * Space ID is always I/O for 32-bit legacy address fields
             */
            AcpiTbInitGenericAddress (Address64, ACPI_ADR_SPACE_SYSTEM_IO,
                *ACPI_ADD_PTR (UINT8, &AcpiGbl_FADT, FadtInfoTable[i].Length),
                (UINT64) Address32, FadtInfoTable[i].Name);
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbValidateFadt
 *
 * PARAMETERS:  Table           - Pointer to the FADT to be validated
 *
 * RETURN:      None
 *
 * DESCRIPTION: Validate various important fields within the FADT. If a problem
 *              is found, issue a message, but no status is returned.
 *              Used by both the table manager and the disassembler.
 *
 * Possible additional checks:
 * (AcpiGbl_FADT.Pm1EventLength >= 4)
 * (AcpiGbl_FADT.Pm1ControlLength >= 2)
 * (AcpiGbl_FADT.PmTimerLength >= 4)
 * Gpe block lengths must be multiple of 2
 *
 ******************************************************************************/

static void
AcpiTbValidateFadt (
    void)
{
    char                    *Name;
    ACPI_GENERIC_ADDRESS    *Address64;
    UINT8                   Length;
    UINT32                  i;


    /*
     * Check for FACS and DSDT address mismatches. An address mismatch between
     * the 32-bit and 64-bit address fields (FIRMWARE_CTRL/X_FIRMWARE_CTRL and
     * DSDT/X_DSDT) would indicate the presence of two FACS or two DSDT tables.
     */
    if (AcpiGbl_FADT.Facs &&
        (AcpiGbl_FADT.XFacs != (UINT64) AcpiGbl_FADT.Facs))
    {
        ACPI_BIOS_WARNING ((AE_INFO,
            "32/64X FACS address mismatch in FADT - "
            "0x%8.8X/0x%8.8X%8.8X, using 32",
            AcpiGbl_FADT.Facs, ACPI_FORMAT_UINT64 (AcpiGbl_FADT.XFacs)));

        AcpiGbl_FADT.XFacs = (UINT64) AcpiGbl_FADT.Facs;
    }

    if (AcpiGbl_FADT.Dsdt &&
        (AcpiGbl_FADT.XDsdt != (UINT64) AcpiGbl_FADT.Dsdt))
    {
        ACPI_BIOS_WARNING ((AE_INFO,
            "32/64X DSDT address mismatch in FADT - "
            "0x%8.8X/0x%8.8X%8.8X, using 32",
            AcpiGbl_FADT.Dsdt, ACPI_FORMAT_UINT64 (AcpiGbl_FADT.XDsdt)));

        AcpiGbl_FADT.XDsdt = (UINT64) AcpiGbl_FADT.Dsdt;
    }

    /* If Hardware Reduced flag is set, we are all done */

    if (AcpiGbl_ReducedHardware)
    {
        return;
    }

    /* Examine all of the 64-bit extended address fields (X fields) */

    for (i = 0; i < ACPI_FADT_INFO_ENTRIES; i++)
    {
        /*
         * Generate pointer to the 64-bit address, get the register
         * length (width) and the register name
         */
        Address64 = ACPI_ADD_PTR (ACPI_GENERIC_ADDRESS,
                        &AcpiGbl_FADT, FadtInfoTable[i].Address64);
        Length = *ACPI_ADD_PTR (UINT8,
                        &AcpiGbl_FADT, FadtInfoTable[i].Length);
        Name = FadtInfoTable[i].Name;

        /*
         * For each extended field, check for length mismatch between the
         * legacy length field and the corresponding 64-bit X length field.
         * Note: If the legacy length field is > 0xFF bits, ignore this
         * check. (GPE registers can be larger than the 64-bit GAS structure
         * can accomodate, 0xFF bits).
         */
        if (Address64->Address &&
           (ACPI_MUL_8 (Length) <= ACPI_UINT8_MAX) &&
           (Address64->BitWidth != ACPI_MUL_8 (Length)))
        {
            ACPI_BIOS_WARNING ((AE_INFO,
                "32/64X length mismatch in FADT/%s: %u/%u",
                Name, ACPI_MUL_8 (Length), Address64->BitWidth));
        }

        if (FadtInfoTable[i].Type & ACPI_FADT_REQUIRED)
        {
            /*
             * Field is required (PM1aEvent, PM1aControl, PmTimer).
             * Both the address and length must be non-zero.
             */
            if (!Address64->Address || !Length)
            {
                ACPI_BIOS_ERROR ((AE_INFO,
                    "Required FADT field %s has zero address and/or length: "
                    "0x%8.8X%8.8X/0x%X",
                    Name, ACPI_FORMAT_UINT64 (Address64->Address), Length));
            }
        }
        else if (FadtInfoTable[i].Type & ACPI_FADT_SEPARATE_LENGTH)
        {
            /*
             * Field is optional (PM2Control, GPE0, GPE1) AND has its own
             * length field. If present, both the address and length must
             * be valid.
             */
            if ((Address64->Address && !Length) ||
                (!Address64->Address && Length))
            {
                ACPI_BIOS_WARNING ((AE_INFO,
                    "Optional FADT field %s has zero address or length: "
                    "0x%8.8X%8.8X/0x%X",
                    Name, ACPI_FORMAT_UINT64 (Address64->Address), Length));
            }
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbSetupFadtRegisters
 *
 * PARAMETERS:  None, uses AcpiGbl_FADT.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Initialize global ACPI PM1 register definitions. Optionally,
 *              force FADT register definitions to their default lengths.
 *
 ******************************************************************************/

static void
AcpiTbSetupFadtRegisters (
    void)
{
    ACPI_GENERIC_ADDRESS    *Target64;
    ACPI_GENERIC_ADDRESS    *Source64;
    UINT8                   Pm1RegisterByteWidth;
    UINT32                  i;


    /*
     * Optionally check all register lengths against the default values and
     * update them if they are incorrect.
     */
    if (AcpiGbl_UseDefaultRegisterWidths)
    {
        for (i = 0; i < ACPI_FADT_INFO_ENTRIES; i++)
        {
            Target64 = ACPI_ADD_PTR (ACPI_GENERIC_ADDRESS, &AcpiGbl_FADT,
                FadtInfoTable[i].Address64);

            /*
             * If a valid register (Address != 0) and the (DefaultLength > 0)
             * (Not a GPE register), then check the width against the default.
             */
            if ((Target64->Address) &&
                (FadtInfoTable[i].DefaultLength > 0) &&
                (FadtInfoTable[i].DefaultLength != Target64->BitWidth))
            {
                ACPI_BIOS_WARNING ((AE_INFO,
                    "Invalid length for FADT/%s: %u, using default %u",
                    FadtInfoTable[i].Name, Target64->BitWidth,
                    FadtInfoTable[i].DefaultLength));

                /* Incorrect size, set width to the default */

                Target64->BitWidth = FadtInfoTable[i].DefaultLength;
            }
        }
    }

    /*
     * Get the length of the individual PM1 registers (enable and status).
     * Each register is defined to be (event block length / 2). Extra divide
     * by 8 converts bits to bytes.
     */
    Pm1RegisterByteWidth = (UINT8)
        ACPI_DIV_16 (AcpiGbl_FADT.XPm1aEventBlock.BitWidth);

    /*
     * Calculate separate GAS structs for the PM1x (A/B) Status and Enable
     * registers. These addresses do not appear (directly) in the FADT, so it
     * is useful to pre-calculate them from the PM1 Event Block definitions.
     *
     * The PM event blocks are split into two register blocks, first is the
     * PM Status Register block, followed immediately by the PM Enable
     * Register block. Each is of length (Pm1EventLength/2)
     *
     * Note: The PM1A event block is required by the ACPI specification.
     * However, the PM1B event block is optional and is rarely, if ever,
     * used.
     */

    for (i = 0; i < ACPI_FADT_PM_INFO_ENTRIES; i++)
    {
        Source64 = ACPI_ADD_PTR (ACPI_GENERIC_ADDRESS, &AcpiGbl_FADT,
            FadtPmInfoTable[i].Source);

        if (Source64->Address)
        {
            AcpiTbInitGenericAddress (FadtPmInfoTable[i].Target,
                Source64->SpaceId, Pm1RegisterByteWidth,
                Source64->Address +
                    (FadtPmInfoTable[i].RegisterNum * Pm1RegisterByteWidth),
                "PmRegisters");
        }
    }
}
