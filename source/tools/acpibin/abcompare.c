/******************************************************************************
 *
 * Module Name: abcompare - compare AML files
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2017, Intel Corp.
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


ACPI_TABLE_HEADER           Header1;
ACPI_TABLE_HEADER           Header2;

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

static void
AbPrintHeadersInfo (
    ACPI_TABLE_HEADER       *Header,
    ACPI_TABLE_HEADER       *Header2);


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

    if (!AcpiUtValidNameseg (Header->Signature))
    {
        printf ("Header signature is invalid\n");
        return (FALSE);
    }

    return (TRUE);
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
    const UINT8             *Limit;
    const UINT8             *Rover;
    UINT8                   Sum = 0;


    if (Buffer && Length)
    {
        /* Buffer and Length are valid */

        Limit = (UINT8 *) Buffer + Length;

        for (Rover = Buffer; Rover < Limit; Rover++)
        {
            Sum = (UINT8) (Sum + *Rover);
        }
    }

    return (Sum);
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
    printf ("OEM ID            : %.6s\n",     Header->OemId);
    printf ("OEM Table ID      : %.8s\n",     Header->OemTableId);
    printf ("OEM Revision      : %8.8X\n",    Header->OemRevision);
    printf ("ASL Compiler ID   : %.4s\n",     Header->AslCompilerId);
    printf ("Compiler Revision : %8.8X\n",    Header->AslCompilerRevision);
    printf ("\n");
}

static void
AbPrintHeadersInfo (
    ACPI_TABLE_HEADER       *Header,
    ACPI_TABLE_HEADER       *Header2)
{

    /* Display header information for both headers */

    printf ("Signature          %8.4s : %4.4s\n",    Header->Signature, Header2->Signature);
    printf ("Length             %8.8X : %8.8X\n",    Header->Length, Header2->Length);
    printf ("Revision           %8.2X : %2.2X\n",    Header->Revision, Header2->Revision);
    printf ("Checksum           %8.2X : %2.2X\n",    Header->Checksum, Header2->Checksum);
    printf ("OEM ID             %8.6s : %.6s\n",     Header->OemId, Header2->OemId);
    printf ("OEM Table ID       %8.8s : %.8s\n",     Header->OemTableId, Header2->OemTableId);
    printf ("OEM Revision       %8.8X : %8.8X\n",    Header->OemRevision, Header2->OemRevision);
    printf ("ASL Compiler ID    %8.4s : %.4s\n",     Header->AslCompilerId, Header2->AslCompilerId);
    printf ("Compiler Revision  %8.8X : %8.8X\n",    Header->AslCompilerRevision, Header2->AslCompilerRevision);
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
    char                    *FilePath)
{
    UINT32                  Actual;
    FILE                    *File;


    File = fopen (FilePath, "rb");
    if (!File)
    {
        printf ("Could not open file %s\n", FilePath);
        return;
    }

    Actual = fread (&Header1, 1, sizeof (ACPI_TABLE_HEADER), File);
    fclose (File);

    if (Actual != sizeof (ACPI_TABLE_HEADER))
    {
        printf ("File %s does not contain a valid ACPI table header\n", FilePath);
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
    char                    *FilePath)
{
    UINT32                  Actual;
    ACPI_TABLE_HEADER       *Table;
    UINT8                   Checksum;
    FILE                    *File;


    File = fopen (FilePath, "rb");
    if (!File)
    {
        printf ("Could not open file %s\n", FilePath);
        return;
    }

    Actual = fread (&Header1, 1, sizeof (ACPI_TABLE_HEADER), File);
    if (Actual < sizeof (ACPI_TABLE_HEADER))
    {
        printf ("File %s does not contain a valid ACPI table header\n", FilePath);
        goto Exit1;
    }

    if (!AbValidateHeader (&Header1))
    {
        goto Exit1;
    }

    if (!Gbl_TerseMode)
    {
        AbPrintHeaderInfo (&Header1);
    }

    /* Allocate a buffer to hold the entire table */

    Table = AcpiOsAllocate (Header1.Length);
    if (!Table)
    {
        printf ("Could not allocate buffer for table\n");
        goto Exit1;
    }

    /* Read the entire table, including header */

    fseek (File, 0, SEEK_SET);
    Actual = fread (Table, 1, Header1.Length, File);
    if (Actual != Header1.Length)
    {
        printf ("Could not read table, length %u\n", Header1.Length);
        goto Exit2;
    }

    /* Compute the checksum for the table */

    Table->Checksum = 0;

    Checksum = (UINT8) (0 - AcpiTbSumTable (Table, Table->Length));
    printf ("Computed checksum: 0x%X\n\n", Checksum);

    if (Header1.Checksum == Checksum)
    {
        printf ("Checksum OK in AML file, not updating\n");
        goto Exit2;
    }

    /* Open the target file for writing, to update checksum */

    fclose (File);
    File = fopen (FilePath, "r+b");
    if (!File)
    {
        printf ("Could not open file %s for writing\n", FilePath);
        goto Exit2;
    }

    /* Set the checksum, write the new header */

    Header1.Checksum = Checksum;

    Actual = fwrite (&Header1, 1, sizeof (ACPI_TABLE_HEADER), File);
    if (Actual != sizeof (ACPI_TABLE_HEADER))
    {
        printf ("Could not write updated table header\n");
        goto Exit2;
    }

    printf ("Wrote new checksum\n");

Exit2:
    AcpiOsFree (Table);

Exit1:
    if (File)
    {
        fclose (File);
    }
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
    FILE                    *File1;
    FILE                    *File2;
    int                     Status = -1;


    File1 = fopen (File1Path, "rb");
    if (!File1)
    {
        printf ("Could not open file %s\n", File1Path);
        return (-1);
    }

    File2 = fopen (File2Path, "rb");
    if (!File2)
    {
        printf ("Could not open file %s\n", File2Path);
        goto Exit1;
    }

    /* Read the ACPI header from each file */

    Actual1 = fread (&Header1, 1, sizeof (ACPI_TABLE_HEADER), File1);
    if (Actual1 != sizeof (ACPI_TABLE_HEADER))
    {
        printf ("File %s does not contain an ACPI table header\n", File1Path);
        goto Exit2;
    }

    Actual2 = fread (&Header2, 1, sizeof (ACPI_TABLE_HEADER), File2);
    if (Actual2 != sizeof (ACPI_TABLE_HEADER))
    {
        printf ("File %s does not contain an ACPI table header\n", File2Path);
        goto Exit2;
    }

    if ((!AbValidateHeader (&Header1)) ||
        (!AbValidateHeader (&Header2)))
    {
        goto Exit2;
    }

    /* Table signatures must match */

    if (*((UINT32 *) Header1.Signature) != *((UINT32 *) Header2.Signature))
    {
        printf ("Table signatures do not match\n");
        goto Exit2;
    }

    if (!Gbl_TerseMode)
    {
        /* Display header information */

        printf ("Comparing %s to %s\n", File1Path, File2Path);
        AbPrintHeadersInfo (&Header1, &Header2);
    }

    if (memcmp (&Header1, &Header2, sizeof (ACPI_TABLE_HEADER)))
    {
        printf ("Headers do not match exactly\n");
        HeaderMismatch = TRUE;
    }

    /* Do the byte-by-byte compare */

    printf ("Compare offset: %u\n", AbGbl_CompareOffset);
    if (AbGbl_CompareOffset)
    {
        fseek (File2, AbGbl_CompareOffset, SEEK_CUR);
    }

    Actual1 = fread (&Char1, 1, 1, File1);
    Actual2 = fread (&Char2, 1, 1, File2);
    Offset = sizeof (ACPI_TABLE_HEADER);

    while ((Actual1 == 1) && (Actual2 == 1))
    {
        if (Char1 != Char2)
        {
            printf ("Error - Byte mismatch at offset %8.4X: 0x%2.2X 0x%2.2X\n",
                Offset, Char1, Char2);
            Mismatches++;
            if ((Mismatches > 100) && (!AbGbl_DisplayAllMiscompares))
            {
                printf ("100 Mismatches: Too many mismatches\n");
                goto Exit2;
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
    if (Mismatches == 0)
    {
        Status = 0;
    }

Exit2:
    fclose (File2);

Exit1:
    fclose (File1);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AbGetFile
 *
 * DESCRIPTION: Open a file and read it entirely into a new buffer
 *
 ******************************************************************************/

static char *
AbGetFile (
    char                    *Filename,
    UINT32                  *FileSize)
{
    FILE                    *File;
    UINT32                  Size;
    char                    *Buffer = NULL;
    size_t                  Actual;


    /* Binary mode does not alter CR/LF pairs */

    File = fopen (Filename, "rb");
    if (!File)
    {
        printf ("Could not open file %s\n", Filename);
        return (NULL);
    }

    /* Need file size to allocate a buffer */

    Size = CmGetFileSize (File);
    if (Size == ACPI_UINT32_MAX)
    {
        printf ("Could not get file size (seek) for %s\n", Filename);
        goto ErrorExit;
    }

    /* Allocate a buffer for the entire file */

    Buffer = calloc (Size, 1);
    if (!Buffer)
    {
        printf ("Could not allocate buffer of size %u\n", Size);
        goto ErrorExit;
    }

    /* Read the entire file */

    Actual = fread (Buffer, 1, Size, File);
    if (Actual != Size)
    {
        printf ("Could not read the input file %s\n", Filename);
        free (Buffer);
        Buffer = NULL;
        goto ErrorExit;
    }

    *FileSize = Size;

ErrorExit:
    fclose (File);
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
    FILE                    *FileOutHandle;
    UINT32                  FileSize = 0;
    int                     Status = -1;


    /* Get the entire AML file, validate header */

    FileBuffer = AbGetFile (File1Path, &FileSize);
    if (!FileBuffer)
    {
        return (-1);
    }

    printf ("Input file:  %s contains %u (0x%X) bytes\n",
        File1Path, FileSize, FileSize);

    FileOutHandle = fopen (File2Path, "wb");
    if (!FileOutHandle)
    {
        printf ("Could not open file %s\n", File2Path);
        goto Exit1;
    }

    if (!AbValidateHeader ((ACPI_TABLE_HEADER *) FileBuffer))
    {
        goto Exit2;
    }

    /* Convert binary AML to text, using common dump buffer routine */

    AcpiGbl_DebugFile = FileOutHandle;
    AcpiGbl_DbOutputFlags = ACPI_DB_REDIRECTABLE_OUTPUT;

    AcpiOsPrintf ("%4.4s @ 0x%8.8X\n",
        ((ACPI_TABLE_HEADER *) FileBuffer)->Signature, 0);

    AcpiUtDumpBuffer ((UINT8 *) FileBuffer, FileSize, DB_BYTE_DISPLAY, 0);

    /* Summary for the output file */

    FileSize = CmGetFileSize (FileOutHandle);
    printf ("Output file: %s contains %u (0x%X) bytes\n\n",
        File2Path, FileSize, FileSize);

    Status = 0;

Exit2:
    fclose (FileOutHandle);

Exit1:
    free (FileBuffer);
    return (Status);
}
