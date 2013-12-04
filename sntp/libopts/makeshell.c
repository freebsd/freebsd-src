
/**
 * \file makeshell.c
 *
 * Time-stamp:      "2011-04-20 11:06:57 bkorb"
 *
 *  This module will interpret the options set in the tOptions
 *  structure and create a Bourne shell script capable of parsing them.
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

tOptions * optionParseShellOptions = NULL;

/* * * * * * * * * * * * * * * * * * * * *
 *
 *  Setup Format Strings
 */
static char const zStartMarker[] =
"# # # # # # # # # # -- do not modify this marker --\n#\n"
"#  DO NOT EDIT THIS SECTION";

static char const zPreamble[] =
"%s OF %s\n#\n"
"#  From here to the next `-- do not modify this marker --',\n"
"#  the text has been generated %s\n";

static char const zEndPreamble[] =
"#  From the %s option definitions\n#\n";

static char const zMultiDef[] = "\n"
"if test -z \"${%1$s_%2$s}\"\n"
"then\n"
"  %1$s_%2$s_CT=0\n"
"else\n"
"  %1$s_%2$s_CT=1\n"
"  %1$s_%2$s_1=\"${%1$s_%2$s}\"\n"
"fi\n"
"export %1$s_%2$s_CT";

static char const zSingleDef[] = "\n"
"%1$s_%2$s=\"${%1$s_%2$s-'%3$s'}\"\n"
"%1$s_%2$s_set=false\n"
"export %1$s_%2$s\n";

static char const zSingleNoDef[] = "\n"
"%1$s_%2$s=\"${%1$s_%2$s}\"\n"
"%1$s_%2$s_set=false\n"
"export %1$s_%2$s\n";

/* * * * * * * * * * * * * * * * * * * * *
 *
 *  LOOP START
 *
 *  The loop may run in either of two modes:
 *  all options are named options (loop only)
 *  regular, marked option processing.
 */
static char const zLoopCase[] = "\n"
"OPT_PROCESS=true\n"
"OPT_ARG=\"$1\"\n\n"
"while ${OPT_PROCESS} && [ $# -gt 0 ]\ndo\n"
"    OPT_ELEMENT=''\n"
"    OPT_ARG_VAL=''\n\n"
     /*
      *  'OPT_ARG' may or may not match the current $1
      */
"    case \"${OPT_ARG}\" in\n"
"    -- )\n"
"        OPT_PROCESS=false\n"
"        shift\n"
"        ;;\n\n";

static char const zLoopOnly[] = "\n"
"OPT_ARG=\"$1\"\n\n"
"while [ $# -gt 0 ]\ndo\n"
"    OPT_ELEMENT=''\n"
"    OPT_ARG_VAL=''\n\n"
"    OPT_ARG=\"${1}\"\n";

/* * * * * * * * * * * * * * * *
 *
 *  CASE SELECTORS
 *
 *  If the loop runs as a regular option loop,
 *  then we must have selectors for each acceptable option
 *  type (long option, flag character and non-option)
 */
static char const zLongSelection[] =
"    --* )\n";

static char const zFlagSelection[] =
"    -* )\n";

static char const zEndSelection[] =
"        ;;\n\n";

static char const zNoSelection[] =
"    * )\n"
"         OPT_PROCESS=false\n"
"         ;;\n"
"    esac\n\n";

/* * * * * * * * * * * * * * * *
 *
 *  LOOP END
 */
static char const zLoopEnd[] =
"    if [ -n \"${OPT_ARG_VAL}\" ]\n"
"    then\n"
"        eval %1$s_${OPT_NAME}${OPT_ELEMENT}=\"'${OPT_ARG_VAL}'\"\n"
"        export %1$s_${OPT_NAME}${OPT_ELEMENT}\n"
"    fi\n"
"done\n\n"
"unset OPT_PROCESS || :\n"
"unset OPT_ELEMENT || :\n"
"unset OPT_ARG || :\n"
"unset OPT_ARG_NEEDED || :\n"
"unset OPT_NAME || :\n"
"unset OPT_CODE || :\n"
"unset OPT_ARG_VAL || :\n%2$s";

static char const zTrailerMarker[] = "\n"
"# # # # # # # # # #\n#\n"
"#  END OF AUTOMATED OPTION PROCESSING\n"
"#\n# # # # # # # # # # -- do not modify this marker --\n";

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  OPTION SELECTION
 */
static char const zOptionCase[] =
"        case \"${OPT_CODE}\" in\n";

static char const zOptionPartName[] =
"        '%s' | \\\n";

static char const zOptionFullName[] =
"        '%s' )\n";

static char const zOptionFlag[] =
"        '%c' )\n";

static char const zOptionEndSelect[] =
"            ;;\n\n";

static char const zOptionUnknown[] =
"        * )\n"
"            echo Unknown %s: \"${OPT_CODE}\" >&2\n"
"            echo \"$%s_USAGE_TEXT\"\n"
"            exit 1\n"
"            ;;\n"
"        esac\n\n";

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  OPTION PROCESSING
 *
 *  Formats for emitting the text for handling particular options
 */
static char const zTextExit[] =
"            echo \"$%s_%s_TEXT\"\n"
"            exit 0\n";

static char const zPagedUsageExit[] =
"            echo \"$%s_LONGUSAGE_TEXT\" | ${PAGER-more}\n"
"            exit 0\n";

static char const zCmdFmt[] =
"            %s\n";

static char const zCountTest[] =
"            if [ $%1$s_%2$s_CT -ge %3$d ] ; then\n"
"                echo Error:  more than %3$d %2$s options >&2\n"
"                echo \"$%1$s_USAGE_TEXT\"\n"
"                exit 1 ; fi\n";

static char const zMultiArg[] =
"            %1$s_%2$s_CT=`expr ${%1$s_%2$s_CT} + 1`\n"
"            OPT_ELEMENT=\"_${%1$s_%2$s_CT}\"\n"
"            OPT_NAME='%2$s'\n";

static char const zSingleArg[] =
"            if [ -n \"${%1$s_%2$s}\" ] && ${%1$s_%2$s_set} ; then\n"
"                echo Error:  duplicate %2$s option >&2\n"
"                echo \"$%1$s_USAGE_TEXT\"\n"
"                exit 1 ; fi\n"
"            %1$s_%2$s_set=true\n"
"            OPT_NAME='%2$s'\n";

static char const zNoMultiArg[] =
"            %1$s_%2$s_CT=0\n"
"            OPT_ELEMENT=''\n"
"            %1$s_%2$s='%3$s'\n"
"            export %1$s_%2$s\n"
"            OPT_NAME='%2$s'\n";

static char const zNoSingleArg[] =
"            if [ -n \"${%1$s_%2$s}\" ] && ${%1$s_%2$s_set} ; then\n"
"                echo Error:  duplicate %2$s option >&2\n"
"                echo \"$%1$s_USAGE_TEXT\"\n"
"                exit 1 ; fi\n"
"            %1$s_%2$s_set=true\n"
"            %1$s_%2$s='%3$s'\n"
"            export %1$s_%2$s\n"
"            OPT_NAME='%2$s'\n";

static char const zMayArg[]  =
"            eval %1$s_%2$s${OPT_ELEMENT}=true\n"
"            export %1$s_%2$s${OPT_ELEMENT}\n"
"            OPT_ARG_NEEDED=OK\n";

static char const zMustArg[] =
"            OPT_ARG_NEEDED=YES\n";

static char const zCantArg[] =
"            eval %1$s_%2$s${OPT_ELEMENT}=true\n"
"            export %1$s_%2$s${OPT_ELEMENT}\n"
"            OPT_ARG_NEEDED=NO\n";

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  LONG OPTION PROCESSING
 *
 *  Formats for emitting the text for handling long option types
 */
static char const zLongOptInit[] =
"        OPT_CODE=`echo \"X${OPT_ARG}\"|sed 's/^X-*//'`\n"
"        shift\n"
"        OPT_ARG=\"$1\"\n\n"
"        case \"${OPT_CODE}\" in *=* )\n"
"            OPT_ARG_VAL=`echo \"${OPT_CODE}\"|sed 's/^[^=]*=//'`\n"
"            OPT_CODE=`echo \"${OPT_CODE}\"|sed 's/=.*$//'` ;; esac\n\n";

static char const zLongOptArg[] =
"        case \"${OPT_ARG_NEEDED}\" in\n"
"        NO )\n"
"            OPT_ARG_VAL=''\n"
"            ;;\n\n"
"        YES )\n"
"            if [ -z \"${OPT_ARG_VAL}\" ]\n"
"            then\n"
"                if [ $# -eq 0 ]\n"
"                then\n"
"                    echo No argument provided for ${OPT_NAME} option >&2\n"
"                    echo \"$%s_USAGE_TEXT\"\n"
"                    exit 1\n"
"                fi\n\n"
"                OPT_ARG_VAL=\"${OPT_ARG}\"\n"
"                shift\n"
"                OPT_ARG=\"$1\"\n"
"            fi\n"
"            ;;\n\n"
"        OK )\n"
"            if [ -z \"${OPT_ARG_VAL}\" ] && [ $# -gt 0 ]\n"
"            then\n"
"                case \"${OPT_ARG}\" in -* ) ;; * )\n"
"                    OPT_ARG_VAL=\"${OPT_ARG}\"\n"
"                    shift\n"
"                    OPT_ARG=\"$1\" ;; esac\n"
"            fi\n"
"            ;;\n"
"        esac\n";

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  FLAG OPTION PROCESSING
 *
 *  Formats for emitting the text for handling flag option types
 */
static char const zFlagOptInit[] =
"        OPT_CODE=`echo \"X${OPT_ARG}\" | sed 's/X-\\(.\\).*/\\1/'`\n"
"        OPT_ARG=` echo \"X${OPT_ARG}\" | sed 's/X-.//'`\n\n";

static char const zFlagOptArg[] =
"        case \"${OPT_ARG_NEEDED}\" in\n"
"        NO )\n"
"            if [ -n \"${OPT_ARG}\" ]\n"
"            then\n"
"                OPT_ARG=-\"${OPT_ARG}\"\n"
"            else\n"
"                shift\n"
"                OPT_ARG=\"$1\"\n"
"            fi\n"
"            ;;\n\n"
"        YES )\n"
"            if [ -n \"${OPT_ARG}\" ]\n"
"            then\n"
"                OPT_ARG_VAL=\"${OPT_ARG}\"\n\n"
"            else\n"
"                if [ $# -eq 0 ]\n"
"                then\n"
"                    echo No argument provided for ${OPT_NAME} option >&2\n"
"                    echo \"$%s_USAGE_TEXT\"\n"
"                    exit 1\n"
"                fi\n"
"                shift\n"
"                OPT_ARG_VAL=\"$1\"\n"
"            fi\n\n"
"            shift\n"
"            OPT_ARG=\"$1\"\n"
"            ;;\n\n"
"        OK )\n"
"            if [ -n \"${OPT_ARG}\" ]\n"
"            then\n"
"                OPT_ARG_VAL=\"${OPT_ARG}\"\n"
"                shift\n"
"                OPT_ARG=\"$1\"\n\n"
"            else\n"
"                shift\n"
"                if [ $# -gt 0 ]\n"
"                then\n"
"                    case \"$1\" in -* ) ;; * )\n"
"                        OPT_ARG_VAL=\"$1\"\n"
"                        shift ;; esac\n"
"                    OPT_ARG=\"$1\"\n"
"                fi\n"
"            fi\n"
"            ;;\n"
"        esac\n";

tSCC* pzShell = NULL;
static char*  pzLeader  = NULL;
static char*  pzTrailer = NULL;

/* = = = START-STATIC-FORWARD = = = */
static void
emit_var_text(char const * prog, char const * var, int fdin);

static void
textToVariable(tOptions * pOpts, teTextTo whichVar, tOptDesc * pOD);

static void
emitUsage(tOptions* pOpts);

static void
emitSetup(tOptions* pOpts);

static void
printOptionAction(tOptions* pOpts, tOptDesc* pOptDesc);

static void
printOptionInaction(tOptions* pOpts, tOptDesc* pOptDesc);

static void
emitFlag(tOptions* pOpts);

static void
emitMatchExpr(tCC* pzMatchName, tOptDesc* pCurOpt, tOptions* pOpts);

static void
emitLong(tOptions* pOpts);

static void
openOutput(char const* pzFile);
/* = = = END-STATIC-FORWARD = = = */

/*=export_func  optionParseShell
 * private:
 *
 * what:  Decipher a boolean value
 * arg:   + tOptions* + pOpts    + program options descriptor +
 *
 * doc:
 *  Emit a shell script that will parse the command line options.
=*/
void
optionParseShell(tOptions* pOpts)
{
    /*
     *  Check for our SHELL option now.
     *  IF the output file contains the "#!" magic marker,
     *  it will override anything we do here.
     */
    if (HAVE_GENSHELL_OPT(SHELL))
        pzShell = GENSHELL_OPT_ARG(SHELL);

    else if (! ENABLED_GENSHELL_OPT(SHELL))
        pzShell = NULL;

    else if ((pzShell = getenv("SHELL")),
             pzShell == NULL)

        pzShell = POSIX_SHELL;

    /*
     *  Check for a specified output file
     */
    if (HAVE_GENSHELL_OPT(SCRIPT))
        openOutput(GENSHELL_OPT_ARG(SCRIPT));

    emitUsage(pOpts);
    emitSetup(pOpts);

    /*
     *  There are four modes of option processing.
     */
    switch (pOpts->fOptSet & (OPTPROC_LONGOPT|OPTPROC_SHORTOPT)) {
    case OPTPROC_LONGOPT:
        fputs(zLoopCase,        stdout);

        fputs(zLongSelection,   stdout);
        fputs(zLongOptInit,     stdout);
        emitLong(pOpts);
        printf(zLongOptArg,     pOpts->pzPROGNAME);
        fputs(zEndSelection,    stdout);

        fputs(zNoSelection,     stdout);
        break;

    case 0:
        fputs(zLoopOnly,        stdout);
        fputs(zLongOptInit,     stdout);
        emitLong(pOpts);
        printf(zLongOptArg,     pOpts->pzPROGNAME);
        break;

    case OPTPROC_SHORTOPT:
        fputs(zLoopCase,        stdout);

        fputs(zFlagSelection,   stdout);
        fputs(zFlagOptInit,     stdout);
        emitFlag(pOpts);
        printf(zFlagOptArg,     pOpts->pzPROGNAME);
        fputs(zEndSelection,    stdout);

        fputs(zNoSelection,     stdout);
        break;

    case OPTPROC_LONGOPT|OPTPROC_SHORTOPT:
        fputs(zLoopCase,        stdout);

        fputs(zLongSelection,   stdout);
        fputs(zLongOptInit,     stdout);
        emitLong(pOpts);
        printf(zLongOptArg,     pOpts->pzPROGNAME);
        fputs(zEndSelection,    stdout);

        fputs(zFlagSelection,   stdout);
        fputs(zFlagOptInit,     stdout);
        emitFlag(pOpts);
        printf(zFlagOptArg,     pOpts->pzPROGNAME);
        fputs(zEndSelection,    stdout);

        fputs(zNoSelection,     stdout);
        break;
    }

    printf(zLoopEnd, pOpts->pzPROGNAME, zTrailerMarker);
    if ((pzTrailer != NULL) && (*pzTrailer != '\0'))
        fputs(pzTrailer, stdout);
    else if (ENABLED_GENSHELL_OPT(SHELL))
        printf("\nenv | grep '^%s_'\n", pOpts->pzPROGNAME);

    fflush(stdout);
    fchmod(STDOUT_FILENO, 0755);
    fclose(stdout);
    if (ferror(stdout)) {
        fputs(zOutputFail, stderr);
        exit(EXIT_FAILURE);
    }
}

#ifdef HAVE_WORKING_FORK
static void
emit_var_text(char const * prog, char const * var, int fdin)
{
    FILE * fp   = fdopen(fdin, "r" FOPEN_BINARY_FLAG);
    int    nlct = 0; /* defer newlines and skip trailing ones */

    printf("%s_%s_TEXT='", prog, var);
    if (fp == NULL)
        goto skip_text;

    for (;;) {
        int  ch = fgetc(fp);
        switch (ch) {

        case '\n':
            nlct++;
            break;

        case '\'':
            while (nlct > 0) {
                fputc('\n', stdout);
                nlct--;
            }
            fputs("'\\''", stdout);
            break;

        case EOF:
            goto endCharLoop;

        default:
            while (nlct > 0) {
                fputc('\n', stdout);
                nlct--;
            }
            fputc(ch, stdout);
            break;
        }
    } endCharLoop:;

    fclose(fp);

skip_text:

    fputs("'\n\n", stdout);
}

#endif

/*
 *  The purpose of this function is to assign "long usage", short usage
 *  and version information to a shell variable.  Rather than wind our
 *  way through all the logic necessary to emit the text directly, we
 *  fork(), have our child process emit the text the normal way and
 *  capture the output in the parent process.
 */
static void
textToVariable(tOptions * pOpts, teTextTo whichVar, tOptDesc * pOD)
{
#   define _TT_(n) static char const z ## n [] = #n;
    TEXTTO_TABLE
#   undef _TT_
#   define _TT_(n) z ## n ,
      static char const * apzTTNames[] = { TEXTTO_TABLE };
#   undef _TT_

#if ! defined(HAVE_WORKING_FORK)
    printf("%1$s_%2$s_TEXT='no %2$s text'\n",
           pOpts->pzPROGNAME, apzTTNames[ whichVar ]);
#else
    int  pipeFd[2];

    fflush(stdout);
    fflush(stderr);

    if (pipe(pipeFd) != 0) {
        fprintf(stderr, zBadPipe, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    switch (fork()) {
    case -1:
        fprintf(stderr, zForkFail, errno, strerror(errno), pOpts->pzProgName);
        exit(EXIT_FAILURE);
        break;

    case 0:
        /*
         * Send both stderr and stdout to the pipe.  No matter which
         * descriptor is used, we capture the output on the read end.
         */
        dup2(pipeFd[1], STDERR_FILENO);
        dup2(pipeFd[1], STDOUT_FILENO);
        close(pipeFd[0]);

        switch (whichVar) {
        case TT_LONGUSAGE:
            (*(pOpts->pUsageProc))(pOpts, EXIT_SUCCESS);
            /* NOTREACHED */

        case TT_USAGE:
            (*(pOpts->pUsageProc))(pOpts, EXIT_FAILURE);
            /* NOTREACHED */

        case TT_VERSION:
            if (pOD->fOptState & OPTST_ALLOC_ARG) {
                AGFREE(pOD->optArg.argString);
                pOD->fOptState &= ~OPTST_ALLOC_ARG;
            }
            pOD->optArg.argString = "c";
            optionPrintVersion(pOpts, pOD);
            /* NOTREACHED */

        default:
            exit(EXIT_FAILURE);
        }

    default:
        close(pipeFd[1]);
    }

    emit_var_text(pOpts->pzPROGNAME, apzTTNames[whichVar], pipeFd[0]);
#endif
}


static void
emitUsage(tOptions* pOpts)
{
    char zTimeBuf[AO_NAME_SIZE];

    /*
     *  First, switch stdout to the output file name.
     *  Then, change the program name to the one defined
     *  by the definitions (rather than the current
     *  executable name).  Down case the upper cased name.
     */
    if (pzLeader != NULL)
        fputs(pzLeader, stdout);

    {
        tSCC    zStdout[] = "stdout";
        tCC*    pzOutName;

        {
            time_t    curTime = time(NULL);
            struct tm*  pTime = localtime(&curTime);
            strftime(zTimeBuf, AO_NAME_SIZE, "%A %B %e, %Y at %r %Z", pTime );
        }

        if (HAVE_GENSHELL_OPT(SCRIPT))
             pzOutName = GENSHELL_OPT_ARG(SCRIPT);
        else pzOutName = zStdout;

        if ((pzLeader == NULL) && (pzShell != NULL))
            printf("#! %s\n", pzShell);

        printf(zPreamble, zStartMarker, pzOutName, zTimeBuf);
    }

    printf(zEndPreamble, pOpts->pzPROGNAME);

    /*
     *  Get a copy of the original program name in lower case and
     *  fill in an approximation of the program name from it.
     */
    {
        char *       pzPN = zTimeBuf;
        char const * pz   = pOpts->pzPROGNAME;
        char **      pp;

        for (;;) {
            if ((*pzPN++ = tolower(*pz++)) == '\0')
                break;
        }

        pp = (char **)(void *)&(pOpts->pzProgPath);
        *pp = zTimeBuf;
        pp  = (char **)(void *)&(pOpts->pzProgName);
        *pp = zTimeBuf;
    }

    textToVariable(pOpts, TT_LONGUSAGE, NULL);
    textToVariable(pOpts, TT_USAGE,     NULL);

    {
        tOptDesc* pOptDesc = pOpts->pOptDesc;
        int       optionCt = pOpts->optCt;

        for (;;) {
            if (pOptDesc->pOptProc == optionPrintVersion) {
                textToVariable(pOpts, TT_VERSION, pOptDesc);
                break;
            }

            if (--optionCt <= 0)
                break;
            pOptDesc++;
        }
    }
}


static void
emitSetup(tOptions* pOpts)
{
    tOptDesc* pOptDesc = pOpts->pOptDesc;
    int       optionCt = pOpts->presetOptCt;
    char const* pzFmt;
    char const* pzDefault;

    for (;optionCt > 0; pOptDesc++, --optionCt) {
        char zVal[16];

        /*
         *  Options that are either usage documentation or are compiled out
         *  are not to be processed.
         */
        if (SKIP_OPT(pOptDesc) || (pOptDesc->pz_NAME == NULL))
            continue;

        if (pOptDesc->optMaxCt > 1)
             pzFmt = zMultiDef;
        else pzFmt = zSingleDef;

        /*
         *  IF this is an enumeration/bitmask option, then convert the value
         *  to a string before printing the default value.
         */
        switch (OPTST_GET_ARGTYPE(pOptDesc->fOptState)) {
        case OPARG_TYPE_ENUMERATION:
            (*(pOptDesc->pOptProc))(OPTPROC_EMIT_SHELL, pOptDesc );
            pzDefault = pOptDesc->optArg.argString;
            break;

        /*
         *  Numeric and membership bit options are just printed as a number.
         */
        case OPARG_TYPE_NUMERIC:
            snprintf(zVal, sizeof(zVal), "%d",
                     (int)pOptDesc->optArg.argInt);
            pzDefault = zVal;
            break;

        case OPARG_TYPE_MEMBERSHIP:
            snprintf(zVal, sizeof(zVal), "%lu",
                     (unsigned long)pOptDesc->optArg.argIntptr);
            pzDefault = zVal;
            break;

        case OPARG_TYPE_BOOLEAN:
            pzDefault = (pOptDesc->optArg.argBool) ? "true" : "false";
            break;

        default:
            if (pOptDesc->optArg.argString == NULL) {
                if (pzFmt == zSingleDef)
                    pzFmt = zSingleNoDef;
                pzDefault = NULL;
            }
            else
                pzDefault = pOptDesc->optArg.argString;
        }

        printf(pzFmt, pOpts->pzPROGNAME, pOptDesc->pz_NAME, pzDefault);
    }
}


static void
printOptionAction(tOptions* pOpts, tOptDesc* pOptDesc)
{
    if (pOptDesc->pOptProc == optionPrintVersion)
        printf(zTextExit, pOpts->pzPROGNAME, "VERSION");

    else if (pOptDesc->pOptProc == optionPagedUsage)
        printf(zPagedUsageExit, pOpts->pzPROGNAME);

    else if (pOptDesc->pOptProc == optionLoadOpt) {
        printf(zCmdFmt, "echo 'Warning:  Cannot load options files' >&2");
        printf(zCmdFmt, "OPT_ARG_NEEDED=YES");

    } else if (pOptDesc->pz_NAME == NULL) {

        if (pOptDesc->pOptProc == NULL) {
            printf(zCmdFmt, "echo 'Warning:  Cannot save options files' "
                    ">&2");
            printf(zCmdFmt, "OPT_ARG_NEEDED=OK");
        } else
            printf(zTextExit, pOpts->pzPROGNAME, "LONGUSAGE");

    } else {
        if (pOptDesc->optMaxCt == 1)
            printf(zSingleArg, pOpts->pzPROGNAME, pOptDesc->pz_NAME);
        else {
            if ((unsigned)pOptDesc->optMaxCt < NOLIMIT)
                printf(zCountTest, pOpts->pzPROGNAME,
                       pOptDesc->pz_NAME, pOptDesc->optMaxCt);

            printf(zMultiArg, pOpts->pzPROGNAME, pOptDesc->pz_NAME);
        }

        /*
         *  Fix up the args.
         */
        if (OPTST_GET_ARGTYPE(pOptDesc->fOptState) == OPARG_TYPE_NONE) {
            printf(zCantArg, pOpts->pzPROGNAME, pOptDesc->pz_NAME);

        } else if (pOptDesc->fOptState & OPTST_ARG_OPTIONAL) {
            printf(zMayArg,  pOpts->pzPROGNAME, pOptDesc->pz_NAME);

        } else {
            fputs(zMustArg, stdout);
        }
    }
    fputs(zOptionEndSelect, stdout);
}


static void
printOptionInaction(tOptions* pOpts, tOptDesc* pOptDesc)
{
    if (pOptDesc->pOptProc == optionLoadOpt) {
        printf(zCmdFmt, "echo 'Warning:  Cannot suppress the loading of "
                "options files' >&2");

    } else if (pOptDesc->optMaxCt == 1)
        printf(zNoSingleArg, pOpts->pzPROGNAME,
               pOptDesc->pz_NAME, pOptDesc->pz_DisablePfx);
    else
        printf(zNoMultiArg, pOpts->pzPROGNAME,
               pOptDesc->pz_NAME, pOptDesc->pz_DisablePfx);

    printf(zCmdFmt, "OPT_ARG_NEEDED=NO");
    fputs(zOptionEndSelect, stdout);
}


static void
emitFlag(tOptions* pOpts)
{
    tOptDesc* pOptDesc = pOpts->pOptDesc;
    int       optionCt = pOpts->optCt;

    fputs(zOptionCase, stdout);

    for (;optionCt > 0; pOptDesc++, --optionCt) {

        if (SKIP_OPT(pOptDesc))
            continue;

        if (IS_GRAPHIC_CHAR(pOptDesc->optValue)) {
            printf(zOptionFlag, pOptDesc->optValue);
            printOptionAction(pOpts, pOptDesc);
        }
    }
    printf(zOptionUnknown, "flag", pOpts->pzPROGNAME);
}


/*
 *  Emit the match text for a long option
 */
static void
emitMatchExpr(tCC* pzMatchName, tOptDesc* pCurOpt, tOptions* pOpts)
{
    tOptDesc* pOD = pOpts->pOptDesc;
    int       oCt = pOpts->optCt;
    int       min = 1;
    char      zName[ 256 ];
    char*     pz  = zName;

    for (;;) {
        int matchCt = 0;

        /*
         *  Omit the current option, Documentation opts and compiled out opts.
         */
        if ((pOD == pCurOpt) || SKIP_OPT(pOD)){
            if (--oCt <= 0)
                break;
            pOD++;
            continue;
        }

        /*
         *  Check each character of the name case insensitively.
         *  They must not be the same.  They cannot be, because it would
         *  not compile correctly if they were.
         */
        while (  toupper(pOD->pz_Name[matchCt])
              == toupper(pzMatchName[matchCt]))
            matchCt++;

        if (matchCt > min)
            min = matchCt;

        /*
         *  Check the disablement name, too.
         */
        if (pOD->pz_DisableName != NULL) {
            matchCt = 0;
            while (  toupper(pOD->pz_DisableName[matchCt])
                  == toupper(pzMatchName[matchCt]))
                matchCt++;
            if (matchCt > min)
                min = matchCt;
        }
        if (--oCt <= 0)
            break;
        pOD++;
    }

    /*
     *  IF the 'min' is all or one short of the name length,
     *  THEN the entire string must be matched.
     */
    if (  (pzMatchName[min  ] == NUL)
       || (pzMatchName[min+1] == NUL) )
        printf(zOptionFullName, pzMatchName);

    else {
        int matchCt = 0;
        for (; matchCt <= min; matchCt++)
            *pz++ = pzMatchName[matchCt];

        for (;;) {
            *pz = NUL;
            printf(zOptionPartName, zName);
            *pz++ = pzMatchName[matchCt++];
            if (pzMatchName[matchCt] == NUL) {
                *pz = NUL;
                printf(zOptionFullName, zName);
                break;
            }
        }
    }
}


/*
 *  Emit GNU-standard long option handling code
 */
static void
emitLong(tOptions* pOpts)
{
    tOptDesc* pOD = pOpts->pOptDesc;
    int       ct  = pOpts->optCt;

    fputs(zOptionCase, stdout);

    /*
     *  do each option, ...
     */
    do  {
        /*
         *  Documentation & compiled-out options
         */
        if (SKIP_OPT(pOD))
            continue;

        emitMatchExpr(pOD->pz_Name, pOD, pOpts);
        printOptionAction(pOpts, pOD);

        /*
         *  Now, do the same thing for the disablement version of the option.
         */
        if (pOD->pz_DisableName != NULL) {
            emitMatchExpr(pOD->pz_DisableName, pOD, pOpts);
            printOptionInaction(pOpts, pOD);
        }
    } while (pOD++, --ct > 0);

    printf(zOptionUnknown, "option", pOpts->pzPROGNAME);
}


static void
openOutput(char const* pzFile)
{
    FILE* fp;
    char* pzData = NULL;
    struct stat stbf;

    do  {
        char*    pzScan;
        size_t sizeLeft;

        /*
         *  IF we cannot stat the file,
         *  THEN assume we are creating a new file.
         *       Skip the loading of the old data.
         */
        if (stat(pzFile, &stbf) != 0)
            break;

        /*
         *  The file must be a regular file
         */
        if (! S_ISREG(stbf.st_mode)) {
            fprintf(stderr, zNotFile, pzFile);
            exit(EXIT_FAILURE);
        }

        pzData = AGALOC(stbf.st_size + 1, "file data");
        fp = fopen(pzFile, "r" FOPEN_BINARY_FLAG);

        sizeLeft = (unsigned)stbf.st_size;
        pzScan   = pzData;

        /*
         *  Read in all the data as fast as our OS will let us.
         */
        for (;;) {
            int inct = fread((void*)pzScan, (size_t)1, sizeLeft, fp);
            if (inct == 0)
                break;

            pzScan   += inct;
            sizeLeft -= inct;

            if (sizeLeft == 0)
                break;
        }

        /*
         *  NUL-terminate the leader and look for the trailer
         */
        *pzScan = '\0';
        fclose(fp);
        pzScan  = strstr(pzData, zStartMarker);
        if (pzScan == NULL) {
            pzTrailer = pzData;
            break;
        }

        *(pzScan++) = NUL;
        pzScan  = strstr(pzScan, zTrailerMarker);
        if (pzScan == NULL) {
            pzTrailer = pzData;
            break;
        }

        /*
         *  Check to see if the data contains our marker.
         *  If it does, then we will skip over it
         */
        pzTrailer = pzScan + sizeof(zTrailerMarker) - 1;
        pzLeader  = pzData;
    } while (AG_FALSE);

    if (freopen(pzFile, "w" FOPEN_BINARY_FLAG, stdout) != stdout) {
        fprintf(stderr, zFreopenFail, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}


/*=export_func genshelloptUsage
 * private:
 * what: The usage function for the genshellopt generated program
 *
 * arg:  + tOptions* + pOpts    + program options descriptor +
 * arg:  + int       + exitCode + usage text type to produce +
 *
 * doc:
 *  This function is used to create the usage strings for the option
 *  processing shell script code.  Two child processes are spawned
 *  each emitting the usage text in either the short (error exit)
 *  style or the long style.  The generated program will capture this
 *  and create shell script variables containing the two types of text.
=*/
void
genshelloptUsage(tOptions * pOpts, int exitCode)
{
#if ! defined(HAVE_WORKING_FORK)
    optionUsage(pOpts, exitCode);
#else
    /*
     *  IF not EXIT_SUCCESS,
     *  THEN emit the short form of usage.
     */
    if (exitCode != EXIT_SUCCESS)
        optionUsage(pOpts, exitCode);
    fflush(stderr);
    fflush(stdout);
    if (ferror(stdout) || ferror(stderr))
        exit(EXIT_FAILURE);

    option_usage_fp = stdout;

    /*
     *  First, print our usage
     */
    switch (fork()) {
    case -1:
        optionUsage(pOpts, EXIT_FAILURE);
        /* NOTREACHED */

    case 0:
        pagerState = PAGER_STATE_CHILD;
        optionUsage(pOpts, EXIT_SUCCESS);
        /* NOTREACHED */
        _exit(EXIT_FAILURE);

    default:
    {
        int  sts;
        wait(&sts);
    }
    }

    /*
     *  Generate the pzProgName, since optionProcess() normally
     *  gets it from the command line
     */
    {
        char *  pz;
        char ** pp = (char **)(void *)&(optionParseShellOptions->pzProgName);
        AGDUPSTR(pz, optionParseShellOptions->pzPROGNAME, "program name");
        *pp = pz;
        while (*pz != NUL) {
            *pz = tolower(*pz);
            pz++;
        }
    }

    /*
     *  Separate the makeshell usage from the client usage
     */
    fprintf(option_usage_fp, zGenshell, optionParseShellOptions->pzProgName);
    fflush(option_usage_fp);

    /*
     *  Now, print the client usage.
     */
    switch (fork()) {
    case 0:
        pagerState = PAGER_STATE_CHILD;
        /*FALLTHROUGH*/
    case -1:
        optionUsage(optionParseShellOptions, EXIT_FAILURE);

    default:
    {
        int  sts;
        wait(&sts);
    }
    }

    fflush(stdout);
    if (ferror(stdout)) {
        fputs(zOutputFail, stderr);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
#endif
}

/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/makeshell.c */
