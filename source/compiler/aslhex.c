/******************************************************************************
 *
 * Module Name: aslhex - ASCII hex output file generation (C, ASM, and ASL)
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

#include "aslcompiler.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("ashex")

/*
 * This module emits ASCII hex output files in either C, ASM, or ASL format
 */

/* Local prototypes */

static void
HxDoHexOutputC (
    void);

static void
HxDoHexOutputAsl (
    void);

static void
HxDoHexOutputAsm (
    void);

static UINT32
HxReadAmlOutputFile (
    UINT8                   *Buffer);


/*******************************************************************************
 *
 * FUNCTION:    HxDoHexOutput
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create the hex output file. Note: data is obtained by reading
 *              the entire AML output file that was previously generated.
 *
 ******************************************************************************/

void
HxDoHexOutput (
    void)
{

    switch (Gbl_HexOutputFlag)
    {
    case HEX_OUTPUT_C:

        HxDoHexOutputC ();
        break;

    case HEX_OUTPUT_ASM:

        HxDoHexOutputAsm ();
        break;

    case HEX_OUTPUT_ASL:

        HxDoHexOutputAsl ();
        break;

    default:

        /* No other output types supported */

        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    HxReadAmlOutputFile
 *
 * PARAMETERS:  Buffer              - Where to return data
 *
 * RETURN:      None
 *
 * DESCRIPTION: Read a line of the AML output prior to formatting the data
 *
 ******************************************************************************/

static UINT32
HxReadAmlOutputFile (
    UINT8                   *Buffer)
{
    UINT32                  Actual;


    Actual = fread (Buffer, 1, HEX_TABLE_LINE_SIZE,
        Gbl_Files[ASL_FILE_AML_OUTPUT].Handle);

    if (ferror (Gbl_Files[ASL_FILE_AML_OUTPUT].Handle))
    {
        FlFileError (ASL_FILE_AML_OUTPUT, ASL_MSG_READ);
        AslAbort ();
    }

    return (Actual);
}


/*******************************************************************************
 *
 * FUNCTION:    HxDoHexOutputC
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create the hex output file. This is the same data as the AML
 *              output file, but formatted into hex/ascii bytes suitable for
 *              inclusion into a C source file.
 *
 ******************************************************************************/

static void
HxDoHexOutputC (
    void)
{
    UINT8                   FileData[HEX_TABLE_LINE_SIZE];
    UINT32                  LineLength;
    UINT32                  Offset = 0;
    UINT32                  AmlFileSize;
    UINT32                  i;


    /* Get AML size, seek back to start */

    AmlFileSize = FlGetFileSize (ASL_FILE_AML_OUTPUT);
    FlSeekFile (ASL_FILE_AML_OUTPUT, 0);

    FlPrintFile (ASL_FILE_HEX_OUTPUT, " * C source code output\n");
    FlPrintFile (ASL_FILE_HEX_OUTPUT, " * AML code block contains 0x%X bytes\n *\n */\n",
        AmlFileSize);
    FlPrintFile (ASL_FILE_HEX_OUTPUT, "unsigned char AmlCode[] =\n{\n");

    while (Offset < AmlFileSize)
    {
        /* Read enough bytes needed for one output line */

        LineLength = HxReadAmlOutputFile (FileData);
        if (!LineLength)
        {
            break;
        }

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "    ");

        for (i = 0; i < LineLength; i++)
        {
            /*
             * Print each hex byte.
             * Add a comma until the very last byte of the AML file
             * (Some C compilers complain about a trailing comma)
             */
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "0x%2.2X", FileData[i]);
            if ((Offset + i + 1) < AmlFileSize)
            {
                FlPrintFile (ASL_FILE_HEX_OUTPUT, ",");
            }
            else
            {
                FlPrintFile (ASL_FILE_HEX_OUTPUT, " ");
            }
        }

        /* Add fill spaces if needed for last line */

        if (LineLength < HEX_TABLE_LINE_SIZE)
        {
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "%*s",
                5 * (HEX_TABLE_LINE_SIZE - LineLength), " ");
        }

        /* Emit the offset and ascii dump for the entire line */

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "  /* %8.8X", Offset);
        LsDumpAsciiInComment (ASL_FILE_HEX_OUTPUT, LineLength, FileData);

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "%*s*/\n",
            HEX_TABLE_LINE_SIZE - LineLength + 1, " ");

        Offset += LineLength;
    }

    FlPrintFile (ASL_FILE_HEX_OUTPUT, "};\n");
}


/*******************************************************************************
 *
 * FUNCTION:    HxDoHexOutputAsl
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create the hex output file. This is the same data as the AML
 *              output file, but formatted into hex/ascii bytes suitable for
 *              inclusion into a C source file.
 *
 ******************************************************************************/

static void
HxDoHexOutputAsl (
    void)
{
    UINT8                   FileData[HEX_TABLE_LINE_SIZE];
    UINT32                  LineLength;
    UINT32                  Offset = 0;
    UINT32                  AmlFileSize;
    UINT32                  i;


    /* Get AML size, seek back to start */

    AmlFileSize = FlGetFileSize (ASL_FILE_AML_OUTPUT);
    FlSeekFile (ASL_FILE_AML_OUTPUT, 0);

    FlPrintFile (ASL_FILE_HEX_OUTPUT, " * ASL source code output\n");
    FlPrintFile (ASL_FILE_HEX_OUTPUT, " * AML code block contains 0x%X bytes\n *\n */\n",
        AmlFileSize);
    FlPrintFile (ASL_FILE_HEX_OUTPUT, "    Name (BUF1, Buffer()\n    {\n");

    while (Offset < AmlFileSize)
    {
        /* Read enough bytes needed for one output line */

        LineLength = HxReadAmlOutputFile (FileData);
        if (!LineLength)
        {
            break;
        }

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "        ");

        for (i = 0; i < LineLength; i++)
        {
            /*
             * Print each hex byte.
             * Add a comma until the very last byte of the AML file
             * (Some C compilers complain about a trailing comma)
             */
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "0x%2.2X", FileData[i]);
            if ((Offset + i + 1) < AmlFileSize)
            {
                FlPrintFile (ASL_FILE_HEX_OUTPUT, ",");
            }
            else
            {
                FlPrintFile (ASL_FILE_HEX_OUTPUT, " ");
            }
        }

        /* Add fill spaces if needed for last line */

        if (LineLength < HEX_TABLE_LINE_SIZE)
        {
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "%*s",
                5 * (HEX_TABLE_LINE_SIZE - LineLength), " ");
        }

        /* Emit the offset and ascii dump for the entire line */

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "  /* %8.8X", Offset);
        LsDumpAsciiInComment (ASL_FILE_HEX_OUTPUT, LineLength, FileData);

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "%*s*/\n",
            HEX_TABLE_LINE_SIZE - LineLength + 1, " ");

        Offset += LineLength;
    }

    FlPrintFile (ASL_FILE_HEX_OUTPUT, "    })\n");
}


/*******************************************************************************
 *
 * FUNCTION:    HxDoHexOutputAsm
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create the hex output file. This is the same data as the AML
 *              output file, but formatted into hex/ascii bytes suitable for
 *              inclusion into a ASM source file.
 *
 ******************************************************************************/

static void
HxDoHexOutputAsm (
    void)
{
    UINT8                   FileData[HEX_TABLE_LINE_SIZE];
    UINT32                  LineLength;
    UINT32                  Offset = 0;
    UINT32                  AmlFileSize;
    UINT32                  i;


    /* Get AML size, seek back to start */

    AmlFileSize = FlGetFileSize (ASL_FILE_AML_OUTPUT);
    FlSeekFile (ASL_FILE_AML_OUTPUT, 0);

    FlPrintFile (ASL_FILE_HEX_OUTPUT, "; Assembly code source output\n");
    FlPrintFile (ASL_FILE_HEX_OUTPUT, "; AML code block contains 0x%X bytes\n;\n",
        AmlFileSize);

    while (Offset < AmlFileSize)
    {
        /* Read enough bytes needed for one output line */

        LineLength = HxReadAmlOutputFile (FileData);
        if (!LineLength)
        {
            break;
        }

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "  db  ");

        for (i = 0; i < LineLength; i++)
        {
            /*
             * Print each hex byte.
             * Add a comma until the last byte of the line
             */
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "0%2.2Xh", FileData[i]);
            if ((i + 1) < LineLength)
            {
                FlPrintFile (ASL_FILE_HEX_OUTPUT, ",");
            }
        }

        FlPrintFile (ASL_FILE_HEX_OUTPUT, " ");

        /* Add fill spaces if needed for last line */

        if (LineLength < HEX_TABLE_LINE_SIZE)
        {
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "%*s",
                5 * (HEX_TABLE_LINE_SIZE - LineLength), " ");
        }

        /* Emit the offset and ascii dump for the entire line */

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "  ; %8.8X", Offset);
        LsDumpAsciiInComment (ASL_FILE_HEX_OUTPUT, LineLength, FileData);

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "\n");

        Offset += LineLength;
    }

    FlPrintFile (ASL_FILE_HEX_OUTPUT, "\n");
}
