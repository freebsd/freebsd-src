/*
 *  EDIT THIS FILE WITH CAUTION  (ntpq-opts.h)
 *
 *  It has been AutoGen-ed  May 25, 2024 at 12:04:21 AM by AutoGen 5.18.16
 *  From the definitions    ntpq-opts.def
 *  and the template file   options
 *
 * Generated from AutoOpts 42:1:17 templates.
 *
 *  AutoOpts is a copyrighted work.  This header file is not encumbered
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
/**
 *  This file contains the programmatic interface to the Automated
 *  Options generated for the ntpq program.
 *  These macros are documented in the AutoGen info file in the
 *  "AutoOpts" chapter.  Please refer to that doc for usage help.
 */
#ifndef AUTOOPTS_NTPQ_OPTS_H_GUARD
#define AUTOOPTS_NTPQ_OPTS_H_GUARD 1
#include "config.h"
#include <autoopts/options.h>
#include <stdarg.h>
#include <stdnoreturn.h>

/**
 *  Ensure that the library used for compiling this generated header is at
 *  least as new as the version current when the header template was released
 *  (not counting patch version increments).  Also ensure that the oldest
 *  tolerable version is at least as old as what was current when the header
 *  template was released.
 */
#define AO_TEMPLATE_VERSION 172033
#if (AO_TEMPLATE_VERSION < OPTIONS_MINIMUM_VERSION) \
 || (AO_TEMPLATE_VERSION > OPTIONS_STRUCT_VERSION)
# error option template version mismatches autoopts/options.h header
  Choke Me.
#endif

#if GCC_VERSION > 40400
#define NOT_REACHED __builtin_unreachable();
#else
#define NOT_REACHED
#endif

/**
 *  Enumeration of each option type for ntpq
 */
typedef enum {
    INDEX_OPT_IPV4             =  0,
    INDEX_OPT_IPV6             =  1,
    INDEX_OPT_COMMAND          =  2,
    INDEX_OPT_DEBUG_LEVEL      =  3,
    INDEX_OPT_SET_DEBUG_LEVEL  =  4,
    INDEX_OPT_INTERACTIVE      =  5,
    INDEX_OPT_NUMERIC          =  6,
    INDEX_OPT_OLD_RV           =  7,
    INDEX_OPT_PEERS            =  8,
    INDEX_OPT_REFID            =  9,
    INDEX_OPT_UNCONNECTED      = 10,
    INDEX_OPT_WIDE             = 11,
    INDEX_OPT_VERSION          = 12,
    INDEX_OPT_HELP             = 13,
    INDEX_OPT_MORE_HELP        = 14,
    INDEX_OPT_SAVE_OPTS        = 15,
    INDEX_OPT_LOAD_OPTS        = 16
} teOptIndex;
/** count of all options for ntpq */
#define OPTION_CT    17
/** ntpq version */
#define NTPQ_VERSION       "4.2.8p18"
/** Full ntpq version text */
#define NTPQ_FULL_VERSION  "ntpq 4.2.8p18"

/**
 *  Interface defines for all options.  Replace "n" with the UPPER_CASED
 *  option name (as in the teOptIndex enumeration above).
 *  e.g. HAVE_OPT(IPV4)
 */
#define         DESC(n) (ntpqOptions.pOptDesc[INDEX_OPT_## n])
/** 'true' if an option has been specified in any way */
#define     HAVE_OPT(n) (! UNUSED_OPT(& DESC(n)))
/** The string argument to an option. The argument type must be \"string\". */
#define      OPT_ARG(n) (DESC(n).optArg.argString)
/** Mask the option state revealing how an option was specified.
 *  It will be one and only one of \a OPTST_SET, \a OPTST_PRESET,
 * \a OPTST_DEFINED, \a OPTST_RESET or zero.
 */
#define    STATE_OPT(n) (DESC(n).fOptState & OPTST_SET_MASK)
/** Count of option's occurrances *on the command line*. */
#define    COUNT_OPT(n) (DESC(n).optOccCt)
/** mask of \a OPTST_SET and \a OPTST_DEFINED. */
#define    ISSEL_OPT(n) (SELECTED_OPT(&DESC(n)))
/** 'true' if \a HAVE_OPT would yield 'false'. */
#define ISUNUSED_OPT(n) (UNUSED_OPT(& DESC(n)))
/** 'true' if OPTST_DISABLED bit not set. */
#define  ENABLED_OPT(n) (! DISABLED_OPT(& DESC(n)))
/** number of stacked option arguments.
 *  Valid only for stacked option arguments. */
#define  STACKCT_OPT(n) (((tArgList*)(DESC(n).optCookie))->useCt)
/** stacked argument vector.
 *  Valid only for stacked option arguments. */
#define STACKLST_OPT(n) (((tArgList*)(DESC(n).optCookie))->apzArgs)
/** Reset an option. */
#define    CLEAR_OPT(n) STMTS( \
                DESC(n).fOptState &= OPTST_PERSISTENT_MASK;   \
                if ((DESC(n).fOptState & OPTST_INITENABLED) == 0) \
                    DESC(n).fOptState |= OPTST_DISABLED; \
                DESC(n).optCookie = NULL )
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**
 *  Enumeration of ntpq exit codes
 */
typedef enum {
    NTPQ_EXIT_SUCCESS         = 0,
    NTPQ_EXIT_FAILURE         = 1,
    NTPQ_EXIT_USAGE_ERROR     = 64,
    NTPQ_EXIT_NO_CONFIG_INPUT = 66,
    NTPQ_EXIT_LIBOPTS_FAILURE = 70
}   ntpq_exit_code_t;
/** @} */
/**
 *  Make sure there are no #define name conflicts with the option names
 */
#ifndef     NO_OPTION_NAME_WARNINGS
# ifdef    IPV4
#  warning undefining IPV4 due to option name conflict
#  undef   IPV4
# endif
# ifdef    IPV6
#  warning undefining IPV6 due to option name conflict
#  undef   IPV6
# endif
# ifdef    COMMAND
#  warning undefining COMMAND due to option name conflict
#  undef   COMMAND
# endif
# ifdef    DEBUG_LEVEL
#  warning undefining DEBUG_LEVEL due to option name conflict
#  undef   DEBUG_LEVEL
# endif
# ifdef    SET_DEBUG_LEVEL
#  warning undefining SET_DEBUG_LEVEL due to option name conflict
#  undef   SET_DEBUG_LEVEL
# endif
# ifdef    INTERACTIVE
#  warning undefining INTERACTIVE due to option name conflict
#  undef   INTERACTIVE
# endif
# ifdef    NUMERIC
#  warning undefining NUMERIC due to option name conflict
#  undef   NUMERIC
# endif
# ifdef    OLD_RV
#  warning undefining OLD_RV due to option name conflict
#  undef   OLD_RV
# endif
# ifdef    PEERS
#  warning undefining PEERS due to option name conflict
#  undef   PEERS
# endif
# ifdef    REFID
#  warning undefining REFID due to option name conflict
#  undef   REFID
# endif
# ifdef    UNCONNECTED
#  warning undefining UNCONNECTED due to option name conflict
#  undef   UNCONNECTED
# endif
# ifdef    WIDE
#  warning undefining WIDE due to option name conflict
#  undef   WIDE
# endif
#else  /* NO_OPTION_NAME_WARNINGS */
# undef IPV4
# undef IPV6
# undef COMMAND
# undef DEBUG_LEVEL
# undef SET_DEBUG_LEVEL
# undef INTERACTIVE
# undef NUMERIC
# undef OLD_RV
# undef PEERS
# undef REFID
# undef UNCONNECTED
# undef WIDE
#endif  /*  NO_OPTION_NAME_WARNINGS */

/**
 *  Interface defines for specific options.
 * @{
 */
#define VALUE_OPT_IPV4           '4'
#define VALUE_OPT_IPV6           '6'
#define VALUE_OPT_COMMAND        'c'
#define VALUE_OPT_DEBUG_LEVEL    'd'
#define VALUE_OPT_SET_DEBUG_LEVEL 'D'

#define OPT_VALUE_SET_DEBUG_LEVEL (DESC(SET_DEBUG_LEVEL).optArg.argInt)
#define VALUE_OPT_INTERACTIVE    'i'
#define VALUE_OPT_NUMERIC        'n'
#define VALUE_OPT_OLD_RV         0x1001
#define VALUE_OPT_PEERS          'p'
#define VALUE_OPT_REFID          'r'

typedef enum {
    REFID_HASH, REFID_IPV4
} te_Refid;
#define OPT_REFID_VAL2STR(_v)    optionKeywordName(&DESC(REFID), (_v))
#define OPT_VALUE_REFID          (DESC(REFID).optArg.argEnum)
#define VALUE_OPT_UNCONNECTED    'u'
#define VALUE_OPT_WIDE           'w'
/** option flag (value) for help-value option */
#define VALUE_OPT_HELP          '?'
/** option flag (value) for more-help-value option */
#define VALUE_OPT_MORE_HELP     '!'
/** option flag (value) for version-value option */
#define VALUE_OPT_VERSION       0x1002
/** option flag (value) for save-opts-value option */
#define VALUE_OPT_SAVE_OPTS     '>'
/** option flag (value) for load-opts-value option */
#define VALUE_OPT_LOAD_OPTS     '<'
#define SET_OPT_SAVE_OPTS(a)   STMTS( \
        DESC(SAVE_OPTS).fOptState &= OPTST_PERSISTENT_MASK; \
        DESC(SAVE_OPTS).fOptState |= OPTST_SET; \
        DESC(SAVE_OPTS).optArg.argString = (char const*)(a))
/*
 *  Interface defines not associated with particular options
 */
#define ERRSKIP_OPTERR  STMTS(ntpqOptions.fOptSet &= ~OPTPROC_ERRSTOP)
#define ERRSTOP_OPTERR  STMTS(ntpqOptions.fOptSet |= OPTPROC_ERRSTOP)
#define RESTART_OPT(n)  STMTS( \
                ntpqOptions.curOptIdx = (n); \
                ntpqOptions.pzCurOpt  = NULL )
#define START_OPT       RESTART_OPT(1)
#define USAGE(c)        (*ntpqOptions.pUsageProc)(&ntpqOptions, c)

#ifdef  __cplusplus
extern "C" {
#endif


/* * * * * *
 *
 *  Declare the ntpq option descriptor.
 */
extern tOptions ntpqOptions;

#if defined(ENABLE_NLS)
# ifndef _
#   include <stdio.h>
#   ifndef HAVE_GETTEXT
      extern char * gettext(char const *);
#   else
#     include <libintl.h>
#   endif

# ifndef ATTRIBUTE_FORMAT_ARG
#   define ATTRIBUTE_FORMAT_ARG(_a)
# endif

static inline char* aoGetsText(char const* pz) ATTRIBUTE_FORMAT_ARG(1);
static inline char* aoGetsText(char const* pz) {
    if (pz == NULL) return NULL;
    return (char*)gettext(pz);
}
#   define _(s)  aoGetsText(s)
# endif /* _() */

# define OPT_NO_XLAT_CFG_NAMES  STMTS(ntpqOptions.fOptSet |= \
                                    OPTPROC_NXLAT_OPT_CFG;)
# define OPT_NO_XLAT_OPT_NAMES  STMTS(ntpqOptions.fOptSet |= \
                                    OPTPROC_NXLAT_OPT|OPTPROC_NXLAT_OPT_CFG;)

# define OPT_XLAT_CFG_NAMES     STMTS(ntpqOptions.fOptSet &= \
                                  ~(OPTPROC_NXLAT_OPT|OPTPROC_NXLAT_OPT_CFG);)
# define OPT_XLAT_OPT_NAMES     STMTS(ntpqOptions.fOptSet &= \
                                  ~OPTPROC_NXLAT_OPT;)

#else   /* ENABLE_NLS */
# define OPT_NO_XLAT_CFG_NAMES
# define OPT_NO_XLAT_OPT_NAMES

# define OPT_XLAT_CFG_NAMES
# define OPT_XLAT_OPT_NAMES

# ifndef _
#   define _(_s)  _s
# endif
#endif  /* ENABLE_NLS */


#ifdef  __cplusplus
}
#endif
#endif /* AUTOOPTS_NTPQ_OPTS_H_GUARD */

/* ntpq-opts.h ends here */
