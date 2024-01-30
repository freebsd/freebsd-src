/******************************************************************************
 *
 * Module Name: axutils - Utility functions for acpixtract tool.
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

#include "acpixtract.h"


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

void
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
 * FUNCTION:    AxIsFileAscii
 *
 * PARAMETERS:  Handle              - To open input file
 *
 * RETURN:      TRUE if file is entirely ASCII and printable
 *
 * DESCRIPTION: Verify that the input file is entirely ASCII.
 *
 ******************************************************************************/

BOOLEAN
AxIsFileAscii (
    FILE                    *Handle)
{
    UINT8                   Byte;
    UINT32                  Offset = 0;


    /* Read the entire file */

    fseek (Handle, 0, SEEK_SET);
    while (fread (&Byte, 1, 1, Handle) == 1)
    {
        /*
         * Ignore null characters. Some acpidump-type utilities insert
         * a few of these, probably a bug in the utility. As long as these
         * characters are in lines that are tossed (non-data), they
         * won't cause a problem.
         */
        if (!Byte)
        {
            continue;
        }

        /* Check for an ASCII character */

        if (!ACPI_IS_ASCII (Byte))
        {
            printf ("Found non-ascii char: %2.2X at file offset %u (0x%X)\n",
                Byte, Offset, Offset);
            if (!Gbl_ForceExtraction)
            {
                goto ErrorExit;
            }
        }

        /* Ensure character is either printable or a "space" char */

        else if (!isprint (Byte) && !isspace (Byte))
        {
            printf ("Found non-printable char: %2.2X at file offset %u (0x%X)\n",
                Byte, Offset, Offset);
            if (!Gbl_ForceExtraction)
            {
                goto ErrorExit;
            }
        }

        Offset++;
    }

    /* File is OK (100% ASCII) */

    fseek (Handle, 0, SEEK_SET);
    return (TRUE);

ErrorExit:

    printf ("File appears to be binary "
        "(contains non-text or non-ascii characters)\n");
    fseek (Handle, 0, SEEK_SET);
    return (FALSE);
}


/******************************************************************************
 *
 * FUNCTION:    AxIsEmptyLine
 *
 * PARAMETERS:  Buffer              - Line from input file
 *
 * RETURN:      TRUE if line is empty (zero or more blanks only)
 *
 * DESCRIPTION: Determine if an input line is empty.
 *
 ******************************************************************************/

BOOLEAN
AxIsEmptyLine (
    char                    *Buffer)
{

    /* Skip all spaces */

    while (*Buffer == ' ')
    {
        Buffer++;
    }

    /* Line is empty when a Unix or DOS-style line terminator is found. */

    if ((*Buffer == '\r') || (*Buffer == '\n'))
    {
        return (1);
    }

    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    AxIsHexDataLine
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status. 1 if the table header is valid, 0 otherwise.
 *
 * DESCRIPTION: Check for a valid line of hex data of the form:
 *
 *  00a0: 0c 00 00 00 03 00 00 00 43 48 41 35 0c 00 00 00  ........CHA5....
 *
 ******************************************************************************/

BOOLEAN
AxIsHexDataLine (
    void)
{

    if (AxIsEmptyLine (Gbl_LineBuffer) ||
        (Gbl_LineBuffer[0] != ' '))
    {
        return (FALSE);
    }

    if (!strstr (Gbl_LineBuffer, ": "))
    {
        /* Not valid data line */

        return (FALSE);
    }

    return (TRUE);
}


/******************************************************************************
 *
 * FUNCTION:    AxIsDataBlockHeader
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status. 1 if the table header is valid, 0 otherwise.
 *
 * DESCRIPTION: Check if the ACPI table identifier in the input acpidump text
 *              file is valid (of the form: <sig> @ <addr>).
 *
 ******************************************************************************/

BOOLEAN
AxIsDataBlockHeader (
    void)
{

    /* Ignore lines that are too short to be header lines */

    if (strlen (Gbl_LineBuffer) < AX_MIN_BLOCK_HEADER_LENGTH)
    {
        return (FALSE);
    }

    /* Ignore empty lines and lines that start with a space */

    if (AxIsEmptyLine (Gbl_LineBuffer) ||
        (Gbl_LineBuffer[0] == ' '))
    {
        return (FALSE);
    }

    /*
     * Ignore lines that are not headers of the form <sig> @ <addr>.
     * Basically, just look for the '@' symbol, surrounded by spaces.
     *
     * Examples of headers that must be supported:
     *
     * DSDT @ 0x737e4000
     * XSDT @ 0x737f2fff
     * RSD PTR @ 0xf6cd0
     * SSDT @ (nil)
     */
    if (!AX_IS_TABLE_BLOCK_HEADER)
    {
        return (FALSE);
    }

    AxNormalizeSignature (Gbl_LineBuffer);
    return (TRUE);
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

void
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
 * FUNCTION:    AxConvertToBinary
 *
 * PARAMETERS:  InputLine           - One line from the input acpidump file
 *              OutputData          - Where the converted data is returned
 *
 * RETURN:      The number of bytes actually converted
 *
 * DESCRIPTION: Convert one line of ascii text binary (up to 16 bytes)
 *
 * NOTE: Assumes the input data line has been validated to be of the form:
 *
 *  0010: 48 53 57 55 4c 54 2d 52 01 00 00 00 49 4e 54 4c  HSWULT-R....INTL
 *
 ******************************************************************************/

int
AxConvertToBinary (
    char                    *InputLine,
    unsigned char           *OutputData)
{
    char                    *ColonDelimiter;
    int                     BytesConverted;
    int                     Converted[16];
    int                     i;


    /*
     * Terminate input line immediately after the data. Otherwise, the
     * second line below will not scan correctly.
     *
     * This handles varying lengths for the offset: line prefix. This support
     * for tables larger than 1mb was added 05/2018.
     *
     *    00b0: 03 00 00 00 43 48 41 36 0c 00 00 00 03 00 00 00  ....CHA6........
     *    00c0: 43 48 41 37                                      CHA7
     *
     *    012340b0: 03 00 00 00 43 48 41 36 0c 00 00 00 03 00 00 00  ....CHA6........
     *    012340c0: 43 48 41 37                                      CHA7
     */
    ColonDelimiter = strchr (InputLine, ':');
    ColonDelimiter [AX_HEX_DATA_LENGTH] = 0;

    /*
     * Convert one line of table data, of the form:
     * <offset>: <up to 16 bytes of hex data> <ASCII representation> <newline>
     *
     * Example:
     * 02C0: 5F 53 42 5F 4C 4E 4B 44 00 12 13 04 0C FF FF 08  _SB_LNKD........
     */
    BytesConverted = sscanf (InputLine,
        "%*s %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X",
        &Converted[0],  &Converted[1],  &Converted[2],  &Converted[3],
        &Converted[4],  &Converted[5],  &Converted[6],  &Converted[7],
        &Converted[8],  &Converted[9],  &Converted[10], &Converted[11],
        &Converted[12], &Converted[13], &Converted[14], &Converted[15]);

    if (BytesConverted == EOF)
    {
        printf ("EOF while converting ASCII line to binary\n");
        return (-1);
    }

    /*
     * Pack converted data into a byte array.
     * Note: BytesConverted == 0 is acceptable.
     */
    for (i = 0; i < BytesConverted; i++)
    {
        OutputData[i] = (unsigned char) Converted[i];
    }

    return (BytesConverted);
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

unsigned int
AxCountTableInstances (
    char                    *InputPathname,
    char                    *Signature)
{
    FILE                    *InputFile;
    unsigned int            Instances = 0;


    InputFile = fopen (InputPathname, "r");
    if (!InputFile)
    {
        printf ("Could not open input file %s\n", InputPathname);
        return (0);
    }

    /* Count the number of instances of this signature */

    while (fgets (Gbl_InstanceBuffer, AX_LINE_BUFFER_SIZE, InputFile))
    {
        /* Ignore empty lines and lines that start with a space */

        if (AxIsEmptyLine (Gbl_InstanceBuffer) ||
            (Gbl_InstanceBuffer[0] == ' '))
        {
            continue;
        }

        AxNormalizeSignature (Gbl_InstanceBuffer);
        if (ACPI_COMPARE_NAMESEG (Gbl_InstanceBuffer, Signature))
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

unsigned int
AxGetNextInstance (
    char                    *InputPathname,
    char                    *Signature)
{
    AX_TABLE_INFO           *Info;


    Info = Gbl_TableListHead;
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
            printf ("Could not allocate memory (0x%X bytes)\n",
                (unsigned int) sizeof (AX_TABLE_INFO));
            exit (0);
        }

        Info->Signature = *(UINT32 *) Signature;
        Info->Instances = AxCountTableInstances (InputPathname, Signature);
        Info->NextInstance = 1;
        Info->Next = Gbl_TableListHead;
        Gbl_TableListHead = Info;
    }

    if (Info->Instances > 1)
    {
        return (Info->NextInstance++);
    }

    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    AxConvertAndWrite
 *
 * PARAMETERS:  OutputFile              - Where to write the binary data
 *              ThisSignature           - Signature of current ACPI table
 *
 * RETURN:      Length of the converted line
 *
 * DESCRIPTION: Convert one line of input hex ascii text to binary, and write
 *              the binary data to the table output file.
 *
 * NOTE: Assumes the input data line has been validated to be of the form:
 *
 *  0010: 48 53 57 55 4c 54 2d 52 01 00 00 00 49 4e 54 4c  HSWULT-R....INTL
 *
 ******************************************************************************/

int
AxConvertAndWrite (
    FILE                    *OutputFile,
    char                    *ThisSignature)
{
    int                     BytesWritten;
    int                     BytesConverted;


    /* Convert one line of ascii hex data to binary */

    BytesConverted = AxConvertToBinary (Gbl_LineBuffer, Gbl_BinaryData);
    if (BytesConverted == EOF)
    {
        return (EOF);
    }
    if (!BytesConverted)
    {
        return (0);
    }

    /* Write the binary data */

    BytesWritten = fwrite (Gbl_BinaryData, 1, BytesConverted, OutputFile);
    if (BytesWritten != BytesConverted)
    {
        printf ("Error while writing file %s\n", Gbl_OutputFilename);
        return (-1);
    }

    return (BytesWritten);
}


/******************************************************************************
 *
 * FUNCTION:    AxDumpTableHeader
 *
 * PARAMETERS:  Header          - A binary ACPI table header
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display the contents of a standard ACPI table header
 *
 ******************************************************************************/

void
AxDumpTableHeader (
    unsigned char           *Header)
{
    ACPI_TABLE_HEADER       *TableHeader = (ACPI_TABLE_HEADER *) (void *) Header;
    ACPI_TABLE_RSDP         *Rsdp = (ACPI_TABLE_RSDP *) (void *) Header;
    ACPI_TABLE_FACS         *Facs = (ACPI_TABLE_FACS *) (void *) Header;


    /* RSDP has an oddball signature and header */

    if (!strncmp (TableHeader->Signature, "RSD PTR ", 8))
    {
        AxCheckAscii ((char *) &Header[9], 6);

        Gbl_TableCount++;
        printf (" %.2u) %5.4s  0x%8.8X  0x%2.2X  \"%6.6s\"\n",
            Gbl_TableCount, "RSDP", Rsdp->Length, Rsdp->Revision, Rsdp->OemId);
        return;
    }

    if (!AcpiUtValidNameseg (TableHeader->Signature))
    {
        return;
    }

    /* Signature and Table length */

    Gbl_TableCount++;
    printf (" %.2u) %5.4s  0x%8.8X", Gbl_TableCount, TableHeader->Signature,
        TableHeader->Length);

    /* FACS has only signature and length */

    if (ACPI_COMPARE_NAMESEG (TableHeader->Signature, "FACS"))
    {
        printf ("  0x%2.2X\n", Facs->Version);
        return;
    }

    /* OEM IDs and Compiler IDs */

    AxCheckAscii (TableHeader->OemId, 6);
    AxCheckAscii (TableHeader->OemTableId, 8);
    AxCheckAscii (TableHeader->AslCompilerId, 4);

    printf (
        "  0x%2.2X  \"%6.6s\"  \"%8.8s\"  0x%8.8X"
        "  \"%4.4s\"   0x%8.8X\n",
        TableHeader->Revision, TableHeader->OemId,
        TableHeader->OemTableId, TableHeader->OemRevision,
        TableHeader->AslCompilerId, TableHeader->AslCompilerRevision);
}


#ifdef _AX_FUTURE_ENHANCEMENTS

/* Possible enhancement to validate table lengths */

void
AxCheckTableLengths (
    UINT32                  ByteCount,
    UINT32                  AmlByteCount)
{

    if (AmlByteCount == 0)
    {
        return;
    }

    if (ByteCount == 0)
    {
        return;
    }

    if ((ByteCount < sizeof (ACPI_TABLE_HEADER)) &&
        (ByteCount >= ACPI_NAMESEG_SIZE))
    {
        printf ("  : (Table too short for an ACPI table)");
    }

    else if (ByteCount != AmlByteCount)
    {
        printf ("  : (Hex data length mismatch with AML length 0x%X)",
            AmlByteCount);
    }

    printf ("\n");
}
#endif
