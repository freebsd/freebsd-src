/******************************************************************************
 *
 * Module Name: apfiles - File-related functions for acpidump utility
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

#include "acpidump.h"


/* Local prototypes */

static int
ApIsExistingFile (
    char                    *Pathname);


/******************************************************************************
 *
 * FUNCTION:    ApIsExistingFile
 *
 * PARAMETERS:  Pathname            - Output filename
 *
 * RETURN:      0 on success
 *
 * DESCRIPTION: Query for file overwrite if it already exists.
 *
 ******************************************************************************/

static int
ApIsExistingFile (
    char                    *Pathname)
{
#if !defined(_GNU_EFI) && !defined(_EDK2_EFI)
    struct stat             StatInfo;
    int                     InChar;


    if (!stat (Pathname, &StatInfo))
    {
        fprintf (stderr, "Target path already exists, overwrite? [y|n] ");

        InChar = fgetc (stdin);
        if (InChar == '\n')
        {
            InChar = fgetc (stdin);
        }

        if (InChar != 'y' && InChar != 'Y')
        {
            return (-1);
        }
    }
#endif

    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    ApOpenOutputFile
 *
 * PARAMETERS:  Pathname            - Output filename
 *
 * RETURN:      Open file handle
 *
 * DESCRIPTION: Open a text output file for acpidump. Checks if file already
 *              exists.
 *
 ******************************************************************************/

int
ApOpenOutputFile (
    char                    *Pathname)
{
    ACPI_FILE               File;


    /* If file exists, prompt for overwrite */

    if (ApIsExistingFile (Pathname) != 0)
    {
        return (-1);
    }

    /* Point stdout to the file */

    File = fopen (Pathname, "w");
    if (!File)
    {
        fprintf (stderr, "Could not open output file: %s\n", Pathname);
        return (-1);
    }

    /* Save the file and path */

    Gbl_OutputFile = File;
    Gbl_OutputFilename = Pathname;
    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    ApWriteToBinaryFile
 *
 * PARAMETERS:  Table               - ACPI table to be written
 *              Instance            - ACPI table instance no. to be written
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write an ACPI table to a binary file. Builds the output
 *              filename from the table signature.
 *
 ******************************************************************************/

int
ApWriteToBinaryFile (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Instance)
{
    char                    Filename[ACPI_NAMESEG_SIZE + 16];
    char                    InstanceStr [16];
    ACPI_FILE               File;
    ACPI_SIZE               Actual;
    UINT32                  TableLength;


    /* Obtain table length */

    TableLength = ApGetTableLength (Table);

    /* Construct lower-case filename from the table local signature */

    if (ACPI_VALIDATE_RSDP_SIG (Table->Signature))
    {
        ACPI_COPY_NAMESEG (Filename, ACPI_RSDP_NAME);
    }
    else
    {
        ACPI_COPY_NAMESEG (Filename, Table->Signature);
    }

    Filename[0] = (char) tolower ((int) Filename[0]);
    Filename[1] = (char) tolower ((int) Filename[1]);
    Filename[2] = (char) tolower ((int) Filename[2]);
    Filename[3] = (char) tolower ((int) Filename[3]);
    Filename[ACPI_NAMESEG_SIZE] = 0;

    /* Handle multiple SSDTs - create different filenames for each */

    if (Instance > 0)
    {
        snprintf (InstanceStr, sizeof (InstanceStr), "%u", Instance);
        strcat (Filename, InstanceStr);
    }

    strcat (Filename, FILE_SUFFIX_BINARY_TABLE);

    if (Gbl_VerboseMode)
    {
        fprintf (stderr,
            "Writing [%4.4s] to binary file: %s 0x%X (%u) bytes\n",
            Table->Signature, Filename, Table->Length, Table->Length);
    }

    /* Open the file and dump the entire table in binary mode */

    File = fopen (Filename, "wb");
    if (!File)
    {
        fprintf (stderr, "Could not open output file: %s\n", Filename);
        return (-1);
    }

    Actual = fwrite (Table, 1, TableLength, File);
    if (Actual != TableLength)
    {
        fprintf (stderr, "Error writing binary output file: %s\n", Filename);
        fclose (File);
        return (-1);
    }

    fclose (File);
    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    ApGetTableFromFile
 *
 * PARAMETERS:  Pathname            - File containing the binary ACPI table
 *              OutFileSize         - Where the file size is returned
 *
 * RETURN:      Buffer containing the ACPI table. NULL on error.
 *
 * DESCRIPTION: Open a file and read it entirely into a new buffer
 *
 ******************************************************************************/

ACPI_TABLE_HEADER *
ApGetTableFromFile (
    char                    *Pathname,
    UINT32                  *OutFileSize)
{
    ACPI_TABLE_HEADER       *Buffer = NULL;
    ACPI_FILE               File;
    UINT32                  FileSize;
    ACPI_SIZE               Actual;


    /* Must use binary mode */

    File = fopen (Pathname, "rb");
    if (!File)
    {
        fprintf (stderr, "Could not open input file: %s\n", Pathname);
        return (NULL);
    }

    /* Need file size to allocate a buffer */

    FileSize = CmGetFileSize (File);
    if (FileSize == ACPI_UINT32_MAX)
    {
        fprintf (stderr,
            "Could not get input file size: %s\n", Pathname);
        goto Cleanup;
    }

    /* Allocate a buffer for the entire file */

    Buffer = ACPI_ALLOCATE_ZEROED (FileSize);
    if (!Buffer)
    {
        fprintf (stderr,
            "Could not allocate file buffer of size: %u\n", FileSize);
        goto Cleanup;
    }

    /* Read the entire file */

    Actual = fread (Buffer, 1, FileSize, File);
    if (Actual != FileSize)
    {
        fprintf (stderr, "Could not read input file: %s\n", Pathname);
        ACPI_FREE (Buffer);
        Buffer = NULL;
        goto Cleanup;
    }

    *OutFileSize = FileSize;

Cleanup:
    fclose (File);
    return (Buffer);
}
