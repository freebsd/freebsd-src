/*  
 *  EDIT THIS FILE WITH CAUTION  (ntpdsim-opts.h)
 *  
 *  It has been AutoGen-ed  Sunday August 17, 2008 at 05:20:13 AM EDT
 *  From the definitions    ntpdsim-opts.def
 *  and the template file   options
 *
 * Generated from AutoOpts 29:0:4 templates.
 */

/*
 *  This file was produced by an AutoOpts template.  AutoOpts is a
 *  copyrighted work.  This header file is not encumbered by AutoOpts
 *  licensing, but is provided under the licensing terms chosen by the
 *  ntpdsim author or copyright holder.  AutoOpts is licensed under
 *  the terms of the LGPL.  The redistributable library (``libopts'') is
 *  licensed under the terms of either the LGPL or, at the users discretion,
 *  the BSD license.  See the AutoOpts and/or libopts sources for details.
 *
 * This source file is copyrighted and licensed under the following terms:
 *
 * ntpdsim copyright 1970-2008 David L. Mills and/or others - all rights reserved
 *
 * see html/copyright.html
 */
/*
 *  This file contains the programmatic interface to the Automated
 *  Options generated for the ntpdsim program.
 *  These macros are documented in the AutoGen info file in the
 *  "AutoOpts" chapter.  Please refer to that doc for usage help.
 */
#ifndef AUTOOPTS_NTPDSIM_OPTS_H_GUARD
#define AUTOOPTS_NTPDSIM_OPTS_H_GUARD
#include "config.h"
#include <autoopts/options.h>

/*
 *  Ensure that the library used for compiling this generated header is at
 *  least as new as the version current when the header template was released
 *  (not counting patch version increments).  Also ensure that the oldest
 *  tolerable version is at least as old as what was current when the header
 *  template was released.
 */
#define AO_TEMPLATE_VERSION 118784
#if (AO_TEMPLATE_VERSION < OPTIONS_MINIMUM_VERSION) \
 || (AO_TEMPLATE_VERSION > OPTIONS_STRUCT_VERSION)
# error option template version mismatches autoopts/options.h header
  Choke Me.
#endif

/*
 *  Enumeration of each option:
 */
typedef enum {
        INDEX_OPT_IPV4             =  0,
        INDEX_OPT_IPV6             =  1,
        INDEX_OPT_AUTHREQ          =  2,
        INDEX_OPT_AUTHNOREQ        =  3,
        INDEX_OPT_BCASTSYNC        =  4,
        INDEX_OPT_SIMBROADCASTDELAY =  5,
        INDEX_OPT_CONFIGFILE       =  6,
        INDEX_OPT_PHASENOISE       =  7,
        INDEX_OPT_DEBUG_LEVEL      =  8,
        INDEX_OPT_SET_DEBUG_LEVEL  =  9,
        INDEX_OPT_DRIFTFILE        = 10,
        INDEX_OPT_PANICGATE        = 11,
        INDEX_OPT_SIMSLEW          = 12,
        INDEX_OPT_JAILDIR          = 13,
        INDEX_OPT_INTERFACE        = 14,
        INDEX_OPT_KEYFILE          = 15,
        INDEX_OPT_LOGFILE          = 16,
        INDEX_OPT_NOVIRTUALIPS     = 17,
        INDEX_OPT_MODIFYMMTIMER    = 18,
        INDEX_OPT_NOFORK           = 19,
        INDEX_OPT_NICE             = 20,
        INDEX_OPT_SERVERTIME       = 21,
        INDEX_OPT_PIDFILE          = 22,
        INDEX_OPT_PRIORITY         = 23,
        INDEX_OPT_QUIT             = 24,
        INDEX_OPT_PROPAGATIONDELAY = 25,
        INDEX_OPT_UPDATEINTERVAL   = 26,
        INDEX_OPT_STATSDIR         = 27,
        INDEX_OPT_ENDSIMTIME       = 28,
        INDEX_OPT_TRUSTEDKEY       = 29,
        INDEX_OPT_FREQERR          = 30,
        INDEX_OPT_WALKNOISE        = 31,
        INDEX_OPT_USER             = 32,
        INDEX_OPT_VAR              = 33,
        INDEX_OPT_DVAR             = 34,
        INDEX_OPT_SLEW             = 35,
        INDEX_OPT_NDELAY           = 36,
        INDEX_OPT_PDELAY           = 37,
        INDEX_OPT_VERSION          = 38,
        INDEX_OPT_HELP             = 39,
        INDEX_OPT_MORE_HELP        = 40,
        INDEX_OPT_SAVE_OPTS        = 41,
        INDEX_OPT_LOAD_OPTS        = 42
} teOptIndex;

#define OPTION_CT    43
#define NTPDSIM_VERSION       "4.2.4p5"
#define NTPDSIM_FULL_VERSION  "ntpdsim - NTP daemon simulation program - Ver. 4.2.4p5"

/*
 *  Interface defines for all options.  Replace "n" with the UPPER_CASED
 *  option name (as in the teOptIndex enumeration above).
 *  e.g. HAVE_OPT( IPV4 )
 */
#define         DESC(n) (ntpdsimOptions.pOptDesc[INDEX_OPT_## n])
#define     HAVE_OPT(n) (! UNUSED_OPT(& DESC(n)))
#define      OPT_ARG(n) (DESC(n).optArg.argString)
#define    STATE_OPT(n) (DESC(n).fOptState & OPTST_SET_MASK)
#define    COUNT_OPT(n) (DESC(n).optOccCt)
#define    ISSEL_OPT(n) (SELECTED_OPT(&DESC(n)))
#define ISUNUSED_OPT(n) (UNUSED_OPT(& DESC(n)))
#define  ENABLED_OPT(n) (! DISABLED_OPT(& DESC(n)))
#define  STACKCT_OPT(n) (((tArgList*)(DESC(n).optCookie))->useCt)
#define STACKLST_OPT(n) (((tArgList*)(DESC(n).optCookie))->apzArgs)
#define    CLEAR_OPT(n) STMTS( \
                DESC(n).fOptState &= OPTST_PERSISTENT_MASK;   \
                if ( (DESC(n).fOptState & OPTST_INITENABLED) == 0) \
                    DESC(n).fOptState |= OPTST_DISABLED; \
                DESC(n).optCookie = NULL )

/*
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
# ifdef    AUTHREQ
#  warning undefining AUTHREQ due to option name conflict
#  undef   AUTHREQ
# endif
# ifdef    AUTHNOREQ
#  warning undefining AUTHNOREQ due to option name conflict
#  undef   AUTHNOREQ
# endif
# ifdef    BCASTSYNC
#  warning undefining BCASTSYNC due to option name conflict
#  undef   BCASTSYNC
# endif
# ifdef    SIMBROADCASTDELAY
#  warning undefining SIMBROADCASTDELAY due to option name conflict
#  undef   SIMBROADCASTDELAY
# endif
# ifdef    CONFIGFILE
#  warning undefining CONFIGFILE due to option name conflict
#  undef   CONFIGFILE
# endif
# ifdef    PHASENOISE
#  warning undefining PHASENOISE due to option name conflict
#  undef   PHASENOISE
# endif
# ifdef    DEBUG_LEVEL
#  warning undefining DEBUG_LEVEL due to option name conflict
#  undef   DEBUG_LEVEL
# endif
# ifdef    SET_DEBUG_LEVEL
#  warning undefining SET_DEBUG_LEVEL due to option name conflict
#  undef   SET_DEBUG_LEVEL
# endif
# ifdef    DRIFTFILE
#  warning undefining DRIFTFILE due to option name conflict
#  undef   DRIFTFILE
# endif
# ifdef    PANICGATE
#  warning undefining PANICGATE due to option name conflict
#  undef   PANICGATE
# endif
# ifdef    SIMSLEW
#  warning undefining SIMSLEW due to option name conflict
#  undef   SIMSLEW
# endif
# ifdef    JAILDIR
#  warning undefining JAILDIR due to option name conflict
#  undef   JAILDIR
# endif
# ifdef    INTERFACE
#  warning undefining INTERFACE due to option name conflict
#  undef   INTERFACE
# endif
# ifdef    KEYFILE
#  warning undefining KEYFILE due to option name conflict
#  undef   KEYFILE
# endif
# ifdef    LOGFILE
#  warning undefining LOGFILE due to option name conflict
#  undef   LOGFILE
# endif
# ifdef    NOVIRTUALIPS
#  warning undefining NOVIRTUALIPS due to option name conflict
#  undef   NOVIRTUALIPS
# endif
# ifdef    MODIFYMMTIMER
#  warning undefining MODIFYMMTIMER due to option name conflict
#  undef   MODIFYMMTIMER
# endif
# ifdef    NOFORK
#  warning undefining NOFORK due to option name conflict
#  undef   NOFORK
# endif
# ifdef    NICE
#  warning undefining NICE due to option name conflict
#  undef   NICE
# endif
# ifdef    SERVERTIME
#  warning undefining SERVERTIME due to option name conflict
#  undef   SERVERTIME
# endif
# ifdef    PIDFILE
#  warning undefining PIDFILE due to option name conflict
#  undef   PIDFILE
# endif
# ifdef    PRIORITY
#  warning undefining PRIORITY due to option name conflict
#  undef   PRIORITY
# endif
# ifdef    QUIT
#  warning undefining QUIT due to option name conflict
#  undef   QUIT
# endif
# ifdef    PROPAGATIONDELAY
#  warning undefining PROPAGATIONDELAY due to option name conflict
#  undef   PROPAGATIONDELAY
# endif
# ifdef    UPDATEINTERVAL
#  warning undefining UPDATEINTERVAL due to option name conflict
#  undef   UPDATEINTERVAL
# endif
# ifdef    STATSDIR
#  warning undefining STATSDIR due to option name conflict
#  undef   STATSDIR
# endif
# ifdef    ENDSIMTIME
#  warning undefining ENDSIMTIME due to option name conflict
#  undef   ENDSIMTIME
# endif
# ifdef    TRUSTEDKEY
#  warning undefining TRUSTEDKEY due to option name conflict
#  undef   TRUSTEDKEY
# endif
# ifdef    FREQERR
#  warning undefining FREQERR due to option name conflict
#  undef   FREQERR
# endif
# ifdef    WALKNOISE
#  warning undefining WALKNOISE due to option name conflict
#  undef   WALKNOISE
# endif
# ifdef    USER
#  warning undefining USER due to option name conflict
#  undef   USER
# endif
# ifdef    VAR
#  warning undefining VAR due to option name conflict
#  undef   VAR
# endif
# ifdef    DVAR
#  warning undefining DVAR due to option name conflict
#  undef   DVAR
# endif
# ifdef    SLEW
#  warning undefining SLEW due to option name conflict
#  undef   SLEW
# endif
# ifdef    NDELAY
#  warning undefining NDELAY due to option name conflict
#  undef   NDELAY
# endif
# ifdef    PDELAY
#  warning undefining PDELAY due to option name conflict
#  undef   PDELAY
# endif
#else  /* NO_OPTION_NAME_WARNINGS */
# undef IPV4
# undef IPV6
# undef AUTHREQ
# undef AUTHNOREQ
# undef BCASTSYNC
# undef SIMBROADCASTDELAY
# undef CONFIGFILE
# undef PHASENOISE
# undef DEBUG_LEVEL
# undef SET_DEBUG_LEVEL
# undef DRIFTFILE
# undef PANICGATE
# undef SIMSLEW
# undef JAILDIR
# undef INTERFACE
# undef KEYFILE
# undef LOGFILE
# undef NOVIRTUALIPS
# undef MODIFYMMTIMER
# undef NOFORK
# undef NICE
# undef SERVERTIME
# undef PIDFILE
# undef PRIORITY
# undef QUIT
# undef PROPAGATIONDELAY
# undef UPDATEINTERVAL
# undef STATSDIR
# undef ENDSIMTIME
# undef TRUSTEDKEY
# undef FREQERR
# undef WALKNOISE
# undef USER
# undef VAR
# undef DVAR
# undef SLEW
# undef NDELAY
# undef PDELAY
#endif  /*  NO_OPTION_NAME_WARNINGS */

/*
 *  Interface defines for specific options.
 */
#define VALUE_OPT_IPV4           '4'
#define WHICH_OPT_IPV4           (DESC(IPV4).optActualValue)
#define WHICH_IDX_IPV4           (DESC(IPV4).optActualIndex)
#define VALUE_OPT_IPV6           '6'
#define VALUE_OPT_AUTHREQ        'a'
#define VALUE_OPT_AUTHNOREQ      'A'
#define VALUE_OPT_BCASTSYNC      'b'
#define VALUE_OPT_SIMBROADCASTDELAY 'B'
#define VALUE_OPT_CONFIGFILE     'c'
#define VALUE_OPT_PHASENOISE     'C'
#ifdef DEBUG
#define VALUE_OPT_DEBUG_LEVEL    'd'
#endif /* DEBUG */
#ifdef DEBUG
#define VALUE_OPT_SET_DEBUG_LEVEL 'D'
#endif /* DEBUG */
#define VALUE_OPT_DRIFTFILE      'f'
#define VALUE_OPT_PANICGATE      'g'
#define VALUE_OPT_SIMSLEW        'H'
#define VALUE_OPT_JAILDIR        'i'
#define VALUE_OPT_INTERFACE      'I'
#define VALUE_OPT_KEYFILE        'k'
#define VALUE_OPT_LOGFILE        'l'
#define VALUE_OPT_NOVIRTUALIPS   'L'
#ifdef SYS_WINNT
#define VALUE_OPT_MODIFYMMTIMER  'M'
#endif /* SYS_WINNT */
#define VALUE_OPT_NOFORK         'n'
#define VALUE_OPT_NICE           'N'
#define VALUE_OPT_SERVERTIME     'O'
#define VALUE_OPT_PIDFILE        'p'
#define VALUE_OPT_PRIORITY       'P'
#define OPT_VALUE_PRIORITY       (DESC(PRIORITY).optArg.argInt)
#define VALUE_OPT_QUIT           'q'
#define VALUE_OPT_PROPAGATIONDELAY 'r'
#define VALUE_OPT_UPDATEINTERVAL 'U'
#define OPT_VALUE_UPDATEINTERVAL (DESC(UPDATEINTERVAL).optArg.argInt)
#define VALUE_OPT_STATSDIR       's'
#define VALUE_OPT_ENDSIMTIME     'S'
#define VALUE_OPT_TRUSTEDKEY     't'
#define VALUE_OPT_FREQERR        'T'
#define VALUE_OPT_WALKNOISE      'W'
#define VALUE_OPT_USER           'u'
#define VALUE_OPT_VAR            'v'
#define VALUE_OPT_DVAR           'V'
#define VALUE_OPT_SLEW           'x'
#define VALUE_OPT_NDELAY         'Y'
#define VALUE_OPT_PDELAY         'Z'

#define VALUE_OPT_VERSION       'v'
#define VALUE_OPT_HELP          '?'
#define VALUE_OPT_MORE_HELP     '!'
#define VALUE_OPT_SAVE_OPTS     '>'
#define VALUE_OPT_LOAD_OPTS     '<'
#define SET_OPT_SAVE_OPTS(a)   STMTS( \
        DESC(SAVE_OPTS).fOptState &= OPTST_PERSISTENT_MASK; \
        DESC(SAVE_OPTS).fOptState |= OPTST_SET; \
        DESC(SAVE_OPTS).optArg.argString = (char const*)(a) )
/*
 *  Interface defines not associated with particular options
 */
#define ERRSKIP_OPTERR  STMTS( ntpdsimOptions.fOptSet &= ~OPTPROC_ERRSTOP )
#define ERRSTOP_OPTERR  STMTS( ntpdsimOptions.fOptSet |= OPTPROC_ERRSTOP )
#define RESTART_OPT(n)  STMTS( \
                ntpdsimOptions.curOptIdx = (n); \
                ntpdsimOptions.pzCurOpt  = NULL )
#define START_OPT       RESTART_OPT(1)
#define USAGE(c)        (*ntpdsimOptions.pUsageProc)( &ntpdsimOptions, c )
/* extracted from /usr/local/gnu/share/autogen/opthead.tpl near line 360 */

/* * * * * *
 *
 *  Declare the ntpdsim option descriptor.
 */
#ifdef  __cplusplus
extern "C" {
#endif

extern tOptions   ntpdsimOptions;

#ifndef _
#  if ENABLE_NLS
#    include <stdio.h>
     static inline char* aoGetsText( char const* pz ) {
         if (pz == NULL) return NULL;
         return (char*)gettext( pz );
     }
#    define _(s)  aoGetsText(s)
#  else  /* ENABLE_NLS */
#    define _(s)  s
#  endif /* ENABLE_NLS */
#endif

#ifdef  __cplusplus
}
#endif
#endif /* AUTOOPTS_NTPDSIM_OPTS_H_GUARD */
/* ntpdsim-opts.h ends here */
