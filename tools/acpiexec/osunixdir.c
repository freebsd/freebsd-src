
/******************************************************************************
 *
 * Module Name: osunixdir - Unix directory access interfaces
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
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fnmatch.h>
#include <ctype.h>
#include <sys/stat.h>

#include "acpisrc.h"

/*
 * Allocated structure returned from OsOpenDirectory
 */
typedef struct ExternalFindInfo
{
    char                        *DirPathname;
    DIR                         *DirPtr;
    char                        temp_buffer[128];
    char                        *WildcardSpec;
    char                        RequestedFileType;

} EXTERNAL_FIND_INFO;


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsOpenDirectory
 *
 * PARAMETERS:  DirPathname         - Full pathname to the directory
 *              WildcardSpec        - string of the form "*.c", etc.
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
    EXTERNAL_FIND_INFO      *ExternalInfo;
    DIR                     *dir;


    /* Allocate the info struct that will be returned to the caller */

    ExternalInfo = calloc (sizeof (EXTERNAL_FIND_INFO), 1);
    if (!ExternalInfo)
    {
        return (NULL);
    }

    /* Get the directory stream */

    dir = opendir (DirPathname);
    if (!dir)
    {
        free (ExternalInfo);
        return (NULL);
    }

    /* Save the info in the return structure */

    ExternalInfo->WildcardSpec = WildcardSpec;
    ExternalInfo->RequestedFileType = RequestedFileType;
    ExternalInfo->DirPathname = DirPathname;
    ExternalInfo->DirPtr = dir;
    return (ExternalInfo);
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
    EXTERNAL_FIND_INFO      *ExternalInfo = DirHandle;
    struct dirent           *dir_entry;
    char                    *temp_str;
    int                     str_len;
    struct stat             temp_stat;
    int                     err;


    while ((dir_entry = readdir (ExternalInfo->DirPtr)))
    {
        if (!fnmatch (ExternalInfo->WildcardSpec, dir_entry->d_name, 0))
        {
            if (dir_entry->d_name[0] == '.')
            {
                continue;
            }

            str_len = strlen (dir_entry->d_name) +
                        strlen (ExternalInfo->DirPathname) + 2;

            temp_str = calloc (str_len, 1);
            if (!temp_str)
            {
                printf ("Could not allocate buffer for temporary string\n");
                return (NULL);
            }

            strcpy (temp_str, ExternalInfo->DirPathname);
            strcat (temp_str, "/");
            strcat (temp_str, dir_entry->d_name);

            err = stat (temp_str, &temp_stat);
            free (temp_str);
            if (err == -1)
            {
                printf ("stat() error - should not happen\n");
                return (NULL);
            }

            if ((S_ISDIR (temp_stat.st_mode)
                && (ExternalInfo->RequestedFileType == REQUEST_DIR_ONLY))
               ||
               ((!S_ISDIR (temp_stat.st_mode)
                && ExternalInfo->RequestedFileType == REQUEST_FILE_ONLY)))
            {
                /* copy to a temp buffer because dir_entry struct is on the stack */

                strcpy (ExternalInfo->temp_buffer, dir_entry->d_name);
                return (ExternalInfo->temp_buffer);
            }
        }
    }

    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsCloseDirectory
 *
 * PARAMETERS:  DirHandle           - Created via AcpiOsOpenDirectory
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Close the open directory and cleanup.
 *
 ******************************************************************************/

void
AcpiOsCloseDirectory (
    void                    *DirHandle)
{
    EXTERNAL_FIND_INFO      *ExternalInfo = DirHandle;


    /* Close the directory and free allocations */

    closedir (ExternalInfo->DirPtr);
    free (DirHandle);
}


/* Other functions acpisrc uses but that aren't standard on Unix */

/*******************************************************************************
 *
 * FUNCTION:    strlwr
 *
 * PARAMETERS:  str                 - String to be lowercased.
 *
 * RETURN:      str.
 *
 * DESCRIPTION: Lowercase a string in-place.
 *
 ******************************************************************************/

char *
strlwr  (
   char         *str)
{
    int         length;
    int         i;


    length = strlen (str);

    for (i = 0; i < length; i++)
    {
        str[i] = tolower ((int) str[i]);
    }

    return (str);
}
