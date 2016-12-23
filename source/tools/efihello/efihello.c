/******************************************************************************
 *
 * Module Name: efihello - very simple ACPICA/EFI integration example
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

#include "acpi.h"
#include "accommon.h"
#include "acapps.h"


#define LINE_SIZE           256
static char                 LineBuffer[LINE_SIZE];

/******************************************************************************
 *
 * FUNCTION:    main
 *
 * PARAMETERS:  argc/argv           - Standard argc/argv
 *
 * RETURN:      Status
 *
 * DESCRIPTION: C main function for efihello
 *
 ******************************************************************************/

#ifndef _GNU_EFI
int ACPI_SYSTEM_XFACE
main (
    int                     argc,
    char                    *argv[])
#else
int ACPI_SYSTEM_XFACE
acpi_main (
    int                     argc,
    char                    *argv[])
#endif
{
    ACPI_FILE               File;
    BOOLEAN                 DoCloseFile = FALSE;
    char                    *Result;


    AcpiOsInitialize ();

    printf ("argc=%d\n", argc);

    if (argc > 1)
    {
        File = fopen (argv[1], "r");
        if (!File)
        {
            printf ("Failed to open %s.\n", argv[1]);
            return (-1);
        }
        DoCloseFile = TRUE;
    }
    else
    {
        File = stdin;
    }

    while (1)
    {
        Result = fgets (LineBuffer, LINE_SIZE, File);
        if (!Result)
        {
            printf ("Failed to read %s.\n", argv[1]);
            fclose (File);
            return (-2);
        }

        printf ("%s", LineBuffer);

        if (strncmp (Result, "exit", 4) == 0)
        {
            break;
        }
    }


    if (DoCloseFile)
    {
        fclose (File);
    }
    return (0);
}
