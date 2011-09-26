/******************************************************************************
 *
 * Module Name: acpixtract - convert ascii ACPI tables to binary
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

#include "acpi.h"
#include "accommon.h"
#include "acapps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Local prototypes */

static void
AxStrlwr (
    char                    *String);

static void
AxCheckAscii (
    char                    *Name,
    int                     Count);

static void
AxNormalizeSignature (
    char                    *Signature);

static unsigned int
AxGetNextInstance (
    char                    *InputPathname,
    char                    *Signature);

static size_t
AxGetTableHeader (
    FILE                    *InputFile,
    unsigned char           *OutputData);

static unsigned int
AxCountTableInstances (
    char                    *InputPathname,
    char                    *Signature);

int
AxExtractTables (
    char                    *InputPathname,
    char                    *Signature,
    unsigned int            MinimumInstances);

int
AxListTables (
    char                    *InputPathname);

static size_t
AxConvertLine (
    char                    *InputLine,
    unsigned char           *OutputData);


typedef struct AxTableInfo
{
    UINT32                  Signature;
    unsigned int            Instances;
    unsigned int            NextInstance;
    struct AxTableInfo      *Next;

} AX_TABLE_INFO;

/* Extraction states */

#define AX_STATE_FIND_HEADER        0
#define AX_STATE_EXTRACT_DATA       1

/* Miscellaneous constants */

#define AX_LINE_BUFFER_SIZE         256
#define AX_MIN_TABLE_NAME_LENGTH    6   /* strlen ("DSDT @") */


static AX_TABLE_INFO        *AxTableListHead = NULL;
static char                 Filename[16];
static unsigned char        Data[16];
static char                 LineBuffer[AX_LINE_BUFFER_SIZE];
static char                 HeaderBuffer[AX_LINE_BUFFER_SIZE];
static char                 InstanceBuffer[AX_LINE_BUFFER_SIZE];


/*******************************************************************************
 *
 * FUNCTION:    AxStrlwr
 *
 * PARAMETERS:  String              - Ascii string
 *
 * RETURN:      None
 *
 * DESCRIPTION: String lowercase function.
 *
 ******************************************************************************/

static void
AxStrlwr (
    char                    *String)
{

    while (*String)
    {
        *String = (char) tolower ((int) *String);
        String++;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AxCheckAscii
 *
 * PARAMETERS:  Name                - Ascii string, at least as long as Count
 *              Count               - Number of characters to check
 *
 * RETURN:      None
 *
 * DESCRIPTION: Ensure that the requested number of characters are printable
 *              Ascii characters. Sets non-printable and null chars to <space>.
 *
 ******************************************************************************/

static void
AxCheckAscii (
    char                    *Name,
    int                     Count)
{
    int                     i;


    for (i = 0; i < Count; i++)
    {
        if (!Name[i] || !isprint ((int) Name[i]))
        {
            Name[i] = ' ';
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AxNormalizeSignature
 *
 * PARAMETERS:  Name                - Ascii string containing an ACPI signature
 *
 * RETURN:      None
 *
 * DESCRIPTION: Change "RSD PTR" to "RSDP"
 *
 ******************************************************************************/

static void
AxNormalizeSignature (
    char                    *Signature)
{

    if (!strncmp (Signature, "RSD ", 4))
    {
        Signature[3] = 'P';
    }
}


/******************************************************************************
 *
 * FUNCTION:    AxConvertLine
 *
 * PARAMETERS:  InputLine           - One line from the input acpidump file
 *              OutputData          - Where the converted data is returned
 *
 * RETURN:      The number of bytes actually converted
 *
 * DESCRIPTION: Convert one line of ascii text binary (up to 16 bytes)
 *
 ******************************************************************************/

static size_t
AxConvertLine (
    char                    *InputLine,
    unsigned char           *OutputData)
{
    char                    *End;
    int                     BytesConverted;
    int                     Converted[16];
    int                     i;


    /* Terminate the input line at the end of the actual data (for sscanf) */

    End = strstr (InputLine + 2, "  ");
    if (!End)
    {
        return (0); /* Don't understand the format */
    }
    *End = 0;

    /*
     * Convert one line of table data, of the form:
     * <offset>: <up to 16 bytes of hex data> <ASCII representation> <newline>
     *
     * Example:
     * 02C0: 5F 53 42 5F 4C 4E 4B 44 00 12 13 04 0C FF FF 08  _SB_LNKD........
     */
    BytesConverted = sscanf (InputLine,
        "%*s %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
        &Converted[0],  &Converted[1],  &Converted[2],  &Converted[3],
        &Converted[4],  &Converted[5],  &Converted[6],  &Converted[7],
        &Converted[8],  &Converted[9],  &Converted[10], &Converted[11],
        &Converted[12], &Converted[13], &Converted[14], &Converted[15]);

    /* Pack converted data into a byte array */

    for (i = 0; i < BytesConverted; i++)
    {
        OutputData[i] = (unsigned char) Converted[i];
    }

    return ((size_t) BytesConverted);
}


/******************************************************************************
 *
 * FUNCTION:    AxGetTableHeader
 *
 * PARAMETERS:  InputFile           - Handle for the input acpidump file
 *              OutputData          - Where the table header is returned
 *
 * RETURN:      The actual number of bytes converted
 *
 * DESCRIPTION: Extract and convert an ACPI table header
 *
 ******************************************************************************/

static size_t
AxGetTableHeader (
    FILE                    *InputFile,
    unsigned char           *OutputData)
{
    size_t                  BytesConverted;
    size_t                  TotalConverted = 0;
    int                     i;


    /* Get the full 36 byte ACPI table header, requires 3 input text lines */

    for (i = 0; i < 3; i++)
    {
        if (!fgets (HeaderBuffer, AX_LINE_BUFFER_SIZE, InputFile))
        {
            return (TotalConverted);
        }

        BytesConverted = AxConvertLine (HeaderBuffer, OutputData);
        TotalConverted += BytesConverted;
        OutputData += 16;

        if (BytesConverted != 16)
        {
            return (TotalConverted);
        }
    }

    return (TotalConverted);
}


/******************************************************************************
 *
 * FUNCTION:    AxCountTableInstances
 *
 * PARAMETERS:  InputPathname       - Filename for acpidump file
 *              Signature           - Requested signature to count
 *
 * RETURN:      The number of instances of the signature
 *
 * DESCRIPTION: Count the instances of tables with the given signature within
 *              the input acpidump file.
 *
 ******************************************************************************/

static unsigned int
AxCountTableInstances (
    char                    *InputPathname,
    char                    *Signature)
{
    FILE                    *InputFile;
    unsigned int            Instances = 0;


    InputFile = fopen (InputPathname, "rt");
    if (!InputFile)
    {
        printf ("Could not open %s\n", InputPathname);
        return (0);
    }

    /* Count the number of instances of this signature */

    while (fgets (InstanceBuffer, AX_LINE_BUFFER_SIZE, InputFile))
    {
        /* Ignore empty lines and lines that start with a space */

        if ((InstanceBuffer[0] == ' ') ||
            (InstanceBuffer[0] == '\n'))
        {
            continue;
        }

        AxNormalizeSignature (InstanceBuffer);
        if (!strncmp (InstanceBuffer, Signature, 4))
        {
            Instances++;
        }
    }

    fclose (InputFile);
    return (Instances);
}


/******************************************************************************
 *
 * FUNCTION:    AxGetNextInstance
 *
 * PARAMETERS:  InputPathname       - Filename for acpidump file
 *              Signature           - Requested ACPI signature
 *
 * RETURN:      The next instance number for this signature. Zero if this
 *              is the first instance of this signature.
 *
 * DESCRIPTION: Get the next instance number of the specified table. If this
 *              is the first instance of the table, create a new instance
 *              block. Note: only SSDT and PSDT tables can have multiple
 *              instances.
 *
 ******************************************************************************/

static unsigned int
AxGetNextInstance (
    char                    *InputPathname,
    char                    *Signature)
{
    AX_TABLE_INFO           *Info;


    Info = AxTableListHead;
    while (Info)
    {
        if (*(UINT32 *) Signature == Info->Signature)
        {
            break;
        }

        Info = Info->Next;
    }

    if (!Info)
    {
        /* Signature not found, create new table info block */

        Info = malloc (sizeof (AX_TABLE_INFO));
        if (!Info)
        {
            printf ("Could not allocate memory\n");
            exit (0);
        }

        Info->Signature = *(UINT32 *) Signature;
        Info->Instances = AxCountTableInstances (InputPathname, Signature);
        Info->NextInstance = 1;
        Info->Next = AxTableListHead;
        AxTableListHead = Info;
    }

    if (Info->Instances > 1)
    {
        return (Info->NextInstance++);
    }

    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    AxExtractTables
 *
 * PARAMETERS:  InputPathname       - Filename for acpidump file
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
    size_t                  BytesWritten;
    size_t                  TotalBytesWritten = 0;
    size_t                  BytesConverted;
    unsigned int            State = AX_STATE_FIND_HEADER;
    unsigned int            FoundTable = 0;
    unsigned int            Instances = 0;
    unsigned int            ThisInstance;
    char                    ThisSignature[4];
    int                     Status = 0;


    /* Open input in text mode, output is in binary mode */

    InputFile = fopen (InputPathname, "rt");
    if (!InputFile)
    {
        printf ("Could not open %s\n", InputPathname);
        return (-1);
    }

    if (Signature)
    {
        /* Are there enough instances of the table to continue? */

        AxNormalizeSignature (Signature);

        Instances = AxCountTableInstances (InputPathname, Signature);
        if (Instances < MinimumInstances)
        {
            printf ("Table %s was not found in %s\n", Signature, InputPathname);
            Status = -1;
            goto CleanupAndExit;
        }

        if (Instances == 0)
        {
            goto CleanupAndExit;
        }
    }

    /* Convert all instances of the table to binary */

    while (fgets (LineBuffer, AX_LINE_BUFFER_SIZE, InputFile))
    {
        switch (State)
        {
        case AX_STATE_FIND_HEADER:

            /* Ignore lines that are too short to be header lines */

            if (strlen (LineBuffer) < AX_MIN_TABLE_NAME_LENGTH)
            {
                continue;
            }

            /* Ignore empty lines and lines that start with a space */

            if ((LineBuffer[0] == ' ') ||
                (LineBuffer[0] == '\n'))
            {
                continue;
            }

            /*
             * Ignore lines that are not of the form <sig> @ <addr>.
             * Examples of lines that must be supported:
             *
             * DSDT @ 0x737e4000
             * XSDT @ 0x737f2fff
             * RSD PTR @ 0xf6cd0
             * SSDT @ (nil)
             */
            if (!strstr (LineBuffer, " @ "))
            {
                continue;
            }

            AxNormalizeSignature (LineBuffer);
            strncpy (ThisSignature, LineBuffer, 4);

            if (Signature)
            {
                /* Ignore signatures that don't match */

                if (strncmp (ThisSignature, Signature, 4))
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
                sprintf (Filename, "%4.4s%u.dat", ThisSignature, ThisInstance);
            }
            else
            {
                sprintf (Filename, "%4.4s.dat", ThisSignature);
            }

            AxStrlwr (Filename);
            OutputFile = fopen (Filename, "w+b");
            if (!OutputFile)
            {
                printf ("Could not open %s\n", Filename);
                Status = -1;
                goto CleanupAndExit;
            }

            State = AX_STATE_EXTRACT_DATA;
            TotalBytesWritten = 0;
            FoundTable = 1;
            continue;

        case AX_STATE_EXTRACT_DATA:

            /* Empty line or non-data line terminates the data */

            if ((LineBuffer[0] == '\n') ||
                (LineBuffer[0] != ' '))
            {
                fclose (OutputFile);
                OutputFile = NULL;
                State = AX_STATE_FIND_HEADER;

                printf ("Acpi table [%4.4s] - %u bytes written to %s\n",
                    ThisSignature, (unsigned int) TotalBytesWritten, Filename);
                continue;
            }

            /* Convert the ascii data (one line of text) to binary */

            BytesConverted = AxConvertLine (LineBuffer, Data);

            /* Write the binary data */

            BytesWritten = fwrite (Data, 1, BytesConverted, OutputFile);
            if (BytesWritten != BytesConverted)
            {
                printf ("Write error on %s\n", Filename);
                fclose (OutputFile);
                OutputFile = NULL;
                Status = -1;
                goto CleanupAndExit;
            }

            TotalBytesWritten += BytesConverted;
            continue;

        default:
            Status = -1;
            goto CleanupAndExit;
        }
    }

    if (!FoundTable)
    {
        printf ("Table %s was not found in %s\n", Signature, InputPathname);
    }


CleanupAndExit:

    if (OutputFile)
    {
        fclose (OutputFile);
        if (State == AX_STATE_EXTRACT_DATA)
        {
            /* Received an EOF while extracting data */

            printf ("Acpi table [%4.4s] - %u bytes written to %s\n",
                ThisSignature, (unsigned int) TotalBytesWritten, Filename);
        }
    }

    fclose (InputFile);
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
    int                     TableCount = 0;
    ACPI_TABLE_HEADER       *TableHeader = (ACPI_TABLE_HEADER *) (void *) Header;


    /* Open input in text mode, output is in binary mode */

    InputFile = fopen (InputPathname, "rt");
    if (!InputFile)
    {
        printf ("Could not open %s\n", InputPathname);
        return (-1);
    }

    /* Dump the headers for all tables found in the input file */

    printf ("\nSignature Length Revision  OemId     OemTableId"
            "   OemRevision CompilerId CompilerRevision\n\n");

    while (fgets (LineBuffer, AX_LINE_BUFFER_SIZE, InputFile))
    {
        /* Ignore empty lines and lines that start with a space */

        if ((LineBuffer[0] == ' ') ||
            (LineBuffer[0] == '\n'))
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
            printf ("%8.4s                   \"%6.6s\"\n", "RSDP", &Header[9]);
            TableCount++;
            continue;
        }

        /* Minimum size for table with standard header */

        if (HeaderSize < sizeof (ACPI_TABLE_HEADER))
        {
            continue;
        }

        /* Signature and Table length */

        TableCount++;
        printf ("%8.4s % 7d", TableHeader->Signature, TableHeader->Length);

        /* FACS has only signature and length */

        if (!strncmp (TableHeader->Signature, "FACS", 4))
        {
            printf ("\n");
            continue;
        }

        /* OEM IDs and Compiler IDs */

        AxCheckAscii (TableHeader->OemId, 6);
        AxCheckAscii (TableHeader->OemTableId, 8);
        AxCheckAscii (TableHeader->AslCompilerId, 4);

        printf ("     %2.2X    \"%6.6s\"  \"%8.8s\"    %8.8X    \"%4.4s\"     %8.8X\n",
            TableHeader->Revision, TableHeader->OemId,
            TableHeader->OemTableId, TableHeader->OemRevision,
            TableHeader->AslCompilerId, TableHeader->AslCompilerRevision);
    }

    printf ("\nFound %u ACPI tables\n", TableCount);
    fclose (InputFile);
    return (0);
}
