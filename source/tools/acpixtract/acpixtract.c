/******************************************************************************
 *
 * Module Name: acpixtract - convert ascii ACPI tables to binary
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

    InputFile = fopen (InputPathname, "rt");
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

    InputFile = fopen (InputPathname, "rt");
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

    InputFile = fopen (InputPathname, "rt");
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
