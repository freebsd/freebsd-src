/******************************************************************************
 *
 * Module Name: abcompare - compare AML files
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2023, Intel Corp.
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
        if (fseek (File2, AbGbl_CompareOffset, SEEK_CUR))
        {
            printf ("Seek error on file %s\n", File2Path);
            goto Exit2;
        }
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
    char                    *DataBuffer = NULL;
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

    DataBuffer = calloc (Size, 1);
    if (!DataBuffer)
    {
        printf ("Could not allocate buffer of size %u\n", Size);
        goto ErrorExit;
    }

    /* Read the entire file */

    Actual = fread (DataBuffer, 1, Size, File);
    if (Actual != Size)
    {
        printf ("Could not read the input file %s\n", Filename);
        free (DataBuffer);
        DataBuffer = NULL;
        goto ErrorExit;
    }

    *FileSize = Size;

ErrorExit:
    fclose (File);
    return (DataBuffer);
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
