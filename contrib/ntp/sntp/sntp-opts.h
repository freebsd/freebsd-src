/*  
 *  EDIT THIS FILE WITH CAUTION  (sntp-opts.h)
 *  
 *  It has been AutoGen-ed  Tuesday December  8, 2009 at 08:14:49 AM EST
 *  From the definitions    sntp-opts.def
 *  and the template file   options
 *
 * Generated from AutoOpts 29:0:4 templates.
 */

/*
 *  This file was produced by an AutoOpts template.  AutoOpts is a
 *  copyrighted work.  This header file is not encumbered by AutoOpts
 *  licensing, but is provided under the licensing terms chosen by the
 *  sntp author or copyright holder.  AutoOpts is licensed under
 *  the terms of the LGPL.  The redistributable library (``libopts'') is
 *  licensed under the terms of either the LGPL or, at the users discretion,
 *  the BSD license.  See the AutoOpts and/or libopts sources for details.
 *
 * This source file is copyrighted and licensed under the following terms:
 *
 * sntp copyright 1970-2006 ntp.org - all rights reserved
 *
 *         General Public Licence for the software known as MSNTP
 *         ------------------------------------------------------
 * 
 * 	  (c) Copyright, N.M. Maclaren, 1996, 1997, 2000
 * 	  (c) Copyright, University of Cambridge, 1996, 1997, 2000
 * 
 * 
 * 
 * Free use of MSNTP in source and binary forms is permitted, provided that this
 * entire licence is duplicated in all copies, and that any documentation,
 * announcements, and other materials related to use acknowledge that the software
 * was developed by N.M. Maclaren (hereafter refered to as the Author) at the
 * University of Cambridge.  Neither the name of the Author nor the University of
 * Cambridge may be used to endorse or promote products derived from this material
 * without specific prior written permission.
 * 
 * The Author and the University of Cambridge retain the copyright and all other
 * legal rights to the software and make it available non-exclusively.  All users
 * must ensure that the software in all its derivations carries a copyright notice
 * in the form:
 * 	  (c) Copyright N.M. Maclaren,
 * 	  (c) Copyright University of Cambridge.
 * 
 * 
 * 
 *                            NO WARRANTY
 * 
 * Because the MSNTP software is licensed free of charge, the Author and the
 * University of Cambridge provide absolutely no warranty, either expressed or
 * implied, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The entire risk as to
 * the quality and performance of the MSNTP software is with you.  Should MSNTP
 * prove defective, you assume the cost of all necessary servicing or repair.
 * 
 * In no event, unless required by law, will the Author or the University of
 * Cambridge, or any other party who may modify and redistribute this software as
 * permitted in accordance with the provisions below, be liable for damages for
 * any losses whatsoever, including but not limited to lost profits, lost monies,
 * lost or corrupted data, or other special, incidental or consequential losses
 * that may arise out of the use or inability to use the MSNTP software.
 * 
 * 
 * 
 *                          COPYING POLICY
 * 
 * Permission is hereby granted for copying and distribution of copies of the
 * MSNTP source and binary files, and of any part thereof, subject to the
 * following licence conditions:
 * 
 * 1. You may distribute MSNTP or components of MSNTP, with or without additions
 * developed by you or by others.  No charge, other than an "at-cost" distribution
 * fee, may be charged for copies, derivations, or distributions of this material
 * without the express written consent of the copyright holders.
 * 
 * 2. You may also distribute MSNTP along with any other product for sale,
 * provided that the cost of the bundled package is the same regardless of whether
 * MSNTP is included or not, and provided that those interested only in MSNTP must
 * be notified that it is a product freely available from the University of
 * Cambridge.
 * 
 * 3. If you distribute MSNTP software or parts of MSNTP, with or without
 * additions developed by you or others, then you must either make available the
 * source to all portions of the MSNTP system (exclusive of any additions made by
 * you or by others) upon request, or instead you may notify anyone requesting
 * source that it is freely available from the University of Cambridge.
 * 
 * 4. You may not omit any of the copyright notices on either the source files,
 * the executable files, or the documentation.
 * 
 * 5. You may not omit transmission of this License agreement with whatever
 * portions of MSNTP that are distributed.
 * 
 * 6. Any users of this software must be notified that it is without warranty or
 * guarantee of any nature, express or implied, nor is there any fitness for use
 * represented.
 * 
 * 
 * October 1996
 * April 1997
 * October 2000
 */
/*
 *  This file contains the programmatic interface to the Automated
 *  Options generated for the sntp program.
 *  These macros are documented in the AutoGen info file in the
 *  "AutoOpts" chapter.  Please refer to that doc for usage help.
 */
#ifndef AUTOOPTS_SNTP_OPTS_H_GUARD
#define AUTOOPTS_SNTP_OPTS_H_GUARD
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
        INDEX_OPT_UNPRIVPORT       =  2,
        INDEX_OPT_NORMALVERBOSE    =  3,
        INDEX_OPT_EXTRAVERBOSE     =  4,
        INDEX_OPT_MEGAVERBOSE      =  5,
        INDEX_OPT_SETTIMEOFDAY     =  6,
        INDEX_OPT_ADJTIME          =  7,
        INDEX_OPT_VERSION          = 8,
        INDEX_OPT_HELP             = 9,
        INDEX_OPT_MORE_HELP        = 10,
        INDEX_OPT_SAVE_OPTS        = 11,
        INDEX_OPT_LOAD_OPTS        = 12
} teOptIndex;

#define OPTION_CT    13
#define SNTP_VERSION       "4.2.4p8"
#define SNTP_FULL_VERSION  "sntp - standard SNTP program - Ver. 4.2.4p8"

/*
 *  Interface defines for all options.  Replace "n" with the UPPER_CASED
 *  option name (as in the teOptIndex enumeration above).
 *  e.g. HAVE_OPT( IPV4 )
 */
#define         DESC(n) (sntpOptions.pOptDesc[INDEX_OPT_## n])
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
# ifdef    UNPRIVPORT
#  warning undefining UNPRIVPORT due to option name conflict
#  undef   UNPRIVPORT
# endif
# ifdef    NORMALVERBOSE
#  warning undefining NORMALVERBOSE due to option name conflict
#  undef   NORMALVERBOSE
# endif
# ifdef    EXTRAVERBOSE
#  warning undefining EXTRAVERBOSE due to option name conflict
#  undef   EXTRAVERBOSE
# endif
# ifdef    MEGAVERBOSE
#  warning undefining MEGAVERBOSE due to option name conflict
#  undef   MEGAVERBOSE
# endif
# ifdef    SETTIMEOFDAY
#  warning undefining SETTIMEOFDAY due to option name conflict
#  undef   SETTIMEOFDAY
# endif
# ifdef    ADJTIME
#  warning undefining ADJTIME due to option name conflict
#  undef   ADJTIME
# endif
#else  /* NO_OPTION_NAME_WARNINGS */
# undef IPV4
# undef IPV6
# undef UNPRIVPORT
# undef NORMALVERBOSE
# undef EXTRAVERBOSE
# undef MEGAVERBOSE
# undef SETTIMEOFDAY
# undef ADJTIME
#endif  /*  NO_OPTION_NAME_WARNINGS */

/*
 *  Interface defines for specific options.
 */
#define VALUE_OPT_IPV4           '4'
#define WHICH_OPT_IPV4           (DESC(IPV4).optActualValue)
#define WHICH_IDX_IPV4           (DESC(IPV4).optActualIndex)
#define VALUE_OPT_IPV6           '6'
#define VALUE_OPT_UNPRIVPORT     'u'
#define VALUE_OPT_NORMALVERBOSE  'v'
#define VALUE_OPT_EXTRAVERBOSE   'V'
#define VALUE_OPT_MEGAVERBOSE    'W'
#define VALUE_OPT_SETTIMEOFDAY   'r'
#define VALUE_OPT_ADJTIME        'a'

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
#define ERRSKIP_OPTERR  STMTS( sntpOptions.fOptSet &= ~OPTPROC_ERRSTOP )
#define ERRSTOP_OPTERR  STMTS( sntpOptions.fOptSet |= OPTPROC_ERRSTOP )
#define RESTART_OPT(n)  STMTS( \
                sntpOptions.curOptIdx = (n); \
                sntpOptions.pzCurOpt  = NULL )
#define START_OPT       RESTART_OPT(1)
#define USAGE(c)        (*sntpOptions.pUsageProc)( &sntpOptions, c )
/* extracted from /usr/local/gnu/autogen-5.9.1/share/autogen/opthead.tpl near line 360 */

/* * * * * *
 *
 *  Declare the sntp option descriptor.
 */
#ifdef  __cplusplus
extern "C" {
#endif

extern tOptions   sntpOptions;

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
#endif /* AUTOOPTS_SNTP_OPTS_H_GUARD */
/* sntp-opts.h ends here */
