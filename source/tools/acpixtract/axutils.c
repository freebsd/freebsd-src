/******************************************************************************
 *
 * Module Name: axutils - Utility functions for acpixtract tool.
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

int
AxIsEmptyLine (
    char                    *Buffer)
{

    /* Skip all spaces */

    while (*Buffer == ' ')
    {
        Buffer++;
    }

    /* If end-of-line, this line is empty */

    if (*Buffer == '\n')
    {
        return (1);
    }

    return (0);
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

size_t
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

size_t
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
        if (!fgets (Gbl_HeaderBuffer, AX_LINE_BUFFER_SIZE, InputFile))
        {
            return (TotalConverted);
        }

        BytesConverted = AxConvertLine (Gbl_HeaderBuffer, OutputData);
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

unsigned int
AxCountTableInstances (
    char                    *InputPathname,
    char                    *Signature)
{
    FILE                    *InputFile;
    unsigned int            Instances = 0;


    InputFile = fopen (InputPathname, "rt");
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
        if (ACPI_COMPARE_NAME (Gbl_InstanceBuffer, Signature))
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

int
AxIsDataBlockHeader (
    void)
{

    /* Ignore lines that are too short to be header lines */

    if (strlen (Gbl_LineBuffer) < AX_MIN_BLOCK_HEADER_LENGTH)
    {
        return (0);
    }

    /* Ignore empty lines and lines that start with a space */

    if (AxIsEmptyLine (Gbl_LineBuffer) ||
        (Gbl_LineBuffer[0] == ' '))
    {
        return (0);
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
    if (!strstr (Gbl_LineBuffer, " @ "))
    {
        return (0);
    }

    AxNormalizeSignature (Gbl_LineBuffer);
    return (1);
}


/******************************************************************************
 *
 * FUNCTION:    AxProcessOneTextLine
 *
 * PARAMETERS:  OutputFile              - Where to write the binary data
 *              ThisSignature           - Signature of current ACPI table
 *              ThisTableBytesWritten   - Total count of data written
 *
 * RETURN:      Length of the converted line
 *
 * DESCRIPTION: Convert one line of input hex ascii text to binary, and write
 *              the binary data to the table output file.
 *
 ******************************************************************************/

long
AxProcessOneTextLine (
    FILE                    *OutputFile,
    char                    *ThisSignature,
    unsigned int            ThisTableBytesWritten)
{
    size_t                  BytesWritten;
    size_t                  BytesConverted;


    /* Check for the end of this table data block */

    if (AxIsEmptyLine (Gbl_LineBuffer) ||
        (Gbl_LineBuffer[0] != ' '))
    {
        printf (AX_TABLE_INFO_FORMAT,
            ThisSignature, ThisTableBytesWritten, Gbl_OutputFilename);
        return (0);
    }

    /* Convert one line of ascii hex data to binary */

    BytesConverted = AxConvertLine (Gbl_LineBuffer, Gbl_BinaryData);

    /* Write the binary data */

    BytesWritten = fwrite (Gbl_BinaryData, 1, BytesConverted, OutputFile);
    if (BytesWritten != BytesConverted)
    {
        printf ("Error while writing file %s\n", Gbl_OutputFilename);
        return (-1);
    }

    return (BytesWritten);
}
