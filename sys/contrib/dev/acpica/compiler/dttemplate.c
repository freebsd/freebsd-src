/******************************************************************************
 *
 * Module Name: dttemplate - ACPI table template generation
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
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
DtCreateOneTemplateFile (
    char                    *Signature,
    UINT32                  TableCount);

static ACPI_STATUS
DtCreateOneTemplate (
    char                    *Signature,
    UINT32                  TableCount,
    const ACPI_DMTABLE_DATA *TableData);

static ACPI_STATUS
DtCreateAllTemplates (
    void);

static int
DtEmitDefinitionBlock (
    FILE                    *File,
    char                    *Filename,
    char                    *Signature,
    UINT32                  Instance);


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
        ACPI_COMPARE_NAME (Signature, ACPI_SIG_OSDT) ||
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
 * PARAMETERS:  argv                - Standard command line arguments
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create one or more template files.
 *
 ******************************************************************************/

ACPI_STATUS
DtCreateTemplates (
    char                    **argv)
{
    char                    *Signature;
    char                    *End;
    unsigned long           TableCount;
    ACPI_STATUS             Status = AE_OK;


    AslInitializeGlobals ();

    Status = AdInitialize ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /*
     * Special cases for DSDT, ALL, and '*'
     */

    /* Default (no signature option) is DSDT */

    if (AcpiGbl_Optind < 3)
    {
        Status = DtCreateOneTemplateFile (ACPI_SIG_DSDT, 0);
        goto Exit;
    }

    AcpiGbl_Optind--;
    Signature = argv[AcpiGbl_Optind];
    AcpiUtStrupr (Signature);

    /*
     * Multiple SSDT support (-T <ssdt count>)
     */
    TableCount = strtoul (Signature, &End, 0);
    if (Signature != End)
    {
        /* The count is used for table ID and method name - max is 254(+1) */

        if (TableCount > 254)
        {
            fprintf (stderr, "%u SSDTs requested, maximum is 254\n",
                (unsigned int) TableCount);

            Status = AE_LIMIT;
            goto Exit;
        }

        Status = DtCreateOneTemplateFile (ACPI_SIG_DSDT, TableCount);
        goto Exit;
    }

    if (!strcmp (Signature, "ALL"))
    {
        /* Create all available/known templates */

        Status = DtCreateAllTemplates ();
        goto Exit;
    }

    /*
     * Normal case: Create template for each signature
     */
    while (argv[AcpiGbl_Optind])
    {
        Signature = argv[AcpiGbl_Optind];
        AcpiUtStrupr (Signature);

        Status = DtCreateOneTemplateFile (Signature, 0);
        if (ACPI_FAILURE (Status))
        {
            goto Exit;
        }

        AcpiGbl_Optind++;
    }


Exit:
    /* Shutdown ACPICA subsystem */

    (void) AcpiTerminate ();
    CmDeleteCaches ();
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    DtCreateOneTemplateFile
 *
 * PARAMETERS:  Signature           - ACPI table signature
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create one template file of the requested signature.
 *
 ******************************************************************************/

static ACPI_STATUS
DtCreateOneTemplateFile (
    char                    *Signature,
    UINT32                  TableCount)
{
    const ACPI_DMTABLE_DATA *TableData;
    ACPI_STATUS             Status;


    /*
     * Validate signature and get the template data:
     *  1) Signature must be 4 characters
     *  2) Signature must be a recognized ACPI table
     *  3) There must be a template associated with the signature
     */
    if (strlen (Signature) != ACPI_NAME_SIZE)
    {
        fprintf (stderr,
            "%s: Invalid ACPI table signature "
            "(length must be 4 characters)\n", Signature);
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

    /* TableData will point to the template */

    TableData = AcpiDmGetTableData (Signature);
    if (TableData)
    {
        if (!TableData->Template)
        {
            fprintf (stderr, "%4.4s: No template available\n", Signature);
            return (AE_ERROR);
        }
    }
    else if (!AcpiUtIsSpecialTable (Signature))
    {
        fprintf (stderr,
            "%4.4s: Unrecognized ACPI table signature\n", Signature);
        return (AE_ERROR);
    }

    Status = DtCreateOneTemplate (Signature, TableCount, TableData);
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
    const ACPI_DMTABLE_DATA *TableData;
    ACPI_STATUS             Status;


    fprintf (stderr, "Creating all supported Template files\n");

    /* Walk entire ACPI table data structure */

    for (TableData = AcpiDmTableData; TableData->Signature; TableData++)
    {
        /* If table has a template, create the template file */

        if (TableData->Template)
        {
            Status = DtCreateOneTemplate (TableData->Signature,
                0, TableData);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
        }
    }

    /*
     * Create the special ACPI tables:
     * 1) DSDT/SSDT are AML tables, not data tables
     * 2) FACS and RSDP have non-standard headers
     */
    Status = DtCreateOneTemplate (ACPI_SIG_DSDT, 0, NULL);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = DtCreateOneTemplate (ACPI_SIG_SSDT, 0, NULL);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = DtCreateOneTemplate (ACPI_SIG_OSDT, 0, NULL);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = DtCreateOneTemplate (ACPI_SIG_FACS, 0, NULL);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = DtCreateOneTemplate (ACPI_RSDP_NAME, 0, NULL);
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
 *              TableCount          - Used for SSDTs in same file as DSDT
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
    UINT32                  TableCount,
    const ACPI_DMTABLE_DATA  *TableData)
{
    char                    *DisasmFilename;
    FILE                    *File;
    ACPI_STATUS             Status = AE_OK;
    int                     Actual;
    UINT32                  i;


    /* New file will have a .asl suffix */

    DisasmFilename = FlGenerateFilename (
        Signature, FILE_SUFFIX_ASL_CODE);
    if (!DisasmFilename)
    {
        fprintf (stderr, "Could not generate output filename\n");
        return (AE_ERROR);
    }

    AcpiUtStrlwr (DisasmFilename);
    if (!UtQueryForOverwrite (DisasmFilename))
    {
        return (AE_ERROR);
    }

    File = fopen (DisasmFilename, "w+");
    if (!File)
    {
        fprintf (stderr, "Could not open output file %s\n",
            DisasmFilename);
        return (AE_ERROR);
    }

    /* Emit the common file header */

    AcpiOsRedirectOutput (File);

    AcpiOsPrintf ("/*\n");
    AcpiOsPrintf (ACPI_COMMON_HEADER ("iASL Compiler/Disassembler", " * "));

    if (TableCount == 0)
    {
        AcpiOsPrintf (" * Template for [%4.4s] ACPI Table",
            Signature);
    }
    else
    {
        AcpiOsPrintf (" * Template for [%4.4s] and %u [SSDT] ACPI Tables",
            Signature, TableCount);
    }

    /* Dump the actual ACPI table */

    if (TableData)
    {
        /* Normal case, tables that appear in AcpiDmTableData */

        AcpiOsPrintf (" (static data table)\n");

        if (Gbl_VerboseTemplates)
        {
            AcpiOsPrintf (" * Format: [HexOffset DecimalOffset ByteLength]"
                "  FieldName : HexFieldValue\n */\n\n");
        }
        else
        {
            AcpiOsPrintf (" * Format: [ByteLength]"
                "  FieldName : HexFieldValue\n */\n");
        }

        AcpiDmDumpDataTable (ACPI_CAST_PTR (ACPI_TABLE_HEADER,
            TableData->Template));
    }
    else
    {
        /* Special ACPI tables - DSDT, SSDT, OSDT, FACS, RSDP */

        AcpiOsPrintf (" (AML byte code table)\n");
        AcpiOsPrintf (" */\n");

        if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_DSDT))
        {
            Actual = DtEmitDefinitionBlock (
                File, DisasmFilename, ACPI_SIG_DSDT, 1);
            if (Actual < 0)
            {
                Status = AE_ERROR;
                goto Cleanup;
            }

            /* Emit any requested SSDTs into the same file */

            for (i = 1; i <= TableCount; i++)
            {
                Actual = DtEmitDefinitionBlock (
                    File, DisasmFilename, ACPI_SIG_SSDT, i + 1);
                if (Actual < 0)
                {
                    Status = AE_ERROR;
                    goto Cleanup;
                }
            }
        }
        else if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_SSDT))
        {
            Actual = DtEmitDefinitionBlock (
                File, DisasmFilename, ACPI_SIG_SSDT, 1);
            if (Actual < 0)
            {
                Status = AE_ERROR;
                goto Cleanup;
            }
        }
        else if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_OSDT))
        {
            Actual = DtEmitDefinitionBlock (
                File, DisasmFilename, ACPI_SIG_OSDT, 1);
            if (Actual < 0)
            {
                Status = AE_ERROR;
                goto Cleanup;
            }
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
            Status = AE_ERROR;
            goto Cleanup;
        }
    }

    if (TableCount == 0)
    {
        fprintf (stderr,
            "Created ACPI table template for [%4.4s], "
            "written to \"%s\"\n",
            Signature, DisasmFilename);
    }
    else
    {
        fprintf (stderr,
            "Created ACPI table templates for [%4.4s] "
            "and %u [SSDT], written to \"%s\"\n",
            Signature, TableCount, DisasmFilename);
    }

Cleanup:
    fclose (File);
    AcpiOsRedirectOutput (stdout);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    DtEmitDefinitionBlock
 *
 * PARAMETERS:  File                - An open file for the block
 *              Filename            - Filename for same, for error msg(s)
 *              Signature           - ACPI signature for the block
 *              Instance            - Used for multiple SSDTs in the same file
 *
 * RETURN:      Status from fprintf
 *
 * DESCRIPTION: Emit the raw ASL for a complete Definition Block (DSDT or SSDT)
 *
 * Note: The AMLFileName parameter for DefinitionBlock is left as a NULL
 * string. This allows the compiler to create the output AML filename from
 * the input filename.
 *
 ******************************************************************************/

static int
DtEmitDefinitionBlock (
    FILE                    *File,
    char                    *Filename,
    char                    *Signature,
    UINT32                  Instance)
{
    int                     Status;


    Status = fprintf (File,
        "DefinitionBlock (\"\", \"%4.4s\", 2, \"Intel\", \"_%4.4s_%.2X\", 0x00000001)\n"
        "{\n"
        "    Method (%2.2s%.2X)\n"
        "    {\n"
        "    }\n"
        "}\n\n",
        Signature, Signature, Instance, Signature, Instance);

    if (Status < 0)
    {
        fprintf (stderr,
            "Could not write %4.4s to output file %s\n",
            Signature, Filename);
    }

    return (Status);
}
