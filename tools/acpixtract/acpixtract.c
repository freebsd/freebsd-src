
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/* Note: This is a 32-bit program only */

#define VERSION             0x20100107
#define FIND_HEADER         0
#define EXTRACT_DATA        1
#define BUFFER_SIZE         256


/* Local prototypes */

static void
CheckAscii (
    char                    *Name,
    int                     Count);

static void
NormalizeSignature (
    char                    *Signature);

static unsigned int
GetNextInstance (
    char                    *InputPathname,
    char                    *Signature);

static int
ExtractTables (
    char                    *InputPathname,
    char                    *Signature,
    unsigned int            MinimumInstances);

static size_t
GetTableHeader (
    FILE                    *InputFile,
    unsigned char           *OutputData);

static unsigned int
CountTableInstances (
    char                    *InputPathname,
    char                    *Signature);

static int
ListTables (
    char                    *InputPathname);

static size_t
ConvertLine (
    char                    *InputLine,
    unsigned char           *OutputData);

static void
DisplayUsage (
    void);


typedef struct acpi_table_header
{
    char                    Signature[4];
    int                     Length;
    unsigned char           Revision;
    unsigned char           Checksum;
    char                    OemId[6];
    char                    OemTableId[8];
    int                     OemRevision;
    char                    AslCompilerId[4];
    int                     AslCompilerRevision;

} ACPI_TABLE_HEADER;

struct TableInfo
{
    unsigned int            Signature;
    unsigned int            Instances;
    unsigned int            NextInstance;
    struct TableInfo        *Next;
};

static struct TableInfo     *ListHead = NULL;
static char                 Filename[16];
static unsigned char        Data[16];


/******************************************************************************
 *
 * FUNCTION:    DisplayUsage
 *
 * DESCRIPTION: Usage message
 *
 ******************************************************************************/

static void
DisplayUsage (
    void)
{

    printf ("Usage: acpixtract [option] <InputFile>\n");
    printf ("\nExtract binary ACPI tables from text acpidump output\n");
    printf ("Default invocation extracts all DSDTs and SSDTs\n");
    printf ("Version %8.8X\n\n", VERSION);
    printf ("Options:\n");
    printf (" -a                    Extract all tables, not just DSDT/SSDT\n");
    printf (" -l                    List table summaries, do not extract\n");
    printf (" -s<Signature>         Extract all tables named <Signature>\n");
    printf ("\n");
}


/*******************************************************************************
 *
 * FUNCTION:    CheckAscii
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
CheckAscii (
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
 * FUNCTION:    NormalizeSignature
 *
 * PARAMETERS:  Name                - Ascii string containing an ACPI signature
 *
 * RETURN:      None
 *
 * DESCRIPTION: Change "RSD PTR" to "RSDP"
 *
 ******************************************************************************/

static void
NormalizeSignature (
    char                    *Signature)
{

    if (!strncmp (Signature, "RSD ", 4))
    {
        Signature[3] = 'P';
    }
}


/******************************************************************************
 *
 * FUNCTION:    ConvertLine
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
ConvertLine (
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
 * FUNCTION:    GetTableHeader
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
GetTableHeader (
    FILE                    *InputFile,
    unsigned char           *OutputData)
{
    size_t                  BytesConverted;
    size_t                  TotalConverted = 0;
    char                    Buffer[BUFFER_SIZE];
    int                     i;


    /* Get the full 36 byte header, requires 3 lines */

    for (i = 0; i < 3; i++)
    {
        if (!fgets (Buffer, BUFFER_SIZE, InputFile))
        {
            return (TotalConverted);
        }

        BytesConverted = ConvertLine (Buffer, OutputData);
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
 * FUNCTION:    CountTableInstances
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
CountTableInstances (
    char                    *InputPathname,
    char                    *Signature)
{
    char                    Buffer[BUFFER_SIZE];
    FILE                    *InputFile;
    unsigned int            Instances = 0;


    InputFile = fopen (InputPathname, "rt");
    if (!InputFile)
    {
        printf ("Could not open %s\n", InputPathname);
        return (0);
    }

    /* Count the number of instances of this signature */

    while (fgets (Buffer, BUFFER_SIZE, InputFile))
    {
        /* Ignore empty lines and lines that start with a space */

        if ((Buffer[0] == ' ') ||
            (Buffer[0] == '\n'))
        {
            continue;
        }

        NormalizeSignature (Buffer);
        if (!strncmp (Buffer, Signature, 4))
        {
            Instances++;
        }
    }

    fclose (InputFile);
    return (Instances);
}


/******************************************************************************
 *
 * FUNCTION:    GetNextInstance
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
GetNextInstance (
    char                    *InputPathname,
    char                    *Signature)
{
    struct TableInfo        *Info;


    Info = ListHead;
    while (Info)
    {
        if (*(unsigned int *) Signature == Info->Signature)
        {
            break;
        }

        Info = Info->Next;
    }

    if (!Info)
    {
        /* Signature not found, create new table info block */

        Info = malloc (sizeof (struct TableInfo));
        if (!Info)
        {
            printf ("Could not allocate memory\n");
            exit (0);
        }

        Info->Signature = *(unsigned int *) Signature;
        Info->Instances = CountTableInstances (InputPathname, Signature);
        Info->NextInstance = 1;
        Info->Next = ListHead;
        ListHead = Info;
    }

    if (Info->Instances > 1)
    {
        return (Info->NextInstance++);
    }

    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    ExtractTables
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

static int
ExtractTables (
    char                    *InputPathname,
    char                    *Signature,
    unsigned int            MinimumInstances)
{
    char                    Buffer[BUFFER_SIZE];
    FILE                    *InputFile;
    FILE                    *OutputFile = NULL;
    size_t                  BytesWritten;
    size_t                  TotalBytesWritten = 0;
    size_t                  BytesConverted;
    unsigned int            State = FIND_HEADER;
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

        NormalizeSignature (Signature);

        Instances = CountTableInstances (InputPathname, Signature);
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

    while (fgets (Buffer, BUFFER_SIZE, InputFile))
    {
        switch (State)
        {
        case FIND_HEADER:

            /* Ignore empty lines and lines that start with a space */

            if ((Buffer[0] == ' ') ||
                (Buffer[0] == '\n'))
            {
                continue;
            }

            NormalizeSignature (Buffer);
            strncpy (ThisSignature, Buffer, 4);

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
            ThisInstance = GetNextInstance (InputPathname, ThisSignature);

            /* Build an output filename and create/open the output file */

            if (ThisInstance > 0)
            {
                sprintf (Filename, "%4.4s%u.dat", ThisSignature, ThisInstance);
            }
            else
            {
                sprintf (Filename, "%4.4s.dat", ThisSignature);
            }

            OutputFile = fopen (Filename, "w+b");
            if (!OutputFile)
            {
                printf ("Could not open %s\n", Filename);
                Status = -1;
                goto CleanupAndExit;
            }

            State = EXTRACT_DATA;
            TotalBytesWritten = 0;
            FoundTable = 1;
            continue;

        case EXTRACT_DATA:

            /* Empty line or non-data line terminates the data */

            if ((Buffer[0] == '\n') ||
                (Buffer[0] != ' '))
            {
                fclose (OutputFile);
                OutputFile = NULL;
                State = FIND_HEADER;

                printf ("Acpi table [%4.4s] - %u bytes written to %s\n",
                    ThisSignature, (unsigned int) TotalBytesWritten, Filename);
                continue;
            }

            /* Convert the ascii data (one line of text) to binary */

            BytesConverted = ConvertLine (Buffer, Data);

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
        if (State == EXTRACT_DATA)
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
 * FUNCTION:    ListTables
 *
 * PARAMETERS:  InputPathname       - Filename for acpidump file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Display info for all ACPI tables found in input. Does not
 *              perform an actual extraction of the tables.
 *
 ******************************************************************************/

static int
ListTables (
    char                    *InputPathname)
{
    FILE                    *InputFile;
    char                    Buffer[BUFFER_SIZE];
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

    while (fgets (Buffer, BUFFER_SIZE, InputFile))
    {
        /* Ignore empty lines and lines that start with a space */

        if ((Buffer[0] == ' ') ||
            (Buffer[0] == '\n'))
        {
            continue;
        }

        /* Get the 36 byte header and display the fields */

        HeaderSize = GetTableHeader (InputFile, Header);
        if (HeaderSize < 16)
        {
            continue;
        }

        /* RSDP has an oddball signature and header */

        if (!strncmp (TableHeader->Signature, "RSD PTR ", 8))
        {
            CheckAscii ((char *) &Header[9], 6);
            printf ("%8.4s                   \"%6.6s\"\n", "RSDP", &Header[9]);
            TableCount++;
            continue;
        }

        /* Minimum size for table with standard header */

        if (HeaderSize < 36)
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

        CheckAscii (TableHeader->OemId, 6);
        CheckAscii (TableHeader->OemTableId, 8);
        CheckAscii (TableHeader->AslCompilerId, 4);

        printf ("     %2.2X    \"%6.6s\"  \"%8.8s\"    %8.8X    \"%4.4s\"     %8.8X\n",
            TableHeader->Revision, TableHeader->OemId,
            TableHeader->OemTableId, TableHeader->OemRevision,
            TableHeader->AslCompilerId, TableHeader->AslCompilerRevision);
    }

    printf ("\nFound %u ACPI tables [%8.8X]\n", TableCount, VERSION);
    fclose (InputFile);
    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    main
 *
 * DESCRIPTION: C main function
 *
 ******************************************************************************/

int
main (
    int                     argc,
    char                    *argv[])
{
    int                     Status;


    if (argc < 2)
    {
        DisplayUsage ();
        return (0);
    }

    if (argv[1][0] == '-')
    {
        if (argc < 3)
        {
            DisplayUsage ();
            return (0);
        }

        switch (argv[1][1])
        {
        case 'a':

            /* Extract all tables found */

            return (ExtractTables (argv[2], NULL, 0));

        case 'l':

            /* List tables only, do not extract */

            return (ListTables (argv[2]));

        case 's':

            /* Extract only tables with this signature */

            return (ExtractTables (argv[2], &argv[1][2], 1));

        default:
            DisplayUsage ();
            return (0);
        }
    }

    /*
     * Default output is the DSDT and all SSDTs. One DSDT is required,
     * any SSDTs are optional.
     */
    Status = ExtractTables (argv[1], "DSDT", 1);
    if (Status)
    {
        return (Status);
    }

    Status = ExtractTables (argv[1], "SSDT", 0);
    return (Status);
}


