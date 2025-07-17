/*
 *  EDIT THIS FILE WITH CAUTION  (ntpq-opts.c)
 *
 *  It has been AutoGen-ed  May 25, 2024 at 12:04:21 AM by AutoGen 5.18.16
 *  From the definitions    ntpq-opts.def
 *  and the template file   options
 *
 * Generated from AutoOpts 42:1:17 templates.
 *
 *  AutoOpts is a copyrighted work.  This source file is not encumbered
 *  by AutoOpts licensing, but is provided under the licensing terms chosen
 *  by the ntpq author or copyright holder.  AutoOpts is
 *  licensed under the terms of the LGPL.  The redistributable library
 *  (``libopts'') is licensed under the terms of either the LGPL or, at the
 *  users discretion, the BSD license.  See the AutoOpts and/or libopts sources
 *  for details.
 *
 * The ntpq program is copyrighted and licensed
 * under the following terms:
 *
 *  Copyright (C) 1992-2024 The University of Delaware and Network Time Foundation, all rights reserved.
 *  This is free software. It is licensed for use, modification and
 *  redistribution under the terms of the NTP License, copies of which
 *  can be seen at:
 *    <http://ntp.org/license>
 *    <http://opensource.org/licenses/ntp-license.php>
 *
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose with or without fee is hereby granted,
 *  provided that the above copyright notice appears in all copies and that
 *  both the copyright notice and this permission notice appear in
 *  supporting documentation, and that the name The University of Delaware not be used in
 *  advertising or publicity pertaining to distribution of the software
 *  without specific, written prior permission. The University of Delaware and Network Time Foundation makes no
 *  representations about the suitability this software for any purpose. It
 *  is provided "as is" without express or implied warranty.
 */

#ifndef __doxygen__
#define OPTION_CODE_COMPILE 1
#include "ntpq-opts.h"
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef  __cplusplus
extern "C" {
#endif
extern FILE * option_usage_fp;
#define zCopyright      (ntpq_opt_strs+0)
#define zLicenseDescrip (ntpq_opt_strs+341)

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

/**
 *  static const strings for ntpq options
 */
static char const ntpq_opt_strs[2068] =
/*     0 */ "ntpq 4.2.8p18\n"
            "Copyright (C) 1992-2024 The University of Delaware and Network Time Foundation, all rights reserved.\n"
            "This is free software. It is licensed for use, modification and\n"
            "redistribution under the terms of the NTP License, copies of which\n"
            "can be seen at:\n"
            "  <http://ntp.org/license>\n"
            "  <http://opensource.org/licenses/ntp-license.php>\n\0"
/*   341 */ "Permission to use, copy, modify, and distribute this software and its\n"
            "documentation for any purpose with or without fee is hereby granted,\n"
            "provided that the above copyright notice appears in all copies and that\n"
            "both the copyright notice and this permission notice appear in supporting\n"
            "documentation, and that the name The University of Delaware not be used in\n"
            "advertising or publicity pertaining to distribution of the software without\n"
            "specific, written prior permission.  The University of Delaware and Network\n"
            "Time Foundation makes no representations about the suitability this\n"
            "software for any purpose.  It is provided \"as is\" without express or\n"
            "implied warranty.\n\0"
/*  1009 */ "Force IPv4 name resolution\0"
/*  1036 */ "IPV4\0"
/*  1041 */ "ipv4\0"
/*  1046 */ "Force IPv6 name resolution\0"
/*  1073 */ "IPV6\0"
/*  1078 */ "ipv6\0"
/*  1083 */ "run a command and exit\0"
/*  1106 */ "COMMAND\0"
/*  1114 */ "command\0"
/*  1122 */ "Increase debug verbosity level\0"
/*  1153 */ "DEBUG_LEVEL\0"
/*  1165 */ "debug-level\0"
/*  1177 */ "Set the debug verbosity level\0"
/*  1207 */ "SET_DEBUG_LEVEL\0"
/*  1223 */ "set-debug-level\0"
/*  1239 */ "Force ntpq to operate in interactive mode\0"
/*  1281 */ "INTERACTIVE\0"
/*  1293 */ "interactive\0"
/*  1305 */ "numeric host addresses\0"
/*  1328 */ "NUMERIC\0"
/*  1336 */ "numeric\0"
/*  1344 */ "Always output status line with readvar\0"
/*  1383 */ "OLD_RV\0"
/*  1390 */ "old-rv\0"
/*  1397 */ "Print a list of the peers\0"
/*  1423 */ "PEERS\0"
/*  1429 */ "peers\0"
/*  1435 */ "Set default display type for S2+ refids\0"
/*  1475 */ "REFID\0"
/*  1481 */ "refid\0"
/*  1487 */ "Use unconnected UDP to communicate with ntpd (default on Windows)\0"
/*  1553 */ "UNCONNECTED\0"
/*  1565 */ "unconnected\0"
/*  1577 */ "Display the full 'remote' value\0"
/*  1609 */ "WIDE\0"
/*  1614 */ "wide\0"
/*  1619 */ "display extended usage information and exit\0"
/*  1663 */ "help\0"
/*  1668 */ "extended usage information passed thru pager\0"
/*  1713 */ "more-help\0"
/*  1723 */ "output version information and exit\0"
/*  1759 */ "version\0"
/*  1767 */ "save the option state to a config file\0"
/*  1806 */ "save-opts\0"
/*  1816 */ "load options from a config file\0"
/*  1848 */ "LOAD_OPTS\0"
/*  1858 */ "no-load-opts\0"
/*  1871 */ "no\0"
/*  1874 */ "NTPQ\0"
/*  1879 */ "ntpq - standard NTP query program - Ver. 4.2.8p18\n"
            "Usage:  %s [ -<flag> [<val>] | --<name>[{=| }<val>] ]... [ host ...]\n\0"
/*  1999 */ "$HOME\0"
/*  2005 */ ".\0"
/*  2007 */ ".ntprc\0"
/*  2014 */ "https://bugs.ntp.org, bugs@ntp.org\0"
/*  2049 */ "ntpq 4.2.8p18\0"
/*  2063 */ "hash";

/**
 *  ipv4 option description with
 *  "Must also have options" and "Incompatible options":
 */
/** Descriptive text for the ipv4 option */
#define IPV4_DESC      (ntpq_opt_strs+1009)
/** Upper-cased name for the ipv4 option */
#define IPV4_NAME      (ntpq_opt_strs+1036)
/** Name string for the ipv4 option */
#define IPV4_name      (ntpq_opt_strs+1041)
/** Other options that appear in conjunction with the ipv4 option */
static int const aIpv4CantList[] = {
    INDEX_OPT_IPV6, NO_EQUIVALENT };
/** Compiled in flag settings for the ipv4 option */
#define IPV4_FLAGS     (OPTST_DISABLED)

/**
 *  ipv6 option description with
 *  "Must also have options" and "Incompatible options":
 */
/** Descriptive text for the ipv6 option */
#define IPV6_DESC      (ntpq_opt_strs+1046)
/** Upper-cased name for the ipv6 option */
#define IPV6_NAME      (ntpq_opt_strs+1073)
/** Name string for the ipv6 option */
#define IPV6_name      (ntpq_opt_strs+1078)
/** Other options that appear in conjunction with the ipv6 option */
static int const aIpv6CantList[] = {
    INDEX_OPT_IPV4, NO_EQUIVALENT };
/** Compiled in flag settings for the ipv6 option */
#define IPV6_FLAGS     (OPTST_DISABLED)

/**
 *  command option description:
 */
/** Descriptive text for the command option */
#define COMMAND_DESC      (ntpq_opt_strs+1083)
/** Upper-cased name for the command option */
#define COMMAND_NAME      (ntpq_opt_strs+1106)
/** Name string for the command option */
#define COMMAND_name      (ntpq_opt_strs+1114)
/** Compiled in flag settings for the command option */
#define COMMAND_FLAGS     (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

/**
 *  debug-level option description:
 */
/** Descriptive text for the debug-level option */
#define DEBUG_LEVEL_DESC      (ntpq_opt_strs+1122)
/** Upper-cased name for the debug-level option */
#define DEBUG_LEVEL_NAME      (ntpq_opt_strs+1153)
/** Name string for the debug-level option */
#define DEBUG_LEVEL_name      (ntpq_opt_strs+1165)
/** Compiled in flag settings for the debug-level option */
#define DEBUG_LEVEL_FLAGS     (OPTST_DISABLED)

/**
 *  set-debug-level option description:
 */
/** Descriptive text for the set-debug-level option */
#define SET_DEBUG_LEVEL_DESC      (ntpq_opt_strs+1177)
/** Upper-cased name for the set-debug-level option */
#define SET_DEBUG_LEVEL_NAME      (ntpq_opt_strs+1207)
/** Name string for the set-debug-level option */
#define SET_DEBUG_LEVEL_name      (ntpq_opt_strs+1223)
/** Compiled in flag settings for the set-debug-level option */
#define SET_DEBUG_LEVEL_FLAGS     (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_NUMERIC))

/**
 *  interactive option description with
 *  "Must also have options" and "Incompatible options":
 */
/** Descriptive text for the interactive option */
#define INTERACTIVE_DESC      (ntpq_opt_strs+1239)
/** Upper-cased name for the interactive option */
#define INTERACTIVE_NAME      (ntpq_opt_strs+1281)
/** Name string for the interactive option */
#define INTERACTIVE_name      (ntpq_opt_strs+1293)
/** Other options that appear in conjunction with the interactive option */
static int const aInteractiveCantList[] = {
    INDEX_OPT_COMMAND,
    INDEX_OPT_PEERS, NO_EQUIVALENT };
/** Compiled in flag settings for the interactive option */
#define INTERACTIVE_FLAGS     (OPTST_DISABLED)

/**
 *  numeric option description:
 */
/** Descriptive text for the numeric option */
#define NUMERIC_DESC      (ntpq_opt_strs+1305)
/** Upper-cased name for the numeric option */
#define NUMERIC_NAME      (ntpq_opt_strs+1328)
/** Name string for the numeric option */
#define NUMERIC_name      (ntpq_opt_strs+1336)
/** Compiled in flag settings for the numeric option */
#define NUMERIC_FLAGS     (OPTST_DISABLED)

/**
 *  old-rv option description:
 */
/** Descriptive text for the old-rv option */
#define OLD_RV_DESC      (ntpq_opt_strs+1344)
/** Upper-cased name for the old-rv option */
#define OLD_RV_NAME      (ntpq_opt_strs+1383)
/** Name string for the old-rv option */
#define OLD_RV_name      (ntpq_opt_strs+1390)
/** Compiled in flag settings for the old-rv option */
#define OLD_RV_FLAGS     (OPTST_DISABLED)

/**
 *  peers option description with
 *  "Must also have options" and "Incompatible options":
 */
/** Descriptive text for the peers option */
#define PEERS_DESC      (ntpq_opt_strs+1397)
/** Upper-cased name for the peers option */
#define PEERS_NAME      (ntpq_opt_strs+1423)
/** Name string for the peers option */
#define PEERS_name      (ntpq_opt_strs+1429)
/** Other options that appear in conjunction with the peers option */
static int const aPeersCantList[] = {
    INDEX_OPT_INTERACTIVE, NO_EQUIVALENT };
/** Compiled in flag settings for the peers option */
#define PEERS_FLAGS     (OPTST_DISABLED)

/**
 *  refid option description:
 */
/** Descriptive text for the refid option */
#define REFID_DESC      (ntpq_opt_strs+1435)
/** Upper-cased name for the refid option */
#define REFID_NAME      (ntpq_opt_strs+1475)
/** Name string for the refid option */
#define REFID_name      (ntpq_opt_strs+1481)
/** The compiled in default value for the refid option argument */
#define REFID_DFT_ARG   ((char const*)REFID_IPV4)
/** Compiled in flag settings for the refid option */
#define REFID_FLAGS     (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_ENUMERATION))

/**
 *  unconnected option description:
 */
/** Descriptive text for the unconnected option */
#define UNCONNECTED_DESC      (ntpq_opt_strs+1487)
/** Upper-cased name for the unconnected option */
#define UNCONNECTED_NAME      (ntpq_opt_strs+1553)
/** Name string for the unconnected option */
#define UNCONNECTED_name      (ntpq_opt_strs+1565)
/** Compiled in flag settings for the unconnected option */
#define UNCONNECTED_FLAGS     (OPTST_DISABLED)

/**
 *  wide option description:
 */
/** Descriptive text for the wide option */
#define WIDE_DESC      (ntpq_opt_strs+1577)
/** Upper-cased name for the wide option */
#define WIDE_NAME      (ntpq_opt_strs+1609)
/** Name string for the wide option */
#define WIDE_name      (ntpq_opt_strs+1614)
/** Compiled in flag settings for the wide option */
#define WIDE_FLAGS     (OPTST_DISABLED)

/*
 *  Help/More_Help/Version option descriptions:
 */
#define HELP_DESC       (ntpq_opt_strs+1619)
#define HELP_name       (ntpq_opt_strs+1663)
#ifdef HAVE_WORKING_FORK
#define MORE_HELP_DESC  (ntpq_opt_strs+1668)
#define MORE_HELP_name  (ntpq_opt_strs+1713)
#define MORE_HELP_FLAGS (OPTST_IMM | OPTST_NO_INIT)
#else
#define MORE_HELP_DESC  HELP_DESC
#define MORE_HELP_name  HELP_name
#define MORE_HELP_FLAGS (OPTST_OMITTED | OPTST_NO_INIT)
#endif
#ifdef NO_OPTIONAL_OPT_ARGS
#  define VER_FLAGS     (OPTST_IMM | OPTST_NO_INIT)
#else
#  define VER_FLAGS     (OPTST_SET_ARGTYPE(OPARG_TYPE_STRING) | \
                         OPTST_ARG_OPTIONAL | OPTST_IMM | OPTST_NO_INIT)
#endif
#define VER_DESC        (ntpq_opt_strs+1723)
#define VER_name        (ntpq_opt_strs+1759)
#define SAVE_OPTS_DESC  (ntpq_opt_strs+1767)
#define SAVE_OPTS_name  (ntpq_opt_strs+1806)
#define LOAD_OPTS_DESC     (ntpq_opt_strs+1816)
#define LOAD_OPTS_NAME     (ntpq_opt_strs+1848)
#define NO_LOAD_OPTS_name  (ntpq_opt_strs+1858)
#define LOAD_OPTS_pfx      (ntpq_opt_strs+1871)
#define LOAD_OPTS_name     (NO_LOAD_OPTS_name + 3)
/**
 *  Declare option callback procedures
 */
extern tOptProc
    ntpOptionPrintVersion,   ntpq_custom_opt_handler, optionBooleanVal,
    optionNestedVal,         optionNumericVal,        optionPagedUsage,
    optionResetOpt,          optionStackArg,          optionTimeDate,
    optionTimeVal,           optionUnstackArg,        optionVendorOption;
static tOptProc
    doOptDebug_Level, doOptRefid, doUsageOpt;
#define VER_PROC        ntpOptionPrintVersion

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**
 *  Define the ntpq Option Descriptions.
 * This is an array of OPTION_CT entries, one for each
 * option that the ntpq program responds to.
 */
static tOptDesc optDesc[OPTION_CT] = {
  {  /* entry idx, value */ 0, VALUE_OPT_IPV4,
     /* equiv idx, value */ 0, VALUE_OPT_IPV4,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ IPV4_FLAGS, 0,
     /* last opt argumnt */ { NULL }, /* --ipv4 */
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aIpv4CantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ IPV4_DESC, IPV4_NAME, IPV4_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 1, VALUE_OPT_IPV6,
     /* equiv idx, value */ 1, VALUE_OPT_IPV6,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ IPV6_FLAGS, 0,
     /* last opt argumnt */ { NULL }, /* --ipv6 */
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aIpv6CantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ IPV6_DESC, IPV6_NAME, IPV6_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 2, VALUE_OPT_COMMAND,
     /* equiv idx, value */ 2, VALUE_OPT_COMMAND,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ COMMAND_FLAGS, 0,
     /* last opt argumnt */ { NULL }, /* --command */
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ ntpq_custom_opt_handler,
     /* desc, NAME, name */ COMMAND_DESC, COMMAND_NAME, COMMAND_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 3, VALUE_OPT_DEBUG_LEVEL,
     /* equiv idx, value */ 3, VALUE_OPT_DEBUG_LEVEL,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ DEBUG_LEVEL_FLAGS, 0,
     /* last opt argumnt */ { NULL }, /* --debug-level */
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ doOptDebug_Level,
     /* desc, NAME, name */ DEBUG_LEVEL_DESC, DEBUG_LEVEL_NAME, DEBUG_LEVEL_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 4, VALUE_OPT_SET_DEBUG_LEVEL,
     /* equiv idx, value */ 4, VALUE_OPT_SET_DEBUG_LEVEL,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ SET_DEBUG_LEVEL_FLAGS, 0,
     /* last opt argumnt */ { NULL }, /* --set-debug-level */
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionNumericVal,
     /* desc, NAME, name */ SET_DEBUG_LEVEL_DESC, SET_DEBUG_LEVEL_NAME, SET_DEBUG_LEVEL_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 5, VALUE_OPT_INTERACTIVE,
     /* equiv idx, value */ 5, VALUE_OPT_INTERACTIVE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ INTERACTIVE_FLAGS, 0,
     /* last opt argumnt */ { NULL }, /* --interactive */
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aInteractiveCantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ INTERACTIVE_DESC, INTERACTIVE_NAME, INTERACTIVE_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 6, VALUE_OPT_NUMERIC,
     /* equiv idx, value */ 6, VALUE_OPT_NUMERIC,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ NUMERIC_FLAGS, 0,
     /* last opt argumnt */ { NULL }, /* --numeric */
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ NUMERIC_DESC, NUMERIC_NAME, NUMERIC_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 7, VALUE_OPT_OLD_RV,
     /* equiv idx, value */ 7, VALUE_OPT_OLD_RV,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ OLD_RV_FLAGS, 0,
     /* last opt argumnt */ { NULL }, /* --old-rv */
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ OLD_RV_DESC, OLD_RV_NAME, OLD_RV_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 8, VALUE_OPT_PEERS,
     /* equiv idx, value */ 8, VALUE_OPT_PEERS,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ PEERS_FLAGS, 0,
     /* last opt argumnt */ { NULL }, /* --peers */
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aPeersCantList,
     /* option proc      */ ntpq_custom_opt_handler,
     /* desc, NAME, name */ PEERS_DESC, PEERS_NAME, PEERS_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 9, VALUE_OPT_REFID,
     /* equiv idx, value */ 9, VALUE_OPT_REFID,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ REFID_FLAGS, 0,
     /* last opt argumnt */ { REFID_DFT_ARG },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ doOptRefid,
     /* desc, NAME, name */ REFID_DESC, REFID_NAME, REFID_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 10, VALUE_OPT_UNCONNECTED,
     /* equiv idx, value */ 10, VALUE_OPT_UNCONNECTED,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ UNCONNECTED_FLAGS, 0,
     /* last opt argumnt */ { NULL }, /* --unconnected */
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ UNCONNECTED_DESC, UNCONNECTED_NAME, UNCONNECTED_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 11, VALUE_OPT_WIDE,
     /* equiv idx, value */ 11, VALUE_OPT_WIDE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ WIDE_FLAGS, 0,
     /* last opt argumnt */ { NULL }, /* --wide */
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ WIDE_DESC, WIDE_NAME, WIDE_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ INDEX_OPT_VERSION, VALUE_OPT_VERSION,
     /* equiv idx value  */ NO_EQUIVALENT, VALUE_OPT_VERSION,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ VER_FLAGS, AOUSE_VERSION,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ VER_PROC,
     /* desc, NAME, name */ VER_DESC, NULL, VER_name,
     /* disablement strs */ NULL, NULL },



  {  /* entry idx, value */ INDEX_OPT_HELP, VALUE_OPT_HELP,
     /* equiv idx value  */ NO_EQUIVALENT, VALUE_OPT_HELP,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ OPTST_IMM | OPTST_NO_INIT, AOUSE_HELP,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ doUsageOpt,
     /* desc, NAME, name */ HELP_DESC, NULL, HELP_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ INDEX_OPT_MORE_HELP, VALUE_OPT_MORE_HELP,
     /* equiv idx value  */ NO_EQUIVALENT, VALUE_OPT_MORE_HELP,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ MORE_HELP_FLAGS, AOUSE_MORE_HELP,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL,  NULL,
     /* option proc      */ optionPagedUsage,
     /* desc, NAME, name */ MORE_HELP_DESC, NULL, MORE_HELP_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ INDEX_OPT_SAVE_OPTS, VALUE_OPT_SAVE_OPTS,
     /* equiv idx value  */ NO_EQUIVALENT, VALUE_OPT_SAVE_OPTS,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ OPTST_SET_ARGTYPE(OPARG_TYPE_STRING)
                       | OPTST_ARG_OPTIONAL | OPTST_NO_INIT, AOUSE_SAVE_OPTS,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL,  NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ SAVE_OPTS_DESC, NULL, SAVE_OPTS_name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ INDEX_OPT_LOAD_OPTS, VALUE_OPT_LOAD_OPTS,
     /* equiv idx value  */ NO_EQUIVALENT, VALUE_OPT_LOAD_OPTS,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ OPTST_SET_ARGTYPE(OPARG_TYPE_STRING)
			  | OPTST_DISABLE_IMM, AOUSE_LOAD_OPTS,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionLoadOpt,
     /* desc, NAME, name */ LOAD_OPTS_DESC, LOAD_OPTS_NAME, LOAD_OPTS_name,
     /* disablement strs */ NO_LOAD_OPTS_name, LOAD_OPTS_pfx }
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/** Reference to the upper cased version of ntpq. */
#define zPROGNAME       (ntpq_opt_strs+1874)
/** Reference to the title line for ntpq usage. */
#define zUsageTitle     (ntpq_opt_strs+1879)
/** ntpq configuration file name. */
#define zRcName         (ntpq_opt_strs+2007)
/** Directories to search for ntpq config files. */
static char const * const apzHomeList[3] = {
    ntpq_opt_strs+1999,
    ntpq_opt_strs+2005,
    NULL };
/** The ntpq program bug email address. */
#define zBugsAddr       (ntpq_opt_strs+2014)
/** Clarification/explanation of what ntpq does. */
#define zExplain        (NULL)
/** Extra detail explaining what ntpq does. */
#define zDetail         (NULL)
/** The full version string for ntpq. */
#define zFullVersion    (ntpq_opt_strs+2049)
/* extracted from optcode.tlib near line 342 */

#if defined(ENABLE_NLS)
# define OPTPROC_BASE OPTPROC_TRANSLATE
  static tOptionXlateProc translate_option_strings;
#else
# define OPTPROC_BASE OPTPROC_NONE
# define translate_option_strings NULL
#endif /* ENABLE_NLS */

#define ntpq_full_usage (NULL)
#define ntpq_short_usage (NULL)

#endif /* not defined __doxygen__ */

/*
 *  Create the static procedure(s) declared above.
 */
/**
 * The callout function that invokes the optionUsage function.
 *
 * @param[in] opts the AutoOpts option description structure
 * @param[in] od   the descriptor for the "help" (usage) option.
 * @noreturn
 */
static void
doUsageOpt(tOptions * opts, tOptDesc * od)
{
    int ex_code;
    ex_code = NTPQ_EXIT_SUCCESS;
    optionUsage(&ntpqOptions, ex_code);
    /* NOTREACHED */
    exit(NTPQ_EXIT_FAILURE);
    (void)opts;
    (void)od;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**
 * Code to handle the debug-level option.
 *
 * @param[in] pOptions the ntpq options data structure
 * @param[in,out] pOptDesc the option descriptor for this option.
 */
static void
doOptDebug_Level(tOptions* pOptions, tOptDesc* pOptDesc)
{
    /*
     * Be sure the flag-code[0] handles special values for the options pointer
     * viz. (poptions <= OPTPROC_EMIT_LIMIT) *and also* the special flag bit
     * ((poptdesc->fOptState & OPTST_RESET) != 0) telling the option to
     * reset its state.
     */
    /* extracted from debug-opt.def, line 15 */
OPT_VALUE_SET_DEBUG_LEVEL++;
    (void)pOptDesc;
    (void)pOptions;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**
 * Code to handle the refid option.
 * Set the default display format for S2+ refids.
 * @param[in] pOptions the ntpq options data structure
 * @param[in,out] pOptDesc the option descriptor for this option.
 */
static void
doOptRefid(tOptions* pOptions, tOptDesc* pOptDesc)
{

/* extracted from optmain.tlib near line 945 */
    static char const * const names[2] = {
        ntpq_opt_strs+2063, ntpq_opt_strs+1041 };

    if (pOptions <= OPTPROC_EMIT_LIMIT) {
        (void) optionEnumerationVal(pOptions, pOptDesc, names, 2);
        return; /* protect AutoOpts client code from internal callbacks */
    }

    pOptDesc->optArg.argEnum =
        optionEnumerationVal(pOptions, pOptDesc, names, 2);
}
/* extracted from optmain.tlib near line 1250 */

/**
 * The directory containing the data associated with ntpq.
 */
#ifndef  PKGDATADIR
# define PKGDATADIR ""
#endif

/**
 * Information about the person or institution that packaged ntpq
 * for the current distribution.
 */
#ifndef  WITH_PACKAGER
# define ntpq_packager_info NULL
#else
/** Packager information for ntpq. */
static char const ntpq_packager_info[] =
    "Packaged by " WITH_PACKAGER

# ifdef WITH_PACKAGER_VERSION
        " ("WITH_PACKAGER_VERSION")"
# endif

# ifdef WITH_PACKAGER_BUG_REPORTS
    "\nReport ntpq bugs to " WITH_PACKAGER_BUG_REPORTS
# endif
    "\n";
#endif
#ifndef __doxygen__

#endif /* __doxygen__ */
/**
 * The option definitions for ntpq.  The one structure that
 * binds them all.
 */
tOptions ntpqOptions = {
    OPTIONS_STRUCT_VERSION,
    0, NULL,                    /* original argc + argv    */
    ( OPTPROC_BASE
    + OPTPROC_ERRSTOP
    + OPTPROC_SHORTOPT
    + OPTPROC_LONGOPT
    + OPTPROC_NO_REQ_OPT
    + OPTPROC_ENVIRON
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
      INDEX_OPT_SAVE_OPTS, /* save option index */
      NO_EQUIVALENT, /* '-#' option index */
      NO_EQUIVALENT /* index of default opt */
    },
    17 /* full option count */, 12 /* user option count */,
    ntpq_full_usage, ntpq_short_usage,
    NULL, NULL,
    PKGDATADIR, ntpq_packager_info
};

#if ENABLE_NLS
/**
 * This code is designed to translate translatable option text for the
 * ntpq program.  These translations happen upon entry
 * to optionProcess().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_DCGETTEXT
# include <gettext.h>
#endif
#include <autoopts/usage-txt.h>

static char * AO_gettext(char const * pz);
static void   coerce_it(void ** s);

/**
 * AutoGen specific wrapper function for gettext.  It relies on the macro _()
 * to convert from English to the target language, then strdup-duplicates the
 * result string.  It tries the "libopts" domain first, then whatever has been
 * set via the \a textdomain(3) call.
 *
 * @param[in] pz the input text used as a lookup key.
 * @returns the translated text (if there is one),
 *   or the original text (if not).
 */
static char *
AO_gettext(char const * pz)
{
    char * res;
    if (pz == NULL)
        return NULL;
#ifdef HAVE_DCGETTEXT
    /*
     * While processing the option_xlateable_txt data, try to use the
     * "libopts" domain.  Once we switch to the option descriptor data,
     * do *not* use that domain.
     */
    if (option_xlateable_txt.field_ct != 0) {
        res = dgettext("libopts", pz);
        if (res == pz)
            res = (char *)VOIDP(_(pz));
    } else
        res = (char *)VOIDP(_(pz));
#else
    res = (char *)VOIDP(_(pz));
#endif
    if (res == pz)
        return res;
    res = strdup(res);
    if (res == NULL) {
        fputs(_("No memory for duping translated strings\n"), stderr);
        exit(NTPQ_EXIT_FAILURE);
    }
    return res;
}

/**
 * All the pointers we use are marked "* const", but they are stored in
 * writable memory.  Coerce the mutability and set the pointer.
 */
static void coerce_it(void ** s) { *s = AO_gettext(*s);
}

/**
 * Translate all the translatable strings in the ntpqOptions
 * structure defined above.  This is done only once.
 */
static void
translate_option_strings(void)
{
    tOptions * const opts = &ntpqOptions;

    /*
     *  Guard against re-translation.  It won't work.  The strings will have
     *  been changed by the first pass through this code.  One shot only.
     */
    if (option_xlateable_txt.field_ct != 0) {
        /*
         *  Do the translations.  The first pointer follows the field count
         *  field.  The field count field is the size of a pointer.
         */
        char ** ppz = (char**)VOIDP(&(option_xlateable_txt));
        int     ix  = option_xlateable_txt.field_ct;

        do {
            ppz++; /* skip over field_ct */
            *ppz = AO_gettext(*ppz);
        } while (--ix > 0);
        /* prevent re-translation and disable "libopts" domain lookup */
        option_xlateable_txt.field_ct = 0;

        coerce_it(VOIDP(&(opts->pzCopyright)));
        coerce_it(VOIDP(&(opts->pzCopyNotice)));
        coerce_it(VOIDP(&(opts->pzFullVersion)));
        coerce_it(VOIDP(&(opts->pzUsageTitle)));
        coerce_it(VOIDP(&(opts->pzExplain)));
        coerce_it(VOIDP(&(opts->pzDetail)));
        {
            tOptDesc * od = opts->pOptDesc;
            for (ix = opts->optCt; ix > 0; ix--, od++)
                coerce_it(VOIDP(&(od->pzText)));
        }
    }
}
#endif /* ENABLE_NLS */

#ifdef DO_NOT_COMPILE_THIS_CODE_IT_IS_FOR_GETTEXT
/** I18N function strictly for xgettext.  Do not compile. */
static void bogus_function(void) {
  /* TRANSLATORS:

     The following dummy function was crated solely so that xgettext can
     extract the correct strings.  These strings are actually referenced
     by a field name in the ntpqOptions structure noted in the
     comments below.  The literal text is defined in ntpq_opt_strs.

     NOTE: the strings below are segmented with respect to the source string
     ntpq_opt_strs.  The strings above are handed off for translation
     at run time a paragraph at a time.  Consequently, they are presented here
     for translation a paragraph at a time.

     ALSO: often the description for an option will reference another option
     by name.  These are set off with apostrophe quotes (I hope).  Do not
     translate option names.
   */
  /* referenced via ntpqOptions.pzCopyright */
  puts(_("ntpq 4.2.8p18\n\
Copyright (C) 1992-2024 The University of Delaware and Network Time Foundation, all rights reserved.\n\
This is free software. It is licensed for use, modification and\n\
redistribution under the terms of the NTP License, copies of which\n\
can be seen at:\n"));
  puts(_("  <http://ntp.org/license>\n\
  <http://opensource.org/licenses/ntp-license.php>\n"));

  /* referenced via ntpqOptions.pzCopyNotice */
  puts(_("Permission to use, copy, modify, and distribute this software and its\n\
documentation for any purpose with or without fee is hereby granted,\n\
provided that the above copyright notice appears in all copies and that\n\
both the copyright notice and this permission notice appear in supporting\n\
documentation, and that the name The University of Delaware not be used in\n\
advertising or publicity pertaining to distribution of the software without\n\
specific, written prior permission.  The University of Delaware and Network\n\
Time Foundation makes no representations about the suitability this\n\
software for any purpose.  It is provided \"as is\" without express or\n\
implied warranty.\n"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("Force IPv4 name resolution"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("Force IPv6 name resolution"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("run a command and exit"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("Increase debug verbosity level"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("Set the debug verbosity level"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("Force ntpq to operate in interactive mode"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("numeric host addresses"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("Always output status line with readvar"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("Print a list of the peers"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("Set default display type for S2+ refids"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("Use unconnected UDP to communicate with ntpd (default on Windows)"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("Display the full 'remote' value"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("display extended usage information and exit"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("extended usage information passed thru pager"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("output version information and exit"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("save the option state to a config file"));

  /* referenced via ntpqOptions.pOptDesc->pzText */
  puts(_("load options from a config file"));

  /* referenced via ntpqOptions.pzUsageTitle */
  puts(_("ntpq - standard NTP query program - Ver. 4.2.8p18\n\
Usage:  %s [ -<flag> [<val>] | --<name>[{=| }<val>] ]... [ host ...]\n"));

  /* referenced via ntpqOptions.pzFullVersion */
  puts(_("ntpq 4.2.8p18"));

  /* referenced via ntpqOptions.pzFullUsage */
  puts(_("<<<NOT-FOUND>>>"));

  /* referenced via ntpqOptions.pzShortUsage */
  puts(_("<<<NOT-FOUND>>>"));
  /* LIBOPTS-MESSAGES: */
#line 67 "../autoopts.c"
  puts(_("allocation of %d bytes failed\n"));
#line 89 "../autoopts.c"
  puts(_("allocation of %d bytes failed\n"));
#line 48 "../init.c"
  puts(_("AutoOpts function called without option descriptor\n"));
#line 81 "../init.c"
  puts(_("\tThis exceeds the compiled library version:  "));
#line 79 "../init.c"
  puts(_("Automated Options Processing Error!\n"
       "\t%s called AutoOpts function with structure version %d:%d:%d.\n"));
#line 78 "../autoopts.c"
  puts(_("realloc of %d bytes at 0x%p failed\n"));
#line 83 "../init.c"
  puts(_("\tThis is less than the minimum library version:  "));
#line 121 "../version.c"
  puts(_("Automated Options version %s\n"
       "\tCopyright (C) 1999-2017 by Bruce Korb - all rights reserved\n"));
#line 49 "../makeshell.c"
  puts(_("(AutoOpts bug):  %s.\n"));
#line 90 "../reset.c"
  puts(_("optionResetOpt() called, but reset-option not configured"));
#line 241 "../usage.c"
  puts(_("could not locate the 'help' option"));
#line 330 "../autoopts.c"
  puts(_("optionProcess() was called with invalid data"));
#line 697 "../usage.c"
  puts(_("invalid argument type specified"));
#line 568 "../find.c"
  puts(_("defaulted to option with optional arg"));
#line 76 "../alias.c"
  puts(_("aliasing option is out of range."));
#line 210 "../enum.c"
  puts(_("%s error:  the keyword '%s' is ambiguous for %s\n"));
#line 78 "../find.c"
  puts(_("  The following options match:\n"));
#line 263 "../find.c"
  puts(_("%s: ambiguous option name: %s (matches %d options)\n"));
#line 161 "../check.c"
  puts(_("%s: Command line arguments required\n"));
#line 43 "../alias.c"
  puts(_("%d %s%s options allowed\n"));
#line 56 "../makeshell.c"
  puts(_("%s error %d (%s) calling %s for '%s'\n"));
#line 268 "../makeshell.c"
  puts(_("interprocess pipe"));
#line 171 "../version.c"
  puts(_("error: version option argument '%c' invalid.  Use:\n"
       "\t'v' - version only\n"
       "\t'c' - version and copyright\n"
       "\t'n' - version and full copyright notice\n"));
#line 58 "../check.c"
  puts(_("%s error:  the '%s' and '%s' options conflict\n"));
#line 187 "../find.c"
  puts(_("%s: The '%s' option has been disabled."));
#line 400 "../find.c"
  puts(_("%s: The '%s' option has been disabled."));
#line 38 "../alias.c"
  puts(_("-equivalence"));
#line 439 "../find.c"
  puts(_("%s: illegal option -- %c\n"));
#line 110 "../reset.c"
  puts(_("%s: illegal option -- %c\n"));
#line 241 "../find.c"
  puts(_("%s: illegal option -- %s\n"));
#line 740 "../find.c"
  puts(_("%s: illegal option -- %s\n"));
#line 118 "../reset.c"
  puts(_("%s: illegal option -- %s\n"));
#line 305 "../find.c"
  puts(_("%s: unknown vendor extension option -- %s\n"));
#line 135 "../enum.c"
  puts(_("  or an integer from %d through %d\n"));
#line 145 "../enum.c"
  puts(_("  or an integer from %d through %d\n"));
#line 696 "../usage.c"
  puts(_("%s error:  invalid option descriptor for %s\n"));
#line 1030 "../usage.c"
  puts(_("%s error:  invalid option descriptor for %s\n"));
#line 355 "../find.c"
  puts(_("%s: invalid option name: %s\n"));
#line 497 "../find.c"
  puts(_("%s: The '%s' option requires an argument.\n"));
#line 150 "../autoopts.c"
  puts(_("(AutoOpts bug):  Equivalenced option '%s' was equivalenced to both\n"
       "\t'%s' and '%s'."));
#line 94 "../check.c"
  puts(_("%s error:  The %s option is required\n"));
#line 602 "../find.c"
  puts(_("%s: The '%s' option cannot have an argument.\n"));
#line 151 "../check.c"
  puts(_("%s: Command line arguments are not allowed.\n"));
#line 568 "../save.c"
  puts(_("error %d (%s) creating %s\n"));
#line 210 "../enum.c"
  puts(_("%s error:  '%s' does not match any %s keywords.\n"));
#line 93 "../reset.c"
  puts(_("%s error: The '%s' option requires an argument.\n"));
#line 122 "../save.c"
  puts(_("error %d (%s) stat-ing %s\n"));
#line 175 "../save.c"
  puts(_("error %d (%s) stat-ing %s\n"));
#line 143 "../restore.c"
  puts(_("%s error: no saved option state\n"));
#line 225 "../autoopts.c"
  puts(_("'%s' is not a command line option.\n"));
#line 113 "../time.c"
  puts(_("%s error:  '%s' is not a recognizable date/time.\n"));
#line 50 "../time.c"
  puts(_("%s error:  '%s' is not a recognizable time duration.\n"));
#line 92 "../check.c"
  puts(_("%s error:  The %s option must appear %d times.\n"));
#line 165 "../numeric.c"
  puts(_("%s error:  '%s' is not a recognizable number.\n"));
#line 176 "../enum.c"
  puts(_("%s error:  %s exceeds %s keyword count\n"));
#line 279 "../usage.c"
  puts(_("Try '%s %s' for more information.\n"));
#line 45 "../alias.c"
  puts(_("one %s%s option allowed\n"));
#line 170 "../makeshell.c"
  puts(_("standard output"));
#line 905 "../makeshell.c"
  puts(_("standard output"));
#line 223 "../usage.c"
  puts(_("standard output"));
#line 364 "../usage.c"
  puts(_("standard output"));
#line 574 "../usage.c"
  puts(_("standard output"));
#line 178 "../version.c"
  puts(_("standard output"));
#line 223 "../usage.c"
  puts(_("standard error"));
#line 364 "../usage.c"
  puts(_("standard error"));
#line 574 "../usage.c"
  puts(_("standard error"));
#line 178 "../version.c"
  puts(_("standard error"));
#line 170 "../makeshell.c"
  puts(_("write"));
#line 905 "../makeshell.c"
  puts(_("write"));
#line 222 "../usage.c"
  puts(_("write"));
#line 363 "../usage.c"
  puts(_("write"));
#line 573 "../usage.c"
  puts(_("write"));
#line 177 "../version.c"
  puts(_("write"));
#line 60 "../numeric.c"
  puts(_("%s error:  %s option value %ld is out of range.\n"));
#line 44 "../check.c"
  puts(_("%s error:  %s option requires the %s option\n"));
#line 121 "../save.c"
  puts(_("%s warning:  cannot save options - %s not regular file\n"));
#line 174 "../save.c"
  puts(_("%s warning:  cannot save options - %s not regular file\n"));
#line 193 "../save.c"
  puts(_("%s warning:  cannot save options - %s not regular file\n"));
#line 567 "../save.c"
  puts(_("%s warning:  cannot save options - %s not regular file\n"));
  /* END-LIBOPTS-MESSAGES */

  /* USAGE-TEXT: */
#line 822 "../usage.c"
  puts(_("\t\t\t\t- an alternate for '%s'\n"));
#line 1097 "../usage.c"
  puts(_("Version, usage and configuration options:"));
#line 873 "../usage.c"
  puts(_("\t\t\t\t- default option for unnamed options\n"));
#line 786 "../usage.c"
  puts(_("\t\t\t\t- disabled as '--%s'\n"));
#line 1066 "../usage.c"
  puts(_(" --- %-14s %s\n"));
#line 1064 "../usage.c"
  puts(_("This option has been disabled"));
#line 813 "../usage.c"
  puts(_("\t\t\t\t- enabled by default\n"));
#line 40 "../alias.c"
  puts(_("%s error:  only "));
#line 1143 "../usage.c"
  puts(_(" - examining environment variables named %s_*\n"));
#line 168 "../file.c"
  puts(_("\t\t\t\t- file must not pre-exist\n"));
#line 172 "../file.c"
  puts(_("\t\t\t\t- file must pre-exist\n"));
#line 329 "../usage.c"
  puts(_("Options are specified by doubled hyphens and their name or by a single\n"
       "hyphen and the flag character.\n"));
#line 882 "../makeshell.c"
  puts(_("\n"
       "= = = = = = = =\n\n"
       "This incarnation of genshell will produce\n"
       "a shell script to parse the options for %s:\n\n"));
#line 142 "../enum.c"
  puts(_("  or an integer mask with any of the lower %d bits set\n"));
#line 846 "../usage.c"
  puts(_("\t\t\t\t- is a set membership option\n"));
#line 867 "../usage.c"
  puts(_("\t\t\t\t- must appear between %d and %d times\n"));
#line 331 "../usage.c"
  puts(_("Options are specified by single or double hyphens and their name.\n"));
#line 853 "../usage.c"
  puts(_("\t\t\t\t- may appear multiple times\n"));
#line 840 "../usage.c"
  puts(_("\t\t\t\t- may not be preset\n"));
#line 1258 "../usage.c"
  puts(_("   Arg Option-Name    Description\n"));
#line 1194 "../usage.c"
  puts(_("  Flg Arg Option-Name    Description\n"));
#line 1252 "../usage.c"
  puts(_("  Flg Arg Option-Name    Description\n"));
#line 1253 "../usage.c"
  puts(_(" %3s %s"));
#line 1259 "../usage.c"
  puts(_(" %3s %s"));
#line 336 "../usage.c"
  puts(_("The '-#<number>' option may omit the hash char\n"));
#line 332 "../usage.c"
  puts(_("All arguments are named options.\n"));
#line 920 "../usage.c"
  puts(_(" - reading file %s"));
#line 358 "../usage.c"
  puts(_("\n"
       "Please send bug reports to:  <%s>\n"));
#line 100 "../version.c"
  puts(_("\n"
       "Please send bug reports to:  <%s>\n"));
#line 129 "../version.c"
  puts(_("\n"
       "Please send bug reports to:  <%s>\n"));
#line 852 "../usage.c"
  puts(_("\t\t\t\t- may NOT appear - preset only\n"));
#line 893 "../usage.c"
  puts(_("\n"
       "The following option preset mechanisms are supported:\n"));
#line 1141 "../usage.c"
  puts(_("\n"
       "The following option preset mechanisms are supported:\n"));
#line 631 "../usage.c"
  puts(_("prohibits these options:\n"));
#line 626 "../usage.c"
  puts(_("prohibits the option '%s'\n"));
#line 81 "../numeric.c"
  puts(_("%s%ld to %ld"));
#line 79 "../numeric.c"
  puts(_("%sgreater than or equal to %ld"));
#line 75 "../numeric.c"
  puts(_("%s%ld exactly"));
#line 68 "../numeric.c"
  puts(_("%sit must lie in one of the ranges:\n"));
#line 68 "../numeric.c"
  puts(_("%sit must be in the range:\n"));
#line 88 "../numeric.c"
  puts(_(", or\n"));
#line 66 "../numeric.c"
  puts(_("%sis scalable with a suffix: k/K/m/M/g/G/t/T\n"));
#line 77 "../numeric.c"
  puts(_("%sless than or equal to %ld"));
#line 339 "../usage.c"
  puts(_("Operands and options may be intermixed.  They will be reordered.\n"));
#line 601 "../usage.c"
  puts(_("requires the option '%s'\n"));
#line 604 "../usage.c"
  puts(_("requires these options:\n"));
#line 1270 "../usage.c"
  puts(_("   Arg Option-Name   Req?  Description\n"));
#line 1264 "../usage.c"
  puts(_("  Flg Arg Option-Name   Req?  Description\n"));
#line 143 "../enum.c"
  puts(_("or you may use a numeric representation.  Preceding these with a '!'\n"
       "will clear the bits, specifying 'none' will clear all bits, and 'all'\n"
       "will set them all.  Multiple entries may be passed as an option\n"
       "argument list.\n"));
#line 859 "../usage.c"
  puts(_("\t\t\t\t- may appear up to %d times\n"));
#line 52 "../enum.c"
  puts(_("The valid \"%s\" option keywords are:\n"));
#line 1101 "../usage.c"
  puts(_("The next option supports vendor supported extra options:"));
#line 722 "../usage.c"
  puts(_("These additional options are:"));
  /* END-USAGE-TEXT */
}
#endif /* uncompilable code */
#ifdef  __cplusplus
}
#endif
/* ntpq-opts.c ends here */
