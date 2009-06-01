
/******************************************************************************
 *
 * Module Name: oswindir - Windows directory access interfaces
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2009, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code.  No other license or right
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
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
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
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
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
 *****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>

#include <acpi.h>

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
        return NULL;
    }

    /* Allocate space for the full wildcard path */

    FullWildcardSpec = calloc (strlen (DirPathname) + strlen (WildcardSpec) + 2, 1);
    if (!FullWildcardSpec)
    {
        printf ("Could not allocate buffer for wildcard pathname\n");
        return NULL;
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
        return NULL;
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
 * RETURN:      Next filename matched.  NULL if no more matches.
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
                return NULL;
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
            return NULL;
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
 * RETURN:      None.
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

