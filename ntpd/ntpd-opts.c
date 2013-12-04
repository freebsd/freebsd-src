/*  
 *  EDIT THIS FILE WITH CAUTION  (ntpd-opts.c)
 *  
 *  It has been AutoGen-ed  December 24, 2011 at 06:34:01 PM by AutoGen 5.12
 *  From the definitions    ntpd-opts.def
 *  and the template file   options
 *
 * Generated from AutoOpts 35:0:10 templates.
 *
 *  AutoOpts is a copyrighted work.  This source file is not encumbered
 *  by AutoOpts licensing, but is provided under the licensing terms chosen
 *  by the ntpd author or copyright holder.  AutoOpts is
 *  licensed under the terms of the LGPL.  The redistributable library
 *  (``libopts'') is licensed under the terms of either the LGPL or, at the
 *  users discretion, the BSD license.  See the AutoOpts and/or libopts sources
 *  for details.
 *
 * This source file is copyrighted and licensed under the following terms:
 *
 *  see html/copyright.html
 *  
 */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define OPTION_CODE_COMPILE 1
#include "ntpd-opts.h"

#ifdef  __cplusplus
extern "C" {
#endif
extern FILE * option_usage_fp;

/* TRANSLATORS: choose the translation for option names wisely because you
                cannot ever change your mind. */
static char const zCopyright[38] =
"ntpd 4.2.6p5\n\
see html/copyright.html\n";
static char const zLicenseDescrip[25] =
"see html/copyright.html\n";

extern tUsageProc optionUsage;

/*
 *  global included definitions
 */
#ifdef __windows
  extern int atoi(const char *);
#else
# include <stdlib.h>
#endif

#ifndef NULL
#  define NULL 0
#endif

/*
 *  Ipv4 option description with
 *  "Must also have options" and "Incompatible options":
 */
static char const zIpv4Text[] =
        "Force IPv4 DNS name resolution";
static char const zIpv4_NAME[]               = "IPV4";
static char const zIpv4_Name[]               = "ipv4";
static const int
    aIpv4CantList[] = {
    INDEX_OPT_IPV6, NO_EQUIVALENT };
#define IPV4_FLAGS       (OPTST_DISABLED)

/*
 *  Ipv6 option description with
 *  "Must also have options" and "Incompatible options":
 */
static char const zIpv6Text[] =
        "Force IPv6 DNS name resolution";
static char const zIpv6_NAME[]               = "IPV6";
static char const zIpv6_Name[]               = "ipv6";
static const int
    aIpv6CantList[] = {
    INDEX_OPT_IPV4, NO_EQUIVALENT };
#define IPV6_FLAGS       (OPTST_DISABLED)

/*
 *  Authreq option description with
 *  "Must also have options" and "Incompatible options":
 */
static char const zAuthreqText[] =
        "Require crypto authentication";
static char const zAuthreq_NAME[]            = "AUTHREQ";
static char const zAuthreq_Name[]            = "authreq";
static const int
    aAuthreqCantList[] = {
    INDEX_OPT_AUTHNOREQ, NO_EQUIVALENT };
#define AUTHREQ_FLAGS       (OPTST_DISABLED)

/*
 *  Authnoreq option description with
 *  "Must also have options" and "Incompatible options":
 */
static char const zAuthnoreqText[] =
        "Do not require crypto authentication";
static char const zAuthnoreq_NAME[]          = "AUTHNOREQ";
static char const zAuthnoreq_Name[]          = "authnoreq";
static const int
    aAuthnoreqCantList[] = {
    INDEX_OPT_AUTHREQ, NO_EQUIVALENT };
#define AUTHNOREQ_FLAGS       (OPTST_DISABLED)

/*
 *  Bcastsync option description:
 */
static char const zBcastsyncText[] =
        "Allow us to sync to broadcast servers";
static char const zBcastsync_NAME[]          = "BCASTSYNC";
static char const zBcastsync_Name[]          = "bcastsync";
#define BCASTSYNC_FLAGS       (OPTST_DISABLED)

/*
 *  Configfile option description:
 */
static char const zConfigfileText[] =
        "configuration file name";
static char const zConfigfile_NAME[]         = "CONFIGFILE";
static char const zConfigfile_Name[]         = "configfile";
#define CONFIGFILE_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Debug_Level option description:
 */
#ifdef DEBUG
static char const zDebug_LevelText[] =
        "Increase output debug message level";
static char const zDebug_Level_NAME[]        = "DEBUG_LEVEL";
static char const zDebug_Level_Name[]        = "debug-level";
#define DEBUG_LEVEL_FLAGS       (OPTST_DISABLED)

#else   /* disable Debug_Level */
#define DEBUG_LEVEL_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zDebug_Level_NAME      NULL
static char const zDebug_Level_Name[] = "debug-level";
static char const zDebug_LevelText[]  = "this package was built using 'configure --disable--debug'";
#endif  /* DEBUG */

/*
 *  Set_Debug_Level option description:
 */
#ifdef DEBUG
static char const zSet_Debug_LevelText[] =
        "Set the output debug message level";
static char const zSet_Debug_Level_NAME[]    = "SET_DEBUG_LEVEL";
static char const zSet_Debug_Level_Name[]    = "set-debug-level";
#define SET_DEBUG_LEVEL_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

#else   /* disable Set_Debug_Level */
#define SET_DEBUG_LEVEL_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zSet_Debug_Level_NAME      NULL
static char const zSet_Debug_Level_Name[] = "set-debug-level";
static char const zSet_Debug_LevelText[]  = "this package was built using 'configure --disable--debug'";
#endif  /* DEBUG */

/*
 *  Driftfile option description:
 */
static char const zDriftfileText[] =
        "frequency drift file name";
static char const zDriftfile_NAME[]          = "DRIFTFILE";
static char const zDriftfile_Name[]          = "driftfile";
#define DRIFTFILE_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Panicgate option description:
 */
static char const zPanicgateText[] =
        "Allow the first adjustment to be Big";
static char const zPanicgate_NAME[]          = "PANICGATE";
static char const zPanicgate_Name[]          = "panicgate";
#define PANICGATE_FLAGS       (OPTST_DISABLED)

/*
 *  Jaildir option description:
 */
#ifdef HAVE_DROPROOT
static char const zJaildirText[] =
        "Jail directory";
static char const zJaildir_NAME[]            = "JAILDIR";
static char const zJaildir_Name[]            = "jaildir";
#define JAILDIR_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

#else   /* disable Jaildir */
#define JAILDIR_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zJaildir_NAME      NULL
static char const zJaildir_Name[] = "jaildir";
static char const zJaildirText[]  = "built without --enable-clockctl or --enable-linuxcaps";
#endif  /* HAVE_DROPROOT */

/*
 *  Interface option description:
 */
static char const zInterfaceText[] =
        "Listen on an interface name or address";
static char const zInterface_NAME[]          = "INTERFACE";
static char const zInterface_Name[]          = "interface";
#define INTERFACE_FLAGS       (OPTST_DISABLED | OPTST_STACKED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Keyfile option description:
 */
static char const zKeyfileText[] =
        "path to symmetric keys";
static char const zKeyfile_NAME[]            = "KEYFILE";
static char const zKeyfile_Name[]            = "keyfile";
#define KEYFILE_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Logfile option description:
 */
static char const zLogfileText[] =
        "path to the log file";
static char const zLogfile_NAME[]            = "LOGFILE";
static char const zLogfile_Name[]            = "logfile";
#define LOGFILE_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Novirtualips option description:
 */
static char const zNovirtualipsText[] =
        "Do not listen to virtual interfaces";
static char const zNovirtualips_NAME[]       = "NOVIRTUALIPS";
static char const zNovirtualips_Name[]       = "novirtualips";
#define NOVIRTUALIPS_FLAGS       (OPTST_DISABLED)

/*
 *  Modifymmtimer option description:
 */
#ifdef SYS_WINNT
static char const zModifymmtimerText[] =
        "Modify Multimedia Timer (Windows only)";
static char const zModifymmtimer_NAME[]      = "MODIFYMMTIMER";
static char const zModifymmtimer_Name[]      = "modifymmtimer";
#define MODIFYMMTIMER_FLAGS       (OPTST_DISABLED)

#else   /* disable Modifymmtimer */
#define MODIFYMMTIMER_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zModifymmtimer_NAME      NULL
#define zModifymmtimerText       NULL
#define zModifymmtimer_Name      NULL
#endif  /* SYS_WINNT */

/*
 *  Nofork option description:
 */
static char const zNoforkText[] =
        "Do not fork";
static char const zNofork_NAME[]             = "NOFORK";
static char const zNofork_Name[]             = "nofork";
#define NOFORK_FLAGS       (OPTST_DISABLED)

/*
 *  Nice option description:
 */
static char const zNiceText[] =
        "Run at high priority";
static char const zNice_NAME[]               = "NICE";
static char const zNice_Name[]               = "nice";
#define NICE_FLAGS       (OPTST_DISABLED)

/*
 *  Pidfile option description:
 */
static char const zPidfileText[] =
        "path to the PID file";
static char const zPidfile_NAME[]            = "PIDFILE";
static char const zPidfile_Name[]            = "pidfile";
#define PIDFILE_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Priority option description:
 */
static char const zPriorityText[] =
        "Process priority";
static char const zPriority_NAME[]           = "PRIORITY";
static char const zPriority_Name[]           = "priority";
#define PRIORITY_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_NUMERIC))

/*
 *  Quit option description:
 */
static char const zQuitText[] =
        "Set the time and quit";
static char const zQuit_NAME[]               = "QUIT";
static char const zQuit_Name[]               = "quit";
#define QUIT_FLAGS       (OPTST_DISABLED)

/*
 *  Propagationdelay option description:
 */
static char const zPropagationdelayText[] =
        "Broadcast/propagation delay";
static char const zPropagationdelay_NAME[]   = "PROPAGATIONDELAY";
static char const zPropagationdelay_Name[]   = "propagationdelay";
#define PROPAGATIONDELAY_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Saveconfigquit option description:
 */
#ifdef SAVECONFIG
static char const zSaveconfigquitText[] =
        "Save parsed configuration and quit";
static char const zSaveconfigquit_NAME[]     = "SAVECONFIGQUIT";
static char const zSaveconfigquit_Name[]     = "saveconfigquit";
#define SAVECONFIGQUIT_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

#else   /* disable Saveconfigquit */
#define SAVECONFIGQUIT_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zSaveconfigquit_NAME      NULL
#define zSaveconfigquitText       NULL
#define zSaveconfigquit_Name      NULL
#endif  /* SAVECONFIG */

/*
 *  Statsdir option description:
 */
static char const zStatsdirText[] =
        "Statistics file location";
static char const zStatsdir_NAME[]           = "STATSDIR";
static char const zStatsdir_Name[]           = "statsdir";
#define STATSDIR_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Trustedkey option description:
 */
static char const zTrustedkeyText[] =
        "Trusted key number";
static char const zTrustedkey_NAME[]         = "TRUSTEDKEY";
static char const zTrustedkey_Name[]         = "trustedkey";
#define TRUSTEDKEY_FLAGS       (OPTST_DISABLED | OPTST_STACKED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  User option description:
 */
#ifdef HAVE_DROPROOT
static char const zUserText[] =
        "Run as userid (or userid:groupid)";
static char const zUser_NAME[]               = "USER";
static char const zUser_Name[]               = "user";
#define USER_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

#else   /* disable User */
#define USER_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zUser_NAME      NULL
static char const zUser_Name[] = "user";
static char const zUserText[]  = "built without --enable-clockctl or --enable-linuxcaps";
#endif  /* HAVE_DROPROOT */

/*
 *  Updateinterval option description:
 */
static char const zUpdateintervalText[] =
        "interval in seconds between scans for new or dropped interfaces";
static char const zUpdateinterval_NAME[]     = "UPDATEINTERVAL";
static char const zUpdateinterval_Name[]     = "updateinterval";
#define UPDATEINTERVAL_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_NUMERIC))

/*
 *  Var option description:
 */
static char const zVarText[] =
        "make ARG an ntp variable (RW)";
static char const zVar_NAME[]                = "VAR";
static char const zVar_Name[]                = "var";
#define VAR_FLAGS       (OPTST_DISABLED | OPTST_STACKED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Dvar option description:
 */
static char const zDvarText[] =
        "make ARG an ntp variable (RW|DEF)";
static char const zDvar_NAME[]               = "DVAR";
static char const zDvar_Name[]               = "dvar";
#define DVAR_FLAGS       (OPTST_DISABLED | OPTST_STACKED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Slew option description:
 */
static char const zSlewText[] =
        "Slew up to 600 seconds";
static char const zSlew_NAME[]               = "SLEW";
static char const zSlew_Name[]               = "slew";
#define SLEW_FLAGS       (OPTST_DISABLED)

/*
 *  Usepcc option description:
 */
#ifdef SYS_WINNT
static char const zUsepccText[] =
        "Use CPU cycle counter (Windows only)";
static char const zUsepcc_NAME[]             = "USEPCC";
static char const zUsepcc_Name[]             = "usepcc";
#define USEPCC_FLAGS       (OPTST_DISABLED)

#else   /* disable Usepcc */
#define USEPCC_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zUsepcc_NAME      NULL
#define zUsepccText       NULL
#define zUsepcc_Name      NULL
#endif  /* SYS_WINNT */

/*
 *  Pccfreq option description:
 */
#ifdef SYS_WINNT
static char const zPccfreqText[] =
        "Force CPU cycle counter use (Windows only)";
static char const zPccfreq_NAME[]            = "PCCFREQ";
static char const zPccfreq_Name[]            = "pccfreq";
#define PCCFREQ_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

#else   /* disable Pccfreq */
#define PCCFREQ_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zPccfreq_NAME      NULL
#define zPccfreqText       NULL
#define zPccfreq_Name      NULL
#endif  /* SYS_WINNT */

/*
 *  Help/More_Help/Version option descriptions:
 */
static char const zHelpText[]          = "Display extended usage information and exit";
static char const zHelp_Name[]         = "help";
#ifdef HAVE_WORKING_FORK
#define OPTST_MORE_HELP_FLAGS   (OPTST_IMM | OPTST_NO_INIT)
static char const zMore_Help_Name[]    = "more-help";
static char const zMore_HelpText[]     = "Extended usage information passed thru pager";
#else
#define OPTST_MORE_HELP_FLAGS   (OPTST_OMITTED | OPTST_NO_INIT)
#define zMore_Help_Name   NULL
#define zMore_HelpText    NULL
#endif
#ifdef NO_OPTIONAL_OPT_ARGS
#  define OPTST_VERSION_FLAGS   OPTST_IMM | OPTST_NO_INIT
#else
#  define OPTST_VERSION_FLAGS   OPTST_SET_ARGTYPE(OPARG_TYPE_STRING) | \
                                OPTST_ARG_OPTIONAL | OPTST_IMM | OPTST_NO_INIT
#endif

static char const zVersionText[]       = "Output version information and exit";
static char const zVersion_Name[]      = "version";
/*
 *  Declare option callback procedures
 */
#ifdef DEBUG
  static tOptProc doOptSet_Debug_Level;
#else /* not DEBUG */
# define doOptSet_Debug_Level NULL
#endif /* def/not DEBUG */
#if defined(TEST_NTPD_OPTS)
/*
 *  Under test, omit argument processing, or call optionStackArg,
 *  if multiple copies are allowed.
 */
static tOptProc
    doUsageOpt;

/*
 *  #define map the "normal" callout procs to the test ones...
 */
#define SET_DEBUG_LEVEL_OPT_PROC optionStackArg


#else /* NOT defined TEST_NTPD_OPTS */
/*
 *  When not under test, there are different procs to use
 */
extern tOptProc
    optionBooleanVal,    optionNestedVal,     optionNumericVal,
    optionPagedUsage,    optionPrintVersion,  optionResetOpt,
    optionStackArg,      optionTimeDate,      optionTimeVal,
    optionUnstackArg,    optionVersionStderr;
static tOptProc
    doUsageOpt;

/*
 *  #define map the "normal" callout procs
 */
#define SET_DEBUG_LEVEL_OPT_PROC doOptSet_Debug_Level

#define SET_DEBUG_LEVEL_OPT_PROC doOptSet_Debug_Level
#endif /* defined(TEST_NTPD_OPTS) */
#ifdef TEST_NTPD_OPTS
# define DOVERPROC optionVersionStderr
#else
# define DOVERPROC optionPrintVersion
#endif /* TEST_NTPD_OPTS */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  Define the Ntpd Option Descriptions.
 */
static tOptDesc optDesc[OPTION_CT] = {
  {  /* entry idx, value */ 0, VALUE_OPT_IPV4,
     /* equiv idx, value */ 0, VALUE_OPT_IPV4,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ IPV4_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aIpv4CantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zIpv4Text, zIpv4_NAME, zIpv4_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 1, VALUE_OPT_IPV6,
     /* equiv idx, value */ 1, VALUE_OPT_IPV6,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ IPV6_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aIpv6CantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zIpv6Text, zIpv6_NAME, zIpv6_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 2, VALUE_OPT_AUTHREQ,
     /* equiv idx, value */ 2, VALUE_OPT_AUTHREQ,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ AUTHREQ_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aAuthreqCantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zAuthreqText, zAuthreq_NAME, zAuthreq_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 3, VALUE_OPT_AUTHNOREQ,
     /* equiv idx, value */ 3, VALUE_OPT_AUTHNOREQ,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ AUTHNOREQ_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aAuthnoreqCantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zAuthnoreqText, zAuthnoreq_NAME, zAuthnoreq_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 4, VALUE_OPT_BCASTSYNC,
     /* equiv idx, value */ 4, VALUE_OPT_BCASTSYNC,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ BCASTSYNC_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zBcastsyncText, zBcastsync_NAME, zBcastsync_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 5, VALUE_OPT_CONFIGFILE,
     /* equiv idx, value */ 5, VALUE_OPT_CONFIGFILE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ CONFIGFILE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zConfigfileText, zConfigfile_NAME, zConfigfile_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 6, VALUE_OPT_DEBUG_LEVEL,
     /* equiv idx, value */ 6, VALUE_OPT_DEBUG_LEVEL,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ DEBUG_LEVEL_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zDebug_LevelText, zDebug_Level_NAME, zDebug_Level_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 7, VALUE_OPT_SET_DEBUG_LEVEL,
     /* equiv idx, value */ 7, VALUE_OPT_SET_DEBUG_LEVEL,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ SET_DEBUG_LEVEL_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ SET_DEBUG_LEVEL_OPT_PROC,
     /* desc, NAME, name */ zSet_Debug_LevelText, zSet_Debug_Level_NAME, zSet_Debug_Level_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 8, VALUE_OPT_DRIFTFILE,
     /* equiv idx, value */ 8, VALUE_OPT_DRIFTFILE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ DRIFTFILE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zDriftfileText, zDriftfile_NAME, zDriftfile_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 9, VALUE_OPT_PANICGATE,
     /* equiv idx, value */ 9, VALUE_OPT_PANICGATE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ PANICGATE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zPanicgateText, zPanicgate_NAME, zPanicgate_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 10, VALUE_OPT_JAILDIR,
     /* equiv idx, value */ 10, VALUE_OPT_JAILDIR,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ JAILDIR_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zJaildirText, zJaildir_NAME, zJaildir_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 11, VALUE_OPT_INTERFACE,
     /* equiv idx, value */ 11, VALUE_OPT_INTERFACE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ INTERFACE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionStackArg,
     /* desc, NAME, name */ zInterfaceText, zInterface_NAME, zInterface_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 12, VALUE_OPT_KEYFILE,
     /* equiv idx, value */ 12, VALUE_OPT_KEYFILE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ KEYFILE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zKeyfileText, zKeyfile_NAME, zKeyfile_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 13, VALUE_OPT_LOGFILE,
     /* equiv idx, value */ 13, VALUE_OPT_LOGFILE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ LOGFILE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zLogfileText, zLogfile_NAME, zLogfile_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 14, VALUE_OPT_NOVIRTUALIPS,
     /* equiv idx, value */ 14, VALUE_OPT_NOVIRTUALIPS,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ NOVIRTUALIPS_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zNovirtualipsText, zNovirtualips_NAME, zNovirtualips_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 15, VALUE_OPT_MODIFYMMTIMER,
     /* equiv idx, value */ 15, VALUE_OPT_MODIFYMMTIMER,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ MODIFYMMTIMER_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zModifymmtimerText, zModifymmtimer_NAME, zModifymmtimer_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 16, VALUE_OPT_NOFORK,
     /* equiv idx, value */ 16, VALUE_OPT_NOFORK,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ NOFORK_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zNoforkText, zNofork_NAME, zNofork_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 17, VALUE_OPT_NICE,
     /* equiv idx, value */ 17, VALUE_OPT_NICE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ NICE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zNiceText, zNice_NAME, zNice_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 18, VALUE_OPT_PIDFILE,
     /* equiv idx, value */ 18, VALUE_OPT_PIDFILE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ PIDFILE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zPidfileText, zPidfile_NAME, zPidfile_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 19, VALUE_OPT_PRIORITY,
     /* equiv idx, value */ 19, VALUE_OPT_PRIORITY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ PRIORITY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionNumericVal,
     /* desc, NAME, name */ zPriorityText, zPriority_NAME, zPriority_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 20, VALUE_OPT_QUIT,
     /* equiv idx, value */ 20, VALUE_OPT_QUIT,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ QUIT_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zQuitText, zQuit_NAME, zQuit_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 21, VALUE_OPT_PROPAGATIONDELAY,
     /* equiv idx, value */ 21, VALUE_OPT_PROPAGATIONDELAY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ PROPAGATIONDELAY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zPropagationdelayText, zPropagationdelay_NAME, zPropagationdelay_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 22, VALUE_OPT_SAVECONFIGQUIT,
     /* equiv idx, value */ 22, VALUE_OPT_SAVECONFIGQUIT,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ SAVECONFIGQUIT_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zSaveconfigquitText, zSaveconfigquit_NAME, zSaveconfigquit_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 23, VALUE_OPT_STATSDIR,
     /* equiv idx, value */ 23, VALUE_OPT_STATSDIR,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ STATSDIR_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zStatsdirText, zStatsdir_NAME, zStatsdir_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 24, VALUE_OPT_TRUSTEDKEY,
     /* equiv idx, value */ 24, VALUE_OPT_TRUSTEDKEY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ TRUSTEDKEY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionStackArg,
     /* desc, NAME, name */ zTrustedkeyText, zTrustedkey_NAME, zTrustedkey_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 25, VALUE_OPT_USER,
     /* equiv idx, value */ 25, VALUE_OPT_USER,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ USER_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zUserText, zUser_NAME, zUser_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 26, VALUE_OPT_UPDATEINTERVAL,
     /* equiv idx, value */ 26, VALUE_OPT_UPDATEINTERVAL,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ UPDATEINTERVAL_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionNumericVal,
     /* desc, NAME, name */ zUpdateintervalText, zUpdateinterval_NAME, zUpdateinterval_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 27, VALUE_OPT_VAR,
     /* equiv idx, value */ 27, VALUE_OPT_VAR,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ VAR_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionStackArg,
     /* desc, NAME, name */ zVarText, zVar_NAME, zVar_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 28, VALUE_OPT_DVAR,
     /* equiv idx, value */ 28, VALUE_OPT_DVAR,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ DVAR_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionStackArg,
     /* desc, NAME, name */ zDvarText, zDvar_NAME, zDvar_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 29, VALUE_OPT_SLEW,
     /* equiv idx, value */ 29, VALUE_OPT_SLEW,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ SLEW_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zSlewText, zSlew_NAME, zSlew_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 30, VALUE_OPT_USEPCC,
     /* equiv idx, value */ 30, VALUE_OPT_USEPCC,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ USEPCC_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zUsepccText, zUsepcc_NAME, zUsepcc_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 31, VALUE_OPT_PCCFREQ,
     /* equiv idx, value */ 31, VALUE_OPT_PCCFREQ,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ PCCFREQ_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zPccfreqText, zPccfreq_NAME, zPccfreq_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ INDEX_OPT_VERSION, VALUE_OPT_VERSION,
     /* equiv idx value  */ NO_EQUIVALENT, 0,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ OPTST_VERSION_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ DOVERPROC,
     /* desc, NAME, name */ zVersionText, NULL, zVersion_Name,
     /* disablement strs */ NULL, NULL },



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
     /* opt state flags  */ OPTST_MORE_HELP_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL,  NULL,
     /* option proc      */ optionPagedUsage,
     /* desc, NAME, name */ zMore_HelpText, NULL, zMore_Help_Name,
     /* disablement strs */ NULL, NULL }
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  Define the Ntpd Option Environment
 */
static char const zPROGNAME[5] = "NTPD";
static char const zUsageTitle[99] =
"ntpd - NTP daemon program - Ver. 4.2.6p5\n\
USAGE:  %s [ -<flag> [<val>] | --<name>[{=| }<val>] ]...\n";
#define zRcName     NULL
#define apzHomeList NULL

static char const zBugsAddr[34]    = "http://bugs.ntp.org, bugs@ntp.org";
#define zExplain NULL
static char const zDetail[7] = "\n\n";
static char const zFullVersion[] = NTPD_FULL_VERSION;
/* extracted from optcode.tlib near line 515 */

#if defined(ENABLE_NLS)
# define OPTPROC_BASE OPTPROC_TRANSLATE
  static tOptionXlateProc translate_option_strings;
#else
# define OPTPROC_BASE OPTPROC_NONE
# define translate_option_strings NULL
#endif /* ENABLE_NLS */


#define ntpd_full_usage NULL
#define ntpd_short_usage NULL
#ifndef  PKGDATADIR
# define PKGDATADIR ""
#endif

#ifndef  WITH_PACKAGER
# define ntpd_packager_info NULL
#else
static char const ntpd_packager_info[] =
    "Packaged by " WITH_PACKAGER

# ifdef WITH_PACKAGER_VERSION
        " ("WITH_PACKAGER_VERSION")"
# endif

# ifdef WITH_PACKAGER_BUG_REPORTS
    "\nReport ntpd bugs to " WITH_PACKAGER_BUG_REPORTS
# endif
    "\n";
#endif

tOptions ntpdOptions = {
    OPTIONS_STRUCT_VERSION,
    0, NULL,                    /* original argc + argv    */
    ( OPTPROC_BASE
    + OPTPROC_ERRSTOP
    + OPTPROC_SHORTOPT
    + OPTPROC_LONGOPT
    + OPTPROC_NO_REQ_OPT
    + OPTPROC_ENVIRON
    + OPTPROC_NO_ARGS
    + OPTPROC_MISUSE ),
    0, NULL,                    /* current option index, current option */
    NULL,         NULL,         zPROGNAME,
    zRcName,      zCopyright,   zLicenseDescrip,
    zFullVersion, apzHomeList,  zUsageTitle,
    zExplain,     zDetail,      optDesc,
    zBugsAddr,                  /* address to send bugs to */
    NULL, NULL,                 /* extensions/saved state  */
    optionUsage, /* usage procedure */
    translate_option_strings,   /* translation procedure */
    /*
     *  Indexes to special options
     */
    { INDEX_OPT_MORE_HELP, /* more-help option index */
      NO_EQUIVALENT, /* save option index */
      NO_EQUIVALENT, /* '-#' option index */
      NO_EQUIVALENT /* index of default opt */
    },
    35 /* full option count */, 32 /* user option count */,
    ntpd_full_usage, ntpd_short_usage,
    NULL, NULL,
    PKGDATADIR, ntpd_packager_info
};

/*
 *  Create the static procedure(s) declared above.
 */
static void
doUsageOpt(tOptions * pOptions, tOptDesc * pOptDesc)
{
    (void)pOptions;
    USAGE(NTPD_EXIT_SUCCESS);
}

#if ! defined(TEST_NTPD_OPTS)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *   For the set-debug-level option, when DEBUG is #define-d.
 */
#ifdef DEBUG
static void
doOptSet_Debug_Level(tOptions* pOptions, tOptDesc* pOptDesc)
{
    /* extracted from ntpdbase-opts.def, line 100 */
DESC(DEBUG_LEVEL).optOccCt = atoi( pOptDesc->pzLastArg );
}
#endif /* defined DEBUG */
#endif /* defined(TEST_NTPD_OPTS) */
/* extracted from optmain.tlib near line 128 */

#if defined(TEST_NTPD_OPTS) /* TEST MAIN PROCEDURE: */

extern void optionPutShell(tOptions*);

int
main(int argc, char ** argv)
{
    int res = NTPD_EXIT_SUCCESS;
    (void)optionProcess(&ntpdOptions, argc, argv);
    optionPutShell(&ntpdOptions);
    res = ferror(stdout);
    if (res != 0)
        fputs("output error writing to stdout\n", stderr);
    return res;
}
#endif  /* defined TEST_NTPD_OPTS */
/* extracted from optcode.tlib near line 666 */

#if ENABLE_NLS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <autoopts/usage-txt.h>

static char* AO_gettext(char const* pz);
static void  coerce_it(void** s);

static char*
AO_gettext(char const* pz)
{
    char* pzRes;
    if (pz == NULL)
        return NULL;
    pzRes = _(pz);
    if (pzRes == pz)
        return pzRes;
    pzRes = strdup(pzRes);
    if (pzRes == NULL) {
        fputs(_("No memory for duping translated strings\n"), stderr);
        exit(NTPD_EXIT_FAILURE);
    }
    return pzRes;
}

static void coerce_it(void** s) { *s = AO_gettext(*s);
}

/*
 *  This invokes the translation code (e.g. gettext(3)).
 */
static void
translate_option_strings(void)
{
    tOptions * const pOpt = &ntpdOptions;

    /*
     *  Guard against re-translation.  It won't work.  The strings will have
     *  been changed by the first pass through this code.  One shot only.
     */
    if (option_usage_text.field_ct != 0) {
        /*
         *  Do the translations.  The first pointer follows the field count
         *  field.  The field count field is the size of a pointer.
         */
        tOptDesc * pOD = pOpt->pOptDesc;
        char **    ppz = (char**)(void*)&(option_usage_text);
        int        ix  = option_usage_text.field_ct;

        do {
            ppz++;
            *ppz = AO_gettext(*ppz);
        } while (--ix > 0);

        coerce_it((void*)&(pOpt->pzCopyright));
        coerce_it((void*)&(pOpt->pzCopyNotice));
        coerce_it((void*)&(pOpt->pzFullVersion));
        coerce_it((void*)&(pOpt->pzUsageTitle));
        coerce_it((void*)&(pOpt->pzExplain));
        coerce_it((void*)&(pOpt->pzDetail));
        coerce_it((void*)&(pOpt->pzPackager));
        option_usage_text.field_ct = 0;

        for (ix = pOpt->optCt; ix > 0; ix--, pOD++)
            coerce_it((void*)&(pOD->pzText));
    }

    if ((pOpt->fOptSet & OPTPROC_NXLAT_OPT_CFG) == 0) {
        tOptDesc * pOD = pOpt->pOptDesc;
        int        ix;

        for (ix = pOpt->optCt; ix > 0; ix--, pOD++) {
            coerce_it((void*)&(pOD->pz_Name));
            coerce_it((void*)&(pOD->pz_DisableName));
            coerce_it((void*)&(pOD->pz_DisablePfx));
        }
        /* prevent re-translation */
        ntpdOptions.fOptSet |= OPTPROC_NXLAT_OPT_CFG | OPTPROC_NXLAT_OPT;
    }
}

#endif /* ENABLE_NLS */

#ifdef  __cplusplus
}
#endif
/* ntpd-opts.c ends here */
