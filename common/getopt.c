
/******************************************************************************
 *
 * Module Name: getopt
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
#include <string.h>
#include "acpi.h"
#include "accommon.h"
#include "acapps.h"

#define ERR(szz,czz) if(AcpiGbl_Opterr){fprintf(stderr,"%s%s%c\n",argv[0],szz,czz);}


int   AcpiGbl_Opterr = 1;
int   AcpiGbl_Optind = 1;
char  *AcpiGbl_Optarg;


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetopt
 *
 * PARAMETERS:  argc, argv          - from main
 *              opts                - options info list
 *
 * RETURN:      Option character or EOF
 *
 * DESCRIPTION: Get the next option
 *
 ******************************************************************************/

int
AcpiGetopt(
    int                     argc,
    char                    **argv,
    char                    *opts)
{
    static int              CurrentCharPtr = 1;
    int                     CurrentChar;
    char                    *OptsPtr;


    if (CurrentCharPtr == 1)
    {
        if (AcpiGbl_Optind >= argc ||
            argv[AcpiGbl_Optind][0] != '-' ||
            argv[AcpiGbl_Optind][1] == '\0')
        {
            return(EOF);
        }
        else if (strcmp (argv[AcpiGbl_Optind], "--") == 0)
        {
            AcpiGbl_Optind++;
            return(EOF);
        }
    }

    /* Get the option */

    CurrentChar = argv[AcpiGbl_Optind][CurrentCharPtr];

    /* Make sure that the option is legal */

    if (CurrentChar == ':' ||
       (OptsPtr = strchr (opts, CurrentChar)) == NULL)
    {
        ERR (": illegal option -- ", CurrentChar);

        if (argv[AcpiGbl_Optind][++CurrentCharPtr] == '\0')
        {
            AcpiGbl_Optind++;
            CurrentCharPtr = 1;
        }

        return ('?');
    }

    /* Option requires an argument? */

    if (*++OptsPtr == ':')
    {
        if (argv[AcpiGbl_Optind][(int) (CurrentCharPtr+1)] != '\0')
        {
            AcpiGbl_Optarg = &argv[AcpiGbl_Optind++][(int) (CurrentCharPtr+1)];
        }
        else if (++AcpiGbl_Optind >= argc)
        {
            ERR (": option requires an argument -- ", CurrentChar);

            CurrentCharPtr = 1;
            return ('?');
        }
        else
        {
            AcpiGbl_Optarg = argv[AcpiGbl_Optind++];
        }

        CurrentCharPtr = 1;
    }

    /* Option has optional single-char arguments? */

    else if (*OptsPtr == '^')
    {
        if (argv[AcpiGbl_Optind][(int) (CurrentCharPtr+1)] != '\0')
        {
            AcpiGbl_Optarg = &argv[AcpiGbl_Optind][(int) (CurrentCharPtr+1)];
        }
        else
        {
            AcpiGbl_Optarg = "^";
        }

        AcpiGbl_Optind++;
        CurrentCharPtr = 1;
    }

    /* Option with no arguments */

    else
    {
        if (argv[AcpiGbl_Optind][++CurrentCharPtr] == '\0')
        {
            CurrentCharPtr = 1;
            AcpiGbl_Optind++;
        }

        AcpiGbl_Optarg = NULL;
    }

    return (CurrentChar);
}
