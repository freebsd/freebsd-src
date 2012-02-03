/*  
 *  EDIT THIS FILE WITH CAUTION  (ntpdsim-opts.c)
 *  
 *  It has been AutoGen-ed  Tuesday December  8, 2009 at 08:13:13 AM EST
 *  From the definitions    ntpdsim-opts.def
 *  and the template file   options
 *
 * Generated from AutoOpts 29:0:4 templates.
 */

/*
 *  This file was produced by an AutoOpts template.  AutoOpts is a
 *  copyrighted work.  This source file is not encumbered by AutoOpts
 *  licensing, but is provided under the licensing terms chosen by the
 *  ntpdsim author or copyright holder.  AutoOpts is licensed under
 *  the terms of the LGPL.  The redistributable library (``libopts'') is
 *  licensed under the terms of either the LGPL or, at the users discretion,
 *  the BSD license.  See the AutoOpts and/or libopts sources for details.
 *
 * This source file is copyrighted and licensed under the following terms:
 *
 * ntpdsim copyright 1970-2009 David L. Mills and/or others - all rights reserved
 *
 * see html/copyright.html
 */


#include <limits.h>

#define OPTION_CODE_COMPILE 1
#include "ntpdsim-opts.h"

#ifdef  __cplusplus
extern "C" {
#endif
tSCC zCopyright[] =
       "ntpdsim copyright (c) 1970-2009 David L. Mills and/or others, all rights reserved";
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
 *  Authreq option description with
 *  "Must also have options" and "Incompatible options":
 */
tSCC    zAuthreqText[] =
        "Require crypto authentication";
tSCC    zAuthreq_NAME[]            = "AUTHREQ";
tSCC    zAuthreq_Name[]            = "authreq";
static const int
    aAuthreqCantList[] = {
    INDEX_OPT_AUTHNOREQ, NO_EQUIVALENT };
#define AUTHREQ_FLAGS       (OPTST_DISABLED)

/*
 *  Authnoreq option description with
 *  "Must also have options" and "Incompatible options":
 */
tSCC    zAuthnoreqText[] =
        "Do not require crypto authentication";
tSCC    zAuthnoreq_NAME[]          = "AUTHNOREQ";
tSCC    zAuthnoreq_Name[]          = "authnoreq";
static const int
    aAuthnoreqCantList[] = {
    INDEX_OPT_AUTHREQ, NO_EQUIVALENT };
#define AUTHNOREQ_FLAGS       (OPTST_DISABLED)

/*
 *  Bcastsync option description:
 */
tSCC    zBcastsyncText[] =
        "Allow us to sync to broadcast servers";
tSCC    zBcastsync_NAME[]          = "BCASTSYNC";
tSCC    zBcastsync_Name[]          = "bcastsync";
#define BCASTSYNC_FLAGS       (OPTST_DISABLED)

/*
 *  Simbroadcastdelay option description:
 */
tSCC    zSimbroadcastdelayText[] =
        "Simulator broadcast delay";
tSCC    zSimbroadcastdelay_NAME[]  = "SIMBROADCASTDELAY";
tSCC    zSimbroadcastdelay_Name[]  = "simbroadcastdelay";
#define SIMBROADCASTDELAY_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Configfile option description:
 */
tSCC    zConfigfileText[] =
        "configuration file name";
tSCC    zConfigfile_NAME[]         = "CONFIGFILE";
tSCC    zConfigfile_Name[]         = "configfile";
#define CONFIGFILE_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Phasenoise option description:
 */
tSCC    zPhasenoiseText[] =
        "Phase noise level";
tSCC    zPhasenoise_NAME[]         = "PHASENOISE";
tSCC    zPhasenoise_Name[]         = "phasenoise";
#define PHASENOISE_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

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
 *  Driftfile option description:
 */
tSCC    zDriftfileText[] =
        "frequency drift file name";
tSCC    zDriftfile_NAME[]          = "DRIFTFILE";
tSCC    zDriftfile_Name[]          = "driftfile";
#define DRIFTFILE_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Panicgate option description:
 */
tSCC    zPanicgateText[] =
        "Allow the first adjustment to be Big";
tSCC    zPanicgate_NAME[]          = "PANICGATE";
tSCC    zPanicgate_Name[]          = "panicgate";
#define PANICGATE_FLAGS       (OPTST_DISABLED)

/*
 *  Simslew option description:
 */
tSCC    zSimslewText[] =
        "Simuator slew";
tSCC    zSimslew_NAME[]            = "SIMSLEW";
tSCC    zSimslew_Name[]            = "simslew";
#define SIMSLEW_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Jaildir option description:
 */
tSCC    zJaildirText[] =
        "Jail directory";
tSCC    zJaildir_NAME[]            = "JAILDIR";
tSCC    zJaildir_Name[]            = "jaildir";
#define JAILDIR_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Interface option description:
 */
tSCC    zInterfaceText[] =
        "Listen on interface";
tSCC    zInterface_NAME[]          = "INTERFACE";
tSCC    zInterface_Name[]          = "interface";
#define INTERFACE_FLAGS       (OPTST_DISABLED | OPTST_STACKED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Keyfile option description:
 */
tSCC    zKeyfileText[] =
        "path to symmetric keys";
tSCC    zKeyfile_NAME[]            = "KEYFILE";
tSCC    zKeyfile_Name[]            = "keyfile";
#define KEYFILE_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Logfile option description:
 */
tSCC    zLogfileText[] =
        "path to the log file";
tSCC    zLogfile_NAME[]            = "LOGFILE";
tSCC    zLogfile_Name[]            = "logfile";
#define LOGFILE_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Novirtualips option description:
 */
tSCC    zNovirtualipsText[] =
        "Do not listen to virtual IPs";
tSCC    zNovirtualips_NAME[]       = "NOVIRTUALIPS";
tSCC    zNovirtualips_Name[]       = "novirtualips";
#define NOVIRTUALIPS_FLAGS       (OPTST_DISABLED)

/*
 *  Modifymmtimer option description:
 */
#ifdef SYS_WINNT
tSCC    zModifymmtimerText[] =
        "Modify Multimedia Timer (Windows only)";
tSCC    zModifymmtimer_NAME[]      = "MODIFYMMTIMER";
tSCC    zModifymmtimer_Name[]      = "modifymmtimer";
#define MODIFYMMTIMER_FLAGS       (OPTST_DISABLED)

#else   /* disable Modifymmtimer */
#define VALUE_OPT_MODIFYMMTIMER NO_EQUIVALENT
#define MODIFYMMTIMER_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zModifymmtimerText       NULL
#define zModifymmtimer_NAME      NULL
#define zModifymmtimer_Name      NULL
#endif  /* SYS_WINNT */

/*
 *  Nofork option description:
 */
tSCC    zNoforkText[] =
        "Do not fork";
tSCC    zNofork_NAME[]             = "NOFORK";
tSCC    zNofork_Name[]             = "nofork";
#define NOFORK_FLAGS       (OPTST_DISABLED)

/*
 *  Nice option description:
 */
tSCC    zNiceText[] =
        "Run at high priority";
tSCC    zNice_NAME[]               = "NICE";
tSCC    zNice_Name[]               = "nice";
#define NICE_FLAGS       (OPTST_DISABLED)

/*
 *  Servertime option description:
 */
tSCC    zServertimeText[] =
        "Server time";
tSCC    zServertime_NAME[]         = "SERVERTIME";
tSCC    zServertime_Name[]         = "servertime";
#define SERVERTIME_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Pidfile option description:
 */
tSCC    zPidfileText[] =
        "path to the PID file";
tSCC    zPidfile_NAME[]            = "PIDFILE";
tSCC    zPidfile_Name[]            = "pidfile";
#define PIDFILE_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Priority option description:
 */
tSCC    zPriorityText[] =
        "Process priority";
tSCC    zPriority_NAME[]           = "PRIORITY";
tSCC    zPriority_Name[]           = "priority";
#define PRIORITY_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_NUMERIC))

/*
 *  Quit option description:
 */
tSCC    zQuitText[] =
        "Set the time and quit";
tSCC    zQuit_NAME[]               = "QUIT";
tSCC    zQuit_Name[]               = "quit";
#define QUIT_FLAGS       (OPTST_DISABLED)

/*
 *  Propagationdelay option description:
 */
tSCC    zPropagationdelayText[] =
        "Broadcast/propagation delay";
tSCC    zPropagationdelay_NAME[]   = "PROPAGATIONDELAY";
tSCC    zPropagationdelay_Name[]   = "propagationdelay";
#define PROPAGATIONDELAY_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Updateinterval option description:
 */
tSCC    zUpdateintervalText[] =
        "interval in seconds between scans for new or dropped interfaces";
tSCC    zUpdateinterval_NAME[]     = "UPDATEINTERVAL";
tSCC    zUpdateinterval_Name[]     = "updateinterval";
#define UPDATEINTERVAL_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_NUMERIC))

/*
 *  Statsdir option description:
 */
tSCC    zStatsdirText[] =
        "Statistics file location";
tSCC    zStatsdir_NAME[]           = "STATSDIR";
tSCC    zStatsdir_Name[]           = "statsdir";
#define STATSDIR_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Endsimtime option description:
 */
tSCC    zEndsimtimeText[] =
        "Simulation end time";
tSCC    zEndsimtime_NAME[]         = "ENDSIMTIME";
tSCC    zEndsimtime_Name[]         = "endsimtime";
#define ENDSIMTIME_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Trustedkey option description:
 */
tSCC    zTrustedkeyText[] =
        "Trusted key number";
tSCC    zTrustedkey_NAME[]         = "TRUSTEDKEY";
tSCC    zTrustedkey_Name[]         = "trustedkey";
#define TRUSTEDKEY_FLAGS       (OPTST_DISABLED | OPTST_STACKED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Freqerr option description:
 */
tSCC    zFreqerrText[] =
        "Simulation frequency error";
tSCC    zFreqerr_NAME[]            = "FREQERR";
tSCC    zFreqerr_Name[]            = "freqerr";
#define FREQERR_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Walknoise option description:
 */
tSCC    zWalknoiseText[] =
        "Simulation random walk noise";
tSCC    zWalknoise_NAME[]          = "WALKNOISE";
tSCC    zWalknoise_Name[]          = "walknoise";
#define WALKNOISE_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  User option description:
 */
tSCC    zUserText[] =
        "Run as userid (or userid:groupid)";
tSCC    zUser_NAME[]               = "USER";
tSCC    zUser_Name[]               = "user";
#define USER_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Var option description:
 */
tSCC    zVarText[] =
        "make ARG an ntp variable (RW)";
tSCC    zVar_NAME[]                = "VAR";
tSCC    zVar_Name[]                = "var";
#define VAR_FLAGS       (OPTST_DISABLED | OPTST_STACKED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Dvar option description:
 */
tSCC    zDvarText[] =
        "make ARG an ntp variable (RW|DEF)";
tSCC    zDvar_NAME[]               = "DVAR";
tSCC    zDvar_Name[]               = "dvar";
#define DVAR_FLAGS       (OPTST_DISABLED | OPTST_STACKED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Slew option description:
 */
tSCC    zSlewText[] =
        "Slew up to 600 seconds";
tSCC    zSlew_NAME[]               = "SLEW";
tSCC    zSlew_Name[]               = "slew";
#define SLEW_FLAGS       (OPTST_DISABLED)

/*
 *  Ndelay option description:
 */
tSCC    zNdelayText[] =
        "Simulation network delay";
tSCC    zNdelay_NAME[]             = "NDELAY";
tSCC    zNdelay_Name[]             = "ndelay";
#define NDELAY_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/*
 *  Pdelay option description:
 */
tSCC    zPdelayText[] =
        "Simulation processing delay";
tSCC    zPdelay_NAME[]             = "PDELAY";
tSCC    zPdelay_Name[]             = "pdelay";
#define PDELAY_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

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
#if defined(TEST_NTPDSIM_OPTS)
/*
 *  Under test, omit argument processing, or call optionStackArg,
 *  if multiple copies are allowed.
 */
extern tOptProc
    optionNumericVal,    optionPagedUsage,    optionStackArg,
    optionVersionStderr;
static tOptProc
    doUsageOpt;

/*
 *  #define map the "normal" callout procs to the test ones...
 */
#define SET_DEBUG_LEVEL_OPT_PROC optionStackArg


#else /* NOT defined TEST_NTPDSIM_OPTS */
/*
 *  When not under test, there are different procs to use
 */
extern tOptProc
    optionNumericVal, optionPagedUsage, optionPrintVersion, optionStackArg;
static tOptProc
    doUsageOpt;

/*
 *  #define map the "normal" callout procs
 */
#define SET_DEBUG_LEVEL_OPT_PROC doOptSet_Debug_Level

#define SET_DEBUG_LEVEL_OPT_PROC doOptSet_Debug_Level
#endif /* defined(TEST_NTPDSIM_OPTS) */
#ifdef TEST_NTPDSIM_OPTS
# define DOVERPROC optionVersionStderr
#else
# define DOVERPROC optionPrintVersion
#endif /* TEST_NTPDSIM_OPTS */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  Define the Ntpdsim Option Descriptions.
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

  {  /* entry idx, value */ 5, VALUE_OPT_SIMBROADCASTDELAY,
     /* equiv idx, value */ 5, VALUE_OPT_SIMBROADCASTDELAY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ SIMBROADCASTDELAY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zSimbroadcastdelayText, zSimbroadcastdelay_NAME, zSimbroadcastdelay_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 6, VALUE_OPT_CONFIGFILE,
     /* equiv idx, value */ 6, VALUE_OPT_CONFIGFILE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ CONFIGFILE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zConfigfileText, zConfigfile_NAME, zConfigfile_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 7, VALUE_OPT_PHASENOISE,
     /* equiv idx, value */ 7, VALUE_OPT_PHASENOISE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ PHASENOISE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zPhasenoiseText, zPhasenoise_NAME, zPhasenoise_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 8, VALUE_OPT_DEBUG_LEVEL,
     /* equiv idx, value */ 8, VALUE_OPT_DEBUG_LEVEL,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ DEBUG_LEVEL_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zDebug_LevelText, zDebug_Level_NAME, zDebug_Level_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 9, VALUE_OPT_SET_DEBUG_LEVEL,
     /* equiv idx, value */ 9, VALUE_OPT_SET_DEBUG_LEVEL,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ SET_DEBUG_LEVEL_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ SET_DEBUG_LEVEL_OPT_PROC,
     /* desc, NAME, name */ zSet_Debug_LevelText, zSet_Debug_Level_NAME, zSet_Debug_Level_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 10, VALUE_OPT_DRIFTFILE,
     /* equiv idx, value */ 10, VALUE_OPT_DRIFTFILE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ DRIFTFILE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zDriftfileText, zDriftfile_NAME, zDriftfile_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 11, VALUE_OPT_PANICGATE,
     /* equiv idx, value */ 11, VALUE_OPT_PANICGATE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ PANICGATE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zPanicgateText, zPanicgate_NAME, zPanicgate_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 12, VALUE_OPT_SIMSLEW,
     /* equiv idx, value */ 12, VALUE_OPT_SIMSLEW,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ SIMSLEW_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zSimslewText, zSimslew_NAME, zSimslew_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 13, VALUE_OPT_JAILDIR,
     /* equiv idx, value */ 13, VALUE_OPT_JAILDIR,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ JAILDIR_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zJaildirText, zJaildir_NAME, zJaildir_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 14, VALUE_OPT_INTERFACE,
     /* equiv idx, value */ 14, VALUE_OPT_INTERFACE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ INTERFACE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionStackArg,
     /* desc, NAME, name */ zInterfaceText, zInterface_NAME, zInterface_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 15, VALUE_OPT_KEYFILE,
     /* equiv idx, value */ 15, VALUE_OPT_KEYFILE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ KEYFILE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zKeyfileText, zKeyfile_NAME, zKeyfile_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 16, VALUE_OPT_LOGFILE,
     /* equiv idx, value */ 16, VALUE_OPT_LOGFILE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ LOGFILE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zLogfileText, zLogfile_NAME, zLogfile_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 17, VALUE_OPT_NOVIRTUALIPS,
     /* equiv idx, value */ 17, VALUE_OPT_NOVIRTUALIPS,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ NOVIRTUALIPS_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zNovirtualipsText, zNovirtualips_NAME, zNovirtualips_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 18, VALUE_OPT_MODIFYMMTIMER,
     /* equiv idx, value */ 18, VALUE_OPT_MODIFYMMTIMER,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ MODIFYMMTIMER_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zModifymmtimerText, zModifymmtimer_NAME, zModifymmtimer_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 19, VALUE_OPT_NOFORK,
     /* equiv idx, value */ 19, VALUE_OPT_NOFORK,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ NOFORK_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zNoforkText, zNofork_NAME, zNofork_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 20, VALUE_OPT_NICE,
     /* equiv idx, value */ 20, VALUE_OPT_NICE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ NICE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zNiceText, zNice_NAME, zNice_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 21, VALUE_OPT_SERVERTIME,
     /* equiv idx, value */ 21, VALUE_OPT_SERVERTIME,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ SERVERTIME_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zServertimeText, zServertime_NAME, zServertime_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 22, VALUE_OPT_PIDFILE,
     /* equiv idx, value */ 22, VALUE_OPT_PIDFILE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ PIDFILE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zPidfileText, zPidfile_NAME, zPidfile_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 23, VALUE_OPT_PRIORITY,
     /* equiv idx, value */ 23, VALUE_OPT_PRIORITY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ PRIORITY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionNumericVal,
     /* desc, NAME, name */ zPriorityText, zPriority_NAME, zPriority_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 24, VALUE_OPT_QUIT,
     /* equiv idx, value */ 24, VALUE_OPT_QUIT,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ QUIT_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zQuitText, zQuit_NAME, zQuit_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 25, VALUE_OPT_PROPAGATIONDELAY,
     /* equiv idx, value */ 25, VALUE_OPT_PROPAGATIONDELAY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ PROPAGATIONDELAY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zPropagationdelayText, zPropagationdelay_NAME, zPropagationdelay_Name,
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

  {  /* entry idx, value */ 27, VALUE_OPT_STATSDIR,
     /* equiv idx, value */ 27, VALUE_OPT_STATSDIR,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ STATSDIR_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zStatsdirText, zStatsdir_NAME, zStatsdir_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 28, VALUE_OPT_ENDSIMTIME,
     /* equiv idx, value */ 28, VALUE_OPT_ENDSIMTIME,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ ENDSIMTIME_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zEndsimtimeText, zEndsimtime_NAME, zEndsimtime_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 29, VALUE_OPT_TRUSTEDKEY,
     /* equiv idx, value */ 29, VALUE_OPT_TRUSTEDKEY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ TRUSTEDKEY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionStackArg,
     /* desc, NAME, name */ zTrustedkeyText, zTrustedkey_NAME, zTrustedkey_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 30, VALUE_OPT_FREQERR,
     /* equiv idx, value */ 30, VALUE_OPT_FREQERR,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ FREQERR_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zFreqerrText, zFreqerr_NAME, zFreqerr_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 31, VALUE_OPT_WALKNOISE,
     /* equiv idx, value */ 31, VALUE_OPT_WALKNOISE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ WALKNOISE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zWalknoiseText, zWalknoise_NAME, zWalknoise_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 32, VALUE_OPT_USER,
     /* equiv idx, value */ 32, VALUE_OPT_USER,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ USER_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zUserText, zUser_NAME, zUser_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 33, VALUE_OPT_VAR,
     /* equiv idx, value */ 33, VALUE_OPT_VAR,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ VAR_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionStackArg,
     /* desc, NAME, name */ zVarText, zVar_NAME, zVar_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 34, VALUE_OPT_DVAR,
     /* equiv idx, value */ 34, VALUE_OPT_DVAR,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ DVAR_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionStackArg,
     /* desc, NAME, name */ zDvarText, zDvar_NAME, zDvar_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 35, VALUE_OPT_SLEW,
     /* equiv idx, value */ 35, VALUE_OPT_SLEW,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ SLEW_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zSlewText, zSlew_NAME, zSlew_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 36, VALUE_OPT_NDELAY,
     /* equiv idx, value */ 36, VALUE_OPT_NDELAY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ NDELAY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zNdelayText, zNdelay_NAME, zNdelay_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 37, VALUE_OPT_PDELAY,
     /* equiv idx, value */ 37, VALUE_OPT_PDELAY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ PDELAY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zPdelayText, zPdelay_NAME, zPdelay_Name,
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
 *  Define the Ntpdsim Option Environment
 */
tSCC   zPROGNAME[]   = "NTPDSIM";
tSCC   zUsageTitle[] =
"ntpdsim - NTP daemon simulation program - Ver. 4.2.4p8\n\
USAGE:  %s [ -<flag> [<val>] | --<name>[{=| }<val>] ]...\n";
tSCC   zRcName[]     = ".ntprc";
tSCC*  apzHomeList[] = {
       "$HOME",
       ".",
       NULL };

tSCC   zBugsAddr[]    = "http://bugs.ntp.org, bugs@ntp.org";
#define zExplain NULL
tSCC    zDetail[]     = "\n\n";
tSCC    zFullVersion[] = NTPDSIM_FULL_VERSION;
/* extracted from /usr/local/gnu/autogen-5.9.1/share/autogen/optcode.tpl near line 408 */

#if defined(ENABLE_NLS)
# define OPTPROC_BASE OPTPROC_TRANSLATE
  static tOptionXlateProc translate_option_strings;
#else
# define OPTPROC_BASE OPTPROC_NONE
# define translate_option_strings NULL
#endif /* ENABLE_NLS */

tOptions ntpdsimOptions = {
    OPTIONS_STRUCT_VERSION,
    0, NULL,                    /* original argc + argv    */
    ( OPTPROC_BASE
    + OPTPROC_ERRSTOP
    + OPTPROC_SHORTOPT
    + OPTPROC_LONGOPT
    + OPTPROC_NO_REQ_OPT
    + OPTPROC_ENVIRON
    + OPTPROC_NO_ARGS
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
    43 /* full option count */, 38 /* user option count */
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

#if ! defined(TEST_NTPDSIM_OPTS)

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

#endif /* defined(TEST_NTPDSIM_OPTS) */

/* extracted from /usr/local/gnu/autogen-5.9.1/share/autogen/optmain.tpl near line 92 */

#if defined(TEST_NTPDSIM_OPTS) /* TEST MAIN PROCEDURE: */

int
main( int argc, char** argv )
{
    int res = EXIT_SUCCESS;
    (void)optionProcess( &ntpdsimOptions, argc, argv );
    {
        void optionPutShell( tOptions* );
        optionPutShell( &ntpdsimOptions );
    }
    return res;
}
#endif  /* defined TEST_NTPDSIM_OPTS */
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
  coerce_it((void*)&(ntpdsimOptions._f))

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
        tOptDesc* pOD = ntpdsimOptions.pOptDesc;
        int       ix  = ntpdsimOptions.optCt;

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
/* ntpdsim-opts.c ends here */
