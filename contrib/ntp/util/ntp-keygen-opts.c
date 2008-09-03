/*  
 *  EDIT THIS FILE WITH CAUTION  (ntp-keygen-opts.c)
 *  
 *  It has been AutoGen-ed  Sunday August 17, 2008 at 05:27:33 AM EDT
 *  From the definitions    ntp-keygen-opts.def
 *  and the template file   options
 *
 * Generated from AutoOpts 29:0:4 templates.
 */

/*
 *  This file was produced by an AutoOpts template.  AutoOpts is a
 *  copyrighted work.  This source file is not encumbered by AutoOpts
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


#include <limits.h>
#include <stdio.h>
#define OPTION_CODE_COMPILE 1
#include "ntp-keygen-opts.h"

#ifdef  __cplusplus
extern "C" {
#endif
tSCC zCopyright[] =
       "ntp-keygen copyright (c) 1970-2008 David L. Mills and/or others, all rights reserved";
tSCC zCopyrightNotice[] =
       
/* extracted from ../include/copyright.def near line 8 */
"see html/copyright.html";
extern tUsageProc optionUsage;

/*
 *  global included definitions
 */
#include <stdlib.h>
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
 *  Certificate option description:
 */
#ifdef OPENSSL
tSCC    zCertificateText[] =
        "certificate scheme";
tSCC    zCertificate_NAME[]        = "CERTIFICATE";
tSCC    zCertificate_Name[]        = "certificate";
#define CERTIFICATE_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

#else   /* disable Certificate */
#define VALUE_OPT_CERTIFICATE NO_EQUIVALENT
#define CERTIFICATE_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zCertificateText       NULL
#define zCertificate_NAME      NULL
#define zCertificate_Name      NULL
#endif  /* OPENSSL */

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
 *  Id_Key option description:
 */
#ifdef OPENSSL
tSCC    zId_KeyText[] =
        "Write identity keys";
tSCC    zId_Key_NAME[]             = "ID_KEY";
tSCC    zId_Key_Name[]             = "id-key";
#define ID_KEY_FLAGS       (OPTST_DISABLED)

#else   /* disable Id_Key */
#define VALUE_OPT_ID_KEY NO_EQUIVALENT
#define ID_KEY_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zId_KeyText       NULL
#define zId_Key_NAME      NULL
#define zId_Key_Name      NULL
#endif  /* OPENSSL */

/*
 *  Gq_Params option description:
 */
#ifdef OPENSSL
tSCC    zGq_ParamsText[] =
        "Generate GQ parameters and keys";
tSCC    zGq_Params_NAME[]          = "GQ_PARAMS";
tSCC    zGq_Params_Name[]          = "gq-params";
#define GQ_PARAMS_FLAGS       (OPTST_DISABLED)

#else   /* disable Gq_Params */
#define VALUE_OPT_GQ_PARAMS NO_EQUIVALENT
#define GQ_PARAMS_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zGq_ParamsText       NULL
#define zGq_Params_NAME      NULL
#define zGq_Params_Name      NULL
#endif  /* OPENSSL */

/*
 *  Gq_Keys option description:
 */
#ifdef OPENSSL
tSCC    zGq_KeysText[] =
        "update GQ keys";
tSCC    zGq_Keys_NAME[]            = "GQ_KEYS";
tSCC    zGq_Keys_Name[]            = "gq-keys";
#define GQ_KEYS_FLAGS       (OPTST_DISABLED)

#else   /* disable Gq_Keys */
#define VALUE_OPT_GQ_KEYS NO_EQUIVALENT
#define GQ_KEYS_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zGq_KeysText       NULL
#define zGq_Keys_NAME      NULL
#define zGq_Keys_Name      NULL
#endif  /* OPENSSL */

/*
 *  Host_Key option description:
 */
#ifdef OPENSSL
tSCC    zHost_KeyText[] =
        "generate RSA host key";
tSCC    zHost_Key_NAME[]           = "HOST_KEY";
tSCC    zHost_Key_Name[]           = "host-key";
#define HOST_KEY_FLAGS       (OPTST_DISABLED)

#else   /* disable Host_Key */
#define VALUE_OPT_HOST_KEY NO_EQUIVALENT
#define HOST_KEY_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zHost_KeyText       NULL
#define zHost_Key_NAME      NULL
#define zHost_Key_Name      NULL
#endif  /* OPENSSL */

/*
 *  Iffkey option description:
 */
#ifdef OPENSSL
tSCC    zIffkeyText[] =
        "generate IFF parameters";
tSCC    zIffkey_NAME[]             = "IFFKEY";
tSCC    zIffkey_Name[]             = "iffkey";
#define IFFKEY_FLAGS       (OPTST_DISABLED)

#else   /* disable Iffkey */
#define VALUE_OPT_IFFKEY NO_EQUIVALENT
#define IFFKEY_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zIffkeyText       NULL
#define zIffkey_NAME      NULL
#define zIffkey_Name      NULL
#endif  /* OPENSSL */

/*
 *  Issuer_Name option description:
 */
#ifdef OPENSSL
tSCC    zIssuer_NameText[] =
        "set issuer name";
tSCC    zIssuer_Name_NAME[]        = "ISSUER_NAME";
tSCC    zIssuer_Name_Name[]        = "issuer-name";
#define ISSUER_NAME_FLAGS       (OPTST_DISABLED)

#else   /* disable Issuer_Name */
#define VALUE_OPT_ISSUER_NAME NO_EQUIVALENT
#define ISSUER_NAME_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zIssuer_NameText       NULL
#define zIssuer_Name_NAME      NULL
#define zIssuer_Name_Name      NULL
#endif  /* OPENSSL */

/*
 *  Md5key option description:
 */
tSCC    zMd5keyText[] =
        "generate MD5 keys";
tSCC    zMd5key_NAME[]             = "MD5KEY";
tSCC    zMd5key_Name[]             = "md5key";
#define MD5KEY_FLAGS       (OPTST_DISABLED)

/*
 *  Modulus option description:
 */
#ifdef OPENSSL
tSCC    zModulusText[] =
        "modulus";
tSCC    zModulus_NAME[]            = "MODULUS";
tSCC    zModulus_Name[]            = "modulus";
#define MODULUS_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_NUMERIC))

#else   /* disable Modulus */
#define VALUE_OPT_MODULUS NO_EQUIVALENT
#define MODULUS_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zModulusText       NULL
#define zModulus_NAME      NULL
#define zModulus_Name      NULL
#endif  /* OPENSSL */

/*
 *  Pvt_Cert option description:
 */
#ifdef OPENSSL
tSCC    zPvt_CertText[] =
        "generate PC private certificate";
tSCC    zPvt_Cert_NAME[]           = "PVT_CERT";
tSCC    zPvt_Cert_Name[]           = "pvt-cert";
#define PVT_CERT_FLAGS       (OPTST_DISABLED)

#else   /* disable Pvt_Cert */
#define VALUE_OPT_PVT_CERT NO_EQUIVALENT
#define PVT_CERT_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zPvt_CertText       NULL
#define zPvt_Cert_NAME      NULL
#define zPvt_Cert_Name      NULL
#endif  /* OPENSSL */

/*
 *  Pvt_Passwd option description:
 */
#ifdef OPENSSL
tSCC    zPvt_PasswdText[] =
        "output private password";
tSCC    zPvt_Passwd_NAME[]         = "PVT_PASSWD";
tSCC    zPvt_Passwd_Name[]         = "pvt-passwd";
#define PVT_PASSWD_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

#else   /* disable Pvt_Passwd */
#define VALUE_OPT_PVT_PASSWD NO_EQUIVALENT
#define PVT_PASSWD_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zPvt_PasswdText       NULL
#define zPvt_Passwd_NAME      NULL
#define zPvt_Passwd_Name      NULL
#endif  /* OPENSSL */

/*
 *  Get_Pvt_Passwd option description:
 */
#ifdef OPENSSL
tSCC    zGet_Pvt_PasswdText[] =
        "input private password";
tSCC    zGet_Pvt_Passwd_NAME[]     = "GET_PVT_PASSWD";
tSCC    zGet_Pvt_Passwd_Name[]     = "get-pvt-passwd";
#define GET_PVT_PASSWD_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

#else   /* disable Get_Pvt_Passwd */
#define VALUE_OPT_GET_PVT_PASSWD NO_EQUIVALENT
#define GET_PVT_PASSWD_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zGet_Pvt_PasswdText       NULL
#define zGet_Pvt_Passwd_NAME      NULL
#define zGet_Pvt_Passwd_Name      NULL
#endif  /* OPENSSL */

/*
 *  Sign_Key option description:
 */
#ifdef OPENSSL
tSCC    zSign_KeyText[] =
        "generate sign key (RSA or DSA)";
tSCC    zSign_Key_NAME[]           = "SIGN_KEY";
tSCC    zSign_Key_Name[]           = "sign-key";
#define SIGN_KEY_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

#else   /* disable Sign_Key */
#define VALUE_OPT_SIGN_KEY NO_EQUIVALENT
#define SIGN_KEY_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zSign_KeyText       NULL
#define zSign_Key_NAME      NULL
#define zSign_Key_Name      NULL
#endif  /* OPENSSL */

/*
 *  Subject_Name option description:
 */
#ifdef OPENSSL
tSCC    zSubject_NameText[] =
        "set subject name";
tSCC    zSubject_Name_NAME[]       = "SUBJECT_NAME";
tSCC    zSubject_Name_Name[]       = "subject-name";
#define SUBJECT_NAME_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_STRING))

#else   /* disable Subject_Name */
#define VALUE_OPT_SUBJECT_NAME NO_EQUIVALENT
#define SUBJECT_NAME_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zSubject_NameText       NULL
#define zSubject_Name_NAME      NULL
#define zSubject_Name_Name      NULL
#endif  /* OPENSSL */

/*
 *  Trusted_Cert option description:
 */
#ifdef OPENSSL
tSCC    zTrusted_CertText[] =
        "trusted certificate (TC scheme)";
tSCC    zTrusted_Cert_NAME[]       = "TRUSTED_CERT";
tSCC    zTrusted_Cert_Name[]       = "trusted-cert";
#define TRUSTED_CERT_FLAGS       (OPTST_DISABLED)

#else   /* disable Trusted_Cert */
#define VALUE_OPT_TRUSTED_CERT NO_EQUIVALENT
#define TRUSTED_CERT_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zTrusted_CertText       NULL
#define zTrusted_Cert_NAME      NULL
#define zTrusted_Cert_Name      NULL
#endif  /* OPENSSL */

/*
 *  Mv_Params option description:
 */
#ifdef OPENSSL
tSCC    zMv_ParamsText[] =
        "generate <num> MV parameters";
tSCC    zMv_Params_NAME[]          = "MV_PARAMS";
tSCC    zMv_Params_Name[]          = "mv-params";
#define MV_PARAMS_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_NUMERIC))

#else   /* disable Mv_Params */
#define VALUE_OPT_MV_PARAMS NO_EQUIVALENT
#define MV_PARAMS_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zMv_ParamsText       NULL
#define zMv_Params_NAME      NULL
#define zMv_Params_Name      NULL
#endif  /* OPENSSL */

/*
 *  Mv_Keys option description:
 */
#ifdef OPENSSL
tSCC    zMv_KeysText[] =
        "update <num> MV keys";
tSCC    zMv_Keys_NAME[]            = "MV_KEYS";
tSCC    zMv_Keys_Name[]            = "mv-keys";
#define MV_KEYS_FLAGS       (OPTST_DISABLED \
        | OPTST_SET_ARGTYPE(OPARG_TYPE_NUMERIC))

#else   /* disable Mv_Keys */
#define VALUE_OPT_MV_KEYS NO_EQUIVALENT
#define MV_KEYS_FLAGS       (OPTST_OMITTED | OPTST_NO_INIT)
#define zMv_KeysText       NULL
#define zMv_Keys_NAME      NULL
#define zMv_Keys_Name      NULL
#endif  /* OPENSSL */

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
#ifdef OPENSSL
  static tOptProc doOptModulus;
#else /* not OPENSSL */
# define doOptModulus NULL
#endif /* def/not OPENSSL */
#ifdef OPENSSL
  extern tOptProc optionNumericVal;
#else /* not OPENSSL */
# define optionNumericVal NULL
#endif /* def/not OPENSSL */
#ifdef OPENSSL
  extern tOptProc optionNumericVal;
#else /* not OPENSSL */
# define optionNumericVal NULL
#endif /* def/not OPENSSL */
#if defined(TEST_NTP_KEYGEN_OPTS)
/*
 *  Under test, omit argument processing, or call optionStackArg,
 *  if multiple copies are allowed.
 */
extern tOptProc
    optionNumericVal, optionPagedUsage, optionVersionStderr;
static tOptProc
    doOptModulus, doUsageOpt;

/*
 *  #define map the "normal" callout procs to the test ones...
 */
#define SET_DEBUG_LEVEL_OPT_PROC optionStackArg


#else /* NOT defined TEST_NTP_KEYGEN_OPTS */
/*
 *  When not under test, there are different procs to use
 */
extern tOptProc
    optionPagedUsage, optionPrintVersion;
static tOptProc
    doUsageOpt;

/*
 *  #define map the "normal" callout procs
 */
#define SET_DEBUG_LEVEL_OPT_PROC doOptSet_Debug_Level

#define SET_DEBUG_LEVEL_OPT_PROC doOptSet_Debug_Level
#endif /* defined(TEST_NTP_KEYGEN_OPTS) */
#ifdef TEST_NTP_KEYGEN_OPTS
# define DOVERPROC optionVersionStderr
#else
# define DOVERPROC optionPrintVersion
#endif /* TEST_NTP_KEYGEN_OPTS */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  Define the Ntp_Keygen Option Descriptions.
 */
static tOptDesc optDesc[ OPTION_CT ] = {
  {  /* entry idx, value */ 0, VALUE_OPT_CERTIFICATE,
     /* equiv idx, value */ 0, VALUE_OPT_CERTIFICATE,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ CERTIFICATE_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zCertificateText, zCertificate_NAME, zCertificate_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 1, VALUE_OPT_DEBUG_LEVEL,
     /* equiv idx, value */ 1, VALUE_OPT_DEBUG_LEVEL,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ DEBUG_LEVEL_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zDebug_LevelText, zDebug_Level_NAME, zDebug_Level_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 2, VALUE_OPT_SET_DEBUG_LEVEL,
     /* equiv idx, value */ 2, VALUE_OPT_SET_DEBUG_LEVEL,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, NOLIMIT, 0,
     /* opt state flags  */ SET_DEBUG_LEVEL_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ SET_DEBUG_LEVEL_OPT_PROC,
     /* desc, NAME, name */ zSet_Debug_LevelText, zSet_Debug_Level_NAME, zSet_Debug_Level_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 3, VALUE_OPT_ID_KEY,
     /* equiv idx, value */ 3, VALUE_OPT_ID_KEY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ ID_KEY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zId_KeyText, zId_Key_NAME, zId_Key_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 4, VALUE_OPT_GQ_PARAMS,
     /* equiv idx, value */ 4, VALUE_OPT_GQ_PARAMS,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ GQ_PARAMS_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zGq_ParamsText, zGq_Params_NAME, zGq_Params_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 5, VALUE_OPT_GQ_KEYS,
     /* equiv idx, value */ 5, VALUE_OPT_GQ_KEYS,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ GQ_KEYS_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zGq_KeysText, zGq_Keys_NAME, zGq_Keys_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 6, VALUE_OPT_HOST_KEY,
     /* equiv idx, value */ 6, VALUE_OPT_HOST_KEY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ HOST_KEY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zHost_KeyText, zHost_Key_NAME, zHost_Key_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 7, VALUE_OPT_IFFKEY,
     /* equiv idx, value */ 7, VALUE_OPT_IFFKEY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ IFFKEY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zIffkeyText, zIffkey_NAME, zIffkey_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 8, VALUE_OPT_ISSUER_NAME,
     /* equiv idx, value */ 8, VALUE_OPT_ISSUER_NAME,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ ISSUER_NAME_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zIssuer_NameText, zIssuer_Name_NAME, zIssuer_Name_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 9, VALUE_OPT_MD5KEY,
     /* equiv idx, value */ 9, VALUE_OPT_MD5KEY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ MD5KEY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zMd5keyText, zMd5key_NAME, zMd5key_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 10, VALUE_OPT_MODULUS,
     /* equiv idx, value */ 10, VALUE_OPT_MODULUS,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ MODULUS_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ doOptModulus,
     /* desc, NAME, name */ zModulusText, zModulus_NAME, zModulus_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 11, VALUE_OPT_PVT_CERT,
     /* equiv idx, value */ 11, VALUE_OPT_PVT_CERT,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ PVT_CERT_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zPvt_CertText, zPvt_Cert_NAME, zPvt_Cert_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 12, VALUE_OPT_PVT_PASSWD,
     /* equiv idx, value */ 12, VALUE_OPT_PVT_PASSWD,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ PVT_PASSWD_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zPvt_PasswdText, zPvt_Passwd_NAME, zPvt_Passwd_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 13, VALUE_OPT_GET_PVT_PASSWD,
     /* equiv idx, value */ 13, VALUE_OPT_GET_PVT_PASSWD,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ GET_PVT_PASSWD_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zGet_Pvt_PasswdText, zGet_Pvt_Passwd_NAME, zGet_Pvt_Passwd_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 14, VALUE_OPT_SIGN_KEY,
     /* equiv idx, value */ 14, VALUE_OPT_SIGN_KEY,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ SIGN_KEY_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zSign_KeyText, zSign_Key_NAME, zSign_Key_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 15, VALUE_OPT_SUBJECT_NAME,
     /* equiv idx, value */ 15, VALUE_OPT_SUBJECT_NAME,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ SUBJECT_NAME_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zSubject_NameText, zSubject_Name_NAME, zSubject_Name_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 16, VALUE_OPT_TRUSTED_CERT,
     /* equiv idx, value */ 16, VALUE_OPT_TRUSTED_CERT,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ TRUSTED_CERT_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ NULL,
     /* desc, NAME, name */ zTrusted_CertText, zTrusted_Cert_NAME, zTrusted_Cert_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 17, VALUE_OPT_MV_PARAMS,
     /* equiv idx, value */ 17, VALUE_OPT_MV_PARAMS,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ MV_PARAMS_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionNumericVal,
     /* desc, NAME, name */ zMv_ParamsText, zMv_Params_NAME, zMv_Params_Name,
     /* disablement strs */ NULL, NULL },

  {  /* entry idx, value */ 18, VALUE_OPT_MV_KEYS,
     /* equiv idx, value */ 18, VALUE_OPT_MV_KEYS,
     /* equivalenced to  */ NO_EQUIVALENT,
     /* min, max, act ct */ 0, 1, 0,
     /* opt state flags  */ MV_KEYS_FLAGS, 0,
     /* last opt argumnt */ { NULL },
     /* arg list/cookie  */ NULL,
     /* must/cannot opts */ NULL, NULL,
     /* option proc      */ optionNumericVal,
     /* desc, NAME, name */ zMv_KeysText, zMv_Keys_NAME, zMv_Keys_Name,
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
 *  Define the Ntp_Keygen Option Environment
 */
tSCC   zPROGNAME[]   = "NTP_KEYGEN";
tSCC   zUsageTitle[] =
"ntp-keygen (ntp) - Create a NTP host key - Ver. 4.2.4p5\n\
USAGE:  %s [ -<flag> [<val>] | --<name>[{=| }<val>] ]...\n";
tSCC   zRcName[]     = ".ntprc";
tSCC*  apzHomeList[] = {
       "$HOME",
       ".",
       NULL };

tSCC   zBugsAddr[]    = "http://bugs.ntp.isc.org, bugs@ntp.org";
#define zExplain NULL
tSCC    zDetail[]     = "\n\
If there is no new host key, look for an existing one.\n\
If one is not found, create it.\n";
tSCC    zFullVersion[] = NTP_KEYGEN_FULL_VERSION;
/* extracted from /usr/local/gnu/share/autogen/optcode.tpl near line 408 */

#if defined(ENABLE_NLS)
# define OPTPROC_BASE OPTPROC_TRANSLATE
  static tOptionXlateProc translate_option_strings;
#else
# define OPTPROC_BASE OPTPROC_NONE
# define translate_option_strings NULL
#endif /* ENABLE_NLS */

tOptions ntp_keygenOptions = {
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
    24 /* full option count */, 19 /* user option count */
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

#if ! defined(TEST_NTP_KEYGEN_OPTS)

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

#endif /* defined(TEST_NTP_KEYGEN_OPTS) */

/* * * * * * *
 *
 *   For the modulus option, when OPENSSL is #define-d.
 */
#ifdef OPENSSL
static void
doOptModulus(
    tOptions*   pOptions,
    tOptDesc*   pOptDesc )
{
    static const struct {const int rmin, rmax;} rng[ 1 ] = {
        { 256, 2048 } };
    int val;
    int ix;
    char const* pzIndent = "\t\t\t\t  ";
    extern FILE* option_usage_fp;

    if (pOptDesc == NULL) /* usage is requesting range list
                             option_usage_fp has already been set */
        goto emit_ranges;

    val = atoi( pOptDesc->optArg.argString );
    for (ix = 0; ix < 1; ix++) {
        if (val < rng[ix].rmin)
            continue;  /* ranges need not be ordered. */
        if (val == rng[ix].rmin)
            goto valid_return;
        if (rng[ix].rmax == INT_MIN)
            continue;
        if (val <= rng[ix].rmax)
            goto valid_return;
    }

    option_usage_fp = stderr;
    fprintf(stderr, _("%s error:  %s option value ``%s''is out of range.\n"),
            pOptions->pzProgName, pOptDesc->pz_Name, pOptDesc->optArg.argString);
    pzIndent = "\t";

  emit_ranges:
    fprintf( option_usage_fp, _("%sit must lie in the range: %d to %d\n"),
             pzIndent, rng[0].rmin, rng[0].rmax );
    if (pOptDesc == NULL)
        return;

    USAGE( EXIT_FAILURE );
    /* NOTREACHED */
    return;

  valid_return:
    pOptDesc->optArg.argInt = val;
}
#endif /* defined OPENSSL */

/* extracted from /usr/local/gnu/share/autogen/optmain.tpl near line 92 */

#if defined(TEST_NTP_KEYGEN_OPTS) /* TEST MAIN PROCEDURE: */

int
main( int argc, char** argv )
{
    int res = EXIT_SUCCESS;
    (void)optionProcess( &ntp_keygenOptions, argc, argv );
    {
        void optionPutShell( tOptions* );
        optionPutShell( &ntp_keygenOptions );
    }
    return res;
}
#endif  /* defined TEST_NTP_KEYGEN_OPTS */
/* extracted from /usr/local/gnu/share/autogen/optcode.tpl near line 514 */

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
  coerce_it((void*)&(ntp_keygenOptions._f))

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
        tOptDesc* pOD = ntp_keygenOptions.pOptDesc;
        int       ix  = ntp_keygenOptions.optCt;

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
/* ntp-keygen-opts.c ends here */
