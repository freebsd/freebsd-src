
/*
 * \file save.c
 *
 * Time-stamp:      "2011-04-06 09:21:44 bkorb"
 *
 *  This module's routines will take the currently set options and
 *  store them into an ".rc" file for re-interpretation the next
 *  time the invoking program is run.
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

static char const  zWarn[] = "%s WARNING:  cannot save options - ";
static char const close_xml[] = "</%s>\n";

/* = = = START-STATIC-FORWARD = = = */
static tCC*
findDirName(tOptions* pOpts, int* p_free);

static char const *
findFileName(tOptions * pOpts, int * p_free_name);

static void
printEntry(
    FILE *     fp,
    tOptDesc * p,
    tCC*       pzLA );

static void
print_a_value(FILE * fp, int depth, tOptDesc * pOD, tOptionValue const * ovp);

static void
print_a_string(FILE * fp, char const * name, char const * pz);

static void
printValueList(FILE * fp, char const * name, tArgList * al);

static void
printHierarchy(FILE * fp, tOptDesc * p);

static FILE *
openSaveFile(tOptions* pOpts);

static void
printNoArgOpt(FILE * fp, tOptDesc * p, tOptDesc * pOD);

static void
printStringArg(FILE * fp, tOptDesc * pOD);

static void
printEnumArg(FILE * fp, tOptDesc * pOD);

static void
printSetMemberArg(FILE * fp, tOptDesc * pOD);

static void
printFileArg(FILE * fp, tOptDesc * pOD, tOptions* pOpts);
/* = = = END-STATIC-FORWARD = = = */

static tCC*
findDirName(tOptions* pOpts, int* p_free)
{
    tCC*  pzDir;

    if (  (pOpts->specOptIdx.save_opts == NO_EQUIVALENT)
       || (pOpts->specOptIdx.save_opts == 0))
        return NULL;

    pzDir = pOpts->pOptDesc[ pOpts->specOptIdx.save_opts ].optArg.argString;
    if ((pzDir != NULL) && (*pzDir != NUL))
        return pzDir;

    /*
     *  This function only works if there is a directory where
     *  we can stash the RC (INI) file.
     */
    {
        tCC* const* papz = pOpts->papzHomeList;
        if (papz == NULL)
            return NULL;

        while (papz[1] != NULL) papz++;
        pzDir = *papz;
    }

    /*
     *  IF it does not require deciphering an env value, then just copy it
     */
    if (*pzDir != '$')
        return pzDir;

    {
        tCC*  pzEndDir = strchr(++pzDir, DIRCH);
        char* pzFileName;
        char* pzEnv;

        if (pzEndDir != NULL) {
            char z[ AO_NAME_SIZE ];
            if ((pzEndDir - pzDir) > AO_NAME_LIMIT )
                return NULL;
            memcpy(z, pzDir, (size_t)(pzEndDir - pzDir));
            z[pzEndDir - pzDir] = NUL;
            pzEnv = getenv(z);
        } else {

            /*
             *  Make sure we can get the env value (after stripping off
             *  any trailing directory or file names)
             */
            pzEnv = getenv(pzDir);
        }

        if (pzEnv == NULL) {
            fprintf(stderr, zWarn, pOpts->pzProgName);
            fprintf(stderr, zNotDef, pzDir);
            return NULL;
        }

        if (pzEndDir == NULL)
            return pzEnv;

        {
            size_t sz = strlen(pzEnv) + strlen(pzEndDir) + 2;
            pzFileName = (char*)AGALOC(sz, "dir name");
        }

        if (pzFileName == NULL)
            return NULL;

        *p_free = 1;
        /*
         *  Glue together the full name into the allocated memory.
         *  FIXME: We lose track of this memory.
         */
        sprintf(pzFileName, "%s/%s", pzEnv, pzEndDir);
        return pzFileName;
    }
}


static char const *
findFileName(tOptions * pOpts, int * p_free_name)
{
    struct stat stBuf;
    int    free_dir_name = 0;

    char const * pzDir = findDirName(pOpts, &free_dir_name);
    if (pzDir == NULL)
        return NULL;

    /*
     *  See if we can find the specified directory.  We use a once-only loop
     *  structure so we can bail out early.
     */
    if (stat(pzDir, &stBuf) != 0) do {
        char z[AG_PATH_MAX];
        char * dirchp;

        /*
         *  IF we could not, check to see if we got a full
         *  path to a file name that has not been created yet.
         */
        if (errno != ENOENT) {
        bogus_name:
            fprintf(stderr, zWarn, pOpts->pzProgName);
            fprintf(stderr, zNoStat, errno, strerror(errno), pzDir);
            if (free_dir_name)
                AGFREE((void*)pzDir);
            return NULL;
        }

        /*
         *  Strip off the last component, stat the remaining string and
         *  that string must name a directory
         */
        dirchp = strrchr(pzDir, DIRCH);
        if (dirchp == NULL) {
            stBuf.st_mode = S_IFREG;
            break; /* found directory -- viz.,  "." */
        }

        if ((dirchp - pzDir) >= sizeof(z))
            goto bogus_name;

        memcpy(z, pzDir, (size_t)(dirchp - pzDir));
        z[dirchp - pzDir] = NUL;

        if ((stat(z, &stBuf) != 0) || ! S_ISDIR(stBuf.st_mode))
            goto bogus_name;
        stBuf.st_mode = S_IFREG; /* file within this directory */
    } while (0);

    /*
     *  IF what we found was a directory,
     *  THEN tack on the config file name
     */
    if (S_ISDIR(stBuf.st_mode)) {
        size_t sz = strlen(pzDir) + strlen(pOpts->pzRcName) + 2;

        {
            char*  pzPath = (char*)AGALOC(sz, "file name");
#ifdef HAVE_SNPRINTF
            snprintf(pzPath, sz, "%s/%s", pzDir, pOpts->pzRcName);
#else
            sprintf(pzPath, "%s/%s", pzDir, pOpts->pzRcName);
#endif
            if (free_dir_name)
                AGFREE((void*)pzDir);
            pzDir = pzPath;
            free_dir_name = 1;
        }

        /*
         *  IF we cannot stat the object for any reason other than
         *     it does not exist, then we bail out
         */
        if (stat(pzDir, &stBuf) != 0) {
            if (errno != ENOENT) {
                fprintf(stderr, zWarn, pOpts->pzProgName);
                fprintf(stderr, zNoStat, errno, strerror(errno),
                        pzDir);
                AGFREE((void*)pzDir);
                return NULL;
            }

            /*
             *  It does not exist yet, but it will be a regular file
             */
            stBuf.st_mode = S_IFREG;
        }
    }

    /*
     *  Make sure that whatever we ultimately found, that it either is
     *  or will soon be a file.
     */
    if (! S_ISREG(stBuf.st_mode)) {
        fprintf(stderr, zWarn, pOpts->pzProgName);
        fprintf(stderr, zNotFile, pzDir);
        if (free_dir_name)
            AGFREE((void*)pzDir);
        return NULL;
    }

    /*
     *  Get rid of the old file
     */
    unlink(pzDir);
    *p_free_name = free_dir_name;
    return pzDir;
}


static void
printEntry(
    FILE *     fp,
    tOptDesc * p,
    tCC*       pzLA )
{
    /*
     *  There is an argument.  Pad the name so values line up.
     *  Not disabled *OR* this got equivalenced to another opt,
     *  then use current option name.
     *  Otherwise, there must be a disablement name.
     */
    {
        char const * pz;
        if (! DISABLED_OPT(p) || (p->optEquivIndex != NO_EQUIVALENT))
            pz = p->pz_Name;
        else
            pz = p->pz_DisableName;

        fprintf(fp, "%-18s", pz);
    }
    /*
     *  IF the option is numeric only,
     *  THEN the char pointer is really the number
     */
    if (OPTST_GET_ARGTYPE(p->fOptState) == OPARG_TYPE_NUMERIC)
        fprintf(fp, "  %d\n", (int)(t_word)pzLA);

    /*
     *  OTHERWISE, FOR each line of the value text, ...
     */
    else if (pzLA == NULL)
        fputc('\n', fp);

    else {
        fputc(' ', fp); fputc(' ', fp);
        for (;;) {
            tCC* pzNl = strchr(pzLA, '\n');

            /*
             *  IF this is the last line
             *  THEN bail and print it
             */
            if (pzNl == NULL)
                break;

            /*
             *  Print the continuation and the text from the current line
             */
            (void)fwrite(pzLA, (size_t)(pzNl - pzLA), (size_t)1, fp);
            pzLA = pzNl+1; /* advance the Last Arg pointer */
            fputs("\\\n", fp);
        }

        /*
         *  Terminate the entry
         */
        fputs(pzLA, fp);
        fputc('\n', fp);
    }
}


static void
print_a_value(FILE * fp, int depth, tOptDesc * pOD, tOptionValue const * ovp)
{
    static char const bool_atr[]  = "<%1$s type=boolean>%2$s</%1$s>\n";
    static char const numb_atr[]  = "<%1$s type=integer>0x%2$lX</%1$s>\n";
    static char const type_atr[]  = "<%s type=%s>";
    static char const null_atr[]  = "<%s/>\n";

    while (--depth >= 0)
        putc(' ', fp), putc(' ', fp);

    switch (ovp->valType) {
    default:
    case OPARG_TYPE_NONE:
        fprintf(fp, null_atr, ovp->pzName);
        break;

    case OPARG_TYPE_STRING:
        print_a_string(fp, ovp->pzName, ovp->v.strVal);
        break;

    case OPARG_TYPE_ENUMERATION:
    case OPARG_TYPE_MEMBERSHIP:
        if (pOD != NULL) {
            tAoUI     opt_state = pOD->fOptState;
            uintptr_t val = pOD->optArg.argEnum;
            char const * typ = (ovp->valType == OPARG_TYPE_ENUMERATION)
                ? "keyword" : "set-membership";

            fprintf(fp, type_atr, ovp->pzName, typ);

            /*
             *  This is a magic incantation that will convert the
             *  bit flag values back into a string suitable for printing.
             */
            (*(pOD->pOptProc))(OPTPROC_RETURN_VALNAME, pOD );
            if (pOD->optArg.argString != NULL) {
                fputs(pOD->optArg.argString, fp);

                if (ovp->valType != OPARG_TYPE_ENUMERATION) {
                    /*
                     *  set membership strings get allocated
                     */
                    AGFREE((void*)pOD->optArg.argString);
                }
            }

            pOD->optArg.argEnum = val;
            pOD->fOptState = opt_state;
            fprintf(fp, close_xml, ovp->pzName);
            break;
        }
        /* FALLTHROUGH */

    case OPARG_TYPE_NUMERIC:
        fprintf(fp, numb_atr, ovp->pzName, ovp->v.longVal);
        break;

    case OPARG_TYPE_BOOLEAN:
        fprintf(fp, bool_atr, ovp->pzName,
                ovp->v.boolVal ? "true" : "false");
        break;

    case OPARG_TYPE_HIERARCHY:
        printValueList(fp, ovp->pzName, ovp->v.nestVal);
        break;
    }
}


static void
print_a_string(FILE * fp, char const * name, char const * pz)
{
    static char const open_atr[]  = "<%s>";

    fprintf(fp, open_atr, name);
    for (;;) {
        int ch = ((int)*(pz++)) & 0xFF;

        switch (ch) {
        case NUL: goto string_done;

        case '&':
        case '<':
        case '>':
#if __GNUC__ >= 4
        case 1 ... (' ' - 1):
        case ('~' + 1) ... 0xFF:
#endif
            emit_special_char(fp, ch);
            break;

        default:
#if __GNUC__ < 4
            if (  ((ch >= 1) && (ch <= (' ' - 1)))
               || ((ch >= ('~' + 1)) && (ch <= 0xFF)) ) {
                emit_special_char(fp, ch);
                break;
            }
#endif
            putc(ch, fp);
        }
    } string_done:;
    fprintf(fp, close_xml, name);
}


static void
printValueList(FILE * fp, char const * name, tArgList * al)
{
    static int depth = 1;

    int sp_ct;
    int opt_ct;
    void ** opt_list;

    if (al == NULL)
        return;
    opt_ct   = al->useCt;
    opt_list = (void **)al->apzArgs;

    if (opt_ct <= 0) {
        fprintf(fp, "<%s/>\n", name);
        return;
    }

    fprintf(fp, "<%s type=nested>\n", name);

    depth++;
    while (--opt_ct >= 0) {
        tOptionValue const * ovp = *(opt_list++);

        print_a_value(fp, depth, NULL, ovp);
    }
    depth--;

    for (sp_ct = depth; --sp_ct >= 0;)
        putc(' ', fp), putc(' ', fp);
    fprintf(fp, "</%s>\n", name);
}


static void
printHierarchy(FILE * fp, tOptDesc * p)
{
    int opt_ct;
    tArgList * al = p->optCookie;
    void ** opt_list;

    if (al == NULL)
        return;

    opt_ct   = al->useCt;
    opt_list = (void **)al->apzArgs;

    if (opt_ct <= 0)
        return;

    do  {
        tOptionValue const * base = *(opt_list++);
        tOptionValue const * ovp = optionGetValue(base, NULL);

        if (ovp == NULL)
            continue;

        fprintf(fp, "<%s type=nested>\n", p->pz_Name);

        do  {
            print_a_value(fp, 1, p, ovp);

        } while (ovp = optionNextValue(base, ovp),
                 ovp != NULL);

        fprintf(fp, "</%s>\n", p->pz_Name);
    } while (--opt_ct > 0);
}


static FILE *
openSaveFile(tOptions* pOpts)
{
    FILE*     fp;

    {
        int   free_name = 0;
        tCC*  pzFName = findFileName(pOpts, &free_name);
        if (pzFName == NULL)
            return NULL;

        fp = fopen(pzFName, "w" FOPEN_BINARY_FLAG);
        if (fp == NULL) {
            fprintf(stderr, zWarn, pOpts->pzProgName);
            fprintf(stderr, zNoCreat, errno, strerror(errno), pzFName);
            if (free_name)
                AGFREE((void*) pzFName );
            return fp;
        }

        if (free_name)
            AGFREE((void*)pzFName);
    }

    {
        char const*  pz = pOpts->pzUsageTitle;
        fputs("#  ", fp);
        do { fputc(*pz, fp); } while (*(pz++) != '\n');
    }

    {
        time_t  timeVal = time(NULL);
        char*   pzTime  = ctime(&timeVal);

        fprintf(fp, zPresetFile, pzTime);
#ifdef HAVE_ALLOCATED_CTIME
        /*
         *  The return values for ctime(), localtime(), and gmtime()
         *  normally point to static data that is overwritten by each call.
         *  The test to detect allocated ctime, so we leak the memory.
         */
        AGFREE((void*)pzTime);
#endif
    }

    return fp;
}

static void
printNoArgOpt(FILE * fp, tOptDesc * p, tOptDesc * pOD)
{
    /*
     * The aliased to argument indicates whether or not the option
     * is "disabled".  However, the original option has the name
     * string, so we get that there, not with "p".
     */
    char const * pznm =
        (DISABLED_OPT(p)) ? pOD->pz_DisableName : pOD->pz_Name;
    /*
     *  If the option was disabled and the disablement name is NULL,
     *  then the disablement was caused by aliasing.
     *  Use the name as the string to emit.
     */
    if (pznm == NULL)
        pznm = pOD->pz_Name;

    fprintf(fp, "%s\n", pznm);
}

static void
printStringArg(FILE * fp, tOptDesc * pOD)
{
    if (pOD->fOptState & OPTST_STACKED) {
        tArgList*  pAL = (tArgList*)pOD->optCookie;
        int        uct = pAL->useCt;
        tCC**      ppz = pAL->apzArgs;

        /*
         *  un-disable multiple copies of disabled options.
         */
        if (uct > 1)
            pOD->fOptState &= ~OPTST_DISABLED;

        while (uct-- > 0)
            printEntry(fp, pOD, *(ppz++));
    } else {
        printEntry(fp, pOD, pOD->optArg.argString);
    }
}

static void
printEnumArg(FILE * fp, tOptDesc * pOD)
{
    uintptr_t val = pOD->optArg.argEnum;

    /*
     *  This is a magic incantation that will convert the
     *  bit flag values back into a string suitable for printing.
     */
    (*(pOD->pOptProc))(OPTPROC_RETURN_VALNAME, pOD);
    printEntry(fp, pOD, (void*)(pOD->optArg.argString));

    pOD->optArg.argEnum = val;
}

static void
printSetMemberArg(FILE * fp, tOptDesc * pOD)
{
    uintptr_t val = pOD->optArg.argEnum;

    /*
     *  This is a magic incantation that will convert the
     *  bit flag values back into a string suitable for printing.
     */
    (*(pOD->pOptProc))(OPTPROC_RETURN_VALNAME, pOD);
    printEntry(fp, pOD, (void*)(pOD->optArg.argString));

    if (pOD->optArg.argString != NULL) {
        /*
         *  set membership strings get allocated
         */
        AGFREE((void*)pOD->optArg.argString);
        pOD->fOptState &= ~OPTST_ALLOC_ARG;
    }

    pOD->optArg.argEnum = val;
}

static void
printFileArg(FILE * fp, tOptDesc * pOD, tOptions* pOpts)
{
    /*
     *  If the cookie is not NULL, then it has the file name, period.
     *  Otherwise, if we have a non-NULL string argument, then....
     */
    if (pOD->optCookie != NULL)
        printEntry(fp, pOD, pOD->optCookie);

    else if (HAS_originalOptArgArray(pOpts)) {
        char const * orig =
            pOpts->originalOptArgArray[pOD->optIndex].argString;

        if (pOD->optArg.argString == orig)
            return;

        printEntry(fp, pOD, pOD->optArg.argString);
    }
}


/*=export_func  optionSaveFile
 *
 * what:  saves the option state to a file
 *
 * arg:   tOptions*,   pOpts,  program options descriptor
 *
 * doc:
 *
 * This routine will save the state of option processing to a file.  The name
 * of that file can be specified with the argument to the @code{--save-opts}
 * option, or by appending the @code{rcfile} attribute to the last
 * @code{homerc} attribute.  If no @code{rcfile} attribute was specified, it
 * will default to @code{.@i{programname}rc}.  If you wish to specify another
 * file, you should invoke the @code{SET_OPT_SAVE_OPTS(@i{filename})} macro.
 *
 * The recommend usage is as follows:
 * @example
 *    optionProcess(&progOptions, argc, argv);
 *    if (i_want_a_non_standard_place_for_this)
 *        SET_OPT_SAVE_OPTS("myfilename");
 *    optionSaveFile(&progOptions);
 * @end example
 *
 * err:
 *
 * If no @code{homerc} file was specified, this routine will silently return
 * and do nothing.  If the output file cannot be created or updated, a message
 * will be printed to @code{stderr} and the routine will return.
=*/
void
optionSaveFile(tOptions* pOpts)
{
    tOptDesc* pOD;
    int       ct;
    FILE*     fp = openSaveFile(pOpts);

    if (fp == NULL)
        return;

    /*
     *  FOR each of the defined options, ...
     */
    ct  = pOpts->presetOptCt;
    pOD = pOpts->pOptDesc;
    do  {
        tOptDesc*  p;

        /*
         *  IF    the option has not been defined
         *     OR it does not take an initialization value
         *     OR it is equivalenced to another option
         *  THEN continue (ignore it)
         *
         *  Equivalenced options get picked up when the equivalenced-to
         *  option is processed.
         */
        if (UNUSED_OPT(pOD))
            continue;

        if ((pOD->fOptState & OPTST_DO_NOT_SAVE_MASK) != 0)
            continue;

        if (  (pOD->optEquivIndex != NO_EQUIVALENT)
           && (pOD->optEquivIndex != pOD->optIndex))
            continue;

        /*
         *  The option argument data are found at the equivalenced-to option,
         *  but the actual option argument type comes from the original
         *  option descriptor.  Be careful!
         */
        p = ((pOD->fOptState & OPTST_EQUIVALENCE) != 0)
            ? (pOpts->pOptDesc + pOD->optActualIndex) : pOD;

        switch (OPTST_GET_ARGTYPE(pOD->fOptState)) {
        case OPARG_TYPE_NONE:
            printNoArgOpt(fp, p, pOD);
            break;

        case OPARG_TYPE_NUMERIC:
            printEntry(fp, p, (void*)(p->optArg.argInt));
            break;

        case OPARG_TYPE_STRING:
            printStringArg(fp, p);
            break;

        case OPARG_TYPE_ENUMERATION:
            printEnumArg(fp, p);
            break;

        case OPARG_TYPE_MEMBERSHIP:
            printSetMemberArg(fp, p);
            break;

        case OPARG_TYPE_BOOLEAN:
            printEntry(fp, p, p->optArg.argBool ? "true" : "false");
            break;

        case OPARG_TYPE_HIERARCHY:
            printHierarchy(fp, p);
            break;

        case OPARG_TYPE_FILE:
            printFileArg(fp, p, pOpts);
            break;

        default:
            break; /* cannot handle - skip it */
        }
    } while (pOD++, (--ct > 0));

    fclose(fp);
}
/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/save.c */
