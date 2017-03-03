/******************************************************************************
 *
 * Module Name: acpixtract - convert ascii ACPI tables to binary
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2017, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code. No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

#include "acpixtract.h"


/* Local prototypes */

static BOOLEAN
AxIsFileAscii (
    FILE                    *Handle);


/******************************************************************************
 *
 * FUNCTION:    AxExtractTables
 *
 * PARAMETERS:  InputPathname       - Filename for input acpidump file
 *              Signature           - Requested ACPI signature to extract.
 *                                    NULL means extract ALL tables.
 *              MinimumInstances    - Min instances that are acceptable
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert text ACPI tables to binary
 *
 ******************************************************************************/

int
AxExtractTables (
    char                    *InputPathname,
    char                    *Signature,
    unsigned int            MinimumInstances)
{
    FILE                    *InputFile;
    FILE                    *OutputFile = NULL;
    unsigned int            BytesConverted;
    unsigned int            ThisTableBytesWritten = 0;
    unsigned int            FoundTable = 0;
    unsigned int            Instances = 0;
    unsigned int            ThisInstance;
    char                    ThisSignature[5];
    char                    UpperSignature[5];
    int                     Status = 0;
    unsigned int            State = AX_STATE_FIND_HEADER;


    /* Open input in text mode, output is in binary mode */

    InputFile = fopen (InputPathname, "r");
    if (!InputFile)
    {
        printf ("Could not open input file %s\n", InputPathname);
        return (-1);
    }

    if (!AxIsFileAscii (InputFile))
    {
        fclose (InputFile);
        return (-1);
    }

    if (Signature)
    {
        strncpy (UpperSignature, Signature, 4);
        UpperSignature[4] = 0;
        AcpiUtStrupr (UpperSignature);

        /* Are there enough instances of the table to continue? */

        AxNormalizeSignature (UpperSignature);

        Instances = AxCountTableInstances (InputPathname, UpperSignature);
        if (Instances < MinimumInstances)
        {
            printf ("Table [%s] was not found in %s\n",
                UpperSignature, InputPathname);
            fclose (InputFile);
            return (-1);
        }

        if (Instances == 0)
        {
            fclose (InputFile);
            return (-1);
        }
    }

    /* Convert all instances of the table to binary */

    while (fgets (Gbl_LineBuffer, AX_LINE_BUFFER_SIZE, InputFile))
    {
        switch (State)
        {
        case AX_STATE_FIND_HEADER:

            if (!AxIsDataBlockHeader ())
            {
                continue;
            }

            ACPI_MOVE_NAME (ThisSignature, Gbl_LineBuffer);
            if (Signature)
            {
                /* Ignore signatures that don't match */

                if (!ACPI_COMPARE_NAME (ThisSignature, UpperSignature))
                {
                    continue;
                }
            }

            /*
             * Get the instance number for this signature. Only the
             * SSDT and PSDT tables can have multiple instances.
             */
            ThisInstance = AxGetNextInstance (InputPathname, ThisSignature);

            /* Build an output filename and create/open the output file */

            if (ThisInstance > 0)
            {
                /* Add instance number to the output filename */

                sprintf (Gbl_OutputFilename, "%4.4s%u.dat",
                    ThisSignature, ThisInstance);
            }
            else
            {
                sprintf (Gbl_OutputFilename, "%4.4s.dat",
                    ThisSignature);
            }

            AcpiUtStrlwr (Gbl_OutputFilename);
            OutputFile = fopen (Gbl_OutputFilename, "w+b");
            if (!OutputFile)
            {
                printf ("Could not open output file %s\n",
                    Gbl_OutputFilename);
                fclose (InputFile);
                return (-1);
            }

            /*
             * Toss this block header of the form "<sig> @ <addr>" line
             * and move on to the actual data block
             */
            Gbl_TableCount++;
            FoundTable = 1;
            ThisTableBytesWritten = 0;
            State = AX_STATE_EXTRACT_DATA;
            continue;

        case AX_STATE_EXTRACT_DATA:

            /* Empty line or non-data line terminates the data block */

            BytesConverted = AxProcessOneTextLine (
                OutputFile, ThisSignature, ThisTableBytesWritten);
            switch (BytesConverted)
            {
            case 0:

                State = AX_STATE_FIND_HEADER; /* No more data block lines */
                continue;

            case -1:

                goto CleanupAndExit; /* There was a write error */

            default: /* Normal case, get next line */

                ThisTableBytesWritten += BytesConverted;
                continue;
            }

        default:

            Status = -1;
            goto CleanupAndExit;
        }
    }

    if (!FoundTable)
    {
        printf ("No ACPI tables were found in %s\n", InputPathname);
    }


CleanupAndExit:

    if (State == AX_STATE_EXTRACT_DATA)
    {
        /* Received an input file EOF while extracting data */

        printf (AX_TABLE_INFO_FORMAT,
            ThisSignature, ThisTableBytesWritten, Gbl_OutputFilename);
    }

    if (Gbl_TableCount > 1)
    {
        printf ("\n%u binary ACPI tables extracted\n",
            Gbl_TableCount);
    }

    if (OutputFile)
    {
        fclose (OutputFile);
    }

    fclose (InputFile);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AxExtractToMultiAmlFile
 *
 * PARAMETERS:  InputPathname       - Filename for input acpidump file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert all DSDT/SSDT tables to binary and append them all
 *              into a single output file. Used to simplify the loading of
 *              multiple/many SSDTs into a utility like acpiexec -- instead
 *              of creating many separate output files.
 *
 ******************************************************************************/

int
AxExtractToMultiAmlFile (
    char                    *InputPathname)
{
    FILE                    *InputFile;
    FILE                    *OutputFile;
    int                     Status = 0;
    unsigned int            TotalBytesWritten = 0;
    unsigned int            ThisTableBytesWritten = 0;
    unsigned int             BytesConverted;
    char                    ThisSignature[4];
    unsigned int            State = AX_STATE_FIND_HEADER;


    strcpy (Gbl_OutputFilename, AX_MULTI_TABLE_FILENAME);

    /* Open the input file in text mode */

    InputFile = fopen (InputPathname, "r");
    if (!InputFile)
    {
        printf ("Could not open input file %s\n", InputPathname);
        return (-1);
    }

    if (!AxIsFileAscii (InputFile))
    {
        fclose (InputFile);
        return (-1);
    }

    /* Open the output file in binary mode */

    OutputFile = fopen (Gbl_OutputFilename, "w+b");
    if (!OutputFile)
    {
        printf ("Could not open output file %s\n", Gbl_OutputFilename);
        fclose (InputFile);
        return (-1);
    }

    /* Convert the DSDT and all SSDTs to binary */

    while (fgets (Gbl_LineBuffer, AX_LINE_BUFFER_SIZE, InputFile))
    {
        switch (State)
        {
        case AX_STATE_FIND_HEADER:

            if (!AxIsDataBlockHeader ())
            {
                continue;
            }

            ACPI_MOVE_NAME (ThisSignature, Gbl_LineBuffer);

            /* Only want DSDT and SSDTs */

            if (!ACPI_COMPARE_NAME (ThisSignature, ACPI_SIG_DSDT) &&
                !ACPI_COMPARE_NAME (ThisSignature, ACPI_SIG_SSDT))
            {
                continue;
            }

            /*
             * Toss this block header of the form "<sig> @ <addr>" line
             * and move on to the actual data block
             */
            Gbl_TableCount++;
            ThisTableBytesWritten = 0;
            State = AX_STATE_EXTRACT_DATA;
            continue;

        case AX_STATE_EXTRACT_DATA:

            /* Empty line or non-data line terminates the data block */

            BytesConverted = AxProcessOneTextLine (
                OutputFile, ThisSignature, ThisTableBytesWritten);
            switch (BytesConverted)
            {
            case 0:

                State = AX_STATE_FIND_HEADER; /* No more data block lines */
                continue;

            case -1:

                goto CleanupAndExit; /* There was a write error */

            default: /* Normal case, get next line */

                ThisTableBytesWritten += BytesConverted;
                TotalBytesWritten += BytesConverted;
                continue;
            }

        default:

            Status = -1;
            goto CleanupAndExit;
        }
    }


CleanupAndExit:

    if (State == AX_STATE_EXTRACT_DATA)
    {
        /* Received an input file EOF or error while writing data */

        printf (AX_TABLE_INFO_FORMAT,
            ThisSignature, ThisTableBytesWritten, Gbl_OutputFilename);
    }

    printf ("\n%u binary ACPI tables extracted and written to %s (%u bytes)\n",
        Gbl_TableCount, Gbl_OutputFilename, TotalBytesWritten);

    fclose (InputFile);
    fclose (OutputFile);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AxListTables
 *
 * PARAMETERS:  InputPathname       - Filename for acpidump file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Display info for all ACPI tables found in input. Does not
 *              perform an actual extraction of the tables.
 *
 ******************************************************************************/

int
AxListTables (
    char                    *InputPathname)
{
    FILE                    *InputFile;
    size_t                  HeaderSize;
    unsigned char           Header[48];
    ACPI_TABLE_HEADER       *TableHeader = (ACPI_TABLE_HEADER *) (void *) Header;


    /* Open input in text mode, output is in binary mode */

    InputFile = fopen (InputPathname, "r");
    if (!InputFile)
    {
        printf ("Could not open input file %s\n", InputPathname);
        return (-1);
    }

    if (!AxIsFileAscii (InputFile))
    {
        fclose (InputFile);
        return (-1);
    }

    /* Dump the headers for all tables found in the input file */

    printf ("\nSignature  Length      Revision   OemId    OemTableId"
        "   OemRevision CompilerId CompilerRevision\n\n");

    while (fgets (Gbl_LineBuffer, AX_LINE_BUFFER_SIZE, InputFile))
    {
        /* Ignore empty lines and lines that start with a space */

        if (AxIsEmptyLine (Gbl_LineBuffer) ||
            (Gbl_LineBuffer[0] == ' '))
        {
            continue;
        }

        /* Get the 36 byte header and display the fields */

        HeaderSize = AxGetTableHeader (InputFile, Header);
        if (HeaderSize < 16)
        {
            continue;
        }

        /* RSDP has an oddball signature and header */

        if (!strncmp (TableHeader->Signature, "RSD PTR ", 8))
        {
            AxCheckAscii ((char *) &Header[9], 6);
            printf ("%7.4s                          \"%6.6s\"\n", "RSDP",
                &Header[9]);
            Gbl_TableCount++;
            continue;
        }

        /* Minimum size for table with standard header */

        if (HeaderSize < sizeof (ACPI_TABLE_HEADER))
        {
            continue;
        }

        if (!AcpiUtValidNameseg (TableHeader->Signature))
        {
            continue;
        }

        /* Signature and Table length */

        Gbl_TableCount++;
        printf ("%7.4s   0x%8.8X", TableHeader->Signature,
            TableHeader->Length);

        /* FACS has only signature and length */

        if (ACPI_COMPARE_NAME (TableHeader->Signature, "FACS"))
        {
            printf ("\n");
            continue;
        }

        /* OEM IDs and Compiler IDs */

        AxCheckAscii (TableHeader->OemId, 6);
        AxCheckAscii (TableHeader->OemTableId, 8);
        AxCheckAscii (TableHeader->AslCompilerId, 4);

        printf (
            "     0x%2.2X    \"%6.6s\"  \"%8.8s\"   0x%8.8X"
            "    \"%4.4s\"     0x%8.8X\n",
            TableHeader->Revision, TableHeader->OemId,
            TableHeader->OemTableId, TableHeader->OemRevision,
            TableHeader->AslCompilerId, TableHeader->AslCompilerRevision);
    }

    printf ("\nFound %u ACPI tables in %s\n", Gbl_TableCount, InputPathname);
    fclose (InputFile);
    return (0);
}


/*******************************************************************************
 *
 * FUNCTION:    AxIsFileAscii
 *
 * PARAMETERS:  Handle              - To open input file
 *
 * RETURN:      TRUE if file is entirely ASCII and printable
 *
 * DESCRIPTION: Verify that the input file is entirely ASCII.
 *
 ******************************************************************************/

static BOOLEAN
AxIsFileAscii (
    FILE                    *Handle)
{
    UINT8                   Byte;


    /* Read the entire file */

    while (fread (&Byte, 1, 1, Handle) == 1)
    {
        /* Check for an ASCII character */

        if (!ACPI_IS_ASCII (Byte))
        {
            goto ErrorExit;
        }

        /* Ensure character is either printable or a "space" char */

        else if (!isprint (Byte) && !isspace (Byte))
        {
            goto ErrorExit;
        }
    }

    /* File is OK (100% ASCII) */

    fseek (Handle, 0, SEEK_SET);
    return (TRUE);

ErrorExit:

    printf ("File is binary (contains non-text or non-ascii characters)\n");
    fseek (Handle, 0, SEEK_SET);
    return (FALSE);

}
