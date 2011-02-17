/******************************************************************************
 *
 * Module Name: dttemplate - ACPI table template generation
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/include/acapps.h>
#include <contrib/dev/acpica/compiler/dtcompiler.h>
#include <contrib/dev/acpica/compiler/dttemplate.h> /* Contains the hex ACPI table templates */

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dttemplate")


/* Local prototypes */

static BOOLEAN
AcpiUtIsSpecialTable (
    char                    *Signature);

static ACPI_STATUS
DtCreateOneTemplate (
    char                    *Signature,
    ACPI_DMTABLE_DATA       *TableData);

static ACPI_STATUS
DtCreateAllTemplates (
    void);


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtIsSpecialTable
 *
 * PARAMETERS:  Signature           - ACPI table signature
 *
 * RETURN:      TRUE if signature is a special ACPI table
 *
 * DESCRIPTION: Check for valid ACPI tables that are not in the main ACPI
 *              table data structure (AcpiDmTableData).
 *
 ******************************************************************************/

static BOOLEAN
AcpiUtIsSpecialTable (
    char                    *Signature)
{

    if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_DSDT) ||
        ACPI_COMPARE_NAME (Signature, ACPI_SIG_SSDT) ||
        ACPI_COMPARE_NAME (Signature, ACPI_SIG_FACS) ||
        ACPI_COMPARE_NAME (Signature, ACPI_RSDP_NAME))
    {
        return (TRUE);
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    DtCreateTemplates
 *
 * PARAMETERS:  Signature           - ACPI table signature
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create one or more template files.
 *
 ******************************************************************************/

ACPI_STATUS
DtCreateTemplates (
    char                    *Signature)
{
    ACPI_DMTABLE_DATA       *TableData;
    ACPI_STATUS             Status;


    AslInitializeGlobals ();
    AcpiUtStrupr (Signature);

    /* Create all known templates if requested */

    if (!ACPI_STRNCMP (Signature, "ALL", 3))
    {
        Status = DtCreateAllTemplates ();
        return (Status);
    }

    /*
     * Validate signature and get the template data:
     *  1) Signature must be 4 characters
     *  2) Signature must be a recognized ACPI table
     *  3) There must be a template associated with the signature
     */
    if (strlen (Signature) != ACPI_NAME_SIZE)
    {
        fprintf (stderr, "%s, Invalid ACPI table signature\n", Signature);
        return (AE_ERROR);
    }

    /*
     * Some slack for the two strange tables whose name is different than
     * their signatures: MADT->APIC and FADT->FACP.
     */
    if (!strcmp (Signature, "MADT"))
    {
        Signature = "APIC";
    }
    else if (!strcmp (Signature, "FADT"))
    {
        Signature = "FACP";
    }

    TableData = AcpiDmGetTableData (Signature);
    if (TableData)
    {
        if (!TableData->Template)
        {
            fprintf (stderr, "%4.4s, No template available\n", Signature);
            return (AE_ERROR);
        }
    }
    else if (!AcpiUtIsSpecialTable (Signature))
    {
        fprintf (stderr,
            "%4.4s, Unrecognized ACPI table signature\n", Signature);
        return (AE_ERROR);
    }

    Status = AdInitialize ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = DtCreateOneTemplate (Signature, TableData);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    DtCreateAllTemplates
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create all currently defined template files
 *
 ******************************************************************************/

static ACPI_STATUS
DtCreateAllTemplates (
    void)
{
    ACPI_DMTABLE_DATA       *TableData;
    ACPI_STATUS             Status;


    Status = AdInitialize ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    fprintf (stderr, "Creating all supported Template files\n");

    /* Walk entire ACPI table data structure */

    for (TableData = AcpiDmTableData; TableData->Signature; TableData++)
    {
        /* If table has a template, create the template file */

        if (TableData->Template)
        {
            Status = DtCreateOneTemplate (TableData->Signature,
                        TableData);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
        }
    }

    /*
     * Create the "special ACPI tables:
     * 1) DSDT/SSDT are AML tables, not data tables
     * 2) FACS and RSDP have non-standard headers
     */
    Status = DtCreateOneTemplate (ACPI_SIG_DSDT, NULL);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = DtCreateOneTemplate (ACPI_SIG_SSDT, NULL);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = DtCreateOneTemplate (ACPI_SIG_FACS, NULL);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = DtCreateOneTemplate (ACPI_RSDP_NAME, NULL);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    DtCreateOneTemplate
 *
 * PARAMETERS:  Signature           - ACPI signature, NULL terminated.
 *              TableData           - Entry in ACPI table data structure.
 *                                    NULL if a special ACPI table.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create one template source file for the requested ACPI table.
 *
 ******************************************************************************/

static ACPI_STATUS
DtCreateOneTemplate (
    char                    *Signature,
    ACPI_DMTABLE_DATA       *TableData)
{
    char                    *DisasmFilename;
    FILE                    *File;
    ACPI_STATUS             Status = AE_OK;


    /* New file will have a .asl suffix */

    DisasmFilename = FlGenerateFilename (
        Signature, FILE_SUFFIX_ASL_CODE);
    if (!DisasmFilename)
    {
        fprintf (stderr, "Could not generate output filename\n");
        return (AE_ERROR);
    }

    /* Probably should prompt to overwrite the file */

    AcpiUtStrlwr (DisasmFilename);
    File = fopen (DisasmFilename, "w+");
    if (!File)
    {
        fprintf (stderr, "Could not open output file %s\n", DisasmFilename);
        return (AE_ERROR);
    }

    /* Emit the common file header */

    AcpiOsRedirectOutput (File);

    AcpiOsPrintf ("/*\n");
    AcpiOsPrintf (ACPI_COMMON_HEADER ("iASL Compiler/Disassembler", " * "));

    AcpiOsPrintf (" * Template for [%4.4s] ACPI Table\n",
        Signature);

    /* Dump the actual ACPI table */

    if (TableData)
    {
        /* Normal case, tables that appear in AcpiDmTableData */

        if (Gbl_VerboseTemplates)
        {
            AcpiOsPrintf (" * Format: [HexOffset DecimalOffset ByteLength]"
                "  FieldName : HexFieldValue\n */\n\n");
        }
        else
        {
            AcpiOsPrintf (" * Format: [ByteLength]"
                "  FieldName : HexFieldValue\n */\n\n");
        }

        AcpiDmDumpDataTable (ACPI_CAST_PTR (ACPI_TABLE_HEADER,
            TableData->Template));
    }
    else
    {
        /* Special ACPI tables - DSDT, SSDT, FACS, RSDP */

        AcpiOsPrintf (" */\n\n");
        if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_DSDT))
        {
            fwrite (TemplateDsdt, sizeof (TemplateDsdt) -1, 1, File);
        }
        else if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_SSDT))
        {
            fwrite (TemplateSsdt, sizeof (TemplateSsdt) -1, 1, File);
        }
        else if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_FACS))
        {
            AcpiDmDumpDataTable (ACPI_CAST_PTR (ACPI_TABLE_HEADER,
                TemplateFacs));
        }
        else if (ACPI_COMPARE_NAME (Signature, ACPI_RSDP_NAME))
        {
            AcpiDmDumpDataTable (ACPI_CAST_PTR (ACPI_TABLE_HEADER,
                TemplateRsdp));
        }
        else
        {
            fprintf (stderr,
                "%4.4s, Unrecognized ACPI table signature\n", Signature);
            return (AE_ERROR);
        }
    }

    fprintf (stderr,
        "Created ACPI table template for [%4.4s], written to \"%s\"\n",
        Signature, DisasmFilename);

    fclose (File);
    AcpiOsRedirectOutput (stdout);
    ACPI_FREE (DisasmFilename);
    return (Status);
}
