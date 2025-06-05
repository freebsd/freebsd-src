/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * prof_FSp_glue.c --- Deprecated FSSpec functions.  Mac-only.
 */

#include "prof_int.h"

#include <Kerberos/FSpUtils.h>
#include <limits.h>

long KRB5_CALLCONV FSp_profile_init (const FSSpec* files, profile_t *ret_profile);

long KRB5_CALLCONV FSp_profile_init_path (const FSSpec* files, profile_t *ret_profile);

errcode_t KRB5_CALLCONV
FSp_profile_init(files, ret_profile)
    const FSSpec* files;
    profile_t *ret_profile;
{
    unsigned int        fileCount = 0;
    const FSSpec       *nextSpec;
    profile_filespec_t *pathArray = NULL;
    unsigned int        i;
    errcode_t           retval = 0;

    for (nextSpec = files; ; nextSpec++) {
        if ((nextSpec -> vRefNum == 0) &&
            (nextSpec -> parID == 0) &&
            (StrLength (nextSpec -> name) == 0))
            break;
        fileCount++;
    }

    pathArray = (profile_filespec_t *) malloc ((fileCount + 1) * sizeof(const_profile_filespec_t));
    if (pathArray == NULL) {
        retval = ENOMEM;
    }

    if (retval == 0) {
        for (i = 0; i < fileCount + 1; i++) {
            pathArray [i] = NULL;
        }
    }

    if (retval == 0) {
        for (i = 0; i < fileCount; i++) {
            OSStatus err = noErr;

            if (err == noErr) {
                pathArray[i] = (char *) malloc (sizeof(char) * PATH_MAX);
                if (pathArray[i] == NULL) {
                    err = memFullErr;
                }
            }
            /* convert the FSSpec to an path */
            if (err == noErr) {
                err = FSSpecToPOSIXPath (&files[i], pathArray[i], PATH_MAX);
            }

            if (err == memFullErr) {
                retval = ENOMEM;
                break;
            } else if (err != noErr) {
                retval = ENOENT;
                break;
            }
        }
    }

    if (retval == 0) {
        retval = profile_init ((const_profile_filespec_t *) pathArray,
                               ret_profile);
    }

    if (pathArray != NULL) {
        for (i = 0; i < fileCount; i++) {
            if (pathArray [i] != 0)
                free (pathArray [i]);
        }
        free (pathArray);
    }

    return retval;
}

errcode_t KRB5_CALLCONV
FSp_profile_init_path(files, ret_profile)
    const FSSpec* files;
    profile_t *ret_profile;
{
    return FSp_profile_init (files, ret_profile);
}
