
/*
 * \file usage.c
 *
 * Time-stamp:      "2011-02-01 14:42:37 bkorb"
 *
 *  This module implements the default usage procedure for
 *  Automated Options.  It may be overridden, of course.
 *
 *  Sort options:
    --start=END-[S]TATIC-FORWARD --patt='^/\*($|[^:])' \
    --out=xx.c key='^[a-zA-Z0-9_]+\(' --trail='^/\*:' \
    --spac=2 --input=usage.c
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

#define OPTPROC_L_N_S  (OPTPROC_LONGOPT | OPTPROC_SHORTOPT)

/* = = = START-STATIC-FORWARD = = = */
static void
set_usage_flags(tOptions * opts, char const * flg_txt);

static inline ag_bool
do_gnu_usage(tOptions * pOpts);

static inline ag_bool
skip_misuse_usage(tOptions * pOpts);

static void
print_usage_details(tOptions * opts, int exit_code);

static void
prt_extd_usage(tOptions * pOptions, tOptDesc * pOD, arg_types_t * pAT);

static void
prt_ini_list(char const * const * papz, ag_bool * pInitIntro,
             char const * pzRc, char const * pzPN);

static void
prt_preamble(tOptions * pOptions, tOptDesc * pOD, arg_types_t * pAT);

static void
prt_one_usage(tOptions * pOptions, tOptDesc * pOD, arg_types_t * pAT);

static void
prt_opt_usage(tOptions * pOpts, int ex_code, char const * pOptTitle);

static void
prt_prog_detail(tOptions* pOptions);

static int
setGnuOptFmts(tOptions* pOpts, tCC** ppT);

static int
setStdOptFmts(tOptions* pOpts, tCC** ppT);
/* = = = END-STATIC-FORWARD = = = */

/*
 *  NB: no entry may be a prefix of another entry
 */
#define AOFLAG_TABLE                            \
    _aof_(gnu,             OPTPROC_GNUUSAGE )   \
    _aof_(autoopts,        ~OPTPROC_GNUUSAGE)   \
    _aof_(no_misuse_usage, OPTPROC_MISUSE   )   \
    _aof_(misuse_usage,    ~OPTPROC_MISUSE  )

static void
set_usage_flags(tOptions * opts, char const * flg_txt)
{
    typedef struct {
        size_t          fnm_len;
        uint32_t        fnm_mask;
        char const *    fnm_name;
    } ao_flag_names_t;

#   define _aof_(_n, _f)   AOUF_ ## _n ## _ID,
    typedef enum { AOFLAG_TABLE AOUF_COUNT } ao_flag_id_t;
#   undef  _aof_

#   define _aof_(_n, _f)   AOUF_ ## _n = (1 << AOUF_ ## _n ## _ID),
    typedef enum { AOFLAG_TABLE } ao_flags_t;
#   undef  _aof_

#   define _aof_(_n, _f)   { sizeof(#_n)-1, _f, #_n },
    static ao_flag_names_t const fn_table[AOUF_COUNT] = {
        AOFLAG_TABLE
    };
#   undef  _aof_

    ao_flags_t flg = 0;

    if (flg_txt == NULL) {
        flg_txt = getenv("AUTOOPTS_USAGE");
        if (flg_txt == NULL)
            return;
    }

    while (IS_WHITESPACE_CHAR(*flg_txt))  flg_txt++;
    if (*flg_txt == NUL)
        return;

    for (;;) {
        int ix = 0;
        ao_flag_names_t const * fnt = fn_table;

        for (;;) {
            if (strneqvcmp(flg_txt, fnt->fnm_name, fnt->fnm_len) == 0)
                break;
            if (++ix >= AOUF_COUNT)
                return;
            fnt++;
        }

        /*
         *  Make sure we have a full match.  Look for whitespace,
         *  a comma, or a NUL byte.
         */
        if (! IS_END_LIST_ENTRY_CHAR(flg_txt[fnt->fnm_len]))
            return;

        flg |= 1 << ix;
        flg_txt  += fnt->fnm_len;
        while (IS_WHITESPACE_CHAR(*flg_txt))  flg_txt++;

        if (*flg_txt == NUL)
            break;

        if (*flg_txt == ',') {
            /*
             *  skip the comma and following white space
             */
            while (IS_WHITESPACE_CHAR(*++flg_txt))  ;
            if (*flg_txt == NUL)
                break;
        }
    }

    {
        ao_flag_names_t const * fnm = fn_table;

        while (flg != 0) {
            if ((flg & 1) != 0) {
                if ((fnm->fnm_mask & OPTPROC_LONGOPT) != 0)
                     opts->fOptSet &= fnm->fnm_mask;
                else opts->fOptSet |= fnm->fnm_mask;
            }
            flg >>= 1;
            fnm++;
        }
    }
}

/*
 *  Figure out if we should try to format usage text sort-of like
 *  the way many GNU programs do.
 */
static inline ag_bool
do_gnu_usage(tOptions * pOpts)
{
    return (pOpts->fOptSet & OPTPROC_GNUUSAGE) ? AG_TRUE : AG_FALSE;
}

/*
 *  Figure out if we should try to format usage text sort-of like
 *  the way many GNU programs do.
 */
static inline ag_bool
skip_misuse_usage(tOptions * pOpts)
{
    return (pOpts->fOptSet & OPTPROC_MISUSE) ? AG_TRUE : AG_FALSE;
}


/*=export_func  optionOnlyUsage
 *
 * what:  Print usage text for just the options
 * arg:   + tOptions*   + pOpts    + program options descriptor +
 * arg:   + int         + ex_code  + exit code for calling exit(3) +
 *
 * doc:
 *  This routine will print only the usage for each option.
 *  This function may be used when the emitted usage must incorporate
 *  information not available to AutoOpts.
=*/
void
optionOnlyUsage(tOptions * pOpts, int ex_code)
{
    char const * pOptTitle = NULL;

    set_usage_flags(pOpts, NULL);
    if ((ex_code != EXIT_SUCCESS) &&
        skip_misuse_usage(pOpts))
        return;

    /*
     *  Determine which header and which option formatting strings to use
     */
    if (do_gnu_usage(pOpts)) {
        (void)setGnuOptFmts(pOpts, &pOptTitle);
    }
    else {
        (void)setStdOptFmts(pOpts, &pOptTitle);
    }

    prt_opt_usage(pOpts, ex_code, pOptTitle);

    fflush(option_usage_fp);
    if (ferror(option_usage_fp) != 0) {
        fputs(zOutputFail, stderr);
        exit(EXIT_FAILURE);
    }
}

static void
print_usage_details(tOptions * opts, int exit_code)
{
    {
        char const * pOptTitle = NULL;

        /*
         *  Determine which header and which option formatting strings to use
         */
        if (do_gnu_usage(opts)) {
            int flen = setGnuOptFmts(opts, &pOptTitle);
            sprintf(zOptFmtLine, zFmtFmt, flen);
            fputc('\n', option_usage_fp);
        }
        else {
            int flen = setStdOptFmts(opts, &pOptTitle);
            sprintf(zOptFmtLine, zFmtFmt, flen);

            /*
             *  When we exit with EXIT_SUCCESS and the first option is a doc
             *  option, we do *NOT* want to emit the column headers.
             *  Otherwise, we do.
             */
            if (  (exit_code != EXIT_SUCCESS)
               || ((opts->pOptDesc->fOptState & OPTST_DOCUMENT) == 0) )

                fputs(pOptTitle, option_usage_fp);
        }

        prt_opt_usage(opts, exit_code, pOptTitle);
    }

    /*
     *  Describe the mechanics of denoting the options
     */
    switch (opts->fOptSet & OPTPROC_L_N_S) {
    case OPTPROC_L_N_S:     fputs(zFlagOkay, option_usage_fp); break;
    case OPTPROC_SHORTOPT:  break;
    case OPTPROC_LONGOPT:   fputs(zNoFlags,  option_usage_fp); break;
    case 0:                 fputs(zOptsOnly, option_usage_fp); break;
    }

    if ((opts->fOptSet & OPTPROC_NUM_OPT) != 0)
        fputs(zNumberOpt, option_usage_fp);

    if ((opts->fOptSet & OPTPROC_REORDER) != 0)
        fputs(zReorder, option_usage_fp);

    if (opts->pzExplain != NULL)
        fputs(opts->pzExplain, option_usage_fp);

    /*
     *  IF the user is asking for help (thus exiting with SUCCESS),
     *  THEN see what additional information we can provide.
     */
    if (exit_code == EXIT_SUCCESS)
        prt_prog_detail(opts);

    /*
     * Give bug notification preference to the packager information
     */
    if (HAS_pzPkgDataDir(opts) && (opts->pzPackager != NULL))
        fputs(opts->pzPackager, option_usage_fp);

    else if (opts->pzBugAddr != NULL)
        fprintf(option_usage_fp, zPlsSendBugs, opts->pzBugAddr);

    fflush(option_usage_fp);

    if (ferror(option_usage_fp) != 0) {
        fputs(zOutputFail, stderr);
        exit(EXIT_FAILURE);
    }
}


/*=export_func  optionUsage
 * private:
 *
 * what:  Print usage text
 * arg:   + tOptions* + pOptions + program options descriptor +
 * arg:   + int       + exitCode + exit code for calling exit(3) +
 *
 * doc:
 *  This routine will print usage in both GNU-standard and AutoOpts-expanded
 *  formats.  The descriptor specifies the default, but AUTOOPTS_USAGE will
 *  over-ride this, providing the value of it is set to either "gnu" or
 *  "autoopts".  This routine will @strong{not} return.
 *
 *  If "exitCode" is "EX_USAGE" (normally 64), then output will to to stdout
 *  and the actual exit code will be "EXIT_SUCCESS".
=*/
void
optionUsage(tOptions * pOptions, int usage_exit_code)
{
    int exit_code =
        (usage_exit_code == EX_USAGE) ? EXIT_SUCCESS : usage_exit_code;

    displayEnum = AG_FALSE;

    /*
     *  Paged usage will preset option_usage_fp to an output file.
     *  If it hasn't already been set, then set it to standard output
     *  on successful exit (help was requested), otherwise error out.
     *
     *  Test the version before obtaining pzFullUsage or pzShortUsage.
     *  These fields do not exist before revision 30.
     */
    {
        char const * pz;

        if (exit_code == EXIT_SUCCESS) {
            pz = (pOptions->structVersion >= 30 * 4096)
                ? pOptions->pzFullUsage : NULL;

            if (option_usage_fp == NULL)
                option_usage_fp = stdout;
        } else {
            pz = (pOptions->structVersion >= 30 * 4096)
                ? pOptions->pzShortUsage : NULL;

            if (option_usage_fp == NULL)
                option_usage_fp = stderr;
        }

        if (pz != NULL) {
            fputs(pz, option_usage_fp);
            exit(exit_code);
        }
    }

    fprintf(option_usage_fp, pOptions->pzUsageTitle, pOptions->pzProgName);
    set_usage_flags(pOptions, NULL);

    if ((exit_code == EXIT_SUCCESS) ||
        (! skip_misuse_usage(pOptions)))

        print_usage_details(pOptions, usage_exit_code);

    exit(exit_code);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *   PER OPTION TYPE USAGE INFORMATION
 */
static void
prt_extd_usage(tOptions * pOptions, tOptDesc * pOD, arg_types_t * pAT)
{
    /*
     *  IF there are option conflicts or dependencies,
     *  THEN print them here.
     */
    if (  (pOD->pOptMust != NULL)
       || (pOD->pOptCant != NULL) ) {

        fputs(zTabHyp, option_usage_fp);

        /*
         *  DEPENDENCIES:
         */
        if (pOD->pOptMust != NULL) {
            const int* pOptNo = pOD->pOptMust;

            fputs(zReqThese, option_usage_fp);
            for (;;) {
                fprintf(option_usage_fp, zTabout,
                        pOptions->pOptDesc[*pOptNo].pz_Name);
                if (*++pOptNo == NO_EQUIVALENT)
                    break;
            }

            if (pOD->pOptCant != NULL)
                fputs(zTabHypAnd, option_usage_fp);
        }

        /*
         *  CONFLICTS:
         */
        if (pOD->pOptCant != NULL) {
            const int* pOptNo = pOD->pOptCant;

            fputs(zProhib, option_usage_fp);
            for (;;) {
                fprintf(option_usage_fp, zTabout,
                        pOptions->pOptDesc[*pOptNo].pz_Name);
                if (*++pOptNo == NO_EQUIVALENT)
                    break;
            }
        }
    }

    /*
     *  IF there is a disablement string
     *  THEN print the disablement info
     */
    if (pOD->pz_DisableName != NULL )
        fprintf(option_usage_fp, zDis, pOD->pz_DisableName);

    /*
     *  Check for argument types that have callbacks with magical properties
     */
    switch (OPTST_GET_ARGTYPE(pOD->fOptState)) {
    case OPARG_TYPE_NUMERIC:
        /*
         *  IF the numeric option has a special callback,
         *  THEN call it, requesting the range or other special info
         */
        if (  (pOD->pOptProc != NULL)
           && (pOD->pOptProc != optionNumericVal) ) {
            (*(pOD->pOptProc))(OPTPROC_EMIT_USAGE, pOD);
        }
        break;

    case OPARG_TYPE_FILE:
        (*(pOD->pOptProc))(OPTPROC_EMIT_USAGE, pOD);
        break;
    }

    /*
     *  IF the option defaults to being enabled,
     *  THEN print that out
     */
    if (pOD->fOptState & OPTST_INITENABLED)
        fputs(zEnab, option_usage_fp);

    /*
     *  IF  the option is in an equivalence class
     *        AND not the designated lead
     *  THEN print equivalence and leave it at that.
     */
    if (  (pOD->optEquivIndex != NO_EQUIVALENT)
       && (pOD->optEquivIndex != pOD->optActualIndex )  )  {
        fprintf(option_usage_fp, zAlt,
                 pOptions->pOptDesc[ pOD->optEquivIndex ].pz_Name);
        return;
    }

    /*
     *  IF this particular option can NOT be preset
     *    AND some form of presetting IS allowed,
     *    AND it is not an auto-managed option (e.g. --help, et al.)
     *  THEN advise that this option may not be preset.
     */
    if (  ((pOD->fOptState & OPTST_NO_INIT) != 0)
       && (  (pOptions->papzHomeList != NULL)
          || (pOptions->pzPROGNAME != NULL)
          )
       && (pOD->optIndex < pOptions->presetOptCt)
       )

        fputs(zNoPreset, option_usage_fp);

    /*
     *  Print the appearance requirements.
     */
    if (OPTST_GET_ARGTYPE(pOD->fOptState) == OPARG_TYPE_MEMBERSHIP)
        fputs(zMembers, option_usage_fp);

    else switch (pOD->optMinCt) {
    case 1:
    case 0:
        switch (pOD->optMaxCt) {
        case 0:       fputs(zPreset, option_usage_fp); break;
        case NOLIMIT: fputs(zNoLim, option_usage_fp);  break;
        case 1:       break;
            /*
             * IF the max is more than one but limited, print "UP TO" message
             */
        default:      fprintf(option_usage_fp, zUpTo, pOD->optMaxCt);  break;
        }
        break;

    default:
        /*
         *  More than one is required.  Print the range.
         */
        fprintf(option_usage_fp, zMust, pOD->optMinCt, pOD->optMaxCt);
    }

    if (  NAMED_OPTS(pOptions)
       && (pOptions->specOptIdx.default_opt == pOD->optIndex))
        fputs(zDefaultOpt, option_usage_fp);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *   Figure out where all the initialization files might live.
 *   This requires translating some environment variables and
 *   testing to see if a name is a directory or a file.  It's
 *   squishy, but important to tell users how to find these files.
 */
static void
prt_ini_list(char const * const * papz, ag_bool * pInitIntro,
             char const * pzRc, char const * pzPN)
{
    char zPath[AG_PATH_MAX+1];

    if (papz == NULL)
        return;

    fputs(zPresetIntro, option_usage_fp);
    *pInitIntro = AG_FALSE;

    for (;;) {
        char const * pzPath = *(papz++);
        char const * pzReal = zPath;

        if (pzPath == NULL)
            break;

        /*
         * Ignore any invalid paths
         */
        if (! optionMakePath(zPath, (int)sizeof(zPath), pzPath, pzPN))
            pzReal = pzPath;

        /*
         * Expand paths that are relative to the executable or installation
         * directories.  Leave alone paths that use environment variables.
         */
        else if ((*pzPath == '$')
                 && ((pzPath[1] == '$') || (pzPath[1] == '@')))
            pzPath = pzReal;

        /*
         *  Print the name of the "homerc" file.  If the "rcfile" name is
         *  not empty, we may or may not print that, too...
         */
        fprintf(option_usage_fp, zPathFmt, pzPath);
        if (*pzRc != NUL) {
            struct stat sb;

            /*
             *  IF the "homerc" file is a directory,
             *  then append the "rcfile" name.
             */
            if ((stat(pzReal, &sb) == 0) && S_ISDIR(sb.st_mode)) {
                fputc(DIRCH, option_usage_fp);
                fputs(pzRc,  option_usage_fp);
            }
        }

        fputc('\n', option_usage_fp);
    }
}


static void
prt_preamble(tOptions * pOptions, tOptDesc * pOD, arg_types_t * pAT)
{
    /*
     *  Flag prefix: IF no flags at all, then omit it.  If not printable
     *  (not allowed for this option), then blank, else print it.
     *  Follow it with a comma if we are doing GNU usage and long
     *  opts are to be printed too.
     */
    if ((pOptions->fOptSet & OPTPROC_SHORTOPT) == 0)
        fputs(pAT->pzSpc, option_usage_fp);

    else if (! IS_GRAPHIC_CHAR(pOD->optValue)) {
        if (  (pOptions->fOptSet & (OPTPROC_GNUUSAGE|OPTPROC_LONGOPT))
           == (OPTPROC_GNUUSAGE|OPTPROC_LONGOPT))
            fputc(' ', option_usage_fp);
        fputs(pAT->pzNoF, option_usage_fp);

    } else {
        fprintf(option_usage_fp, "   -%c", pOD->optValue);
        if (  (pOptions->fOptSet & (OPTPROC_GNUUSAGE|OPTPROC_LONGOPT))
           == (OPTPROC_GNUUSAGE|OPTPROC_LONGOPT))
            fputs(", ", option_usage_fp);
    }
}

/*
 *  Print the usage information for a single option.
 */
static void
prt_one_usage(tOptions * pOptions, tOptDesc * pOD, arg_types_t * pAT)
{
    prt_preamble(pOptions, pOD, pAT);

    {
        char z[ 80 ];
        char const *  pzArgType;

        /*
         *  Determine the argument type string first on its usage, then,
         *  when the option argument is required, base the type string on the
         *  argument type.
         */
        if (pOD->fOptState & OPTST_ARG_OPTIONAL) {
            pzArgType = pAT->pzOpt;

        } else switch (OPTST_GET_ARGTYPE(pOD->fOptState)) {
        case OPARG_TYPE_NONE:        pzArgType = pAT->pzNo;   break;
        case OPARG_TYPE_ENUMERATION: pzArgType = pAT->pzKey;  break;
        case OPARG_TYPE_FILE:        pzArgType = pAT->pzFile; break;
        case OPARG_TYPE_MEMBERSHIP:  pzArgType = pAT->pzKeyL; break;
        case OPARG_TYPE_BOOLEAN:     pzArgType = pAT->pzBool; break;
        case OPARG_TYPE_NUMERIC:     pzArgType = pAT->pzNum;  break;
        case OPARG_TYPE_HIERARCHY:   pzArgType = pAT->pzNest; break;
        case OPARG_TYPE_STRING:      pzArgType = pAT->pzStr;  break;
        case OPARG_TYPE_TIME:        pzArgType = pAT->pzTime; break;
        default:                     goto bogus_desc;
        }

        snprintf(z, sizeof(z), pAT->pzOptFmt, pzArgType, pOD->pz_Name,
                 (pOD->optMinCt != 0) ? pAT->pzReq : pAT->pzOpt);

        fprintf(option_usage_fp, zOptFmtLine, z, pOD->pzText);

        switch (OPTST_GET_ARGTYPE(pOD->fOptState)) {
        case OPARG_TYPE_ENUMERATION:
        case OPARG_TYPE_MEMBERSHIP:
            displayEnum = (pOD->pOptProc != NULL) ? AG_TRUE : displayEnum;
        }
    }
    return;

 bogus_desc:
    fprintf(stderr, zInvalOptDesc, pOD->pz_Name);
    exit(EX_SOFTWARE);
}


/*
 *  Print out the usage information for just the options.
 */
static void
prt_opt_usage(tOptions * pOpts, int ex_code, char const * pOptTitle)
{
    int         ct     = pOpts->optCt;
    int         optNo  = 0;
    tOptDesc *  pOD    = pOpts->pOptDesc;
    int         docCt  = 0;

    do  {
        if ((pOD->fOptState & OPTST_NO_USAGE_MASK) != 0) {

            /*
             * IF      this is a compiled-out option
             *   *AND* usage was requested with "omitted-usage"
             *   *AND* this is NOT abbreviated usage
             * THEN display this option.
             */
            if (  (pOD->fOptState == (OPTST_OMITTED | OPTST_NO_INIT))
               && (pOD->pz_Name != NULL)
               && (ex_code == EXIT_SUCCESS))  {

                char const * why_pz =
                    (pOD->pzText == NULL) ? zDisabledWhy : pOD->pzText;
                prt_preamble(pOpts, pOD, &argTypes);
                fprintf(option_usage_fp, zDisabledOpt, pOD->pz_Name, why_pz);
            }

            continue;
        }

        if ((pOD->fOptState & OPTST_DOCUMENT) != 0) {
            if (ex_code == EXIT_SUCCESS) {
                fprintf(option_usage_fp, argTypes.pzBrk, pOD->pzText,
                        pOptTitle);
                docCt++;
            }

            continue;
        }

        /*
         *  IF       this is the first auto-opt maintained option
         *    *AND*  we are doing a full help
         *    *AND*  there are documentation options
         *    *AND*  the last one was not a doc option,
         *  THEN document that the remaining options are not user opts
         */
        if (  (pOpts->presetOptCt == optNo)
           && (ex_code == EXIT_SUCCESS)
           && (docCt > 0)
           && ((pOD[-1].fOptState & OPTST_DOCUMENT) == 0) )
            fprintf(option_usage_fp, argTypes.pzBrk, zAuto, pOptTitle);

        prt_one_usage(pOpts, pOD, &argTypes);

        /*
         *  IF we were invoked because of the --help option,
         *  THEN print all the extra info
         */
        if (ex_code == EXIT_SUCCESS)
            prt_extd_usage(pOpts, pOD, &argTypes);

    }  while (pOD++, optNo++, (--ct > 0));

    fputc('\n', option_usage_fp);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *   PROGRAM DETAILS
 */
static void
prt_prog_detail(tOptions* pOptions)
{
    ag_bool  initIntro = AG_TRUE;

    /*
     *  Display all the places we look for config files
     */
    prt_ini_list(pOptions->papzHomeList, &initIntro,
                 pOptions->pzRcName, pOptions->pzProgPath);

    /*
     *  Let the user know about environment variable settings
     */
    if ((pOptions->fOptSet & OPTPROC_ENVIRON) != 0) {
        if (initIntro)
            fputs(zPresetIntro, option_usage_fp);

        fprintf(option_usage_fp, zExamineFmt, pOptions->pzPROGNAME);
    }

    /*
     *  IF we found an enumeration,
     *  THEN hunt for it again.  Call the handler proc with a NULL
     *       option struct pointer.  That tells it to display the keywords.
     */
    if (displayEnum) {
        int        ct     = pOptions->optCt;
        int        optNo  = 0;
        tOptDesc*  pOD    = pOptions->pOptDesc;

        fputc('\n', option_usage_fp);
        fflush(option_usage_fp);
        do  {
            switch (OPTST_GET_ARGTYPE(pOD->fOptState)) {
            case OPARG_TYPE_ENUMERATION:
            case OPARG_TYPE_MEMBERSHIP:
                (*(pOD->pOptProc))(OPTPROC_EMIT_USAGE, pOD);
            }
        }  while (pOD++, optNo++, (--ct > 0));
    }

    /*
     *  If there is a detail string, now is the time for that.
     */
    if (pOptions->pzDetail != NULL)
        fputs(pOptions->pzDetail, option_usage_fp);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *   OPTION LINE FORMATTING SETUP
 *
 *  The "OptFmt" formats receive three arguments:
 *  1.  the type of the option's argument
 *  2.  the long name of the option
 *  3.  "YES" or "no ", depending on whether or not the option must appear
 *      on the command line.
 *  These formats are used immediately after the option flag (if used) has
 *  been printed.
 *
 *  Set up the formatting for GNU-style output
 */
static int
setGnuOptFmts(tOptions* pOpts, tCC** ppT)
{
    int  flen = 22;
    *ppT = zNoRq_ShrtTtl;

    argTypes.pzStr  = zGnuStrArg;
    argTypes.pzReq  = zOneSpace;
    argTypes.pzNum  = zGnuNumArg;
    argTypes.pzKey  = zGnuKeyArg;
    argTypes.pzKeyL = zGnuKeyLArg;
    argTypes.pzTime = zGnuTimeArg;
    argTypes.pzFile = zGnuFileArg;
    argTypes.pzBool = zGnuBoolArg;
    argTypes.pzNest = zGnuNestArg;
    argTypes.pzOpt  = zGnuOptArg;
    argTypes.pzNo   = zOneSpace;
    argTypes.pzBrk  = zGnuBreak;
    argTypes.pzNoF  = zSixSpaces;
    argTypes.pzSpc  = zThreeSpaces;

    switch (pOpts->fOptSet & OPTPROC_L_N_S) {
    case OPTPROC_L_N_S:    argTypes.pzOptFmt = zGnuOptFmt;     break;
    case OPTPROC_LONGOPT:  argTypes.pzOptFmt = zGnuOptFmt;     break;
    case 0:                argTypes.pzOptFmt = zGnuOptFmt + 2; break;
    case OPTPROC_SHORTOPT:
        argTypes.pzOptFmt = zShrtGnuOptFmt;
        zGnuStrArg[0] = zGnuNumArg[0] = zGnuKeyArg[0] = zGnuBoolArg[0] = ' ';
        argTypes.pzOpt = " [arg]";
        flen = 8;
        break;
    }

    return flen;
}


/*
 *  Standard (AutoOpts normal) option line formatting
 */
static int
setStdOptFmts(tOptions* pOpts, tCC** ppT)
{
    int  flen = 0;

    argTypes.pzStr  = zStdStrArg;
    argTypes.pzReq  = zStdReqArg;
    argTypes.pzNum  = zStdNumArg;
    argTypes.pzKey  = zStdKeyArg;
    argTypes.pzKeyL = zStdKeyLArg;
    argTypes.pzTime = zStdTimeArg;
    argTypes.pzFile = zStdFileArg;
    argTypes.pzBool = zStdBoolArg;
    argTypes.pzNest = zStdNestArg;
    argTypes.pzOpt  = zStdOptArg;
    argTypes.pzNo   = zStdNoArg;
    argTypes.pzBrk  = zStdBreak;
    argTypes.pzNoF  = zFiveSpaces;
    argTypes.pzSpc  = zTwoSpaces;

    switch (pOpts->fOptSet & (OPTPROC_NO_REQ_OPT | OPTPROC_SHORTOPT)) {
    case (OPTPROC_NO_REQ_OPT | OPTPROC_SHORTOPT):
        *ppT = zNoRq_ShrtTtl;
        argTypes.pzOptFmt = zNrmOptFmt;
        flen = 19;
        break;

    case OPTPROC_NO_REQ_OPT:
        *ppT = zNoRq_NoShrtTtl;
        argTypes.pzOptFmt = zNrmOptFmt;
        flen = 19;
        break;

    case OPTPROC_SHORTOPT:
        *ppT = zReq_ShrtTtl;
        argTypes.pzOptFmt = zReqOptFmt;
        flen = 24;
        break;

    case 0:
        *ppT = zReq_NoShrtTtl;
        argTypes.pzOptFmt = zReqOptFmt;
        flen = 24;
    }

    return flen;
}


/*:
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/usage.c */
