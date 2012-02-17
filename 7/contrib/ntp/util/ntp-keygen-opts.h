/*  
 *  EDIT THIS FILE WITH CAUTION  (ntp-keygen-opts.h)
 *  
 *  It has been AutoGen-ed  Sunday August 17, 2008 at 05:27:32 AM EDT
 *  From the definitions    ntp-keygen-opts.def
 *  and the template file   options
 *
 * Generated from AutoOpts 29:0:4 templates.
 */

/*
 *  This file was produced by an AutoOpts template.  AutoOpts is a
 *  copyrighted work.  This header file is not encumbered by AutoOpts
 *  licensing, but is provided under the licensing terms chosen by the
 *  ntp-keygen author or copyright holder.  AutoOpts is licensed under
 *  the terms of the LGPL.  The redistributable library (``libopts'') is
 *  licensed under the terms of either the LGPL or, at the users discretion,
 *  the BSD license.  See the AutoOpts and/or libopts sources for details.
 *
 * This source file is copyrighted and licensed under the following terms:
 *
 * ntp-keygen copyright 1970-2008 David L. Mills and/or others - all rights reserved
 *
 * see html/copyright.html
 */
/*
 *  This file contains the programmatic interface to the Automated
 *  Options generated for the ntp-keygen program.
 *  These macros are documented in the AutoGen info file in the
 *  "AutoOpts" chapter.  Please refer to that doc for usage help.
 */
#ifndef AUTOOPTS_NTP_KEYGEN_OPTS_H_GUARD
#define AUTOOPTS_NTP_KEYGEN_OPTS_H_GUARD
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
        INDEX_OPT_CERTIFICATE      =  0,
        INDEX_OPT_DEBUG_LEVEL      =  1,
        INDEX_OPT_SET_DEBUG_LEVEL  =  2,
        INDEX_OPT_ID_KEY           =  3,
        INDEX_OPT_GQ_PARAMS        =  4,
        INDEX_OPT_GQ_KEYS          =  5,
        INDEX_OPT_HOST_KEY         =  6,
        INDEX_OPT_IFFKEY           =  7,
        INDEX_OPT_ISSUER_NAME      =  8,
        INDEX_OPT_MD5KEY           =  9,
        INDEX_OPT_MODULUS          = 10,
        INDEX_OPT_PVT_CERT         = 11,
        INDEX_OPT_PVT_PASSWD       = 12,
        INDEX_OPT_GET_PVT_PASSWD   = 13,
        INDEX_OPT_SIGN_KEY         = 14,
        INDEX_OPT_SUBJECT_NAME     = 15,
        INDEX_OPT_TRUSTED_CERT     = 16,
        INDEX_OPT_MV_PARAMS        = 17,
        INDEX_OPT_MV_KEYS          = 18,
        INDEX_OPT_VERSION          = 19,
        INDEX_OPT_HELP             = 20,
        INDEX_OPT_MORE_HELP        = 21,
        INDEX_OPT_SAVE_OPTS        = 22,
        INDEX_OPT_LOAD_OPTS        = 23
} teOptIndex;

#define OPTION_CT    24
#define NTP_KEYGEN_VERSION       "4.2.4p5"
#define NTP_KEYGEN_FULL_VERSION  "ntp-keygen (ntp) - Create a NTP host key - Ver. 4.2.4p5"

/*
 *  Interface defines for all options.  Replace "n" with the UPPER_CASED
 *  option name (as in the teOptIndex enumeration above).
 *  e.g. HAVE_OPT( CERTIFICATE )
 */
#define         DESC(n) (ntp_keygenOptions.pOptDesc[INDEX_OPT_## n])
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
# ifdef    CERTIFICATE
#  warning undefining CERTIFICATE due to option name conflict
#  undef   CERTIFICATE
# endif
# ifdef    DEBUG_LEVEL
#  warning undefining DEBUG_LEVEL due to option name conflict
#  undef   DEBUG_LEVEL
# endif
# ifdef    SET_DEBUG_LEVEL
#  warning undefining SET_DEBUG_LEVEL due to option name conflict
#  undef   SET_DEBUG_LEVEL
# endif
# ifdef    ID_KEY
#  warning undefining ID_KEY due to option name conflict
#  undef   ID_KEY
# endif
# ifdef    GQ_PARAMS
#  warning undefining GQ_PARAMS due to option name conflict
#  undef   GQ_PARAMS
# endif
# ifdef    GQ_KEYS
#  warning undefining GQ_KEYS due to option name conflict
#  undef   GQ_KEYS
# endif
# ifdef    HOST_KEY
#  warning undefining HOST_KEY due to option name conflict
#  undef   HOST_KEY
# endif
# ifdef    IFFKEY
#  warning undefining IFFKEY due to option name conflict
#  undef   IFFKEY
# endif
# ifdef    ISSUER_NAME
#  warning undefining ISSUER_NAME due to option name conflict
#  undef   ISSUER_NAME
# endif
# ifdef    MD5KEY
#  warning undefining MD5KEY due to option name conflict
#  undef   MD5KEY
# endif
# ifdef    MODULUS
#  warning undefining MODULUS due to option name conflict
#  undef   MODULUS
# endif
# ifdef    PVT_CERT
#  warning undefining PVT_CERT due to option name conflict
#  undef   PVT_CERT
# endif
# ifdef    PVT_PASSWD
#  warning undefining PVT_PASSWD due to option name conflict
#  undef   PVT_PASSWD
# endif
# ifdef    GET_PVT_PASSWD
#  warning undefining GET_PVT_PASSWD due to option name conflict
#  undef   GET_PVT_PASSWD
# endif
# ifdef    SIGN_KEY
#  warning undefining SIGN_KEY due to option name conflict
#  undef   SIGN_KEY
# endif
# ifdef    SUBJECT_NAME
#  warning undefining SUBJECT_NAME due to option name conflict
#  undef   SUBJECT_NAME
# endif
# ifdef    TRUSTED_CERT
#  warning undefining TRUSTED_CERT due to option name conflict
#  undef   TRUSTED_CERT
# endif
# ifdef    MV_PARAMS
#  warning undefining MV_PARAMS due to option name conflict
#  undef   MV_PARAMS
# endif
# ifdef    MV_KEYS
#  warning undefining MV_KEYS due to option name conflict
#  undef   MV_KEYS
# endif
#else  /* NO_OPTION_NAME_WARNINGS */
# undef CERTIFICATE
# undef DEBUG_LEVEL
# undef SET_DEBUG_LEVEL
# undef ID_KEY
# undef GQ_PARAMS
# undef GQ_KEYS
# undef HOST_KEY
# undef IFFKEY
# undef ISSUER_NAME
# undef MD5KEY
# undef MODULUS
# undef PVT_CERT
# undef PVT_PASSWD
# undef GET_PVT_PASSWD
# undef SIGN_KEY
# undef SUBJECT_NAME
# undef TRUSTED_CERT
# undef MV_PARAMS
# undef MV_KEYS
#endif  /*  NO_OPTION_NAME_WARNINGS */

/*
 *  Interface defines for specific options.
 */
#ifdef OPENSSL
#define VALUE_OPT_CERTIFICATE    'c'
#endif /* OPENSSL */
#ifdef DEBUG
#define VALUE_OPT_DEBUG_LEVEL    'd'
#endif /* DEBUG */
#ifdef DEBUG
#define VALUE_OPT_SET_DEBUG_LEVEL 'D'
#endif /* DEBUG */
#ifdef OPENSSL
#define VALUE_OPT_ID_KEY         'e'
#endif /* OPENSSL */
#ifdef OPENSSL
#define VALUE_OPT_GQ_PARAMS      'G'
#endif /* OPENSSL */
#ifdef OPENSSL
#define VALUE_OPT_GQ_KEYS        'g'
#endif /* OPENSSL */
#ifdef OPENSSL
#define VALUE_OPT_HOST_KEY       'H'
#endif /* OPENSSL */
#ifdef OPENSSL
#define VALUE_OPT_IFFKEY         'I'
#endif /* OPENSSL */
#ifdef OPENSSL
#define VALUE_OPT_ISSUER_NAME    'i'
#endif /* OPENSSL */
#define VALUE_OPT_MD5KEY         'M'
#ifdef OPENSSL
#define VALUE_OPT_MODULUS        'm'
#define OPT_VALUE_MODULUS        (DESC(MODULUS).optArg.argInt)
#endif /* OPENSSL */
#ifdef OPENSSL
#define VALUE_OPT_PVT_CERT       'P'
#endif /* OPENSSL */
#ifdef OPENSSL
#define VALUE_OPT_PVT_PASSWD     'p'
#endif /* OPENSSL */
#ifdef OPENSSL
#define VALUE_OPT_GET_PVT_PASSWD 'q'
#endif /* OPENSSL */
#ifdef OPENSSL
#define VALUE_OPT_SIGN_KEY       'S'
#endif /* OPENSSL */
#ifdef OPENSSL
#define VALUE_OPT_SUBJECT_NAME   's'
#endif /* OPENSSL */
#ifdef OPENSSL
#define VALUE_OPT_TRUSTED_CERT   'T'
#endif /* OPENSSL */
#ifdef OPENSSL
#define VALUE_OPT_MV_PARAMS      'V'
#define OPT_VALUE_MV_PARAMS      (DESC(MV_PARAMS).optArg.argInt)
#endif /* OPENSSL */
#ifdef OPENSSL
#define VALUE_OPT_MV_KEYS        'v'
#define OPT_VALUE_MV_KEYS        (DESC(MV_KEYS).optArg.argInt)
#endif /* OPENSSL */

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
#define ERRSKIP_OPTERR  STMTS( ntp_keygenOptions.fOptSet &= ~OPTPROC_ERRSTOP )
#define ERRSTOP_OPTERR  STMTS( ntp_keygenOptions.fOptSet |= OPTPROC_ERRSTOP )
#define RESTART_OPT(n)  STMTS( \
                ntp_keygenOptions.curOptIdx = (n); \
                ntp_keygenOptions.pzCurOpt  = NULL )
#define START_OPT       RESTART_OPT(1)
#define USAGE(c)        (*ntp_keygenOptions.pUsageProc)( &ntp_keygenOptions, c )
/* extracted from /usr/local/gnu/share/autogen/opthead.tpl near line 360 */

/* * * * * *
 *
 *  Declare the ntp-keygen option descriptor.
 */
#ifdef  __cplusplus
extern "C" {
#endif

extern tOptions   ntp_keygenOptions;

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
#endif /* AUTOOPTS_NTP_KEYGEN_OPTS_H_GUARD */
/* ntp-keygen-opts.h ends here */
