
/*
 *  $Id: makeshell.c,v 4.20 2007/02/04 17:44:12 bkorb Exp $
 * Time-stamp:      "2007-01-27 06:05:45 bkorb"
 *
 *  This module will interpret the options set in the tOptions
 *  structure and create a Bourne shell script capable of parsing them.
 */

/*
 *  Automated Options copyright 1992-2007 Bruce Korb
 *
 *  Automated Options is free software.
 *  You may redistribute it and/or modify it under the terms of the
 *  GNU General Public License, as published by the Free Software
 *  Foundation; either version 2, or (at your option) any later version.
 *
 *  Automated Options is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Automated Options.  See the file "COPYING".  If not,
 *  write to:  The Free Software Foundation, Inc.,
 *             51 Franklin Street, Fifth Floor,
 *             Boston, MA  02110-1301, USA.
 *
 * As a special exception, Bruce Korb gives permission for additional
 * uses of the text contained in his release of AutoOpts.
 *
 * The exception is that, if you link the AutoOpts library with other
 * files to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public License.
 * Your use of that executable is in no way restricted on account of
 * linking the AutoOpts library code into it.
 *
 * This exception does not however invalidate any other reasons why
 * the executable file might be covered by the GNU General Public License.
 *
 * This exception applies only to the code released by Bruce Korb under
 * the name AutoOpts.  If you copy code from other sources under the
 * General Public License into a copy of AutoOpts, as the General Public
 * License permits, the exception does not apply to the code that you add
 * in this way.  To avoid misleading anyone as to the status of such
 * modified files, you must delete this exception notice from them.
 *
 * If you write modifications of your own for AutoOpts, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 */

tOptions*  pShellParseOptions = NULL;

/* * * * * * * * * * * * * * * * * * * * *
 *
 *  Setup Format Strings
 */
tSCC zStartMarker[] =
"# # # # # # # # # # -- do not modify this marker --\n#\n"
"#  DO NOT EDIT THIS SECTION";

tSCC zPreamble[] =
"%s OF %s\n#\n"
"#  From here to the next `-- do not modify this marker --',\n"
"#  the text has been generated %s\n";

tSCC zEndPreamble[] =
"#  From the %s option definitions\n#\n";

tSCC zMultiDef[] = "\n"
"if test -z \"${%1$s_%2$s}\"\n"
"then\n"
"  %1$s_%2$s_CT=0\n"
"else\n"
"  %1$s_%2$s_CT=1\n"
"  %1$s_%2$s_1=\"${%1$s_%2$s}\"\n"
"fi\n"
"export %1$s_%2$s_CT";

tSCC zSingleDef[] = "\n"
"%1$s_%2$s=\"${%1$s_%2$s-'%3$s'}\"\n"
"%1$s_%2$s_set=false\n"
"export %1$s_%2$s\n";

tSCC zSingleNoDef[] = "\n"
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
tSCC zLoopCase[] = "\n"
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

tSCC zLoopOnly[] = "\n"
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
tSCC zLongSelection[] =
"    --* )\n";

tSCC zFlagSelection[] =
"    -* )\n";

tSCC zEndSelection[] =
"        ;;\n\n";

tSCC zNoSelection[] =
"    * )\n"
"         OPT_PROCESS=false\n"
"         ;;\n"
"    esac\n\n";

/* * * * * * * * * * * * * * * *
 *
 *  LOOP END
 */
tSCC zLoopEnd[] =
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

tSCC zTrailerMarker[] = "\n"
"# # # # # # # # # #\n#\n"
"#  END OF AUTOMATED OPTION PROCESSING\n"
"#\n# # # # # # # # # # -- do not modify this marker --\n";

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  OPTION SELECTION
 */
tSCC zOptionCase[] =
"        case \"${OPT_CODE}\" in\n";

tSCC zOptionPartName[] =
"        '%s' | \\\n";

tSCC zOptionFullName[] =
"        '%s' )\n";

tSCC zOptionFlag[] =
"        '%c' )\n";

tSCC zOptionEndSelect[] =
"            ;;\n\n";

tSCC zOptionUnknown[] =
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
tSCC zTextExit[] =
"            echo \"$%s_%s_TEXT\"\n"
"            exit 0\n";

tSCC zPagedUsageExit[] =
"            echo \"$%s_LONGUSAGE_TEXT\" | ${PAGER-more}\n"
"            exit 0\n";

tSCC zCmdFmt[] =
"            %s\n";

tSCC zCountTest[] =
"            if [ $%1$s_%2$s_CT -ge %3$d ] ; then\n"
"                echo Error:  more than %3$d %2$s options >&2\n"
"                echo \"$%1$s_USAGE_TEXT\"\n"
"                exit 1 ; fi\n";

tSCC zMultiArg[] =
"            %1$s_%2$s_CT=`expr ${%1$s_%2$s_CT} + 1`\n"
"            OPT_ELEMENT=\"_${%1$s_%2$s_CT}\"\n"
"            OPT_NAME='%2$s'\n";

tSCC zSingleArg[] =
"            if [ -n \"${%1$s_%2$s}\" ] && ${%1$s_%2$s_set} ; then\n"
"                echo Error:  duplicate %2$s option >&2\n"
"                echo \"$%1$s_USAGE_TEXT\"\n"
"                exit 1 ; fi\n"
"            %1$s_%2$s_set=true\n"
"            OPT_NAME='%2$s'\n";

tSCC zNoMultiArg[] =
"            %1$s_%2$s_CT=0\n"
"            OPT_ELEMENT=''\n"
"            %1$s_%2$s='%3$s'\n"
"            export %1$s_%2$s\n"
"            OPT_NAME='%2$s'\n";

tSCC zNoSingleArg[] =
"            if [ -n \"${%1$s_%2$s}\" ] && ${%1$s_%2$s_set} ; then\n"
"                echo Error:  duplicate %2$s option >&2\n"
"                echo \"$%1$s_USAGE_TEXT\"\n"
"                exit 1 ; fi\n"
"            %1$s_%2$s_set=true\n"
"            %1$s_%2$s='%3$s'\n"
"            export %1$s_%2$s\n"
"            OPT_NAME='%2$s'\n";

tSCC zMayArg[]  =
"            eval %1$s_%2$s${OPT_ELEMENT}=true\n"
"            export %1$s_%2$s${OPT_ELEMENT}\n"
"            OPT_ARG_NEEDED=OK\n";

tSCC zMustArg[] =
"            OPT_ARG_NEEDED=YES\n";

tSCC zCantArg[] =
"            eval %1$s_%2$s${OPT_ELEMENT}=true\n"
"            export %1$s_%2$s${OPT_ELEMENT}\n"
"            OPT_ARG_NEEDED=NO\n";

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  LONG OPTION PROCESSING
 *
 *  Formats for emitting the text for handling long option types
 */
tSCC zLongOptInit[] =
"        OPT_CODE=`echo \"X${OPT_ARG}\"|sed 's/^X-*//'`\n"
"        shift\n"
"        OPT_ARG=\"$1\"\n\n"
"        case \"${OPT_CODE}\" in *=* )\n"
"            OPT_ARG_VAL=`echo \"${OPT_CODE}\"|sed 's/^[^=]*=//'`\n"
"            OPT_CODE=`echo \"${OPT_CODE}\"|sed 's/=.*$//'` ;; esac\n\n";

tSCC zLongOptArg[] =
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
tSCC zFlagOptInit[] =
"        OPT_CODE=`echo \"X${OPT_ARG}\" | sed 's/X-\\(.\\).*/\\1/'`\n"
"        OPT_ARG=` echo \"X${OPT_ARG}\" | sed 's/X-.//'`\n\n";

tSCC zFlagOptArg[] =
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
/* static forward declarations maintained by :mkfwd */
static void
textToVariable( tOptions* pOpts, teTextTo whichVar, tOptDesc* pOD );

static void
emitUsage( tOptions* pOpts );

static void
emitSetup( tOptions* pOpts );

static void
printOptionAction( tOptions* pOpts, tOptDesc* pOptDesc );

static void
printOptionInaction( tOptions* pOpts, tOptDesc* pOptDesc );

static void
emitFlag( tOptions* pOpts );

static void
emitMatchExpr( tCC* pzMatchName, tOptDesc* pCurOpt, tOptions* pOpts );

static void
emitLong( tOptions* pOpts );

static void
openOutput( char const* pzFile );
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
optionParseShell( tOptions* pOpts )
{
    /*
     *  Check for our SHELL option now.
     *  IF the output file contains the "#!" magic marker,
     *  it will override anything we do here.
     */
    if (HAVE_OPT( SHELL ))
        pzShell = OPT_ARG( SHELL );

    else if (! ENABLED_OPT( SHELL ))
        pzShell = NULL;

    else if ((pzShell = getenv( "SHELL" )),
             pzShell == NULL)

        pzShell = "/bin/sh";

    /*
     *  Check for a specified output file
     */
    if (HAVE_OPT( SCRIPT ))
        openOutput( OPT_ARG( SCRIPT ));

    emitUsage( pOpts );
    emitSetup( pOpts );

    /*
     *  There are four modes of option processing.
     */
    switch (pOpts->fOptSet & (OPTPROC_LONGOPT|OPTPROC_SHORTOPT)) {
    case OPTPROC_LONGOPT:
        fputs( zLoopCase,        stdout );

        fputs( zLongSelection,   stdout );
        fputs( zLongOptInit,     stdout );
        emitLong( pOpts );
        printf( zLongOptArg,     pOpts->pzPROGNAME );
        fputs( zEndSelection,    stdout );

        fputs( zNoSelection,     stdout );
        break;

    case 0:
        fputs( zLoopOnly,        stdout );
        fputs( zLongOptInit,     stdout );
        emitLong( pOpts );
        printf( zLongOptArg,     pOpts->pzPROGNAME );
        break;

    case OPTPROC_SHORTOPT:
        fputs( zLoopCase,        stdout );

        fputs( zFlagSelection,   stdout );
        fputs( zFlagOptInit,     stdout );
        emitFlag( pOpts );
        printf( zFlagOptArg,     pOpts->pzPROGNAME );
        fputs( zEndSelection,    stdout );

        fputs( zNoSelection,     stdout );
        break;

    case OPTPROC_LONGOPT|OPTPROC_SHORTOPT:
        fputs( zLoopCase,        stdout );

        fputs( zLongSelection,   stdout );
        fputs( zLongOptInit,     stdout );
        emitLong( pOpts );
        printf( zLongOptArg,     pOpts->pzPROGNAME );
        fputs( zEndSelection,    stdout );

        fputs( zFlagSelection,   stdout );
        fputs( zFlagOptInit,     stdout );
        emitFlag( pOpts );
        printf( zFlagOptArg,     pOpts->pzPROGNAME );
        fputs( zEndSelection,    stdout );

        fputs( zNoSelection,     stdout );
        break;
    }

    printf( zLoopEnd, pOpts->pzPROGNAME, zTrailerMarker );
    if ((pzTrailer != NULL) && (*pzTrailer != '\0'))
        fputs( pzTrailer, stdout );
    else if (ENABLED_OPT( SHELL ))
        printf( "\nenv | grep '^%s_'\n", pOpts->pzPROGNAME );

    fflush( stdout );
    fchmod( STDOUT_FILENO, 0755 );
    fclose( stdout );
}


static void
textToVariable( tOptions* pOpts, teTextTo whichVar, tOptDesc* pOD )
{
#   define _TT_(n) tSCC z ## n [] = #n;
    TEXTTO_TABLE
#   undef _TT_
#   define _TT_(n) z ## n ,
      static char const*  apzTTNames[] = { TEXTTO_TABLE };
#   undef _TT_

#if defined(__windows__) && !defined(__CYGWIN__)
    printf( "%1$s_%2$s_TEXT='no %2$s text'\n",
            pOpts->pzPROGNAME, apzTTNames[ whichVar ]);
#else
    int  nlHoldCt = 0;
    int  pipeFd[2];
    FILE* fp;

    printf( "%s_%s_TEXT='", pOpts->pzPROGNAME, apzTTNames[ whichVar ]);
    fflush( stdout );

    if (pipe( pipeFd ) != 0) {
        fprintf( stderr, zBadPipe, errno, strerror( errno ));
        exit( EXIT_FAILURE );
    }

    switch (fork()) {
    case -1:
        fprintf( stderr, zForkFail, errno, strerror(errno), pOpts->pzProgName);
        exit( EXIT_FAILURE );
        break;

    case 0:
        dup2( pipeFd[1], STDERR_FILENO );
        dup2( pipeFd[1], STDOUT_FILENO );
        close( pipeFd[0] );

        switch (whichVar) {
        case TT_LONGUSAGE:
            (*(pOpts->pUsageProc))( pOpts, EXIT_SUCCESS );
            /* NOTREACHED */
            exit( EXIT_FAILURE );

        case TT_USAGE:
            (*(pOpts->pUsageProc))( pOpts, EXIT_FAILURE );
            /* NOTREACHED */
            exit( EXIT_FAILURE );

        case TT_VERSION:
            if (pOD->fOptState & OPTST_ALLOC_ARG) {
                AGFREE(pOD->optArg.argString);
                pOD->fOptState &= ~OPTST_ALLOC_ARG;
            }
            pOD->optArg.argString = "c";
            optionPrintVersion( pOpts, pOD );
            /* NOTREACHED */

        default:
            exit( EXIT_FAILURE );
        }

    default:
        close( pipeFd[1] );
        fp = fdopen( pipeFd[0], "r" FOPEN_BINARY_FLAG );
    }

    for (;;) {
        int  ch = fgetc( fp );
        switch (ch) {

        case '\n':
            nlHoldCt++;
            break;

        case '\'':
            while (nlHoldCt > 0) {
                fputc( '\n', stdout );
                nlHoldCt--;
            }
            fputs( "'\\''", stdout );
            break;

        case EOF:
            goto endCharLoop;

        default:
            while (nlHoldCt > 0) {
                fputc( '\n', stdout );
                nlHoldCt--;
            }
            fputc( ch, stdout );
            break;
        }
    } endCharLoop:;

    fputs( "'\n\n", stdout );
    close( pipeFd[0] );
#endif
}


static void
emitUsage( tOptions* pOpts )
{
    char     zTimeBuf[ AO_NAME_SIZE ];

    /*
     *  First, switch stdout to the output file name.
     *  Then, change the program name to the one defined
     *  by the definitions (rather than the current
     *  executable name).  Down case the upper cased name.
     */
    if (pzLeader != NULL)
        fputs( pzLeader, stdout );

    {
        tSCC    zStdout[] = "stdout";
        tCC*    pzOutName;

        {
            time_t    curTime = time( NULL );
            struct tm*  pTime = localtime( &curTime );
            strftime(zTimeBuf, AO_NAME_SIZE, "%A %B %e, %Y at %r %Z", pTime );
        }

        if (HAVE_OPT( SCRIPT ))
             pzOutName = OPT_ARG( SCRIPT );
        else pzOutName = zStdout;

        if ((pzLeader == NULL) && (pzShell != NULL))
            printf( "#! %s\n", pzShell );

        printf( zPreamble, zStartMarker, pzOutName, zTimeBuf );
    }

    /*
     *  Get a copy of the original program name in lower case
     */
    {
        char* pzPN = zTimeBuf;
        tCC*  pz   = pOpts->pzPROGNAME;
        for (;;) {
            if ((*pzPN++ = tolower( *pz++ )) == '\0')
                break;
        }
    }

    printf( zEndPreamble, pOpts->pzPROGNAME );

    pOpts->pzProgPath = pOpts->pzProgName = zTimeBuf;
    textToVariable( pOpts, TT_LONGUSAGE, NULL );
    textToVariable( pOpts, TT_USAGE,     NULL );

    {
        tOptDesc* pOptDesc = pOpts->pOptDesc;
        int       optionCt = pOpts->optCt;

        for (;;) {
            if (pOptDesc->pOptProc == optionPrintVersion) {
                textToVariable( pOpts, TT_VERSION, pOptDesc );
                break;
            }

            if (--optionCt <= 0)
                break;
            pOptDesc++;
        }
    }
}


static void
emitSetup( tOptions* pOpts )
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
            (*(pOptDesc->pOptProc))( (tOptions*)2UL, pOptDesc );
            pzDefault = pOptDesc->optArg.argString;
            break;

        /*
         *  Numeric and membership bit options are just printed as a number.
         */
        case OPARG_TYPE_NUMERIC:
            snprintf( zVal, sizeof( zVal ), "%d",
                      (int)pOptDesc->optArg.argInt );
            pzDefault = zVal;
            break;

        case OPARG_TYPE_MEMBERSHIP:
            snprintf( zVal, sizeof( zVal ), "%lu",
                      (unsigned long)pOptDesc->optArg.argIntptr );
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

        printf( pzFmt, pOpts->pzPROGNAME, pOptDesc->pz_NAME, pzDefault );
    }
}


static void
printOptionAction( tOptions* pOpts, tOptDesc* pOptDesc )
{
    if (pOptDesc->pOptProc == optionPrintVersion)
        printf( zTextExit, pOpts->pzPROGNAME, "VERSION" );

    else if (pOptDesc->pOptProc == optionPagedUsage)
        printf( zPagedUsageExit, pOpts->pzPROGNAME );

    else if (pOptDesc->pOptProc == optionLoadOpt) {
        printf( zCmdFmt, "echo 'Warning:  Cannot load options files' >&2" );
        printf( zCmdFmt, "OPT_ARG_NEEDED=YES" );

    } else if (pOptDesc->pz_NAME == NULL) {

        if (pOptDesc->pOptProc == NULL) {
            printf( zCmdFmt, "echo 'Warning:  Cannot save options files' "
                    ">&2" );
            printf( zCmdFmt, "OPT_ARG_NEEDED=OK" );
        } else
            printf( zTextExit, pOpts->pzPROGNAME, "LONGUSAGE" );

    } else {
        if (pOptDesc->optMaxCt == 1)
            printf( zSingleArg, pOpts->pzPROGNAME, pOptDesc->pz_NAME );
        else {
            if ((unsigned)pOptDesc->optMaxCt < NOLIMIT)
                printf( zCountTest, pOpts->pzPROGNAME,
                        pOptDesc->pz_NAME, pOptDesc->optMaxCt );

            printf( zMultiArg, pOpts->pzPROGNAME, pOptDesc->pz_NAME );
        }

        /*
         *  Fix up the args.
         */
        if (OPTST_GET_ARGTYPE(pOptDesc->fOptState) == OPARG_TYPE_NONE) {
            printf( zCantArg, pOpts->pzPROGNAME, pOptDesc->pz_NAME );

        } else if (pOptDesc->fOptState & OPTST_ARG_OPTIONAL) {
            printf( zMayArg,  pOpts->pzPROGNAME, pOptDesc->pz_NAME );

        } else {
            fputs( zMustArg, stdout );
        }
    }
    fputs( zOptionEndSelect, stdout );
}


static void
printOptionInaction( tOptions* pOpts, tOptDesc* pOptDesc )
{
    if (pOptDesc->pOptProc == optionLoadOpt) {
        printf( zCmdFmt, "echo 'Warning:  Cannot suppress the loading of "
                "options files' >&2" );

    } else if (pOptDesc->optMaxCt == 1)
        printf( zNoSingleArg, pOpts->pzPROGNAME,
                pOptDesc->pz_NAME, pOptDesc->pz_DisablePfx );
    else
        printf( zNoMultiArg, pOpts->pzPROGNAME,
                pOptDesc->pz_NAME, pOptDesc->pz_DisablePfx );

    printf( zCmdFmt, "OPT_ARG_NEEDED=NO" );
    fputs( zOptionEndSelect, stdout );
}


static void
emitFlag( tOptions* pOpts )
{
    tOptDesc* pOptDesc = pOpts->pOptDesc;
    int       optionCt = pOpts->optCt;

    fputs( zOptionCase, stdout );

    for (;optionCt > 0; pOptDesc++, --optionCt) {

        if (SKIP_OPT(pOptDesc))
            continue;

        if (isprint( pOptDesc->optValue )) {
            printf( zOptionFlag, pOptDesc->optValue );
            printOptionAction( pOpts, pOptDesc );
        }
    }
    printf( zOptionUnknown, "flag", pOpts->pzPROGNAME );
}


/*
 *  Emit the match text for a long option
 */
static void
emitMatchExpr( tCC* pzMatchName, tOptDesc* pCurOpt, tOptions* pOpts )
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
        while (  toupper( pOD->pz_Name[matchCt] )
              == toupper( pzMatchName[matchCt] ))
            matchCt++;

        if (matchCt > min)
            min = matchCt;

        /*
         *  Check the disablement name, too.
         */
        if (pOD->pz_DisableName != NULL) {
            matchCt = 0;
            while (  toupper( pOD->pz_DisableName[matchCt] )
                  == toupper( pzMatchName[matchCt] ))
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
        printf( zOptionFullName, pzMatchName );

    else {
        int matchCt = 0;
        for (; matchCt <= min; matchCt++)
            *pz++ = pzMatchName[matchCt];

        for (;;) {
            *pz = NUL;
            printf( zOptionPartName, zName );
            *pz++ = pzMatchName[matchCt++];
            if (pzMatchName[matchCt] == NUL) {
                *pz = NUL;
                printf( zOptionFullName, zName );
                break;
            }
        }
    }
}


/*
 *  Emit GNU-standard long option handling code
 */
static void
emitLong( tOptions* pOpts )
{
    tOptDesc* pOD = pOpts->pOptDesc;
    int       ct  = pOpts->optCt;

    fputs( zOptionCase, stdout );

    /*
     *  do each option, ...
     */
    do  {
        /*
         *  Documentation & compiled-out options
         */
        if (SKIP_OPT(pOD))
            continue;

        emitMatchExpr( pOD->pz_Name, pOD, pOpts );
        printOptionAction( pOpts, pOD );

        /*
         *  Now, do the same thing for the disablement version of the option.
         */
        if (pOD->pz_DisableName != NULL) {
            emitMatchExpr( pOD->pz_DisableName, pOD, pOpts );
            printOptionInaction( pOpts, pOD );
        }
    } while (pOD++, --ct > 0);

    printf( zOptionUnknown, "option", pOpts->pzPROGNAME );
}


static void
openOutput( char const* pzFile )
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
        if (stat( pzFile, &stbf ) != 0)
            break;

        /*
         *  The file must be a regular file
         */
        if (! S_ISREG( stbf.st_mode )) {
            fprintf( stderr, zNotFile, pzFile );
            exit( EXIT_FAILURE );
        }

        pzData = AGALOC(stbf.st_size + 1, "file data");
        fp = fopen( pzFile, "r" FOPEN_BINARY_FLAG );

        sizeLeft = (unsigned)stbf.st_size;
        pzScan   = pzData;

        /*
         *  Read in all the data as fast as our OS will let us.
         */
        for (;;) {
            int inct = fread( (void*)pzScan, (size_t)1, sizeLeft, fp);
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
        fclose( fp );
        pzScan  = strstr( pzData, zStartMarker );
        if (pzScan == NULL) {
            pzTrailer = pzData;
            break;
        }

        *(pzScan++) = NUL;
        pzScan  = strstr( pzScan, zTrailerMarker );
        if (pzScan == NULL) {
            pzTrailer = pzData;
            break;
        }

        /*
         *  Check to see if the data contains
         *  our marker.  If it does, then we will skip over it
         */
        pzTrailer = pzScan + sizeof( zTrailerMarker ) - 1;
        pzLeader  = pzData;
    } while (AG_FALSE);

    freopen( pzFile, "w" FOPEN_BINARY_FLAG, stdout );
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
genshelloptUsage( tOptions*  pOpts, int exitCode )
{
#if defined(__windows__) && !defined(__CYGWIN__)
    optionUsage( pOpts, exitCode );
#else
    /*
     *  IF not EXIT_SUCCESS,
     *  THEN emit the short form of usage.
     */
    if (exitCode != EXIT_SUCCESS)
        optionUsage( pOpts, exitCode );
    fflush( stderr );
    fflush( stdout );

    option_usage_fp = stdout;

    /*
     *  First, print our usage
     */
    switch (fork()) {
    case -1:
        optionUsage( pOpts, EXIT_FAILURE );
        /*NOTREACHED*/
        _exit( EXIT_FAILURE );

    case 0:
        pagerState = PAGER_STATE_CHILD;
        optionUsage( pOpts, EXIT_SUCCESS );
        /*NOTREACHED*/
        _exit( EXIT_FAILURE );

    default:
    {
        int  sts;
        wait( &sts );
    }
    }

    /*
     *  Generate the pzProgName, since optionProcess() normally
     *  gets it from the command line
     */
    {
        char* pz;
        AGDUPSTR( pz, pShellParseOptions->pzPROGNAME, "program name" );
        pShellParseOptions->pzProgName = pz;
        while (*pz != NUL) {
            *pz = tolower( *pz );
            pz++;
        }
    }

    /*
     *  Separate the makeshell usage from the client usage
     */
    fprintf( option_usage_fp, zGenshell, pShellParseOptions->pzProgName );
    fflush( option_usage_fp );

    /*
     *  Now, print the client usage.
     */
    switch (fork()) {
    case 0:
        pagerState = PAGER_STATE_CHILD;
        /*FALLTHROUGH*/
    case -1:
        optionUsage( pShellParseOptions, EXIT_FAILURE );

    default:
    {
        int  sts;
        wait( &sts );
    }
    }

    exit( EXIT_SUCCESS );
#endif
}

/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/makeshell.c */
