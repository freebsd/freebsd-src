
/******************************************************************************
 *
 * Module Name: abcompare - compare AML files
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

#include "acpibin.h"


FILE                        *File1;
FILE                        *File2;
ACPI_TABLE_HEADER           Header1;
ACPI_TABLE_HEADER           Header2;
struct stat                 Gbl_StatBuf;

#define BUFFER_SIZE         256
char                        Buffer[BUFFER_SIZE];


/* Local prototypes */

static BOOLEAN
AbValidateHeader (
    ACPI_TABLE_HEADER       *Header);

static UINT8
AcpiTbSumTable (
    void                    *Buffer,
    UINT32                  Length);

static char *
AbGetFile (
    char                    *Filename,
    UINT32                  *FileSize);

static void
AbPrintHeaderInfo (
    ACPI_TABLE_HEADER       *Header);

ACPI_PHYSICAL_ADDRESS
AeLocalGetRootPointer (
    void);


/*******************************************************************************
 *
 * FUNCTION:    UtHexCharToValue
 *
 * PARAMETERS:  HexChar         - Hex character in Ascii
 *
 * RETURN:      The binary value of the hex character
 *
 * DESCRIPTION: Perform ascii-to-hex translation
 *
 ******************************************************************************/

static UINT8
UtHexCharToValue (
    int                     HexChar,
    UINT8                   *OutBinary)
{

    if (HexChar >= 0x30 && HexChar <= 0x39)
    {
        *OutBinary = (UINT8) (HexChar - 0x30);
        return (1);
    }

    else if (HexChar >= 0x41 && HexChar <= 0x46)
    {
        *OutBinary = (UINT8) (HexChar - 0x37);
        return (1);
    }

    else if (HexChar >= 0x61 && HexChar <= 0x66)
    {
        *OutBinary = (UINT8) (HexChar - 0x57);
        return (1);
    }
    return (0);
}

static UINT8
AbHexByteToBinary (
    char                    *HexString,
    char                    *OutBinary)
{
    UINT8                   Local1;
    UINT8                   Local2;


    if (!UtHexCharToValue (HexString[0], &Local1))
    {
        return (0);
    }
    if (!UtHexCharToValue (HexString[1], &Local2))
    {
        return (0);
    }

    *OutBinary = (UINT8) ((Local1 << 4) | Local2);
    return (2);

}


/******************************************************************************
 *
 * FUNCTION:    AbValidateHeader
 *
 * DESCRIPTION: Check for valid ACPI table header
 *
 ******************************************************************************/

static BOOLEAN
AbValidateHeader (
    ACPI_TABLE_HEADER       *Header)
{

    if (!AcpiUtValidAcpiName (* (UINT32 *) &Header->Signature))
    {
        printf ("Header signature is invalid\n");
        return FALSE;
    }

    return TRUE;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbSumTable
 *
 * PARAMETERS:  Buffer              - Buffer to checksum
 *              Length              - Size of the buffer
 *
 * RETURNS      8 bit checksum of buffer
 *
 * DESCRIPTION: Computes an 8 bit checksum of the buffer(length) and returns it.
 *
 ******************************************************************************/

static UINT8
AcpiTbSumTable (
    void                    *Buffer,
    UINT32                  Length)
{
    const UINT8             *limit;
    const UINT8             *rover;
    UINT8                   sum = 0;


    if (Buffer && Length)
    {
        /* Buffer and Length are valid */

        limit = (UINT8 *) Buffer + Length;

        for (rover = Buffer; rover < limit; rover++)
        {
            sum = (UINT8) (sum + *rover);
        }
    }
    return (sum);
}


/*******************************************************************************
 *
 * FUNCTION:    AbPrintHeaderInfo
 *
 * PARAMETERS:  Header              - An ACPI table header
 *
 * RETURNS      None.
 *
 * DESCRIPTION: Format and display header contents.
 *
 ******************************************************************************/

static void
AbPrintHeaderInfo (
    ACPI_TABLE_HEADER       *Header)
{

    /* Display header information */

    printf ("Signature         : %4.4s\n",    Header->Signature);
    printf ("Length            : %8.8X\n",    Header->Length);
    printf ("Revision          : %2.2X\n",    Header->Revision);
    printf ("Checksum          : %2.2X\n",    Header->Checksum);
    printf ("OEM ID            : %6.6s\n",    Header->OemId);
    printf ("OEM Table ID      : %8.8s\n",    Header->OemTableId);
    printf ("OEM Revision      : %8.8X\n",    Header->OemRevision);
    printf ("ASL Compiler ID   : %4.4s\n",    Header->AslCompilerId);
    printf ("Compiler Revision : %8.8X\n",    Header->AslCompilerRevision);
    printf ("\n");
}


/******************************************************************************
 *
 * FUNCTION:    AbDisplayHeader
 *
 * DESCRIPTION: Display an ACPI table header
 *
 ******************************************************************************/

void
AbDisplayHeader (
    char                    *File1Path)
{
    UINT32                  Actual1;


    File1 = fopen (File1Path, "rb");
    if (!File1)
    {
        printf ("Could not open file %s\n", File1Path);
        return;
    }

    Actual1 = fread (&Header1, 1, sizeof (ACPI_TABLE_HEADER), File1);
    if (Actual1 < sizeof (ACPI_TABLE_HEADER))
    {
        printf ("File %s does not contain an ACPI table header\n", File1Path);
        return;
    }

    if (!AbValidateHeader (&Header1))
    {
        return;
    }

    AbPrintHeaderInfo (&Header1);
}


/******************************************************************************
 *
 * FUNCTION:    AbComputeChecksum
 *
 * DESCRIPTION: Compute proper checksum for an ACPI table
 *
 ******************************************************************************/

void
AbComputeChecksum (
    char                    *File1Path)
{
    UINT32                  Actual1;
    ACPI_TABLE_HEADER       *Table;
    UINT8                   Checksum;


    File1 = fopen (File1Path, "rb");
    if (!File1)
    {
        printf ("Could not open file %s\n", File1Path);
        return;
    }

    Actual1 = fread (&Header1, 1, sizeof (ACPI_TABLE_HEADER), File1);
    if (Actual1 < sizeof (ACPI_TABLE_HEADER))
    {
        printf ("File %s does not contain an ACPI table header\n", File1Path);
        return;
    }

    if (!AbValidateHeader (&Header1))
    {
        return;
    }

    if (!Gbl_TerseMode)
    {
        AbPrintHeaderInfo (&Header1);
    }

    /* Allocate a buffer to hold the entire table */

    Table = AcpiOsAllocate (Header1.Length);
    if (!Table)
    {
        printf ("could not allocate\n");
        return;
    }

    /* Read the entire table, including header */

    fseek (File1, 0, SEEK_SET);
    Actual1 = fread (Table, 1, Header1.Length, File1);
    if (Actual1 < Header1.Length)
    {
        printf ("could not read table\n");
        return;
    }

    /* Compute the checksum for the table */

    Table->Checksum = 0;

    Checksum = (UINT8) (0 - AcpiTbSumTable (Table, Table->Length));
    printf ("Computed checksum: 0x%X\n\n", Checksum);

    if (Header1.Checksum == Checksum)
    {
        printf ("Checksum ok in AML file, not updating\n");
        return;
    }

    /* Open the target file for writing, to update checksum */

    fclose (File1);
    File1 = fopen (File1Path, "r+b");
    if (!File1)
    {
        printf ("Could not open file %s for writing\n", File1Path);
        return;
    }

    /* Set the checksum, write the new header */

    Header1.Checksum = Checksum;

    Actual1 = fwrite (&Header1, 1, sizeof (ACPI_TABLE_HEADER), File1);
    if (Actual1 < sizeof (ACPI_TABLE_HEADER))
    {
        printf ("Could not write updated table header\n");
        return;
    }

    printf ("Wrote new checksum\n");
    return;
}


/******************************************************************************
 *
 * FUNCTION:    AbCompareAmlFiles
 *
 * DESCRIPTION: Compare two AML files
 *
 ******************************************************************************/

int
AbCompareAmlFiles (
    char                    *File1Path,
    char                    *File2Path)
{
    UINT32                  Actual1;
    UINT32                  Actual2;
    UINT32                  Offset;
    UINT8                   Char1;
    UINT8                   Char2;
    UINT8                   Mismatches = 0;
    BOOLEAN                 HeaderMismatch = FALSE;


    File1 = fopen (File1Path, "rb");
    if (!File1)
    {
        printf ("Could not open file %s\n", File1Path);
        return -1;
    }

    File2 = fopen (File2Path, "rb");
    if (!File2)
    {
        printf ("Could not open file %s\n", File2Path);
        return -1;
    }

    /* Read the ACPI header from each file */

    Actual1 = fread (&Header1, 1, sizeof (ACPI_TABLE_HEADER), File1);
    if (Actual1 < sizeof (ACPI_TABLE_HEADER))
    {
        printf ("File %s does not contain an ACPI table header\n", File1Path);
        return -1;
    }

    Actual2 = fread (&Header2, 1, sizeof (ACPI_TABLE_HEADER), File2);
    if (Actual2 < sizeof (ACPI_TABLE_HEADER))
    {
        printf ("File %s does not contain an ACPI table header\n", File2Path);
        return -1;
    }

    if ((!AbValidateHeader (&Header1)) ||
        (!AbValidateHeader (&Header2)))
    {
        return -1;
    }

    /* Table signatures must match */

    if (*((UINT32 *) Header1.Signature) != *((UINT32 *) Header2.Signature))
    {
        printf ("Table signatures do not match\n");
        return -1;
    }

    if (!Gbl_TerseMode)
    {
        /* Display header information */

        AbPrintHeaderInfo (&Header1);
        AbPrintHeaderInfo (&Header2);
    }

    if (memcmp (Header1.Signature, Header2.Signature, sizeof (ACPI_TABLE_HEADER)))
    {
        printf ("Headers do not match exactly\n");
        HeaderMismatch = TRUE;
    }

    /* Do the byte-by-byte compare */

    Actual1 = fread (&Char1, 1, 1, File1);
    Actual2 = fread (&Char2, 1, 1, File2);
    Offset = sizeof (ACPI_TABLE_HEADER);

    while (Actual1 && Actual2)
    {
        if (Char1 != Char2)
        {
            printf ("Error - Byte mismatch at offset %8.8X: 0x%2.2X 0x%2.2X\n",
                Offset, Char1, Char2);
            Mismatches++;
            if (Mismatches > 100)
            {
                printf ("100 Mismatches: Too many mismatches\n");
                return -1;
            }
        }

        Offset++;
        Actual1 = fread (&Char1, 1, 1, File1);
        Actual2 = fread (&Char2, 1, 1, File2);
    }

    if (Actual1)
    {
        printf ("Error - file %s is longer than file %s\n", File1Path, File2Path);
        Mismatches++;
    }
    else if (Actual2)
    {
        printf ("Error - file %s is shorter than file %s\n", File1Path, File2Path);
        Mismatches++;
    }
    else if (!Mismatches)
    {
        if (HeaderMismatch)
        {
            printf ("Files compare exactly after header\n");
        }
        else
        {
            printf ("Files compare exactly\n");
        }
    }

    printf ("%u Mismatches found\n", Mismatches);
    return 0;
}


/******************************************************************************
 *
 * FUNCTION:    AsGetFile
 *
 * DESCRIPTION: Open a file and read it entirely into a new buffer
 *
 ******************************************************************************/

static char *
AbGetFile (
    char                    *Filename,
    UINT32                  *FileSize)
{
    int                     FileHandle;
    UINT32                  Size;
    char                    *Buffer = NULL;


    /* Binary mode does not alter CR/LF pairs */

    FileHandle = open (Filename, O_BINARY | O_RDONLY);
    if (!FileHandle)
    {
        printf ("Could not open %s\n", Filename);
        return NULL;
    }

    /* Need file size to allocate a buffer */

    if (fstat (FileHandle, &Gbl_StatBuf))
    {
        printf ("Could not get file status for %s\n", Filename);
        goto ErrorExit;
    }

    /* Allocate a buffer for the entire file */

    Size = Gbl_StatBuf.st_size;
    Buffer = calloc (Size, 1);
    if (!Buffer)
    {
        printf ("Could not allocate buffer of size %u\n", Size);
        goto ErrorExit;
    }

    /* Read the entire file */

    Size = read (FileHandle, Buffer, Size);
    if (Size == -1)
    {
        printf ("Could not read the input file %s\n", Filename);
        free (Buffer);
        Buffer = NULL;
        goto ErrorExit;
    }

    *FileSize = Size;

ErrorExit:
    close (FileHandle);

    return (Buffer);
}


/******************************************************************************
 *
 * FUNCTION:    AbDumpAmlFile
 *
 * DESCRIPTION: Dump a binary AML file to a text file
 *
 ******************************************************************************/

int
AbDumpAmlFile (
    char                    *File1Path,
    char                    *File2Path)
{
    char                    *FileBuffer;
    UINT32                  FileSize = 0;
    FILE                    *FileOutHandle;


    /* Get the entire AML file, validate header */

    FileBuffer = AbGetFile (File1Path, &FileSize);
    printf ("File %s contains 0x%X bytes\n\n", File1Path, FileSize);

    FileOutHandle = fopen (File2Path, "wb");
    if (!FileOutHandle)
    {
        printf ("Could not open %s\n", File2Path);
        return -1;
    }

    if (!AbValidateHeader ((ACPI_TABLE_HEADER *) FileBuffer))
    {
        return -1;
    }

    /* Convert binary AML to text, using common dump buffer routine */

    AcpiGbl_DebugFile = FileOutHandle;
    AcpiGbl_DbOutputFlags = ACPI_DB_REDIRECTABLE_OUTPUT;

    AcpiOsPrintf ("%4.4s\n", ((ACPI_TABLE_HEADER *) FileBuffer)->Signature);
    AcpiDbgLevel = ACPI_UINT32_MAX;
    AcpiUtDumpBuffer ((UINT8 *) FileBuffer, FileSize,
        DB_BYTE_DISPLAY, ACPI_UINT32_MAX);

    return 0;
}


/******************************************************************************
 *
 * FUNCTION:    AbExtractAmlFile
 *
 * DESCRIPTION: Extract a binary AML file from a text file (as produced by the
 *              DumpAmlFile procedure or the "acpidmp" table utility.
 *
 ******************************************************************************/

int
AbExtractAmlFile (
    char                    *TableSig,
    char                    *File1Path,
    char                    *File2Path)
{
    char                    *Table;
    char                    Value;
    UINT32                  i;
    FILE                    *FileHandle;
    FILE                    *FileOutHandle;
    UINT32                  Count = 0;
    int                     Scanned;


    /* Open in/out files. input is in text mode, output is in binary mode */

    FileHandle = fopen (File1Path, "rt");
    if (!FileHandle)
    {
        printf ("Could not open %s\n", File1Path);
        return -1;
    }

    FileOutHandle = fopen (File2Path, "w+b");
    if (!FileOutHandle)
    {
        printf ("Could not open %s\n", File2Path);
        return -1;
    }

    /* Force input table sig to uppercase */

    AcpiUtStrupr (TableSig);


    /* TBD: examine input for ASCII */


    /* We have an ascii file, grab one line at a time */

    while (fgets (Buffer, BUFFER_SIZE, FileHandle))
    {
        /* The 4-char ACPI signature appears at the beginning of a line */

        if (!strncmp (Buffer, TableSig, 4))
        {
            printf ("Found table [%4.4s]\n", TableSig);

            /*
             * Eat all lines in the table, of the form:
             *   <offset>: <16 bytes of hex data, separated by spaces> <ASCII representation> <newline>
             *
             * Example:
             *
             *   02C0: 5F 53 42 5F 4C 4E 4B 44 00 12 13 04 0C FF FF 08  _SB_LNKD........
             *
             */
            while (fgets (Buffer, BUFFER_SIZE, FileHandle))
            {
                /* Get past the offset, terminated by a colon */

                Table = strchr (Buffer, ':');
                if (!Table)
                {
                    /* No colon, all done */
                    goto Exit;
                }

                Table += 2; /* Eat the colon + space */

                for (i = 0; i < 16; i++)
                {
                    Scanned = AbHexByteToBinary (Table, &Value);
                    if (!Scanned)
                    {
                        goto Exit;
                    }

                    Table += 3; /* Go past this hex byte and space */

                    /* Write the converted (binary) byte */

                    fwrite (&Value, 1, 1, FileOutHandle);
                    Count++;
                }
            }

            /* No more lines, EOF, all done */

            goto Exit;
        }
    }

    /* Searched entire file, no match to table signature */

    printf ("Could not match table signature\n");
    fclose (FileHandle);
    return -1;

Exit:
    printf ("%u (0x%X) bytes written to %s\n", Count, Count, File2Path);
    fclose (FileHandle);
    fclose (FileOutHandle);
    return 0;
}


/******************************************************************************
 *
 * FUNCTION:    Stubs
 *
 * DESCRIPTION: For linkage
 *
 ******************************************************************************/

ACPI_PHYSICAL_ADDRESS
AeLocalGetRootPointer (
    void)
{
    return AE_OK;
}

ACPI_THREAD_ID
AcpiOsGetThreadId (
    void)
{
    return (0xFFFF);
}

ACPI_STATUS
AcpiOsExecute (
    ACPI_EXECUTE_TYPE       Type,
    ACPI_OSD_EXEC_CALLBACK  Function,
    void                    *Context)
{
    return (AE_SUPPORT);
}
