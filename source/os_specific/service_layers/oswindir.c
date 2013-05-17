/******************************************************************************
 *
 * Module Name: oswindir - Windows directory access interfaces
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
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

#include <acpi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>

typedef struct ExternalFindInfo
{
    struct _finddata_t          DosInfo;
    char                        *FullWildcardSpec;
    long                        FindHandle;
    char                        State;
    char                        RequestedFileType;

} EXTERNAL_FIND_INFO;


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsOpenDirectory
 *
 * PARAMETERS:  DirPathname         - Full pathname to the directory
 *              WildcardSpec        - string of the form "*.c", etc.
 *              RequestedFileType   - Either a directory or normal file
 *
 * RETURN:      A directory "handle" to be used in subsequent search operations.
 *              NULL returned on failure.
 *
 * DESCRIPTION: Open a directory in preparation for a wildcard search
 *
 ******************************************************************************/

void *
AcpiOsOpenDirectory (
    char                    *DirPathname,
    char                    *WildcardSpec,
    char                    RequestedFileType)
{
    long                    FindHandle;
    char                    *FullWildcardSpec;
    EXTERNAL_FIND_INFO      *SearchInfo;


    /* No directory path means "use current directory" - use a dot */

    if (!DirPathname || strlen (DirPathname) == 0)
    {
        DirPathname = ".";
    }

    /* Allocate the info struct that will be returned to the caller */

    SearchInfo = calloc (sizeof (EXTERNAL_FIND_INFO), 1);
    if (!SearchInfo)
    {
        return (NULL);
    }

    /* Allocate space for the full wildcard path */

    FullWildcardSpec = calloc (strlen (DirPathname) + strlen (WildcardSpec) + 2, 1);
    if (!FullWildcardSpec)
    {
        printf ("Could not allocate buffer for wildcard pathname\n");
        return (NULL);
    }

    /* Create the full wildcard path */

    strcpy (FullWildcardSpec, DirPathname);
    strcat (FullWildcardSpec, "/");
    strcat (FullWildcardSpec, WildcardSpec);

    /* Initialize the find functions, get first match */

    FindHandle = _findfirst (FullWildcardSpec, &SearchInfo->DosInfo);
    if (FindHandle == -1)
    {
        /* Failure means that no match was found */

        free (FullWildcardSpec);
        free (SearchInfo);
        return (NULL);
    }

    /* Save the info in the return structure */

    SearchInfo->RequestedFileType = RequestedFileType;
    SearchInfo->FullWildcardSpec = FullWildcardSpec;
    SearchInfo->FindHandle = FindHandle;
    SearchInfo->State = 0;
    return (SearchInfo);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsGetNextFilename
 *
 * PARAMETERS:  DirHandle           - Created via AcpiOsOpenDirectory
 *
 * RETURN:      Next filename matched. NULL if no more matches.
 *
 * DESCRIPTION: Get the next file in the directory that matches the wildcard
 *              specification.
 *
 ******************************************************************************/

char *
AcpiOsGetNextFilename (
    void                    *DirHandle)
{
    EXTERNAL_FIND_INFO      *SearchInfo = DirHandle;
    int                     Status;
    char                    FileTypeNotMatched = 1;


    /*
     * Loop while we have matched files but not found any files of
     * the requested type.
     */
    while (FileTypeNotMatched)
    {
        /* On the first call, we already have the first match */

        if (SearchInfo->State == 0)
        {
            /* No longer the first match */

            SearchInfo->State = 1;
        }
        else
        {
            /* Get the next match */

            Status = _findnext (SearchInfo->FindHandle, &SearchInfo->DosInfo);
            if (Status != 0)
            {
                return (NULL);
            }
        }

        /*
         * Found a match, now check to make sure that the file type
         * matches the requested file type (directory or normal file)
         *
         * NOTE: use of the attrib field saves us from doing a very
         * expensive stat() on the file!
         */
        switch (SearchInfo->RequestedFileType)
        {
        case REQUEST_FILE_ONLY:

            /* Anything other than A_SUBDIR is OK */

            if (!(SearchInfo->DosInfo.attrib & _A_SUBDIR))
            {
                FileTypeNotMatched = 0;
            }
            break;

        case REQUEST_DIR_ONLY:

            /* Must have A_SUBDIR bit set */

            if (SearchInfo->DosInfo.attrib & _A_SUBDIR)
            {
                FileTypeNotMatched = 0;
            }
            break;

        default:

            return (NULL);
        }
    }

    return (SearchInfo->DosInfo.name);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsCloseDirectory
 *
 * PARAMETERS:  DirHandle           - Created via AcpiOsOpenDirectory
 *
 * RETURN:      None
 *
 * DESCRIPTION: Close the open directory and cleanup.
 *
 ******************************************************************************/

void
AcpiOsCloseDirectory (
    void                    *DirHandle)
{
    EXTERNAL_FIND_INFO      *SearchInfo = DirHandle;


    /* Close the directory and free allocations */

    _findclose (SearchInfo->FindHandle);
    free (SearchInfo->FullWildcardSpec);
    free (DirHandle);
}
