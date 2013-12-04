
/**
 * \file file.c
 *
 *  Time-stamp:      "2010-07-10 11:00:59 bkorb"
 *
 *  This file is part of AutoOpts, a companion to AutoGen.
 *  AutoOpts is free software.
 *  AutoOpts is Copyright (c) 1992-2011 by Bruce Korb - all rights reserved
 *
 *  AutoOpts is available under any one of two licenses.  The license
 *  in use must be one of these two and the choice is under the control
 *  of the user of the license.
 *
 *   The GNU Lesser General Public License, version 3 or later
 *      See the files "COPYING.lgplv3" and "COPYING.gplv3"
 *
 *   The Modified Berkeley Software Distribution License
 *      See the file "COPYING.mbsd"
 *
 *  These files have the following md5sums:
 *
 *  43b91e8ca915626ed3818ffb1b71248b pkg/libopts/COPYING.gplv3
 *  06a1a2e4760c90ea5e1dad8dfaac4d39 pkg/libopts/COPYING.lgplv3
 *  66a5cedaf62c4b2637025f049f9b826f pkg/libopts/COPYING.mbsd
 */

/*=export_func  optionFileCheck
 * private:
 *
 * what:  Decipher a boolean value
 * arg:   + tOptions*     + pOpts    + program options descriptor  +
 * arg:   + tOptDesc*     + pOptDesc + the descriptor for this arg +
 * arg:   + teOptFileType + ftype    + File handling type          +
 * arg:   + tuFileMode    + mode     + file open mode (if needed)  +
 *
 * doc:
 *   Make sure the named file conforms with the file type mode.
 *   The mode specifies if the file must exist, must not exist or may
 *   (or may not) exist.  The mode may also specify opening the
 *   file: don't, open just the descriptor (fd), or open as a stream
 *   (FILE* pointer).
=*/
void
optionFileCheck(tOptions* pOpts, tOptDesc* pOD,
                teOptFileType ftype, tuFileMode mode)
{
    if (pOpts <= OPTPROC_EMIT_LIMIT) {
        if (pOpts != OPTPROC_EMIT_USAGE)
            return;

        switch (ftype & FTYPE_MODE_EXIST_MASK) {
        case FTYPE_MODE_MUST_NOT_EXIST:
            fputs(zFileCannotExist, option_usage_fp);
            break;

        case FTYPE_MODE_MUST_EXIST:
            fputs(zFileMustExist, option_usage_fp);
            break;
        }
        return;
    }

    if ((pOD->fOptState & OPTST_RESET) != 0) {
        if (pOD->optCookie != NULL)
            AGFREE(pOD->optCookie);
        return;
    }

    {
        struct stat sb;

        errno = 0;

        switch (ftype & FTYPE_MODE_EXIST_MASK) {
        case FTYPE_MODE_MUST_NOT_EXIST:
            if (  (stat(pOD->optArg.argString, &sb) == 0)
               || (errno != ENOENT) ){
                if (errno == 0)
                    errno = EINVAL;
                fprintf(stderr, zFSOptError, errno, strerror(errno),
                        zFSOptErrNoExist, pOD->optArg.argString, pOD->pz_Name);
                pOpts->pUsageProc(pOpts, EXIT_FAILURE);
                /* NOTREACHED */
            }
            /* FALLTHROUGH */

        default:
        case FTYPE_MODE_MAY_EXIST:
        {
            char * p = strrchr(pOD->optArg.argString, DIRCH);
            if (p == NULL)
                break; /* assume "." always exists. */

            *p = NUL;
            if (  (stat(pOD->optArg.argString, &sb) != 0)
               || (errno = EINVAL, ! S_ISDIR(sb.st_mode)) ){
                fprintf(stderr, zFSOptError, errno, strerror(errno),
                        zFSOptErrMayExist, pOD->optArg.argString, pOD->pz_Name);
                pOpts->pUsageProc(pOpts, EXIT_FAILURE);
                /* NOTREACHED */
            }
            if (p != NULL)
                *p = DIRCH;
            break;
        }

        case FTYPE_MODE_MUST_EXIST:
            if (  (stat(pOD->optArg.argString, &sb) != 0)
               || (errno = EINVAL, ! S_ISREG(sb.st_mode)) ){
                fprintf(stderr, zFSOptError, errno, strerror(errno),
                        zFSOptErrMustExist, pOD->optArg.argString,
                        pOD->pz_Name);
                pOpts->pUsageProc(pOpts, EXIT_FAILURE);
                /* NOTREACHED */
            }
            break;
        }
    }

    switch (ftype & FTYPE_MODE_OPEN_MASK) {
    default:
    case FTYPE_MODE_NO_OPEN:
        break;

    case FTYPE_MODE_OPEN_FD:
    {
        int fd = open(pOD->optArg.argString, mode.file_flags);
        if (fd < 0) {
            fprintf(stderr, zFSOptError, errno, strerror(errno),
                    zFSOptErrOpen, pOD->optArg.argString, pOD->pz_Name);
            pOpts->pUsageProc(pOpts, EXIT_FAILURE);
            /* NOTREACHED */
        }

        if ((pOD->fOptState & OPTST_ALLOC_ARG) != 0)
            pOD->optCookie = (void *)pOD->optArg.argString;
        else
            AGDUPSTR(pOD->optCookie, pOD->optArg.argString, "file name");

        pOD->optArg.argFd = fd;
        pOD->fOptState &= ~OPTST_ALLOC_ARG;
        break;
    }

    case FTYPE_MODE_FOPEN_FP:
    {
        FILE* fp = fopen(pOD->optArg.argString, mode.file_mode);
        if (fp == NULL) {
            fprintf(stderr, zFSOptError, errno, strerror(errno),
                    zFSOptErrFopen, pOD->optArg.argString, pOD->pz_Name);
            pOpts->pUsageProc(pOpts, EXIT_FAILURE);
            /* NOTREACHED */
        }

        if ((pOD->fOptState & OPTST_ALLOC_ARG) != 0)
            pOD->optCookie = (void *)pOD->optArg.argString;
        else
            AGDUPSTR(pOD->optCookie, pOD->optArg.argString, "file name");

        pOD->optArg.argFp = fp;
        pOD->fOptState &= ~OPTST_ALLOC_ARG;
        break;
    }
    }
}
/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/file.c */
