/*  
 *  EDIT THIS FILE WITH CAUTION  (sntp-opts.c)
 *  
 *  It has been AutoGen-ed  Tuesday December  8, 2009 at 08:14:49 AM EST
 *  From the definitions    sntp-opts.def
 *  and the template file   options
 *
 * Generated from AutoOpts 29:0:4 templates.
 */

/*
 *  This file was produced by an AutoOpts template.  AutoOpts is a
 *  copyrighted work.  This source file is not encumbered by AutoOpts
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


#include <limits.h>

#define OPTION_CODE_COMPILE 1
#include "sntp-opts.h"

#ifdef  __cplusplus
extern "C" {
#endif
tSCC zCopyright[] =
       "sntp copyright (c) 1970-2006 ntp.org, all rights reserved";
tSCC zCopyrightNotice[] =
       
/* extracted from sntp-opts.def near line 12 */
"        General Public Licence for the software known as MSNTP\n\
        ------------------------------------------------------\n\n\
\t  (c) Copyright, N.M. Maclaren, 1996, 1997, 2000\n\
\t  (c) Copyright, University of Cambridge, 1996, 1997, 2000\n\n\n\n\
Free use of MSNTP in source and binary forms is permitted, provided that this\n\
entire licence is duplicated in all copies, and that any documentation,\n\
announcements, and other materials related to use acknowledge that the software\n\
was developed by N.M. Maclaren (hereafter refered to as the Author) at the\n\
University of Cambridge.  Neither the name of the Author nor the University of\n\
Cambridge may be used to endorse or promote products derived from this material\n\
without specific prior written permission.\n\n\
The Author and the University of Cambridge retain the copyright and all other\n\
legal rights to the software and make it available non-exclusively.  All users\n\
must ensure that the software in all its derivations carries a copyright notice\n\
in the form:\n\
\t  (c) Copyright N.M. Maclaren,\n\
\t  (c) Copyright University of Cambridge.\n\n\n\n\
                           NO WARRANTY\n\n\
Because the MSNTP software is licensed free of charge, the Author and the\n\
University of Cambridge provide absolutely no warranty, either expressed or\n\
implied, including, but not limited to, the implied warranties of\n\
merchantability and fitness for a particular purpose.  The entire risk as to\n\
the quality and performance of the MSNTP software is with you.  Should MSNTP\n\
prove defective, you assume the cost of all necessary servicing or repair.\n\n\
In no event, unless required by law, will the Author or the University of\n\
Cambridge, or any other party who may modify and redistribute this software as\n\
permitted in accordance with the provisions below, be liable for damages for\n\
any losses whatsoever, including but not limited to lost profits, lost monies,\n\
lost or corrupted data, or other special, incidental or consequential losses\n\
that may arise out of the use or inability to use the MSNTP software.\n\n\n\n\
                         COPYING POLICY\n\n\
Permission is hereby granted for copying and distribution of copies of the\n\
MSNTP source and binary files, and of any part thereof, subject to the\n\
following licence conditions:\n\n\
1. You may distribute MSNTP or components of MSNTP, with or without additions\n\
developed by you or by others.  No charge, other than an \"at-cost\" distribution\n\
fee, may be charged for copies, derivations, or distributions of this material\n\
without the express written consent of the copyright holders.\n\n\
2. You may also distribute MSNTP along with any other product for sale,\n\
provided that the cost of the bundled package is the same regardless of whether\n\
MSNTP is included or not, and provided that those interested only in MSNTP must\n\
be notified that it is a product freely available from the University of\n\
Cambridge.\n\n\
3. If you distribute MSNTP software or parts of MSNTP, with or without\n\
additions developed by you or others, then you must either make available the\n\
source to all portions of the MSNTP system (exclusive of any additions made by\n\
you or by others) upon request, or instead you may notify anyone requesting\n\
source that it is freely available from the University of Cambridge.\n\n\
4. You may not omit any of the copyright notices on either the source files,\n\
the executable files, or the documentation.\n\n\
5. You may not omit transmission of this License agreement with whatever\n\
portions of MSNTP that are distributed.\n\n\
6. Any users of this software must be notified that it is without warranty or\n\
guarantee of any nature, express or implied, nor is there any fitness for use\n\
represented.\n\n\n\
October 1996\n\
April 1997\n\
October 2000";
extern tUsageProc optionUsage;

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
 *  Unprivport option description:
 */
tSCC    zUnprivportText[] =
        "Use an unprivileged port";
tSCC    zUnprivport_NAME[]         = "UNPRIVPORT";
tSCC    zUnprivport_Name[]         = "unprivport";
#define UNPRIVPORT_FLAGS       (OPTST_DISABLED)

/*
 *  Normalverbose option description with
 *  "Must also have options" and "Incompatible options":
 */
tSCC    zNormalverboseText[] =
        "Slightly verbose";
tSCC    zNormalverbose_NAME[]      = "NORMALVERBOSE";
tSCC    zNormalverbose_Name[]      = "normalverbose";
static const int
    aNormalverboseCantList[] = {
    INDEX_OPT_EXTRAVERBOSE,
    INDEX_OPT_MEGAVERBOSE, NO_EQUIVALENT };
#define NORMALVERBOSE_FLAGS       (OPTST_DISABLED)

/*
 *  Extraverbose option description with
 *  "Must also have options" and "Incompatible options":
 */
tSCC    zExtraverboseText[] =
        "Extra verbose";
tSCC    zExtraverbose_NAME[]       = "EXTRAVERBOSE";
tSCC    zExtraverbose_Name[]       = "extraverbose";
static const int
    aExtraverboseCantList[] = {
    INDEX_OPT_NORMALVERBOSE,
    INDEX_OPT_MEGAVERBOSE, NO_EQUIVALENT };
#define EXTRAVERBOSE_FLAGS       (OPTST_DISABLED)

/*
 *  Megaverbose option description with
 *  "Must also have options" and "Incompatible options":
 */
tSCC    zMegaverboseText[] =
        "Mega verbose";
tSCC    zMegaverbose_NAME[]        = "MEGAVERBOSE";
tSCC    zMegaverbose_Name[]        = "megaverbose";
static const int
    aMegaverboseCantList[] = {
    INDEX_OPT_NORMALVERBOSE,
    INDEX_OPT_EXTRAVERBOSE, NO_EQUIVALENT };
#define MEGAVERBOSE_FLAGS       (OPTST_DISABLED)

/*
 *  Settimeofday option description with
 *  "Must also have options" and "Incompatible options":
 */
tSCC    zSettimeofdayText[] =
        "Set (step) the time with settimeofday()";
tSCC    zSettimeofday_NAME[]       = "SETTIMEOFDAY";
tSCC    zSettimeofday_Name[]       = "settimeofday";
static const int
    aSettimeofdayCantList[] = {
    INDEX_OPT_ADJTIME, NO_EQUIVALENT };
#define SETTIMEOFDAY_FLAGS       (OPTST_DISABLED)

/*
 *  Adjtime option description with
 *  "Must also have options" and "Incompatible options":
 */
tSCC    zAdjtimeText[] =
        "Set (slew) the time with adjtime()";
tSCC    zAdjtime_NAME[]            = "ADJTIME";
tSCC    zAdjtime_Name[]            = "adjtime";
static const int
    aAdjtimeCantList[] = {
    INDEX_OPT_SETTIMEOFDAY, NO_EQUIVALENT };
#define ADJTIME_FLAGS       (OPTST_DISABLED)

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
#if defined(TEST_SNTP_OPTS)
/*
 *  Under test, omit argument processing, or call optionStackArg,
 *  if multiple copies are allowed.
 */
extern tOptProc
    optionPagedUsage, optionVersionStderr;
static tOptProc
    doUsageOpt;

#else /* NOT defined TEST_SNTP_OPTS */
/*
 *  When not under test, there are different procs to use
 */
extern tOptProc
    optionPagedUsage, optionPrintVersion;
static tOptProc
    doUsageOpt;
#endif /* defined(TEST_SNTP_OPTS) */
#ifdef TEST_SNTP_OPTS
# define DOVERPROC optionVersionStderr
#else
# define DOVERPROC optionPrintVersion
#endif /* TEST_SNTP_OPTS */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  Define the Sntp Option Descriptions.
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

  {  /* entry idx, value */ 2, VALUE_OPT_UNPRIVPORT,
     /* equiv idx, value */ 2, VALUE_OPT_UNPRIVPORT,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ UNPRIVPORT_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zUnprivportText, zUnprivport_NAME, zUnprivport_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 3, VALUE_OPT_NORMALVERBOSE,
     /* equiv idx, value */ 3, VALUE_OPT_NORMALVERBOSE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ NORMALVERBOSE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aNormalverboseCantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zNormalverboseText, zNormalverbose_NAME, zNormalverbose_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 4, VALUE_OPT_EXTRAVERBOSE,
     /* equiv idx, value */ 4, VALUE_OPT_EXTRAVERBOSE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ EXTRAVERBOSE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aExtraverboseCantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zExtraverboseText, zExtraverbose_NAME, zExtraverbose_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 5, VALUE_OPT_MEGAVERBOSE,
     /* equiv idx, value */ 5, VALUE_OPT_MEGAVERBOSE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ MEGAVERBOSE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aMegaverboseCantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zMegaverboseText, zMegaverbose_NAME, zMegaverbose_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 6, VALUE_OPT_SETTIMEOFDAY,
     /* equiv idx, value */ 6, VALUE_OPT_SETTIMEOFDAY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ SETTIMEOFDAY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aSettimeofdayCantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zSettimeofdayText, zSettimeofday_NAME, zSettimeofday_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 7, VALUE_OPT_ADJTIME,
     /* equiv idx, value */ 7, VALUE_OPT_ADJTIME,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ ADJTIME_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, aAdjtimeCantList,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zAdjtimeText, zAdjtime_NAME, zAdjtime_Name,
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
 *  Define the Sntp Option Environment
 */
tSCC   zPROGNAME[]   = "SNTP";
tSCC   zUsageTitle[] =
"sntp - standard SNTP program - Ver. 4.2.4p8\n\
USAGE:  %s [ -<flag> | --<name> ]...\n";
tSCC   zRcName[]     = ".ntprc";
tSCC*  apzHomeList[] = {
       "$HOME",
       ".",
       NULL };

tSCC   zBugsAddr[]    = "http://bugs.ntp.org, bugs@ntp.org";
#define zExplain NULL
tSCC    zDetail[]     = "\n\
.I sntp\n\
can be used as a SNTP client to query a NTP or SNTP server and either display\n\
the time or set the local system's time (given suitable privilege).  It can be\n\
run as an interactive command or in a\n\
.I cron\n\
job.\n\
NTP is the Network Time Protocol (RFC 1305) and SNTP is the\n\
Simple Network Time Protocol (RFC 2030, which supersedes RFC 1769).\n";
tSCC    zFullVersion[] = SNTP_FULL_VERSION;
/* extracted from /usr/local/gnu/autogen-5.9.1/share/autogen/optcode.tpl near line 408 */

#if defined(ENABLE_NLS)
# define OPTPROC_BASE OPTPROC_TRANSLATE
  static tOptionXlateProc translate_option_strings;
#else
# define OPTPROC_BASE OPTPROC_NONE
# define translate_option_strings NULL
#endif /* ENABLE_NLS */

tOptions sntpOptions = {
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
    13 /* full option count */, 8 /* user option count */
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
/* extracted from /usr/local/gnu/autogen-5.9.1/share/autogen/optmain.tpl near line 92 */

#if defined(TEST_SNTP_OPTS) /* TEST MAIN PROCEDURE: */

int
main( int argc, char** argv )
{
    int res = EXIT_SUCCESS;
    (void)optionProcess( &sntpOptions, argc, argv );
    {
        void optionPutShell( tOptions* );
        optionPutShell( &sntpOptions );
    }
    return res;
}
#endif  /* defined TEST_SNTP_OPTS */
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
  coerce_it((void*)&(sntpOptions._f))

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
        tOptDesc* pOD = sntpOptions.pOptDesc;
        int       ix  = sntpOptions.optCt;

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
/* sntp-opts.c ends here */
