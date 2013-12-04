
/*
 * Time-stamp:      "2011-04-22 12:54:28 bkorb"
 *
 *  This module implements the default usage procedure for
 *  Automated Options.  It may be overridden, of course.
 */

/*
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

/*=export_func  optionVersion
 *
 * what:     return the compiled AutoOpts version number
 * ret_type: char const*
 * ret_desc: the version string in constant memory
 * doc:
 *  Returns the full version string compiled into the library.
 *  The returned string cannot be modified.
=*/
char const*
optionVersion(void)
{
    static char const zVersion[] =
        STR(AO_CURRENT.AO_REVISION);

    return zVersion;
}

static void
emit_simple_ver(tOptions * pOpts, FILE * fp)
{
    if (pOpts->pzFullVersion != NULL)
        fputs(pOpts->pzFullVersion, fp);

    else if (pOpts->pzCopyright != NULL) {
        char const * pe = strchr(pOpts->pzCopyright, '\n');
        if (pe == NULL)
            pe = pOpts->pzCopyright + strlen(pOpts->pzCopyright);
        fwrite(pOpts->pzCopyright, 1, pe - pOpts->pzCopyright, fp);
    }

    else {
        char const * pe = strchr(pOpts->pzUsageTitle, '\n');
        if (pe == NULL)
            pe = pOpts->pzUsageTitle + strlen(pOpts->pzUsageTitle);
        fwrite(pOpts->pzUsageTitle, 1, pe - pOpts->pzCopyright, fp);
    }
    fputc('\n', fp);
}

static void
emit_copy_ver(tOptions * pOpts, FILE * fp)
{
    if (pOpts->pzCopyright != NULL)
        fputs(pOpts->pzCopyright, fp);

    else if (pOpts->pzFullVersion != NULL)
        fputs(pOpts->pzFullVersion, fp);

    else {
        char const * pe = strchr(pOpts->pzUsageTitle, '\n');
        if (pe == NULL)
            pe = pOpts->pzUsageTitle + strlen(pOpts->pzUsageTitle);
        fwrite(pOpts->pzUsageTitle, 1, pe - pOpts->pzCopyright, fp);
    }

    fputc('\n', fp);

    if (HAS_pzPkgDataDir(pOpts) && (pOpts->pzPackager != NULL))
        fputs(pOpts->pzPackager, fp);

    else if (pOpts->pzBugAddr != NULL)
        fprintf(fp, zPlsSendBugs, pOpts->pzBugAddr);
}

static void
emit_copy_note(tOptions * pOpts, FILE * fp)
{
    if (pOpts->pzCopyright != NULL) {
        fputs(pOpts->pzCopyright, fp);
        fputc('\n', fp);
    }

    if (pOpts->pzCopyNotice != NULL) {
        fputs(pOpts->pzCopyNotice, fp);
        fputc('\n', fp);
    }

    fprintf(fp, zAO_Ver, optionVersion());

    if (HAS_pzPkgDataDir(pOpts) && (pOpts->pzPackager != NULL))
        fputs(pOpts->pzPackager, fp);

    else if (pOpts->pzBugAddr != NULL)
        fprintf(fp, zPlsSendBugs, pOpts->pzBugAddr);
}

static void
print_ver(tOptions * pOpts, tOptDesc * pOD, FILE * fp)
{
    char ch;

    /*
     *  IF the optional argument flag is off, or the argument
     *  is not provided, then just print the version.
     */
    if (  ((pOD->fOptState & OPTST_ARG_OPTIONAL) == 0)
       || (pOD->optArg.argString == NULL))
         ch = 'v';
    else ch = pOD->optArg.argString[0];

    switch (ch) {
    case NUL: /* arg provided, but empty */
    case 'v': case 'V': emit_simple_ver(pOpts, fp); break;
    case 'c': case 'C': emit_copy_ver(pOpts, fp);   break;
    case 'n': case 'N': emit_copy_note(pOpts, fp);  break;

    default:
        fprintf(stderr, zBadVerArg, ch);
        exit(EXIT_FAILURE);
    }

    fflush(fp);
    if (ferror(fp) != 0) {
        fputs(zOutputFail, stderr);
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

/*=export_func  optionPrintVersion
 * private:
 *
 * what:  Print the program version
 * arg:   + tOptions* + pOpts    + program options descriptor +
 * arg:   + tOptDesc* + pOptDesc + the descriptor for this arg +
 *
 * doc:
 *  This routine will print the version to stdout.
=*/
void
optionPrintVersion(tOptions * pOpts, tOptDesc * pOD)
{
    print_ver(pOpts, pOD, stdout);
}

/*=export_func  optionVersionStderr
 * private:
 *
 * what:  Print the program version to stderr
 * arg:   + tOptions* + pOpts    + program options descriptor +
 * arg:   + tOptDesc* + pOptDesc + the descriptor for this arg +
 *
 * doc:
 *  This routine will print the version to stderr.
=*/
void
optionVersionStderr(tOptions * pOpts, tOptDesc * pOD)
{
    print_ver(pOpts, pOD, stderr);
}

/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/version.c */
