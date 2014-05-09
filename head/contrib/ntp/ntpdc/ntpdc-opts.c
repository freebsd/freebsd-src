/*  
 *  EDIT THIS FILE WITH CAUTION  (ntpdc-opts.c)
 *  
 *  It has been AutoGen-ed  Tuesday December  8, 2009 at 08:14:00 AM EST
 *  From the definitions    ntpdc-opts.def
 *  and the template file   options
 *
 * Generated from AutoOpts 29:0:4 templates.
 */

/*
 *  This file was produced by an AutoOpts template.  AutoOpts is a
 *  copyrighted work.  This source file is not encumbered by AutoOpts
 *  licensing, but is provided under the licensing terms chosen by the
 *  ntpdc author or copyright holder.  AutoOpts is licensed under
 *  the terms of the LGPL.  The redistributable library (``libopts'') is
 *  licensed under the terms of either the LGPL or, at the users discretion,
 *  the BSD license.  See the AutoOpts and/or libopts sources for details.
 *
 * This source file is copyrighted and licensed under the following terms:
 *
 * ntpdc copyright 1970-2009 David L. Mills and/or others - all rights reserved
 *
 * see html/copyright.html
 */


#include <limits.h>

#define OPTION_CODE_COMPILE 1
#include "ntpdc-opts.h"

#ifdef  __cplusplus
extern "C" {
#endif
tSCC zCopyright[] =
       "ntpdc copyright (c) 1970-2009 David L. Mills and/or others, all rights reserved";
tSCC zCopyrightNotice[] =
       
/* extracted from ../include/copyright.def near line 8 */
"see html/copyright.html";
extern tUsageProc optionUsage;

/*
 *  global included definitions
 */
#ifdef __windows
  extern int atoi(const char*);
#else
# include <stdlib.h>
#endif

#ifndef NULL
#  define NULL 0
#endif
#ifndef EXIT_SUCCESS
#  define  EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#  define  EXIT_FAILURE 1
#endif
/*
 *  Ipv4 option description:
 */
tSCC    zIpv4Text[] =
        "Force IPv4 DNS name resolution";
tSCC    zIpv4_NAME[]               = "IPV4";
tSCC    zIpv4_Name[]               = "ipv4";
#define IPV4_FLAGS       (OPTST_DISABLED)

/*
 *  Ipv6 option description:
 */
tSCC    zIpv6Text[] =
        "Force IPv6 DNS name resolution";
tSCC    zIpv6_NAME[]               = "IPV6";
tSCC    zIpv6_Name[]               = "ipv6";
#define IPV6_FLAGS       (OPTST_DISABLED)

/*
 *  Command option description:
 */
tSCC    zCommandText[] =
        "run a command and exit";
tSCC    zCommand_NAME[]            = "COMMAND";
tSCC    zCommand_Name[]            = "command";
#define COMMAND_FLAGS       (OPTST_DISABLED | OPTST_STACKED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Listpeers option description with
 *  "Must also have options" and "Incompatible options":
 */
tSCC    zListpeersText[] =
        "Print a list of the peers";
tSCC    zListpeers_NAME[]          = "LISTPEERS";
tSCC    zListpeers_Name[]          = "listpeers";
static const int
    aListpeersCantList[] = {
    INDEX_OPT_COMMAND, NO_EQUIVALENT };
#define LISTPEERS_FLAGS       (OPTST_DISABLED)

/*
 *  Peers option description with
 *  "Must also have options" and "Incompatible options":
 */
tSCC    zPeersText[] =
        "Print a list of the peers";
tSCC    zPeers_NAME[]              = "PEERS";
tSCC    zPeers_Name[]              = "peers";
static const int
    aPeersCantList[] = {
    INDEX_OPT_COMMAND, NO_EQUIVALENT };
#define PEERS_FLAGS       (OPTST_DISABLED)

/*
 *  Showpeers option description with
 *  "Must also have options" and "Incompatible options":
 */
tSCC    zShowpeersText[] =
        "Show a list of the peers";
tSCC    zShowpeers_NAME[]          = "SHOWPEERS";
tSCC    zShowpeers_Name[]          = "showpeers";
static const int
    aShowpeersCantList[] = {
    INDEX_OPT_COMMAND, NO_EQUIVALENT };
#define SHOWPEERS_FLAGS       (OPTST_DISABLED)

/*
 *  Interactive option description with
 *  "Must also have options" and "Incompatible options":
 */
tSCC    zInteractiveText[] =
        "Force ntpq to operate in interactive mode";
tSCC    zInteractive_NAME[]        = "INTERACTIVE";
tSCC    zInteractive_Name[]        = "interactive";
static const int
    aInteractiveCantList[] = {
    INDEX_OPT_COMMAND,
    INDEX_OPT_LISTPEERS,
    INDEX_OPT_PEERS,
    INDEX_OPT_SHOWPEERS, NO_EQUIVALENT };
#define INTERACTIVE_FLAGS       (OPTST_DISABLED)

/*
 *  Debug_Level option description:
 */
#ifdef DEBUG
tSCC    zDebug_LevelText[] =
        "Increase output debug message level";
tSCC    zDebug_Level_NAME[]        = "DEBUG_LEVEL";
tSCC    zDebug_Level_Name[]        = "debug-level";
#define DEBUG_LEVEL_FLAGS       (OPTST_DISABLED)

#else   /* disable Debug_Level */
#define VALUE_OPT_DEBUG_LEVEL NO_EQUIVALENT
#define DEBUG_LEVEL_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zDebug_LevelText       NULL
#define zDebug_Level_NAME      NULL
#define zDebug_Level_Name      NULL
#endif  /* DEBUG */

/*
 *  Set_Debug_Level option description:
 */
#ifdef DEBUG
tSCC    zSet_Debug_LevelText[] =
        "Set the output debug message level";
tSCC    zSet_Debug_Level_NAME[]    = "SET_DEBUG_LEVEL";
tSCC    zSet_Debug_Level_Name[]    = "set-debug-level";
#define SET_DEBUG_LEVEL_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

#else   /* disable Set_Debug_Level */
#define VALUE_OPT_SET_DEBUG_LEVEL NO_EQUIVALENT
#define SET_DEBUG_LEVEL_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zSet_Debug_LevelText       NULL
#define zSet_Debug_Level_NAME      NULL
#define zSet_Debug_Level_Name      NULL
#endif  /* DEBUG */

/*
 *  Numeric option description:
 */
tSCC    zNumericText[] =
        "numeric host addresses";
tSCC    zNumeric_NAME[]            = "NUMERIC";
tSCC    zNumeric_Name[]            = "numeric";
#define NUMERIC_FLAGS       (OPTST_DISABLED)

/*
 *  Help/More_Help/Version option descriptions:
 */
tSCC zHelpText[]       = "Display usage information and exit";
tSCC zHelp_Name[]      = "help";

tSCC zMore_HelpText[]  = "Extended usage information passed thru pager";
tSCC zMore_Help_Name[] = "more-help";

tSCC zVersionText[]    = "Output version information and exit";
tSCC zVersion_Name[]   = "version";

/*
 *  Save/Load_Opts option description:
 */
tSCC zSave_OptsText[]     = "Save the option state to a config file";
tSCC zSave_Opts_Name[]    = "save-opts";

tSCC zLoad_OptsText[]     = "Load options from a config file";
tSCC zLoad_Opts_NAME[]    = "LOAD_OPTS";

tSCC zNotLoad_Opts_Name[] = "no-load-opts";
tSCC zNotLoad_Opts_Pfx[]  = "no";
#define zLoad_Opts_Name   (zNotLoad_Opts_Name + 3)
/*
 *  Declare option callback procedures
 */
#ifdef DEBUG
  static tOptProc doOptSet_Debug_Level;
#else /* not DEBUG */
# define doOptSet_Debug_Level NULL
#endif /* def/not DEBUG */
#if defined(TEST_NTPDC_OPTS)
/*
 *  Under test, omit argument processing, or call optionStackArg,
 *  if multiple copies are allowed.
 */
extern tOptProc
    optionPagedUsage, optionStackArg, optionVersionStderr;
static tOptProc
    doUsageOpt;

/*
 *  #define map the "normal" callout procs to the test ones...
 */
#define SET_DEBUG_LEVEL_OPT_PROC optionStackArg


#else /* NOT defined TEST_NTPDC_OPTS */
/*
 *  When not under test, there are different procs to use
 */
extern tOptProc
    optionPagedUsage, optionPrintVersion, optionStackArg;
static tOptProc
    doUsageOpt;

/*
 *  #define map the "normal" callout procs
 */
#define SET_DEBUG_LEVEL_OPT_PROC doOptSet_Debug_Level

#define SET_DEBUG_LEVEL_OPT_PROC doOptSet_Debug_Level
#endif /* defined(TEST_NTPDC_OPTS) */
#ifdef TEST_NTPDC_OPTS
# define DOVERPROC optionVersionStderr
#else
# define DOVERPROC optionPrintVersion
#endif /* TEST_NTPDC_OPTS */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  Define the Ntpdc Option Descriptions.
 */
static tOptDesc optDesc[ OPTION_CT ] = {
  {  /* entry idx, value */ 0, VALUE_OPT_IPV4,
     /* equiv idx, value */ NO_EQUIVALENT, 0,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ IPV4_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zIpv4Text, zIpv4_NAME, zIpv4_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 1, VALUE_OPT_IPV6,
     /* equiv idx, value */ NOLIMIT, NOLIMIT,
     /* equivalenced to  */ INDEX_OPT_IPV4,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ IPV6_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zIpv6Text, zIpv6_NAME, zIpv6_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 2, VALUE_OPT_COMMAND,
     /* equiv idx, value */ 2, VALUE_OPT_COMMAND,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ COMMAND_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionStackArg,
     /* desc, NAME, name */ zCommandText, zCommand_NAME, zCommand_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 3, VALUE_OPT_LISTPEERS,
     /* equiv idx, value */ 3, VALUE_OPT_LISTPEERS,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ LISTPEERS_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aListpeersCantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zListpeersText, zListpeers_NAME, zListpeers_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 4, VALUE_OPT_PEERS,
     /* equiv idx, value */ 4, VALUE_OPT_PEERS,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ PEERS_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aPeersCantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zPeersText, zPeers_NAME, zPeers_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 5, VALUE_OPT_SHOWPEERS,
     /* equiv idx, value */ 5, VALUE_OPT_SHOWPEERS,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ SHOWPEERS_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aShowpeersCantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zShowpeersText, zShowpeers_NAME, zShowpeers_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 6, VALUE_OPT_INTERACTIVE,
     /* equiv idx, value */ 6, VALUE_OPT_INTERACTIVE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ INTERACTIVE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aInteractiveCantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zInteractiveText, zInteractive_NAME, zInteractive_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 7, VALUE_OPT_DEBUG_LEVEL,
     /* equiv idx, value */ 7, VALUE_OPT_DEBUG_LEVEL,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ DEBUG_LEVEL_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zDebug_LevelText, zDebug_Level_NAME, zDebug_Level_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 8, VALUE_OPT_SET_DEBUG_LEVEL,
     /* equiv idx, value */ 8, VALUE_OPT_SET_DEBUG_LEVEL,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ SET_DEBUG_LEVEL_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ SET_DEBUG_LEVEL_OPT_PROC,
     /* desc, NAME, name */ zSet_Debug_LevelText, zSet_Debug_Level_NAME, zSet_Debug_Level_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 9, VALUE_OPT_NUMERIC,
     /* equiv idx, value */ 9, VALUE_OPT_NUMERIC,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ NUMERIC_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zNumericText, zNumeric_NAME, zNumeric_Name,
     /* disablement strs */ NULL, NULL },

#ifdef NO_OPTIONAL_OPT_ARGS
#  define VERSION_OPT_FLAGS     OPTST_IMM | OPTST_NO_INIT
#else
#  define VERSION_OPT_FLAGS     OPTST_SET_ARGTYPE(OPARG_TYPE_STRING) | \
                                OPTST_ARG_OPTIONAL | OPTST_IMM | OPTST_NO_INIT
#endif

  {  /* entry idx, value */ INDEX_OPT_VERSION, VALUE_OPT_VERSION,
     /* equiv idx value  */ NO_EQUIVALENT, 0,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ VERSION_OPT_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ DOVERPROC,
     /* desc, NAME, name */ zVersionText, NULL, zVersion_Name,
     /* disablement strs */ NULL, NULL },

#undef VERSION_OPT_FLAGS


  {  /* entry idx, value */ INDEX_OPT_HELP, VALUE_OPT_HELP,
     /* equiv idx value  */ NO_EQUIVALENT, 0,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ OPTST_IMM | OPTST_NO_INIT, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ doUsageOpt,
     /* desc, NAME, name */ zHelpText, NULL, zHelp_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ INDEX_OPT_MORE_HELP, VALUE_OPT_MORE_HELP,
     /* equiv idx value  */ NO_EQUIVALENT, 0,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ OPTST_IMM | OPTST_NO_INIT, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL,  NULL,
     /* option proc      */ optionPagedUsage,
     /* desc, NAME, name */ zMore_HelpText, NULL, zMore_Help_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ INDEX_OPT_SAVE_OPTS, VALUE_OPT_SAVE_OPTS,
     /* equiv idx value  */ NO_EQUIVALENT, 0,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ OPTST_SET_ARGTYPE(OPARG_TYPE_STRING)
                          | OPTST_ARG_OPTIONAL | OPTST_NO_INIT, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL,  NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zSave_OptsText, NULL, zSave_Opts_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ INDEX_OPT_LOAD_OPTS, VALUE_OPT_LOAD_OPTS,
     /* equiv idx value  */ NO_EQUIVALENT, 0,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ OPTST_SET_ARGTYPE(OPARG_TYPE_STRING) \
			  | OPTST_DISABLE_IMM, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionLoadOpt,
     /* desc, NAME, name */ zLoad_OptsText, zLoad_Opts_NAME, zLoad_Opts_Name,
     /* disablement strs */ zNotLoad_Opts_Name, zNotLoad_Opts_Pfx }
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  Define the Ntpdc Option Environment
 */
tSCC   zPROGNAME[]   = "NTPDC";
tSCC   zUsageTitle[] =
"ntpdc - vendor-specific NTP query program - Ver. 4.2.4p8\n\
USAGE:  %s [ -<flag> [<val>] | --<name>[{=| }<val>] ]... [ host ...]\n";
tSCC   zRcName[]     = ".ntprc";
tSCC*  apzHomeList[] = {
       "$HOME",
       ".",
       NULL };

tSCC   zBugsAddr[]    = "http://bugs.ntp.org, bugs@ntp.org";
#define zExplain NULL
tSCC    zDetail[]     = "\n\
The\n\
[= prog-name =]\n\
utility program is used to query an NTP daemon about its\n\
current state and to request changes in that state.\n\
It uses NTP mode 7 control message formats described in the source code.\n\
The program may\n\
be run either in interactive mode or controlled using command line\n\
arguments.\n\
Extensive state and statistics information is available\n\
through the\n\
[= prog-name =]\n\
interface.\n\
In addition, nearly all the\n\
configuration options which can be specified at startup using\n\
ntpd's configuration file may also be specified at run time using\n\
[= prog-name =] .\n";
tSCC    zFullVersion[] = NTPDC_FULL_VERSION;
/* extracted from /usr/local/gnu/autogen-5.9.1/share/autogen/optcode.tpl near line 408 */

#if defined(ENABLE_NLS)
# define OPTPROC_BASE OPTPROC_TRANSLATE
  static tOptionXlateProc translate_option_strings;
#else
# define OPTPROC_BASE OPTPROC_NONE
# define translate_option_strings NULL
#endif /* ENABLE_NLS */

tOptions ntpdcOptions = {
    OPTIONS_STRUCT_VERSION,
    0, NULL,                    /* original argc + argv    */
    ( OPTPROC_BASE
    + OPTPROC_ERRSTOP
    + OPTPROC_SHORTOPT
    + OPTPROC_LONGOPT
    + OPTPROC_NO_REQ_OPT
    + OPTPROC_ENVIRON
    + OPTPROC_HAS_IMMED ),
    0, NULL,                    /* current option index, current option */
    NULL,         NULL,         zPROGNAME,
    zRcName,      zCopyright,   zCopyrightNotice,
    zFullVersion, apzHomeList,  zUsageTitle,
    zExplain,     zDetail,      optDesc,
    zBugsAddr,                  /* address to send bugs to */
    NULL, NULL,                 /* extensions/saved state  */
    optionUsage,       /* usage procedure */
    translate_option_strings,   /* translation procedure */
    /*
     *  Indexes to special options
     */
    { INDEX_OPT_MORE_HELP,
      INDEX_OPT_SAVE_OPTS,
      NO_EQUIVALENT /* index of '-#' option */,
      NO_EQUIVALENT /* index of default opt */
    },
    15 /* full option count */, 10 /* user option count */
};

/*
 *  Create the static procedure(s) declared above.
 */
static void
doUsageOpt(
    tOptions*   pOptions,
    tOptDesc*   pOptDesc )
{
    USAGE( EXIT_SUCCESS );
}

#if ! defined(TEST_NTPDC_OPTS)

/* * * * * * *
 *
 *   For the set-debug-level option, when DEBUG is #define-d.
 */
#ifdef DEBUG
static void
doOptSet_Debug_Level(
    tOptions*   pOptions,
    tOptDesc*   pOptDesc )
{
    /* extracted from ../include/debug-opt.def, line 29 */
DESC(DEBUG_LEVEL).optOccCt = atoi( pOptDesc->pzLastArg );
}
#endif /* defined DEBUG */

#endif /* defined(TEST_NTPDC_OPTS) */

/* extracted from /usr/local/gnu/autogen-5.9.1/share/autogen/optmain.tpl near line 92 */

#if defined(TEST_NTPDC_OPTS) /* TEST MAIN PROCEDURE: */

int
main( int argc, char** argv )
{
    int res = EXIT_SUCCESS;
    (void)optionProcess( &ntpdcOptions, argc, argv );
    {
        void optionPutShell( tOptions* );
        optionPutShell( &ntpdcOptions );
    }
    return res;
}
#endif  /* defined TEST_NTPDC_OPTS */
/* extracted from /usr/local/gnu/autogen-5.9.1/share/autogen/optcode.tpl near line 514 */

#if ENABLE_NLS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <autoopts/usage-txt.h>

static char* AO_gettext( char const* pz );
static void  coerce_it(void** s);

static char*
AO_gettext( char const* pz )
{
    char* pzRes;
    if (pz == NULL)
        return NULL;
    pzRes = _(pz);
    if (pzRes == pz)
        return pzRes;
    pzRes = strdup( pzRes );
    if (pzRes == NULL) {
        fputs( _("No memory for duping translated strings\n"), stderr );
        exit( EXIT_FAILURE );
    }
    return pzRes;
}

static void coerce_it(void** s) { *s = AO_gettext(*s); }
#define COERSION(_f) \
  coerce_it((void*)&(ntpdcOptions._f))

/*
 *  This invokes the translation code (e.g. gettext(3)).
 */
static void
translate_option_strings( void )
{
    /*
     *  Guard against re-translation.  It won't work.  The strings will have
     *  been changed by the first pass through this code.  One shot only.
     */
    if (option_usage_text.field_ct == 0)
        return;
    /*
     *  Do the translations.  The first pointer follows the field count field.
     *  The field count field is the size of a pointer.
     */
    {
        char** ppz = (char**)(void*)&(option_usage_text);
        int    ix  = option_usage_text.field_ct;

        do {
            ppz++;
            *ppz = AO_gettext(*ppz);
        } while (--ix > 0);
    }
    option_usage_text.field_ct = 0;

    {
        tOptDesc* pOD = ntpdcOptions.pOptDesc;
        int       ix  = ntpdcOptions.optCt;

        for (;;) {
            pOD->pzText           = AO_gettext(pOD->pzText);
            pOD->pz_NAME          = AO_gettext(pOD->pz_NAME);
            pOD->pz_Name          = AO_gettext(pOD->pz_Name);
            pOD->pz_DisableName   = AO_gettext(pOD->pz_DisableName);
            pOD->pz_DisablePfx    = AO_gettext(pOD->pz_DisablePfx);
            if (--ix <= 0)
                break;
            pOD++;
        }
    }
    COERSION(pzCopyright);
    COERSION(pzCopyNotice);
    COERSION(pzFullVersion);
    COERSION(pzUsageTitle);
    COERSION(pzExplain);
    COERSION(pzDetail);
}

#endif /* ENABLE_NLS */

#ifdef  __cplusplus
}
#endif
/* ntpdc-opts.c ends here */
