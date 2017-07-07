/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 1989,1990,1991,1992,1993,1994,1995,2000,2001,
 * 2003,2006,2007,2008,2009 by the Massachusetts Institute of Technology,
 * Cambridge, MA, USA.  All Rights Reserved.
 *
 * This software is being provided to you, the LICENSEE, by the
 * Massachusetts Institute of Technology (M.I.T.) under the following
 * license.  By obtaining, using and/or copying this software, you agree
 * that you have read, understood, and will comply with these terms and
 * conditions:
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify and distribute
 * this software and its documentation for any purpose and without fee or
 * royalty is hereby granted, provided that you agree to comply with the
 * following copyright notice and statements, including the disclaimer, and
 * that the same appear on ALL copies of the software and documentation,
 * including modifications that you make for internal use or for
 * distribution:
 *
 * THIS SOFTWARE IS PROVIDED "AS IS", AND M.I.T. MAKES NO REPRESENTATIONS
 * OR WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not
 * limitation, M.I.T. MAKES NO REPRESENTATIONS OR WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF
 * THE LICENSED SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY
 * PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
 *
 * The name of the Massachusetts Institute of Technology or M.I.T. may NOT
 * be used in advertising or publicity pertaining to distribution of the
 * software.  Title to copyright in this software and any associated
 * documentation shall at all times remain with M.I.T., and USER agrees to
 * preserve same.
 *
 * Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 */
/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * This prototype for k5-int.h (Krb5 internals include file)
 * includes the user-visible definitions from krb5.h and then
 * includes other definitions that are not user-visible but are
 * required for compiling Kerberos internal routines.
 *
 * John Gilmore, Cygnus Support, Sat Jan 21 22:45:52 PST 1995
 */

#ifndef _KRB5_INT_H
#define _KRB5_INT_H

#ifdef KRB5_GENERAL__
#error krb5.h included before k5-int.h
#endif /* KRB5_GENERAL__ */

#include "osconf.h"

#if defined(__MACH__) && defined(__APPLE__)
#       include <TargetConditionals.h>
#    if TARGET_RT_MAC_CFM
#       error "Use KfM 4.0 SDK headers for CFM compilation."
#    endif
#endif

/*
 * Begin "k5-config.h"
 */
#ifndef KRB5_CONFIG__
#define KRB5_CONFIG__

/*
 * Machine-type definitions: PC Clone 386 running Microloss Windows
 */

#if defined(_MSDOS) || defined(_WIN32)
#include "win-mac.h"

/* Kerberos Windows initialization file */
#define KERBEROS_INI    "kerberos.ini"
#define INI_FILES       "Files"
#define INI_KRB_CCACHE  "krb5cc"        /* Location of the ccache */
#define INI_KRB5_CONF   "krb5.ini"      /* Location of krb5.conf file */
#endif

#include "autoconf.h"

#ifndef KRB5_SYSTYPES__
#define KRB5_SYSTYPES__

#ifdef HAVE_SYS_TYPES_H         /* From autoconf.h */
#include <sys/types.h>
#else /* HAVE_SYS_TYPES_H */
typedef unsigned long   u_long;
typedef unsigned int    u_int;
typedef unsigned short  u_short;
typedef unsigned char   u_char;
#endif /* HAVE_SYS_TYPES_H */
#endif /* KRB5_SYSTYPES__ */


#include "k5-platform.h"

#define KRB5_KDB_MAX_LIFE       (60*60*24) /* one day */
#define KRB5_KDB_MAX_RLIFE      (60*60*24*7) /* one week */
#define KRB5_KDB_EXPIRATION     2145830400 /* Thu Jan  1 00:00:00 2038 UTC */

/*
 * Windows requires a different api interface to each function. Here
 * just define it as NULL.
 */
#ifndef KRB5_CALLCONV
#define KRB5_CALLCONV
#define KRB5_CALLCONV_C
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

/* #define KRB5_OLD_CRYPTO is done in krb5.h */

#endif /* KRB5_CONFIG__ */

/*
 * End "k5-config.h"
 */

/*
 * After loading the configuration definitions, load the Kerberos definitions.
 */
#include <errno.h>
#include "krb5.h"
#include <krb5/plugin.h>
#include "profile.h"

#include "port-sockets.h"
#include "socket-utils.h"

/* Get mutex support; currently used only for the replay cache.  */
#include "k5-thread.h"

/* Get error info support.  */
#include "k5-err.h"

/* Get string buffer support. */
#include "k5-buf.h"

/* Define tracing macros. */
#include "k5-trace.h"

/* Profile variables.  Constants are named KRB5_CONF_STRING, where STRING
 * matches the variable name.  Keep these alphabetized. */
#define KRB5_CONF_ACL_FILE                     "acl_file"
#define KRB5_CONF_ADMIN_SERVER                 "admin_server"
#define KRB5_CONF_ALLOW_WEAK_CRYPTO            "allow_weak_crypto"
#define KRB5_CONF_AP_REQ_CHECKSUM_TYPE         "ap_req_checksum_type"
#define KRB5_CONF_AUTH_TO_LOCAL                "auth_to_local"
#define KRB5_CONF_AUTH_TO_LOCAL_NAMES          "auth_to_local_names"
#define KRB5_CONF_CANONICALIZE                 "canonicalize"
#define KRB5_CONF_CCACHE_TYPE                  "ccache_type"
#define KRB5_CONF_CLOCKSKEW                    "clockskew"
#define KRB5_CONF_DATABASE_NAME                "database_name"
#define KRB5_CONF_DB_MODULE_DIR                "db_module_dir"
#define KRB5_CONF_DEBUG                        "debug"
#define KRB5_CONF_DEFAULT                      "default"
#define KRB5_CONF_DEFAULT_CCACHE_NAME          "default_ccache_name"
#define KRB5_CONF_DEFAULT_CLIENT_KEYTAB_NAME   "default_client_keytab_name"
#define KRB5_CONF_DEFAULT_DOMAIN               "default_domain"
#define KRB5_CONF_DEFAULT_KEYTAB_NAME          "default_keytab_name"
#define KRB5_CONF_DEFAULT_PRINCIPAL_EXPIRATION "default_principal_expiration"
#define KRB5_CONF_DEFAULT_PRINCIPAL_FLAGS      "default_principal_flags"
#define KRB5_CONF_DEFAULT_REALM                "default_realm"
#define KRB5_CONF_DEFAULT_TGS_ENCTYPES         "default_tgs_enctypes"
#define KRB5_CONF_DEFAULT_TKT_ENCTYPES         "default_tkt_enctypes"
#define KRB5_CONF_DES_CRC_SESSION_SUPPORTED    "des_crc_session_supported"
#define KRB5_CONF_DICT_FILE                    "dict_file"
#define KRB5_CONF_DISABLE                      "disable"
#define KRB5_CONF_DISABLE_LAST_SUCCESS         "disable_last_success"
#define KRB5_CONF_DISABLE_LOCKOUT              "disable_lockout"
#define KRB5_CONF_DNS_CANONICALIZE_HOSTNAME    "dns_canonicalize_hostname"
#define KRB5_CONF_DNS_FALLBACK                 "dns_fallback"
#define KRB5_CONF_DNS_LOOKUP_KDC               "dns_lookup_kdc"
#define KRB5_CONF_DNS_LOOKUP_REALM             "dns_lookup_realm"
#define KRB5_CONF_DNS_URI_LOOKUP               "dns_uri_lookup"
#define KRB5_CONF_DOMAIN_REALM                 "domain_realm"
#define KRB5_CONF_ENABLE_ONLY                  "enable_only"
#define KRB5_CONF_ERR_FMT                      "err_fmt"
#define KRB5_CONF_EXTRA_ADDRESSES              "extra_addresses"
#define KRB5_CONF_FORWARDABLE                  "forwardable"
#define KRB5_CONF_HOST_BASED_SERVICES          "host_based_services"
#define KRB5_CONF_HTTP_ANCHORS                 "http_anchors"
#define KRB5_CONF_IGNORE_ACCEPTOR_HOSTNAME     "ignore_acceptor_hostname"
#define KRB5_CONF_IPROP_ENABLE                 "iprop_enable"
#define KRB5_CONF_IPROP_LISTEN                 "iprop_listen"
#define KRB5_CONF_IPROP_LOGFILE                "iprop_logfile"
#define KRB5_CONF_IPROP_MASTER_ULOGSIZE        "iprop_master_ulogsize"
#define KRB5_CONF_IPROP_PORT                   "iprop_port"
#define KRB5_CONF_IPROP_RESYNC_TIMEOUT         "iprop_resync_timeout"
#define KRB5_CONF_IPROP_SLAVE_POLL             "iprop_slave_poll"
#define KRB5_CONF_K5LOGIN_AUTHORITATIVE        "k5login_authoritative"
#define KRB5_CONF_K5LOGIN_DIRECTORY            "k5login_directory"
#define KRB5_CONF_KADMIND_LISTEN               "kadmind_listen"
#define KRB5_CONF_KADMIND_PORT                 "kadmind_port"
#define KRB5_CONF_KCM_MACH_SERVICE             "kcm_mach_service"
#define KRB5_CONF_KCM_SOCKET                   "kcm_socket"
#define KRB5_CONF_KDC                          "kdc"
#define KRB5_CONF_KDCDEFAULTS                  "kdcdefaults"
#define KRB5_CONF_KDC_DEFAULT_OPTIONS          "kdc_default_options"
#define KRB5_CONF_KDC_LISTEN                   "kdc_listen"
#define KRB5_CONF_KDC_MAX_DGRAM_REPLY_SIZE     "kdc_max_dgram_reply_size"
#define KRB5_CONF_KDC_PORTS                    "kdc_ports"
#define KRB5_CONF_KDC_REQ_CHECKSUM_TYPE        "kdc_req_checksum_type"
#define KRB5_CONF_KDC_TCP_PORTS                "kdc_tcp_ports"
#define KRB5_CONF_KDC_TCP_LISTEN               "kdc_tcp_listen"
#define KRB5_CONF_KDC_TCP_LISTEN_BACKLOG       "kdc_tcp_listen_backlog"
#define KRB5_CONF_KDC_TIMESYNC                 "kdc_timesync"
#define KRB5_CONF_KEY_STASH_FILE               "key_stash_file"
#define KRB5_CONF_KPASSWD_LISTEN               "kpasswd_listen"
#define KRB5_CONF_KPASSWD_PORT                 "kpasswd_port"
#define KRB5_CONF_KPASSWD_SERVER               "kpasswd_server"
#define KRB5_CONF_KRB524_SERVER                "krb524_server"
#define KRB5_CONF_LDAP_CONNS_PER_SERVER        "ldap_conns_per_server"
#define KRB5_CONF_LDAP_KADMIND_DN              "ldap_kadmind_dn"
#define KRB5_CONF_LDAP_KADMIND_SASL_AUTHCID    "ldap_kadmind_sasl_authcid"
#define KRB5_CONF_LDAP_KADMIND_SASL_AUTHZID    "ldap_kadmind_sasl_authzid"
#define KRB5_CONF_LDAP_KADMIND_SASL_MECH       "ldap_kadmind_sasl_mech"
#define KRB5_CONF_LDAP_KADMIND_SASL_REALM      "ldap_kadmind_sasl_realm"
#define KRB5_CONF_LDAP_KDC_DN                  "ldap_kdc_dn"
#define KRB5_CONF_LDAP_KDC_SASL_AUTHCID        "ldap_kdc_sasl_authcid"
#define KRB5_CONF_LDAP_KDC_SASL_AUTHZID        "ldap_kdc_sasl_authzid"
#define KRB5_CONF_LDAP_KDC_SASL_MECH           "ldap_kdc_sasl_mech"
#define KRB5_CONF_LDAP_KDC_SASL_REALM          "ldap_kdc_sasl_realm"
#define KRB5_CONF_LDAP_KERBEROS_CONTAINER_DN   "ldap_kerberos_container_dn"
#define KRB5_CONF_LDAP_SERVERS                 "ldap_servers"
#define KRB5_CONF_LDAP_SERVICE_PASSWORD_FILE   "ldap_service_password_file"
#define KRB5_CONF_LIBDEFAULTS                  "libdefaults"
#define KRB5_CONF_LOGGING                      "logging"
#define KRB5_CONF_MASTER_KDC                   "master_kdc"
#define KRB5_CONF_MASTER_KEY_NAME              "master_key_name"
#define KRB5_CONF_MASTER_KEY_TYPE              "master_key_type"
#define KRB5_CONF_MAX_LIFE                     "max_life"
#define KRB5_CONF_MAX_RENEWABLE_LIFE           "max_renewable_life"
#define KRB5_CONF_MODULE                       "module"
#define KRB5_CONF_NOADDRESSES                  "noaddresses"
#define KRB5_CONF_NO_HOST_REFERRAL             "no_host_referral"
#define KRB5_CONF_PERMITTED_ENCTYPES           "permitted_enctypes"
#define KRB5_CONF_PLUGINS                      "plugins"
#define KRB5_CONF_PLUGIN_BASE_DIR              "plugin_base_dir"
#define KRB5_CONF_PREFERRED_PREAUTH_TYPES      "preferred_preauth_types"
#define KRB5_CONF_PROXIABLE                    "proxiable"
#define KRB5_CONF_RDNS                         "rdns"
#define KRB5_CONF_REALMS                       "realms"
#define KRB5_CONF_REALM_TRY_DOMAINS            "realm_try_domains"
#define KRB5_CONF_REJECT_BAD_TRANSIT           "reject_bad_transit"
#define KRB5_CONF_RENEW_LIFETIME               "renew_lifetime"
#define KRB5_CONF_RESTRICT_ANONYMOUS_TO_TGT    "restrict_anonymous_to_tgt"
#define KRB5_CONF_SAFE_CHECKSUM_TYPE           "safe_checksum_type"
#define KRB5_CONF_SUPPORTED_ENCTYPES           "supported_enctypes"
#define KRB5_CONF_TICKET_LIFETIME              "ticket_lifetime"
#define KRB5_CONF_UDP_PREFERENCE_LIMIT         "udp_preference_limit"
#define KRB5_CONF_UNLOCKITER                   "unlockiter"
#define KRB5_CONF_V4_INSTANCE_CONVERT          "v4_instance_convert"
#define KRB5_CONF_V4_REALM                     "v4_realm"
#define KRB5_CONF_VERIFY_AP_REQ_NOFAIL         "verify_ap_req_nofail"

/* Cache configuration variables */
#define KRB5_CC_CONF_FAST_AVAIL                "fast_avail"
#define KRB5_CC_CONF_PA_CONFIG_DATA            "pa_config_data"
#define KRB5_CC_CONF_PA_TYPE                   "pa_type"
#define KRB5_CC_CONF_PROXY_IMPERSONATOR        "proxy_impersonator"
#define KRB5_CC_CONF_REFRESH_TIME              "refresh_time"

/* Error codes used in KRB_ERROR protocol messages.
   Return values of library routines are based on a different error table
   (which allows non-ambiguous error codes between subsystems) */

/* KDC errors */
#define KDC_ERR_NONE                    0 /* No error */
#define KDC_ERR_NAME_EXP                1 /* Client's entry in DB expired */
#define KDC_ERR_SERVICE_EXP             2 /* Server's entry in DB expired */
#define KDC_ERR_BAD_PVNO                3 /* Requested pvno not supported */
#define KDC_ERR_C_OLD_MAST_KVNO         4 /* C's key encrypted in old master */
#define KDC_ERR_S_OLD_MAST_KVNO         5 /* S's key encrypted in old master */
#define KDC_ERR_C_PRINCIPAL_UNKNOWN     6 /* Client not found in Kerberos DB */
#define KDC_ERR_S_PRINCIPAL_UNKNOWN     7 /* Server not found in Kerberos DB */
#define KDC_ERR_PRINCIPAL_NOT_UNIQUE    8 /* Multiple entries in Kerberos DB */
#define KDC_ERR_NULL_KEY                9 /* The C or S has a null key */
#define KDC_ERR_CANNOT_POSTDATE         10 /* Tkt ineligible for postdating */
#define KDC_ERR_NEVER_VALID             11 /* Requested starttime > endtime */
#define KDC_ERR_POLICY                  12 /* KDC policy rejects request */
#define KDC_ERR_BADOPTION               13 /* KDC can't do requested opt. */
#define KDC_ERR_ENCTYPE_NOSUPP          14 /* No support for encryption type */
#define KDC_ERR_SUMTYPE_NOSUPP          15 /* No support for checksum type */
#define KDC_ERR_PADATA_TYPE_NOSUPP      16 /* No support for padata type */
#define KDC_ERR_TRTYPE_NOSUPP           17 /* No support for transited type */
#define KDC_ERR_CLIENT_REVOKED          18 /* C's creds have been revoked */
#define KDC_ERR_SERVICE_REVOKED         19 /* S's creds have been revoked */
#define KDC_ERR_TGT_REVOKED             20 /* TGT has been revoked */
#define KDC_ERR_CLIENT_NOTYET           21 /* C not yet valid */
#define KDC_ERR_SERVICE_NOTYET          22 /* S not yet valid */
#define KDC_ERR_KEY_EXP                 23 /* Password has expired */
#define KDC_ERR_PREAUTH_FAILED          24 /* Preauthentication failed */
#define KDC_ERR_PREAUTH_REQUIRED        25 /* Additional preauthentication */
                                           /* required */
#define KDC_ERR_SERVER_NOMATCH          26 /* Requested server and */
                                           /* ticket don't match*/
#define KDC_ERR_MUST_USE_USER2USER      27 /* Server principal valid for */
                                           /*   user2user only */
#define KDC_ERR_PATH_NOT_ACCEPTED       28 /* KDC policy rejected transited */
                                           /*   path */
#define KDC_ERR_SVC_UNAVAILABLE         29 /* A service is not
                                            * available that is
                                            * required to process the
                                            * request */
/* Application errors */
#define KRB_AP_ERR_BAD_INTEGRITY 31     /* Decrypt integrity check failed */
#define KRB_AP_ERR_TKT_EXPIRED  32      /* Ticket expired */
#define KRB_AP_ERR_TKT_NYV      33      /* Ticket not yet valid */
#define KRB_AP_ERR_REPEAT       34      /* Request is a replay */
#define KRB_AP_ERR_NOT_US       35      /* The ticket isn't for us */
#define KRB_AP_ERR_BADMATCH     36      /* Ticket/authenticator don't match */
#define KRB_AP_ERR_SKEW         37      /* Clock skew too great */
#define KRB_AP_ERR_BADADDR      38      /* Incorrect net address */
#define KRB_AP_ERR_BADVERSION   39      /* Protocol version mismatch */
#define KRB_AP_ERR_MSG_TYPE     40      /* Invalid message type */
#define KRB_AP_ERR_MODIFIED     41      /* Message stream modified */
#define KRB_AP_ERR_BADORDER     42      /* Message out of order */
#define KRB_AP_ERR_BADKEYVER    44      /* Key version is not available */
#define KRB_AP_ERR_NOKEY        45      /* Service key not available */
#define KRB_AP_ERR_MUT_FAIL     46      /* Mutual authentication failed */
#define KRB_AP_ERR_BADDIRECTION 47      /* Incorrect message direction */
#define KRB_AP_ERR_METHOD       48      /* Alternative authentication */
                                        /* method required */
#define KRB_AP_ERR_BADSEQ       49      /* Incorrect sequence numnber */
                                        /* in message */
#define KRB_AP_ERR_INAPP_CKSUM  50      /* Inappropriate type of */
                                        /* checksum in message */
#define KRB_AP_PATH_NOT_ACCEPTED 51     /* Policy rejects transited path */
#define KRB_ERR_RESPONSE_TOO_BIG 52     /* Response too big for UDP, */
                                        /*   retry with TCP */

/* other errors */
#define KRB_ERR_GENERIC         60      /* Generic error (description */
                                        /* in e-text) */
#define KRB_ERR_FIELD_TOOLONG   61      /* Field is too long for impl. */

/* PKINIT server-reported errors */
#define KDC_ERR_CLIENT_NOT_TRUSTED              62 /* client cert not trusted */
#define KDC_ERR_KDC_NOT_TRUSTED                 63
#define KDC_ERR_INVALID_SIG                     64 /* client signature verify failed */
#define KDC_ERR_DH_KEY_PARAMETERS_NOT_ACCEPTED  65 /* invalid Diffie-Hellman parameters */
#define KDC_ERR_CERTIFICATE_MISMATCH            66
#define KRB_AP_ERR_NO_TGT                       67
#define KDC_ERR_WRONG_REALM                     68
#define KRB_AP_ERR_USER_TO_USER_REQUIRED        69
#define KDC_ERR_CANT_VERIFY_CERTIFICATE         70 /* client cert not verifiable to */
                                                   /* trusted root cert */
#define KDC_ERR_INVALID_CERTIFICATE             71 /* client cert had invalid signature */
#define KDC_ERR_REVOKED_CERTIFICATE             72 /* client cert was revoked */
#define KDC_ERR_REVOCATION_STATUS_UNKNOWN       73 /* client cert revoked, reason unknown */
#define KDC_ERR_REVOCATION_STATUS_UNAVAILABLE   74
#define KDC_ERR_CLIENT_NAME_MISMATCH            75 /* mismatch between client cert and */
                                                   /* principal name */
#define KDC_ERR_INCONSISTENT_KEY_PURPOSE        77 /* bad extended key use */
#define KDC_ERR_DIGEST_IN_CERT_NOT_ACCEPTED     78 /* bad digest algorithm in client cert */
#define KDC_ERR_PA_CHECKSUM_MUST_BE_INCLUDED    79 /* missing paChecksum in PA-PK-AS-REQ */
#define KDC_ERR_DIGEST_IN_SIGNED_DATA_NOT_ACCEPTED 80 /* bad digest algorithm in SignedData */
#define KDC_ERR_PUBLIC_KEY_ENCRYPTION_NOT_SUPPORTED 81
#define KRB_AP_ERR_IAKERB_KDC_NOT_FOUND         85 /* The IAKERB proxy could
                                                      not find a KDC */
#define KRB_AP_ERR_IAKERB_KDC_NO_RESPONSE       86 /* The KDC did not respond
                                                      to the IAKERB proxy */
#define KDC_ERR_PREAUTH_EXPIRED                 90 /* RFC 6113 */
#define KDC_ERR_MORE_PREAUTH_DATA_REQUIRED      91 /* RFC 6113 */
#define KRB_ERR_MAX 127 /* err table base max offset for protocol err codes */

/*
 * A null-terminated array of this structure is returned by the KDC as
 * the data part of the ETYPE_INFO preauth type.  It informs the
 * client which encryption types are supported.
 * The  same data structure is used by both etype-info and etype-info2
 * but s2kparams must be null when encoding etype-info.
 */
typedef struct _krb5_etype_info_entry {
    krb5_magic      magic;
    krb5_enctype    etype;
    unsigned int    length;
    krb5_octet      *salt;
    krb5_data s2kparams;
} krb5_etype_info_entry;

/*
 *  This is essentially -1 without sign extension which can screw up
 *  comparisons on 64 bit machines. If the length is this value, then
 *  the salt data is not present. This is to distinguish between not
 *  being set and being of 0 length.
 */
#define KRB5_ETYPE_NO_SALT VALID_UINT_BITS

typedef krb5_etype_info_entry ** krb5_etype_info;

/* RFC 4537 */
typedef struct _krb5_etype_list {
    int             length;
    krb5_enctype    *etypes;
} krb5_etype_list;

/* sam_type values -- informational only */
#define PA_SAM_TYPE_ENIGMA     1   /*  Enigma Logic */
#define PA_SAM_TYPE_DIGI_PATH  2   /*  Digital Pathways */
#define PA_SAM_TYPE_SKEY_K0    3   /*  S/key where  KDC has key 0 */
#define PA_SAM_TYPE_SKEY       4   /*  Traditional S/Key */
#define PA_SAM_TYPE_SECURID    5   /*  Security Dynamics */
#define PA_SAM_TYPE_CRYPTOCARD 6   /*  CRYPTOCard */
#if 1 /* XXX need to figure out who has which numbers assigned */
#define PA_SAM_TYPE_ACTIVCARD_DEC  6   /*  ActivCard decimal mode */
#define PA_SAM_TYPE_ACTIVCARD_HEX  7   /*  ActivCard hex mode */
#define PA_SAM_TYPE_DIGI_PATH_HEX  8   /*  Digital Pathways hex mode */
#endif
#define PA_SAM_TYPE_EXP_BASE    128 /* experimental */
#define PA_SAM_TYPE_GRAIL               (PA_SAM_TYPE_EXP_BASE+0) /* testing */
#define PA_SAM_TYPE_SECURID_PREDICT     (PA_SAM_TYPE_EXP_BASE+1) /* special */

typedef struct _krb5_sam_challenge_2 {
    krb5_data       sam_challenge_2_body;
    krb5_checksum   **sam_cksum;            /* Array of checksums */
} krb5_sam_challenge_2;

typedef struct _krb5_sam_challenge_2_body {
    krb5_magic      magic;
    krb5_int32      sam_type; /* information */
    krb5_flags      sam_flags; /* KRB5_SAM_* values */
    krb5_data       sam_type_name;
    krb5_data       sam_track_id;
    krb5_data       sam_challenge_label;
    krb5_data       sam_challenge;
    krb5_data       sam_response_prompt;
    krb5_data       sam_pk_for_sad;
    krb5_int32      sam_nonce;
    krb5_enctype    sam_etype;
} krb5_sam_challenge_2_body;

typedef struct _krb5_sam_response_2 {
    krb5_magic      magic;
    krb5_int32      sam_type; /* informational */
    krb5_flags      sam_flags; /* KRB5_SAM_* values */
    krb5_data       sam_track_id; /* copied */
    krb5_enc_data   sam_enc_nonce_or_sad; /* krb5_enc_sam_response_enc */
    krb5_int32      sam_nonce;
} krb5_sam_response_2;

typedef struct _krb5_enc_sam_response_enc_2 {
    krb5_magic      magic;
    krb5_int32      sam_nonce;
    krb5_data       sam_sad;
} krb5_enc_sam_response_enc_2;

/*
 * Keep the pkinit definitions in a separate file so that the plugin
 * only has to include k5-int-pkinit.h rather than k5-int.h
 */

#include "k5-int-pkinit.h"

#define KRB5_OTP_FLAG_NEXTOTP        0x40000000
#define KRB5_OTP_FLAG_COMBINE        0x20000000
#define KRB5_OTP_FLAG_COLLECT_PIN    0x10000000
#define KRB5_OTP_FLAG_NO_COLLECT_PIN 0x08000000
#define KRB5_OTP_FLAG_ENCRYPT_NONCE  0x04000000
#define KRB5_OTP_FLAG_SEPARATE_PIN   0x02000000
#define KRB5_OTP_FLAG_CHECK_DIGIT    0x01000000

#define KRB5_OTP_FORMAT_DECIMAL      0x00000000
#define KRB5_OTP_FORMAT_HEXADECIMAL  0x00000001
#define KRB5_OTP_FORMAT_ALPHANUMERIC 0x00000002
#define KRB5_OTP_FORMAT_BINARY       0x00000003
#define KRB5_OTP_FORMAT_BASE64       0x00000004

typedef struct _krb5_otp_tokeninfo {
    krb5_flags flags;
    krb5_data vendor;
    krb5_data challenge;
    krb5_int32 length;          /* -1 for unspecified */
    krb5_int32 format;          /* -1 for unspecified */
    krb5_data token_id;
    krb5_data alg_id;
    krb5_algorithm_identifier **supported_hash_alg;
    krb5_int32 iteration_count; /* -1 for unspecified */
} krb5_otp_tokeninfo;

typedef struct _krb5_pa_otp_challenge {
    krb5_data nonce;
    krb5_data service;
    krb5_otp_tokeninfo **tokeninfo;
    krb5_data salt;
    krb5_data s2kparams;
} krb5_pa_otp_challenge;

typedef struct _krb5_pa_otp_req {
    krb5_int32 flags;
    krb5_data nonce;
    krb5_enc_data enc_data;
    krb5_algorithm_identifier *hash_alg;
    krb5_int32 iteration_count; /* -1 for unspecified */
    krb5_data otp_value;
    krb5_data pin;
    krb5_data challenge;
    krb5_timestamp time;
    krb5_data counter;
    krb5_int32 format;          /* -1 for unspecified */
    krb5_data token_id;
    krb5_data alg_id;
    krb5_data vendor;
} krb5_pa_otp_req;

typedef struct _krb5_kkdcp_message {
    krb5_data kerb_message;
    krb5_data target_domain;
    krb5_int32 dclocator_hint;
} krb5_kkdcp_message;

/* Plain text of an encrypted PA-FX-COOKIE value produced by the KDC. */
typedef struct _krb5_secure_cookie {
    time_t time;
    krb5_pa_data **data;
} krb5_secure_cookie;

#include <stdlib.h>
#include <string.h>

#ifndef HAVE_STRDUP
extern char *strdup (const char *);
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>                   /* struct stat, stat() */
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>                  /* MAXPATHLEN */
#endif

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>                   /* prototypes for file-related
                                           syscalls; flags for open &
                                           friends */
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <stdio.h>

#include "k5-gmt_mktime.h"

/* libos.spec */
krb5_error_code krb5_lock_file(krb5_context, int, int);
krb5_error_code krb5_unlock_file(krb5_context, int);
krb5_error_code krb5_sendto_kdc(krb5_context, const krb5_data *,
                                const krb5_data *, krb5_data *, int *, int);

krb5_error_code krb5int_init_context_kdc(krb5_context *);

struct derived_key {
    krb5_data constant;
    krb5_key dkey;
    struct derived_key *next;
};

/* Internal structure of an opaque key identifier */
struct krb5_key_st {
    krb5_keyblock keyblock;
    int refcount;
    struct derived_key *derived;
    /*
     * Cache of data private to the cipher implementation, which we
     * don't want to have to recompute for every operation.  This may
     * include key schedules, iteration counts, etc.
     *
     * The cipher implementation is responsible for setting this up
     * whenever needed, and the enc_provider key_cleanup method must
     * then be provided to dispose of it.
     */
    void *cache;
};

krb5_error_code
krb5int_arcfour_gsscrypt(const krb5_keyblock *keyblock, krb5_keyusage usage,
                         const krb5_data *kd_data, krb5_crypto_iov *data,
                         size_t num_data);

#define K5_SHA256_HASHLEN (256 / 8)

/* Write the SHA-256 hash of in to out. */
krb5_error_code
k5_sha256(const krb5_data *in, uint8_t out[K5_SHA256_HASHLEN]);

/*
 * Attempt to zero memory in a way that compilers won't optimize out.
 *
 * This mechanism should work even for heap storage about to be freed,
 * or automatic storage right before we return from a function.
 *
 * Then, even if we leak uninitialized memory someplace, or UNIX
 * "core" files get created with world-read access, some of the most
 * sensitive data in the process memory will already be safely wiped.
 *
 * We're not going so far -- yet -- as to try to protect key data that
 * may have been written into swap space....
 */
#ifdef _WIN32
# define zap(ptr, len) SecureZeroMemory(ptr, len)
#elif defined(__STDC_LIB_EXT1__)
/*
 * Use memset_s() which cannot be optimized out.  Avoid memset_s(NULL, 0, 0, 0)
 * which would cause a runtime constraint violation.
 */
static inline void zap(void *ptr, size_t len)
{
    if (len > 0)
        memset_s(ptr, len, 0, len);
}
#elif defined(__GNUC__) || defined(__clang__)
/*
 * Use an asm statement which declares a memory clobber to force the memset to
 * be carried out.  Avoid memset(NULL, 0, 0) which has undefined behavior.
 */
static inline void zap(void *ptr, size_t len)
{
    if (len > 0)
        memset(ptr, 0, len);
    __asm__ __volatile__("" : : "r" (ptr) : "memory");
}
#else
/*
 * Use a function from libkrb5support to defeat inlining unless link-time
 * optimization is used.  The function uses a volatile pointer, which prevents
 * current compilers from optimizing out the memset.
 */
# define zap(ptr, len) krb5int_zap(ptr, len)
#endif

/* Convenience function: zap and free ptr if it is non-NULL. */
static inline void
zapfree(void *ptr, size_t len)
{
    if (ptr != NULL) {
        zap(ptr, len);
        free(ptr);
    }
}

/* Convenience function: zap and free zero-terminated str if it is non-NULL. */
static inline void
zapfreestr(void *str)
{
    if (str != NULL) {
        zap(str, strlen((char *)str));
        free(str);
    }
}

/*
 * Combine two keys (normally used by the hardware preauth mechanism)
 */
krb5_error_code
krb5int_c_combine_keys(krb5_context context, krb5_keyblock *key1,
                       krb5_keyblock *key2, krb5_keyblock *outkey);

void krb5int_c_free_keyblock(krb5_context, krb5_keyblock *key);
void krb5int_c_free_keyblock_contents(krb5_context, krb5_keyblock *);
krb5_error_code krb5int_c_init_keyblock(krb5_context, krb5_enctype enctype,
                                        size_t length, krb5_keyblock **out);
krb5_error_code krb5int_c_copy_keyblock(krb5_context context,
                                        const krb5_keyblock *from,
                                        krb5_keyblock **to);
krb5_error_code krb5int_c_copy_keyblock_contents(krb5_context context,
                                                 const krb5_keyblock *from,
                                                 krb5_keyblock *to);

krb5_error_code krb5_crypto_us_timeofday(krb5_int32 *, krb5_int32 *);

/*
 * End "los-proto.h"
 */

typedef struct _krb5_os_context {
    krb5_magic              magic;
    krb5_int32              time_offset;
    krb5_int32              usec_offset;
    krb5_int32              os_flags;
    char *                  default_ccname;
} *krb5_os_context;

/*
 * Flags for the os_flags field
 *
 * KRB5_OS_TOFFSET_VALID means that the time offset fields are valid.
 * The intention is that this facility to correct the system clocks so
 * that they reflect the "real" time, for systems where for some
 * reason we can't set the system clock.  Instead we calculate the
 * offset between the system time and real time, and store the offset
 * in the os context so that we can correct the system clock as necessary.
 *
 * KRB5_OS_TOFFSET_TIME means that the time offset fields should be
 * returned as the time by the krb5 time routines.  This should only
 * be used for testing purposes (obviously!)
 */
#define KRB5_OS_TOFFSET_VALID   1
#define KRB5_OS_TOFFSET_TIME    2

/* lock mode flags */
#define KRB5_LOCKMODE_SHARED    0x0001
#define KRB5_LOCKMODE_EXCLUSIVE 0x0002
#define KRB5_LOCKMODE_DONTBLOCK 0x0004
#define KRB5_LOCKMODE_UNLOCK    0x0008

/*
 * Begin "preauth.h"
 *
 * (Originally written by Glen Machin at Sandia Labs.)
 */
/*
 * Sandia National Laboratories also makes no representations about the
 * suitability of the modifications, or additions to this software for
 * any purpose.  It is provided "as is" without express or implied warranty.
 */
#ifndef KRB5_PREAUTH__
#define KRB5_PREAUTH__

typedef struct _krb5_pa_enc_ts {
    krb5_timestamp      patimestamp;
    krb5_int32          pausec;
} krb5_pa_enc_ts;

typedef struct _krb5_pa_for_user {
    krb5_principal      user;
    krb5_checksum       cksum;
    krb5_data           auth_package;
} krb5_pa_for_user;

typedef struct _krb5_s4u_userid {
    krb5_int32          nonce;
    krb5_principal      user;
    krb5_data           subject_cert;
    krb5_flags          options;
} krb5_s4u_userid;

#define KRB5_S4U_OPTS_CHECK_LOGON_HOURS         0x40000000 /* check logon hour restrictions */
#define KRB5_S4U_OPTS_USE_REPLY_KEY_USAGE       0x20000000 /* sign with usage 27 instead of 26 */

typedef struct _krb5_pa_s4u_x509_user {
    krb5_s4u_userid     user_id;
    krb5_checksum       cksum;
} krb5_pa_s4u_x509_user;

enum {
    KRB5_FAST_ARMOR_AP_REQUEST = 0x1
};

typedef struct _krb5_fast_armor {
    krb5_int32 armor_type;
    krb5_data armor_value;
} krb5_fast_armor;
typedef struct _krb5_fast_armored_req {
    krb5_magic magic;
    krb5_fast_armor *armor;
    krb5_checksum req_checksum;
    krb5_enc_data enc_part;
} krb5_fast_armored_req;

typedef struct _krb5_fast_req {
    krb5_magic magic;
    krb5_flags fast_options;
    /* padata from req_body is used*/
    krb5_kdc_req *req_body;
} krb5_fast_req;

/* Bits 0-15 are critical in FAST options (RFC 6113 section 7.3). */
#define UNSUPPORTED_CRITICAL_FAST_OPTIONS   0xbfff0000
#define KRB5_FAST_OPTION_HIDE_CLIENT_NAMES  0x40000000

typedef struct _krb5_fast_finished {
    krb5_timestamp timestamp;
    krb5_int32 usec;
    krb5_principal client;
    krb5_checksum ticket_checksum;
} krb5_fast_finished;

typedef struct _krb5_fast_response {
    krb5_magic magic;
    krb5_pa_data **padata;
    krb5_keyblock *strengthen_key;
    krb5_fast_finished *finished;
    krb5_int32 nonce;
} krb5_fast_response;

typedef struct _krb5_ad_kdcissued {
    krb5_checksum ad_checksum;
    krb5_principal i_principal;
    krb5_authdata **elements;
} krb5_ad_kdcissued;

typedef struct _krb5_ad_signedpath_data {
    krb5_principal client;
    krb5_timestamp authtime;
    krb5_principal *delegated;
    krb5_pa_data **method_data;
    krb5_authdata **authorization_data;
} krb5_ad_signedpath_data;

typedef struct _krb5_ad_signedpath {
    krb5_enctype enctype;
    krb5_checksum checksum;
    krb5_principal *delegated;
    krb5_pa_data **method_data;
} krb5_ad_signedpath;

typedef struct _krb5_iakerb_header {
    krb5_data target_realm;
    krb5_data *cookie;
} krb5_iakerb_header;

typedef struct _krb5_iakerb_finished {
    krb5_checksum checksum;
} krb5_iakerb_finished;

typedef struct _krb5_verifier_mac {
    krb5_principal princ;
    krb5_kvno kvno;
    krb5_enctype enctype;
    krb5_checksum checksum;
} krb5_verifier_mac;

/*
 * AD-CAMMAC's other-verifiers field is a sequence of Verifier, which is an
 * extensible choice with only one selection, Verifier-MAC.  For the time being
 * we will represent this field directly as an array of krb5_verifier_mac.
 * That will have to change if other selections are added.
 */
typedef struct _krb5_cammac {
    krb5_authdata **elements;
    krb5_verifier_mac *kdc_verifier;
    krb5_verifier_mac *svc_verifier;
    krb5_verifier_mac **other_verifiers;
} krb5_cammac;

krb5_pa_data *
krb5int_find_pa_data(krb5_context, krb5_pa_data *const *, krb5_preauthtype);
/* Does not return a copy; original padata sequence responsible for freeing*/

void krb5_free_etype_info(krb5_context, krb5_etype_info);

#endif /* KRB5_PREAUTH__ */
/*
 * End "preauth.h"
 */

krb5_error_code
krb5int_copy_data_contents(krb5_context, const krb5_data *, krb5_data *);

krb5_error_code
krb5int_copy_data_contents_add0(krb5_context, const krb5_data *, krb5_data *);

void KRB5_CALLCONV
krb5_free_sam_challenge_2(krb5_context, krb5_sam_challenge_2 *);

void KRB5_CALLCONV
krb5_free_sam_challenge_2_body(krb5_context, krb5_sam_challenge_2_body *);

void KRB5_CALLCONV
krb5_free_sam_response_2(krb5_context, krb5_sam_response_2 *);

void KRB5_CALLCONV
krb5_free_enc_sam_response_enc_2(krb5_context, krb5_enc_sam_response_enc_2 *);

void KRB5_CALLCONV
krb5_free_sam_challenge_2_contents(krb5_context, krb5_sam_challenge_2 *);

void KRB5_CALLCONV
krb5_free_sam_challenge_2_body_contents(krb5_context,
                                        krb5_sam_challenge_2_body *);

void KRB5_CALLCONV
krb5_free_sam_response_2_contents(krb5_context, krb5_sam_response_2 *);

void KRB5_CALLCONV
krb5_free_enc_sam_response_enc_2_contents(krb5_context,
                                          krb5_enc_sam_response_enc_2 * );

void KRB5_CALLCONV
krb5_free_pa_enc_ts(krb5_context, krb5_pa_enc_ts *);

void KRB5_CALLCONV
krb5_free_pa_for_user(krb5_context, krb5_pa_for_user *);

void KRB5_CALLCONV
krb5_free_s4u_userid_contents(krb5_context, krb5_s4u_userid *);

void KRB5_CALLCONV
krb5_free_pa_s4u_x509_user(krb5_context, krb5_pa_s4u_x509_user *);

void KRB5_CALLCONV
krb5_free_pa_pac_req(krb5_context, krb5_pa_pac_req * );

void KRB5_CALLCONV krb5_free_fast_armor(krb5_context, krb5_fast_armor *);
void KRB5_CALLCONV krb5_free_fast_armored_req(krb5_context,
                                              krb5_fast_armored_req *);
void KRB5_CALLCONV krb5_free_fast_req(krb5_context, krb5_fast_req *);
void KRB5_CALLCONV krb5_free_fast_finished(krb5_context, krb5_fast_finished *);
void KRB5_CALLCONV krb5_free_fast_response(krb5_context, krb5_fast_response *);
void KRB5_CALLCONV krb5_free_ad_kdcissued(krb5_context, krb5_ad_kdcissued *);
void KRB5_CALLCONV krb5_free_ad_signedpath(krb5_context, krb5_ad_signedpath *);
void KRB5_CALLCONV krb5_free_iakerb_header(krb5_context, krb5_iakerb_header *);
void KRB5_CALLCONV krb5_free_iakerb_finished(krb5_context,
                                             krb5_iakerb_finished *);
void k5_free_algorithm_identifier(krb5_context context,
                                  krb5_algorithm_identifier *val);
void k5_free_otp_tokeninfo(krb5_context context, krb5_otp_tokeninfo *val);
void k5_free_pa_otp_challenge(krb5_context context,
                              krb5_pa_otp_challenge *val);
void k5_free_pa_otp_req(krb5_context context, krb5_pa_otp_req *val);
void k5_free_kkdcp_message(krb5_context context, krb5_kkdcp_message *val);
void k5_free_cammac(krb5_context context, krb5_cammac *val);
void k5_free_secure_cookie(krb5_context context, krb5_secure_cookie *val);

krb5_error_code
k5_unwrap_cammac_svc(krb5_context context, const krb5_authdata *ad,
                     const krb5_keyblock *key, krb5_authdata ***adata_out);
krb5_error_code
k5_authind_decode(const krb5_authdata *ad, krb5_data ***indicators);

/* #include "krb5/wordsize.h" -- comes in through base-defs.h. */
#include "com_err.h"
#include "k5-plugin.h"

#include <krb5/authdata_plugin.h>

struct _krb5_authdata_context {
    krb5_magic magic;
    int n_modules;
    struct _krb5_authdata_context_module {
        krb5_authdatatype ad_type;
        void *plugin_context;
        authdata_client_plugin_fini_proc client_fini;
        krb5_flags flags;
        krb5plugin_authdata_client_ftable_v0 *ftable;
        authdata_client_request_init_proc client_req_init;
        authdata_client_request_fini_proc client_req_fini;
        const char *name;
        void *request_context;
        void **request_context_pp;
    } *modules;
    struct plugin_dir_handle plugins;
};

typedef struct _krb5_authdata_context *krb5_authdata_context;

void
k5_free_data_ptr_list(krb5_data **list);

void
k5_zapfree_pa_data(krb5_pa_data **val);

void KRB5_CALLCONV
krb5int_free_data_list(krb5_context context, krb5_data *data);

krb5_error_code KRB5_CALLCONV
krb5_authdata_context_init(krb5_context kcontext,
                           krb5_authdata_context *pcontext);

void KRB5_CALLCONV
krb5_authdata_context_free(krb5_context kcontext,
                           krb5_authdata_context context);

krb5_error_code KRB5_CALLCONV
krb5_authdata_export_authdata(krb5_context kcontext,
                              krb5_authdata_context context, krb5_flags usage,
                              krb5_authdata ***pauthdata);

krb5_error_code KRB5_CALLCONV
krb5_authdata_get_attribute_types(krb5_context kcontext,
                                  krb5_authdata_context context,
                                  krb5_data **attrs);

krb5_error_code KRB5_CALLCONV
krb5_authdata_get_attribute(krb5_context kcontext,
                            krb5_authdata_context context,
                            const krb5_data *attribute,
                            krb5_boolean *authenticated,
                            krb5_boolean *complete, krb5_data *value,
                            krb5_data *display_value, int *more);

krb5_error_code KRB5_CALLCONV
krb5_authdata_set_attribute(krb5_context kcontext,
                            krb5_authdata_context context,
                            krb5_boolean complete, const krb5_data *attribute,
                            const krb5_data *value);

krb5_error_code KRB5_CALLCONV
krb5_authdata_delete_attribute(krb5_context kcontext,
                               krb5_authdata_context context,
                               const krb5_data *attribute);

krb5_error_code KRB5_CALLCONV
krb5_authdata_import_attributes(krb5_context kcontext,
                                krb5_authdata_context context,
                                krb5_flags usage, const krb5_data *attributes);

krb5_error_code KRB5_CALLCONV
krb5_authdata_export_attributes(krb5_context kcontext,
                                krb5_authdata_context context,
                                krb5_flags usage, krb5_data **pattributes);

krb5_error_code KRB5_CALLCONV
krb5_authdata_export_internal(krb5_context kcontext,
                              krb5_authdata_context context,
                              krb5_boolean restrict_authenticated,
                              const char *module, void **ptr);

krb5_error_code KRB5_CALLCONV
krb5_authdata_context_copy(krb5_context kcontext, krb5_authdata_context src,
                           krb5_authdata_context *dst);

krb5_error_code KRB5_CALLCONV
krb5_authdata_free_internal(krb5_context kcontext,
                            krb5_authdata_context context, const char *module,
                            void *ptr);

/*** Plugin framework ***/

/*
 * This framework can be used to create pluggable interfaces.  Not all existing
 * pluggable interface use this framework, but new ones should.  A new
 * pluggable interface entails:
 *
 * - An interface ID definition in the list of #defines below.
 *
 * - A name in the interface_names array in lib/krb5/krb/plugins.c.
 *
 * - An installed public header file in include/krb5.  The public header should
 *   include <krb5/plugin.h> and should declare a vtable structure for each
 *   supported major version of the interface.
 *
 * - A consumer API implementation, located within the code unit which makes
 *   use of the pluggable interface.  The consumer API should consist of:
 *
 *   . An interface-specific handle type which contains a vtable structure for
 *     the module (or a union of several such structures, if there are multiple
 *     supported major versions) and, optionally, resource data bound to the
 *     handle.
 *
 *   . An interface-specific loader function which creates a handle or list of
 *     handles.  A list of handles would be created if the interface is a
 *     one-to-many interface where the consumer wants to consult all available
 *     modules; a single handle would be created for an interface where the
 *     consumer wants to consult a specific module.  The loader function should
 *     use k5_plugin_load or k5_plugin_load_all to produce one or a list of
 *     vtable initializer functions, and should use those functions to fill in
 *     the vtable structure for the module (if necessary, trying each supported
 *     major version starting from the most recent).  The loader function can
 *     also bind resource data into the handle based on caller arguments, if
 *     appropriate.
 *
 *   . For each plugin method, a wrapper function which accepts a krb5_context,
 *     a plugin handle, and the method arguments.  Wrapper functions should
 *     invoke the method function contained in the handle's vtable.
 *
 * - Possibly, built-in implementations of the interface, also located within
 *   the code unit which makes use of the interface.  Built-in implementations
 *   must be registered with k5_plugin_register before the first call to
 *   k5_plugin_load or k5_plugin_load_all.
 *
 * A pluggable interface should have one or more currently supported major
 * versions, starting at 1.  Each major version should have a current minor
 * version, also starting at 1.  If new methods are added to a vtable, the
 * minor version should be incremented and the vtable stucture should document
 * where each minor vtable version ends.  If method signatures for a vtable are
 * changed, the major version should be incremented.
 *
 * Plugin module implementations (either built-in or dynamically loaded) should
 * define a function named <interfacename>_<modulename>_initvt, matching the
 * signature of krb5_plugin_initvt_fn as declared in include/krb5/plugin.h.
 * The initvt function should check the given maj_ver argument against its own
 * supported major versions, cast the vtable pointer to the appropriate
 * interface-specific vtable type, and fill in the vtable methods, stopping as
 * appropriate for the given min_ver.  Memory for the vtable structure is
 * allocated by the caller, not by the module.
 *
 * Dynamic plugin modules are registered with the framework through the
 * [plugins] section of the profile, as described in the admin documentation
 * and krb5.conf man page.
 */

struct plugin_mapping;

/* Holds krb5_context information about each pluggable interface. */
struct plugin_interface {
    struct plugin_mapping **modules;
    krb5_boolean configured;
};

/* A list of plugin interface IDs.  Make sure to increment
 * PLUGIN_NUM_INTERFACES when a new interface is added, and add an entry to the
 * interface_names table in lib/krb5/krb/plugin.c. */
#define PLUGIN_INTERFACE_PWQUAL      0
#define PLUGIN_INTERFACE_KADM5_HOOK  1
#define PLUGIN_INTERFACE_CLPREAUTH   2
#define PLUGIN_INTERFACE_KDCPREAUTH  3
#define PLUGIN_INTERFACE_CCSELECT    4
#define PLUGIN_INTERFACE_LOCALAUTH   5
#define PLUGIN_INTERFACE_HOSTREALM   6
#define PLUGIN_INTERFACE_AUDIT       7
#define PLUGIN_INTERFACE_TLS         8
#define PLUGIN_INTERFACE_KDCAUTHDATA 9
#define PLUGIN_NUM_INTERFACES        10

/* Retrieve the plugin module of type interface_id and name modname,
 * storing the result into module. */
krb5_error_code
k5_plugin_load(krb5_context context, int interface_id, const char *modname,
               krb5_plugin_initvt_fn *module);

/* Retrieve all plugin modules of type interface_id, storing the result
 * into modules.  Free the result with k5_plugin_free_handles. */
krb5_error_code
k5_plugin_load_all(krb5_context context, int interface_id,
                   krb5_plugin_initvt_fn **modules);

/* Release a module list allocated by k5_plugin_load_all. */
void
k5_plugin_free_modules(krb5_context context, krb5_plugin_initvt_fn *modules);

/* Register a plugin module of type interface_id and name modname. */
krb5_error_code
k5_plugin_register(krb5_context context, int interface_id, const char *modname,
                   krb5_plugin_initvt_fn module);

/*
 * Register a plugin module which is part of the krb5 tree but is built as a
 * dynamic plugin.  Look for the module in modsubdir relative to the
 * context->base_plugin_dir.
 */
krb5_error_code
k5_plugin_register_dyn(krb5_context context, int interface_id,
                       const char *modname, const char *modsubdir);

/* Destroy the module state within context; used by krb5_free_context. */
void
k5_plugin_free_context(krb5_context context);

struct _kdb5_dal_handle;        /* private, in kdb5.h */
typedef struct _kdb5_dal_handle kdb5_dal_handle;
struct _kdb_log_context;
typedef struct krb5_preauth_context_st krb5_preauth_context;
struct ccselect_module_handle;
struct localauth_module_handle;
struct hostrealm_module_handle;
struct k5_tls_vtable_st;
struct _krb5_context {
    krb5_magic      magic;
    krb5_enctype    *in_tkt_etypes;
    krb5_enctype    *tgs_etypes;
    struct _krb5_os_context os_context;
    char            *default_realm;
    profile_t       profile;
    kdb5_dal_handle *dal_handle;
    int             ser_ctx_count;
    void            *ser_ctx;
    /* allowable clock skew */
    krb5_deltat     clockskew;
    krb5_cksumtype  kdc_req_sumtype;
    krb5_cksumtype  default_ap_req_sumtype;
    krb5_cksumtype  default_safe_sumtype;
    krb5_flags      kdc_default_options;
    krb5_flags      library_options;
    krb5_boolean    profile_secure;
    int             fcc_default_format;
    krb5_prompt_type *prompt_types;
    /* Message size above which we'll try TCP first in send-to-kdc
       type code.  Aside from the 2**16 size limit, we put no
       absolute limit on the UDP packet size.  */
    int             udp_pref_limit;

    /* Use the config-file ktypes instead of app-specified?  */
    krb5_boolean    use_conf_ktypes;

    /* locate_kdc module stuff */
    struct plugin_dir_handle libkrb5_plugins;

    /* preauth module stuff */
    krb5_preauth_context *preauth_context;

    /* cache module stuff */
    struct ccselect_module_handle **ccselect_handles;

    /* localauth module stuff */
    struct localauth_module_handle **localauth_handles;

    /* hostrealm module stuff */
    struct hostrealm_module_handle **hostrealm_handles;

    /* TLS module vtable (if loaded) */
    struct k5_tls_vtable_st *tls;

    /* error detail info */
    struct errinfo err;
    char *err_fmt;

    /* For Sun iprop code; does this really have to be here?  */
    struct _kdb_log_context *kdblog_context;

    krb5_boolean allow_weak_crypto;
    krb5_boolean ignore_acceptor_hostname;
    krb5_boolean dns_canonicalize_hostname;

    krb5_trace_callback trace_callback;
    void *trace_callback_data;

    krb5_pre_send_fn kdc_send_hook;
    void *kdc_send_hook_data;

    krb5_post_recv_fn kdc_recv_hook;
    void *kdc_recv_hook_data;

    struct plugin_interface plugins[PLUGIN_NUM_INTERFACES];
    char *plugin_base_dir;
};

/* could be used in a table to find an etype and initialize a block */


#define KRB5_LIBOPT_SYNC_KDCTIME        0x0001

/* internal message representations */

typedef struct _krb5_safe {
    krb5_magic magic;
    krb5_data user_data;                /* user data */
    krb5_timestamp timestamp;           /* client time, optional */
    krb5_int32 usec;                    /* microsecond portion of time,
                                           optional */
    krb5_ui_4 seq_number;               /* sequence #, optional */
    krb5_address *s_address;    /* sender address */
    krb5_address *r_address;    /* recipient address, optional */
    krb5_checksum *checksum;    /* data integrity checksum */
} krb5_safe;

typedef struct _krb5_priv {
    krb5_magic magic;
    krb5_enc_data enc_part;             /* encrypted part */
} krb5_priv;

typedef struct _krb5_priv_enc_part {
    krb5_magic magic;
    krb5_data user_data;                /* user data */
    krb5_timestamp timestamp;           /* client time, optional */
    krb5_int32 usec;                    /* microsecond portion of time, opt. */
    krb5_ui_4 seq_number;               /* sequence #, optional */
    krb5_address *s_address;    /* sender address */
    krb5_address *r_address;    /* recipient address, optional */
} krb5_priv_enc_part;

void KRB5_CALLCONV krb5_free_safe(krb5_context, krb5_safe *);
void KRB5_CALLCONV krb5_free_priv(krb5_context, krb5_priv *);
void KRB5_CALLCONV krb5_free_priv_enc_part(krb5_context, krb5_priv_enc_part *);

/*
 * Begin "asn1.h"
 */
#ifndef KRB5_ASN1__
#define KRB5_ASN1__

/* ASN.1 encoding knowledge; KEEP IN SYNC WITH ASN.1 defs! */
/* here we use some knowledge of ASN.1 encodings */
/*
  Ticket is APPLICATION 1.
  Authenticator is APPLICATION 2.
  AS_REQ is APPLICATION 10.
  AS_REP is APPLICATION 11.
  TGS_REQ is APPLICATION 12.
  TGS_REP is APPLICATION 13.
  AP_REQ is APPLICATION 14.
  AP_REP is APPLICATION 15.
  KRB_SAFE is APPLICATION 20.
  KRB_PRIV is APPLICATION 21.
  KRB_CRED is APPLICATION 22.
  EncASRepPart is APPLICATION 25.
  EncTGSRepPart is APPLICATION 26.
  EncAPRepPart is APPLICATION 27.
  EncKrbPrivPart is APPLICATION 28.
  EncKrbCredPart is APPLICATION 29.
  KRB_ERROR is APPLICATION 30.
*/
/* allow either constructed or primitive encoding, so check for bit 6
   set or reset */
#define krb5int_is_app_tag(dat,tag)                     \
    ((dat != NULL) && (dat)->length &&                  \
     ((((dat)->data[0] & ~0x20) == ((tag) | 0x40))))
#define krb5_is_krb_ticket(dat)               krb5int_is_app_tag(dat, 1)
#define krb5_is_krb_authenticator(dat)        krb5int_is_app_tag(dat, 2)
#define krb5_is_as_req(dat)                   krb5int_is_app_tag(dat, 10)
#define krb5_is_as_rep(dat)                   krb5int_is_app_tag(dat, 11)
#define krb5_is_tgs_req(dat)                  krb5int_is_app_tag(dat, 12)
#define krb5_is_tgs_rep(dat)                  krb5int_is_app_tag(dat, 13)
#define krb5_is_ap_req(dat)                   krb5int_is_app_tag(dat, 14)
#define krb5_is_ap_rep(dat)                   krb5int_is_app_tag(dat, 15)
#define krb5_is_krb_safe(dat)                 krb5int_is_app_tag(dat, 20)
#define krb5_is_krb_priv(dat)                 krb5int_is_app_tag(dat, 21)
#define krb5_is_krb_cred(dat)                 krb5int_is_app_tag(dat, 22)
#define krb5_is_krb_enc_as_rep_part(dat)      krb5int_is_app_tag(dat, 25)
#define krb5_is_krb_enc_tgs_rep_part(dat)     krb5int_is_app_tag(dat, 26)
#define krb5_is_krb_enc_ap_rep_part(dat)      krb5int_is_app_tag(dat, 27)
#define krb5_is_krb_enc_krb_priv_part(dat)    krb5int_is_app_tag(dat, 28)
#define krb5_is_krb_enc_krb_cred_part(dat)    krb5int_is_app_tag(dat, 29)
#define krb5_is_krb_error(dat)                krb5int_is_app_tag(dat, 30)

/*************************************************************************
 * Prototypes for krb5_encode.c
 *************************************************************************/

/*
  krb5_error_code encode_krb5_structure(const krb5_structure *rep,
  krb5_data **code);
  modifies  *code
  effects   Returns the ASN.1 encoding of *rep in **code.
  Returns ASN1_MISSING_FIELD if a required field is emtpy in *rep.
  Returns ENOMEM if memory runs out.
*/

krb5_error_code
encode_krb5_authenticator(const krb5_authenticator *rep, krb5_data **code);

krb5_error_code
encode_krb5_ticket(const krb5_ticket *rep, krb5_data **code);

krb5_error_code
encode_krb5_enc_tkt_part(const krb5_enc_tkt_part *rep, krb5_data **code);

krb5_error_code
encode_krb5_enc_kdc_rep_part(const krb5_enc_kdc_rep_part *rep,
                             krb5_data **code);

/* yes, the translation is identical to that used for KDC__REP */
krb5_error_code
encode_krb5_as_rep(const krb5_kdc_rep *rep, krb5_data **code);

/* yes, the translation is identical to that used for KDC__REP */
krb5_error_code
encode_krb5_tgs_rep(const krb5_kdc_rep *rep, krb5_data **code);

krb5_error_code
encode_krb5_ap_req(const krb5_ap_req *rep, krb5_data **code);

krb5_error_code
encode_krb5_ap_rep(const krb5_ap_rep *rep, krb5_data **code);

krb5_error_code
encode_krb5_ap_rep_enc_part(const krb5_ap_rep_enc_part *rep, krb5_data **code);

krb5_error_code
encode_krb5_as_req(const krb5_kdc_req *rep, krb5_data **code);

krb5_error_code
encode_krb5_tgs_req(const krb5_kdc_req *rep, krb5_data **code);

krb5_error_code
encode_krb5_kdc_req_body(const krb5_kdc_req *rep, krb5_data **code);

krb5_error_code
encode_krb5_safe(const krb5_safe *rep, krb5_data **code);

struct krb5_safe_with_body {
    krb5_safe *safe;
    krb5_data *body;
};
krb5_error_code
encode_krb5_safe_with_body(const struct krb5_safe_with_body *rep,
                           krb5_data **code);

krb5_error_code
encode_krb5_priv(const krb5_priv *rep, krb5_data **code);

krb5_error_code
encode_krb5_enc_priv_part(const krb5_priv_enc_part *rep, krb5_data **code);

krb5_error_code
encode_krb5_cred(const krb5_cred *rep, krb5_data **code);
krb5_error_code
encode_krb5_checksum(const krb5_checksum *, krb5_data **);

krb5_error_code
encode_krb5_enc_cred_part(const krb5_cred_enc_part *rep, krb5_data **code);

krb5_error_code
encode_krb5_error(const krb5_error *rep, krb5_data **code);

krb5_error_code
encode_krb5_authdata(krb5_authdata *const *rep, krb5_data **code);

krb5_error_code
encode_krb5_padata_sequence(krb5_pa_data *const *rep, krb5_data **code);

krb5_error_code
encode_krb5_typed_data(krb5_pa_data *const *rep, krb5_data **code);

krb5_error_code
encode_krb5_etype_info(krb5_etype_info_entry *const *, krb5_data **code);

krb5_error_code
encode_krb5_etype_info2(krb5_etype_info_entry *const *, krb5_data **code);

krb5_error_code
encode_krb5_pa_enc_ts(const krb5_pa_enc_ts *, krb5_data **);

krb5_error_code
encode_krb5_sam_challenge_2(const krb5_sam_challenge_2 * , krb5_data **);

krb5_error_code
encode_krb5_sam_challenge_2_body(const krb5_sam_challenge_2_body *,
                                 krb5_data **);

krb5_error_code
encode_krb5_enc_sam_response_enc_2(const krb5_enc_sam_response_enc_2 *,
                                   krb5_data **);

krb5_error_code
encode_krb5_sam_response_2(const krb5_sam_response_2 * , krb5_data **);

struct krb5_setpw_req {
    krb5_principal target;
    krb5_data password;
};
krb5_error_code
encode_krb5_setpw_req(const struct krb5_setpw_req *rep, krb5_data **code);

krb5_error_code
encode_krb5_pa_for_user(const krb5_pa_for_user *, krb5_data **);

krb5_error_code
encode_krb5_s4u_userid(const krb5_s4u_userid *, krb5_data **);

krb5_error_code
encode_krb5_pa_s4u_x509_user(const krb5_pa_s4u_x509_user *, krb5_data **);

krb5_error_code
encode_krb5_pa_pac_req(const krb5_pa_pac_req *, krb5_data **);

krb5_error_code
encode_krb5_etype_list(const krb5_etype_list * , krb5_data **);

krb5_error_code
encode_krb5_pa_fx_fast_request(const krb5_fast_armored_req *, krb5_data **);

krb5_error_code
encode_krb5_fast_req(const krb5_fast_req *, krb5_data **);

krb5_error_code
encode_krb5_pa_fx_fast_reply(const krb5_enc_data *, krb5_data **);

krb5_error_code
encode_krb5_iakerb_header(const krb5_iakerb_header *, krb5_data **);

krb5_error_code
encode_krb5_iakerb_finished(const krb5_iakerb_finished *, krb5_data **);

krb5_error_code
encode_krb5_fast_response(const krb5_fast_response *, krb5_data **);

krb5_error_code
encode_krb5_ad_kdcissued(const krb5_ad_kdcissued *, krb5_data **);

krb5_error_code
encode_krb5_ad_signedpath(const krb5_ad_signedpath *, krb5_data **);

krb5_error_code
encode_krb5_ad_signedpath_data(const krb5_ad_signedpath_data *, krb5_data **);

krb5_error_code
encode_krb5_otp_tokeninfo(const krb5_otp_tokeninfo *, krb5_data **);

krb5_error_code
encode_krb5_pa_otp_challenge(const krb5_pa_otp_challenge *, krb5_data **);

krb5_error_code
encode_krb5_pa_otp_req(const krb5_pa_otp_req *, krb5_data **);

krb5_error_code
encode_krb5_pa_otp_enc_req(const krb5_data *, krb5_data **);

krb5_error_code
encode_krb5_kkdcp_message(const krb5_kkdcp_message *, krb5_data **);

krb5_error_code
encode_krb5_cammac(const krb5_cammac *, krb5_data **);

krb5_error_code
encode_utf8_strings(krb5_data *const *ut8fstrings, krb5_data **);

krb5_error_code
encode_krb5_secure_cookie(const krb5_secure_cookie *, krb5_data **);

/*************************************************************************
 * End of prototypes for krb5_encode.c
 *************************************************************************/

krb5_error_code
decode_krb5_sam_challenge_2(const krb5_data *, krb5_sam_challenge_2 **);

krb5_error_code
decode_krb5_sam_challenge_2_body(const krb5_data *,
                                 krb5_sam_challenge_2_body **);

krb5_error_code
decode_krb5_enc_sam_response_enc_2(const krb5_data *,
                                   krb5_enc_sam_response_enc_2 **);

krb5_error_code
decode_krb5_sam_response_2(const krb5_data *, krb5_sam_response_2 **);


/*************************************************************************
 * Prototypes for krb5_decode.c
 *************************************************************************/
/*
  krb5_error_code decode_krb5_structure(const krb5_data *code,
  krb5_structure **rep);

  requires  Expects **rep to not have been allocated;
  a new *rep is allocated regardless of the old value.
  effects   Decodes *code into **rep.
  Returns ENOMEM if memory is exhausted.
  Returns asn1 and krb5 errors.
*/

krb5_error_code
decode_krb5_authenticator(const krb5_data *code, krb5_authenticator **rep);

krb5_error_code
decode_krb5_ticket(const krb5_data *code, krb5_ticket **rep);

krb5_error_code
decode_krb5_encryption_key(const krb5_data *output, krb5_keyblock **rep);

krb5_error_code
decode_krb5_enc_tkt_part(const krb5_data *output, krb5_enc_tkt_part **rep);

krb5_error_code
decode_krb5_enc_kdc_rep_part(const krb5_data *output,
                             krb5_enc_kdc_rep_part **rep);

krb5_error_code
decode_krb5_as_rep(const krb5_data *output, krb5_kdc_rep **rep);

krb5_error_code
decode_krb5_tgs_rep(const krb5_data *output, krb5_kdc_rep **rep);

krb5_error_code
decode_krb5_ap_req(const krb5_data *output, krb5_ap_req **rep);

krb5_error_code
decode_krb5_ap_rep(const krb5_data *output, krb5_ap_rep **rep);

krb5_error_code
decode_krb5_ap_rep_enc_part(const krb5_data *output,
                            krb5_ap_rep_enc_part **rep);

krb5_error_code
decode_krb5_as_req(const krb5_data *output, krb5_kdc_req **rep);

krb5_error_code
decode_krb5_tgs_req(const krb5_data *output, krb5_kdc_req **rep);

krb5_error_code
decode_krb5_kdc_req_body(const krb5_data *output, krb5_kdc_req **rep);

krb5_error_code
decode_krb5_safe(const krb5_data *output, krb5_safe **rep);

krb5_error_code
decode_krb5_safe_with_body(const krb5_data *output, krb5_safe **rep,
                           krb5_data **body);

krb5_error_code
decode_krb5_priv(const krb5_data *output, krb5_priv **rep);

krb5_error_code
decode_krb5_enc_priv_part(const krb5_data *output, krb5_priv_enc_part **rep);
krb5_error_code
decode_krb5_checksum(const krb5_data *, krb5_checksum **);

krb5_error_code
decode_krb5_cred(const krb5_data *output, krb5_cred **rep);

krb5_error_code
decode_krb5_enc_cred_part(const krb5_data *output, krb5_cred_enc_part **rep);

krb5_error_code
decode_krb5_error(const krb5_data *output, krb5_error **rep);

krb5_error_code
decode_krb5_authdata(const krb5_data *output, krb5_authdata ***rep);

krb5_error_code
decode_krb5_padata_sequence(const krb5_data *output, krb5_pa_data ***rep);

krb5_error_code
decode_krb5_typed_data(const krb5_data *, krb5_pa_data ***);

krb5_error_code
decode_krb5_etype_info(const krb5_data *output, krb5_etype_info_entry ***rep);

krb5_error_code
decode_krb5_etype_info2(const krb5_data *output, krb5_etype_info_entry ***rep);

krb5_error_code
decode_krb5_enc_data(const krb5_data *output, krb5_enc_data **rep);

krb5_error_code
decode_krb5_pa_enc_ts(const krb5_data *output, krb5_pa_enc_ts **rep);

krb5_error_code
decode_krb5_setpw_req(const krb5_data *, krb5_data **, krb5_principal *);

krb5_error_code
decode_krb5_pa_for_user(const krb5_data *, krb5_pa_for_user **);

krb5_error_code
decode_krb5_pa_s4u_x509_user(const krb5_data *, krb5_pa_s4u_x509_user **);

krb5_error_code
decode_krb5_pa_pac_req(const krb5_data *, krb5_pa_pac_req **);

krb5_error_code
decode_krb5_etype_list(const krb5_data *, krb5_etype_list **);

krb5_error_code
decode_krb5_pa_fx_fast_request(const krb5_data *, krb5_fast_armored_req **);

krb5_error_code
decode_krb5_fast_req(const krb5_data *, krb5_fast_req **);

krb5_error_code
decode_krb5_pa_fx_fast_reply(const krb5_data *, krb5_enc_data **);

krb5_error_code
decode_krb5_fast_response(const krb5_data *, krb5_fast_response **);

krb5_error_code
decode_krb5_ad_kdcissued(const krb5_data *, krb5_ad_kdcissued **);

krb5_error_code
decode_krb5_ad_signedpath(const krb5_data *, krb5_ad_signedpath **);

krb5_error_code
decode_krb5_iakerb_header(const krb5_data *, krb5_iakerb_header **);

krb5_error_code
decode_krb5_iakerb_finished(const krb5_data *, krb5_iakerb_finished **);

krb5_error_code
decode_krb5_otp_tokeninfo(const krb5_data *, krb5_otp_tokeninfo **);

krb5_error_code
decode_krb5_pa_otp_challenge(const krb5_data *, krb5_pa_otp_challenge **);

krb5_error_code
decode_krb5_pa_otp_req(const krb5_data *, krb5_pa_otp_req **);

krb5_error_code
decode_krb5_pa_otp_enc_req(const krb5_data *, krb5_data **);

krb5_error_code
decode_krb5_kkdcp_message(const krb5_data *, krb5_kkdcp_message **);

krb5_error_code
decode_krb5_cammac(const krb5_data *, krb5_cammac **);

krb5_error_code
decode_utf8_strings(const krb5_data *, krb5_data ***);

krb5_error_code
decode_krb5_secure_cookie(const krb5_data *, krb5_secure_cookie **);

struct _krb5_key_data;          /* kdb.h */

struct ldap_seqof_key_data {
    krb5_int32 mkvno;           /* Master key version number */
    krb5_ui_2 kvno;             /* kvno of key_data elements (all the same) */
    struct _krb5_key_data *key_data;
    krb5_int16 n_key_data;
};
typedef struct ldap_seqof_key_data ldap_seqof_key_data;

krb5_error_code
krb5int_ldap_encode_sequence_of_keys(const ldap_seqof_key_data *val,
                                     krb5_data **code);

krb5_error_code
krb5int_ldap_decode_sequence_of_keys(const krb5_data *in,
                                     ldap_seqof_key_data **rep);

/*************************************************************************
 * End of prototypes for krb5_decode.c
 *************************************************************************/

#endif /* KRB5_ASN1__ */
/*
 * End "asn1.h"
 */


/*
 * Internal krb5 library routines
 */
krb5_error_code
krb5_encrypt_tkt_part(krb5_context, const krb5_keyblock *, krb5_ticket *);

krb5_error_code
krb5_encode_kdc_rep(krb5_context, krb5_msgtype, const krb5_enc_kdc_rep_part *,
                    int using_subkey, const krb5_keyblock *, krb5_kdc_rep *,
                    krb5_data ** );

/* Return true if s is non-empty and composed solely of digits. */
krb5_boolean
k5_is_string_numeric(const char *s);

krb5_error_code
k5_parse_host_string(const char *address, int default_port, char **host_out,
                     int *port_out);

/*
 * [De]Serialization Handle and operations.
 */
struct __krb5_serializer {
    krb5_magic          odtype;
    krb5_error_code     (*sizer) (krb5_context,
                                  krb5_pointer,
                                  size_t *);
    krb5_error_code     (*externalizer) (krb5_context,
                                         krb5_pointer,
                                         krb5_octet **,
                                         size_t *);
    krb5_error_code     (*internalizer) (krb5_context,
                                         krb5_pointer *,
                                         krb5_octet **,
                                         size_t *);
};
typedef const struct __krb5_serializer * krb5_ser_handle;
typedef struct __krb5_serializer krb5_ser_entry;

krb5_ser_handle krb5_find_serializer(krb5_context, krb5_magic);
krb5_error_code krb5_register_serializer(krb5_context, const krb5_ser_entry *);

/* Determine the external size of a particular opaque structure */
krb5_error_code KRB5_CALLCONV
krb5_size_opaque(krb5_context, krb5_magic, krb5_pointer, size_t *);

/* Serialize the structure into a buffer */
krb5_error_code KRB5_CALLCONV
krb5_externalize_opaque(krb5_context, krb5_magic, krb5_pointer, krb5_octet **,
                        size_t *);

/* Deserialize the structure from a buffer */
krb5_error_code KRB5_CALLCONV
krb5_internalize_opaque(krb5_context, krb5_magic, krb5_pointer *,
                        krb5_octet **, size_t *);

/* Serialize data into a buffer */
krb5_error_code
krb5_externalize_data(krb5_context, krb5_pointer, krb5_octet **, size_t *);
/*
 * Initialization routines.
 */

/* Initialize serialization for krb5_[os_]context */
krb5_error_code KRB5_CALLCONV krb5_ser_context_init(krb5_context);

/* Initialize serialization for krb5_auth_context */
krb5_error_code KRB5_CALLCONV krb5_ser_auth_context_init(krb5_context);

/* Initialize serialization for krb5_keytab */
krb5_error_code KRB5_CALLCONV krb5_ser_keytab_init(krb5_context);

/* Initialize serialization for krb5_ccache */
krb5_error_code KRB5_CALLCONV krb5_ser_ccache_init(krb5_context);

/* Initialize serialization for krb5_rcache */
krb5_error_code KRB5_CALLCONV krb5_ser_rcache_init(krb5_context);

/* [De]serialize 4-byte integer */
krb5_error_code KRB5_CALLCONV
krb5_ser_pack_int32(krb5_int32, krb5_octet **, size_t *);

krb5_error_code KRB5_CALLCONV
krb5_ser_unpack_int32(krb5_int32 *, krb5_octet **, size_t *);

/* [De]serialize 8-byte integer */
krb5_error_code KRB5_CALLCONV
krb5_ser_pack_int64(int64_t, krb5_octet **, size_t *);

krb5_error_code KRB5_CALLCONV
krb5_ser_unpack_int64(int64_t *, krb5_octet **, size_t *);

/* [De]serialize byte string */
krb5_error_code KRB5_CALLCONV
krb5_ser_pack_bytes(krb5_octet *, size_t, krb5_octet **, size_t *);

krb5_error_code KRB5_CALLCONV
krb5_ser_unpack_bytes(krb5_octet *, size_t, krb5_octet **, size_t *);

krb5_error_code KRB5_CALLCONV
krb5int_cc_default(krb5_context, krb5_ccache *);

/* Fill in the buffer with random alpha-numeric data. */
krb5_error_code
krb5int_random_string(krb5_context, char *string, unsigned int length);

/* value to use when requesting a keytab entry and KVNO doesn't matter */
#define IGNORE_VNO 0
/* value to use when requesting a keytab entry and enctype doesn't matter */
#define IGNORE_ENCTYPE 0

/* To keep happy libraries which are (for now) accessing internal stuff */

/* Make sure to increment by one when changing the struct */
#define KRB5INT_ACCESS_STRUCT_VERSION 21

typedef struct _krb5int_access {
    krb5_error_code (*auth_con_get_subkey_enctype)(krb5_context,
                                                   krb5_auth_context,
                                                   krb5_enctype *);

    krb5_error_code (*clean_hostname)(krb5_context, const char *, char *,
                                      size_t);

    krb5_error_code (*mandatory_cksumtype)(krb5_context, krb5_enctype,
                                           krb5_cksumtype *);
    krb5_error_code (KRB5_CALLCONV *ser_pack_int64)(int64_t, krb5_octet **,
                                                    size_t *);
    krb5_error_code (KRB5_CALLCONV *ser_unpack_int64)(int64_t *, krb5_octet **,
                                                      size_t *);

    /* Used for KDB LDAP back end.  */
    krb5_error_code
    (*asn1_ldap_encode_sequence_of_keys)(const ldap_seqof_key_data *val,
                                         krb5_data **code);

    krb5_error_code
    (*asn1_ldap_decode_sequence_of_keys)(const krb5_data *in,
                                         ldap_seqof_key_data **);

    /*
     * pkinit asn.1 encode/decode functions
     */
    krb5_error_code
    (*encode_krb5_auth_pack)(const krb5_auth_pack *rep, krb5_data **code);

    krb5_error_code
    (*encode_krb5_auth_pack_draft9)(const krb5_auth_pack_draft9 *rep,
                                    krb5_data **code);

    krb5_error_code
    (*encode_krb5_kdc_dh_key_info)(const krb5_kdc_dh_key_info *rep,
                                   krb5_data **code);

    krb5_error_code
    (*encode_krb5_pa_pk_as_rep)(const krb5_pa_pk_as_rep *rep,
                                krb5_data **code);

    krb5_error_code
    (*encode_krb5_pa_pk_as_rep_draft9)(const krb5_pa_pk_as_rep_draft9 *rep,
                                       krb5_data **code);

    krb5_error_code
    (*encode_krb5_pa_pk_as_req)(const krb5_pa_pk_as_req *rep,
                                krb5_data **code);

    krb5_error_code
    (*encode_krb5_pa_pk_as_req_draft9)(const krb5_pa_pk_as_req_draft9 *rep,
                                       krb5_data **code);

    krb5_error_code
    (*encode_krb5_reply_key_pack)(const krb5_reply_key_pack *,
                                  krb5_data **code);

    krb5_error_code
    (*encode_krb5_reply_key_pack_draft9)(const krb5_reply_key_pack_draft9 *,
                                         krb5_data **code);

    krb5_error_code
    (*encode_krb5_td_dh_parameters)(krb5_algorithm_identifier *const *,
                                    krb5_data **code);

    krb5_error_code
    (*encode_krb5_td_trusted_certifiers)(krb5_external_principal_identifier *
                                         const *, krb5_data **code);

    krb5_error_code
    (*decode_krb5_auth_pack)(const krb5_data *, krb5_auth_pack **);

    krb5_error_code
    (*decode_krb5_auth_pack_draft9)(const krb5_data *,
                                    krb5_auth_pack_draft9 **);

    krb5_error_code
    (*decode_krb5_pa_pk_as_req)(const krb5_data *, krb5_pa_pk_as_req **);

    krb5_error_code
    (*decode_krb5_pa_pk_as_req_draft9)(const krb5_data *,
                                       krb5_pa_pk_as_req_draft9 **);

    krb5_error_code
    (*decode_krb5_pa_pk_as_rep)(const krb5_data *, krb5_pa_pk_as_rep **);

    krb5_error_code
    (*decode_krb5_kdc_dh_key_info)(const krb5_data *, krb5_kdc_dh_key_info **);

    krb5_error_code
    (*decode_krb5_principal_name)(const krb5_data *, krb5_principal_data **);

    krb5_error_code
    (*decode_krb5_reply_key_pack)(const krb5_data *, krb5_reply_key_pack **);

    krb5_error_code
    (*decode_krb5_reply_key_pack_draft9)(const krb5_data *,
                                         krb5_reply_key_pack_draft9 **);

    krb5_error_code
    (*decode_krb5_td_dh_parameters)(const krb5_data *,
                                    krb5_algorithm_identifier ***);

    krb5_error_code
    (*decode_krb5_td_trusted_certifiers)(const krb5_data *,
                                         krb5_external_principal_identifier
                                         ***);

    krb5_error_code
    (*encode_krb5_kdc_req_body)(const krb5_kdc_req *rep, krb5_data **code);

    void
    (KRB5_CALLCONV *free_kdc_req)(krb5_context, krb5_kdc_req * );
    void
    (*set_prompt_types)(krb5_context, krb5_prompt_type *);
} krb5int_access;

#define KRB5INT_ACCESS_VERSION                                          \
    (((krb5_int32)((sizeof(krb5int_access) & 0xFFFF) |                  \
                   (KRB5INT_ACCESS_STRUCT_VERSION << 16))) & 0xFFFFFFFF)

krb5_error_code KRB5_CALLCONV
krb5int_accessor(krb5int_access*, krb5_int32);

typedef struct _krb5_donot_replay {
    krb5_magic magic;
    krb5_ui_4 hash;
    char *server;                       /* null-terminated */
    char *client;                       /* null-terminated */
    char *msghash;                      /* null-terminated */
    krb5_int32 cusec;
    krb5_timestamp ctime;
} krb5_donot_replay;

krb5_error_code KRB5_CALLCONV
krb5int_cc_user_set_default_name(krb5_context context, const char *name);

krb5_error_code krb5_rc_default(krb5_context, krb5_rcache *);
krb5_error_code krb5_rc_resolve_type(krb5_context, krb5_rcache *,
                                     const char *);
krb5_error_code krb5_rc_resolve_full(krb5_context, krb5_rcache *,
                                     const char *);
char *krb5_rc_get_type(krb5_context, krb5_rcache);
char *krb5_rc_default_type(krb5_context);
char *krb5_rc_default_name(krb5_context);
krb5_error_code krb5_auth_to_rep(krb5_context, krb5_tkt_authent *,
                                 krb5_donot_replay *);
krb5_error_code krb5_rc_hash_message(krb5_context context,
                                     const krb5_data *message, char **out);

krb5_error_code KRB5_CALLCONV
krb5_rc_initialize(krb5_context, krb5_rcache, krb5_deltat);

krb5_error_code KRB5_CALLCONV
krb5_rc_recover_or_initialize(krb5_context, krb5_rcache,krb5_deltat);

krb5_error_code KRB5_CALLCONV
krb5_rc_recover(krb5_context, krb5_rcache);

krb5_error_code KRB5_CALLCONV
krb5_rc_destroy(krb5_context, krb5_rcache);

krb5_error_code KRB5_CALLCONV
krb5_rc_close(krb5_context, krb5_rcache);

krb5_error_code KRB5_CALLCONV
krb5_rc_store(krb5_context, krb5_rcache, krb5_donot_replay *);

krb5_error_code KRB5_CALLCONV
krb5_rc_expunge(krb5_context, krb5_rcache);

krb5_error_code KRB5_CALLCONV
krb5_rc_get_lifespan(krb5_context, krb5_rcache,krb5_deltat *);

char *KRB5_CALLCONV
krb5_rc_get_name(krb5_context, krb5_rcache);

krb5_error_code KRB5_CALLCONV
krb5_rc_resolve(krb5_context, krb5_rcache, char *);

/*
 * This structure was exposed and used in macros in krb5 1.2, so do not
 * change its ABI.
 */
typedef struct _krb5_kt_ops {
    krb5_magic magic;
    char *prefix;

    /* routines always present */
    krb5_error_code (KRB5_CALLCONV *resolve)(krb5_context, const char *,
                                             krb5_keytab *);
    krb5_error_code (KRB5_CALLCONV *get_name)(krb5_context, krb5_keytab,
                                              char *, unsigned int);
    krb5_error_code (KRB5_CALLCONV *close)(krb5_context, krb5_keytab);
    krb5_error_code (KRB5_CALLCONV *get)(krb5_context, krb5_keytab,
                                         krb5_const_principal, krb5_kvno,
                                         krb5_enctype, krb5_keytab_entry *);
    krb5_error_code (KRB5_CALLCONV *start_seq_get)(krb5_context, krb5_keytab,
                                                   krb5_kt_cursor *);
    krb5_error_code (KRB5_CALLCONV *get_next)(krb5_context, krb5_keytab,
                                              krb5_keytab_entry *,
                                              krb5_kt_cursor *);
    krb5_error_code (KRB5_CALLCONV *end_get)(krb5_context, krb5_keytab,
                                             krb5_kt_cursor *);
    /* routines to be included on extended version (write routines) */
    krb5_error_code (KRB5_CALLCONV *add)(krb5_context, krb5_keytab,
                                         krb5_keytab_entry *);
    krb5_error_code (KRB5_CALLCONV *remove)(krb5_context, krb5_keytab,
                                            krb5_keytab_entry *);

    /* Handle for serializer */
    const krb5_ser_entry *serializer;
} krb5_kt_ops;

/* Not sure it's ready for exposure just yet.  */
extern krb5_error_code
krb5int_c_mandatory_cksumtype(krb5_context, krb5_enctype, krb5_cksumtype *);

/*
 * Referral definitions and subfunctions.
 */
#define        KRB5_REFERRAL_MAXHOPS    10

struct _krb5_kt {       /* should move into k5-int.h */
    krb5_magic magic;
    const struct _krb5_kt_ops *ops;
    krb5_pointer data;
};

krb5_error_code krb5_set_default_in_tkt_ktypes(krb5_context,
                                               const krb5_enctype *);

krb5_error_code krb5_get_default_in_tkt_ktypes(krb5_context, krb5_enctype **);

krb5_error_code krb5_set_default_tgs_ktypes(krb5_context,
                                            const krb5_enctype *);

krb5_error_code KRB5_CALLCONV
krb5_get_tgs_ktypes(krb5_context, krb5_const_principal, krb5_enctype **);

krb5_boolean krb5_is_permitted_enctype(krb5_context, krb5_enctype);

krb5_boolean KRB5_CALLCONV krb5int_c_weak_enctype(krb5_enctype);

krb5_error_code krb5_kdc_rep_decrypt_proc(krb5_context, const krb5_keyblock *,
                                          krb5_const_pointer, krb5_kdc_rep *);
krb5_error_code KRB5_CALLCONV krb5_decrypt_tkt_part(krb5_context,
                                                    const krb5_keyblock *,
                                                    krb5_ticket * );

krb5_error_code krb5_get_cred_via_tkt(krb5_context, krb5_creds *, krb5_flags,
                                      krb5_address *const *, krb5_creds *,
                                      krb5_creds **);

krb5_error_code KRB5_CALLCONV krb5_copy_addr(krb5_context,
                                             const krb5_address *,
                                             krb5_address **);

void krb5_init_ets(krb5_context);
void krb5_free_ets(krb5_context);
krb5_error_code krb5_generate_subkey(krb5_context, const krb5_keyblock *,
                                     krb5_keyblock **);
krb5_error_code krb5_generate_subkey_extended(krb5_context,
                                              const krb5_keyblock *,
                                              krb5_enctype, krb5_keyblock **);
krb5_error_code krb5_generate_seq_number(krb5_context, const krb5_keyblock *,
                                         krb5_ui_4 *);

krb5_error_code KRB5_CALLCONV krb5_kt_register(krb5_context,
                                               const struct _krb5_kt_ops *);

krb5_error_code k5_kt_get_principal(krb5_context context, krb5_keytab keytab,
                                    krb5_principal *princ_out);

krb5_error_code krb5_principal2salt_norealm(krb5_context, krb5_const_principal,
                                            krb5_data *);

unsigned int KRB5_CALLCONV krb5_get_notification_message(void);

/* chk_trans.c */
krb5_error_code krb5_check_transited_list(krb5_context, const krb5_data *trans,
                                          const krb5_data *realm1,
                                          const krb5_data *realm2);

/* free_rtree.c */
void krb5_free_realm_tree(krb5_context, krb5_principal *);

void KRB5_CALLCONV krb5_free_authenticator_contents(krb5_context,
                                                    krb5_authenticator *);

void KRB5_CALLCONV krb5_free_address(krb5_context, krb5_address *);

void KRB5_CALLCONV krb5_free_enc_tkt_part(krb5_context, krb5_enc_tkt_part *);

void KRB5_CALLCONV krb5_free_tickets(krb5_context, krb5_ticket **);
void KRB5_CALLCONV krb5_free_kdc_req(krb5_context, krb5_kdc_req *);
void KRB5_CALLCONV krb5_free_kdc_rep(krb5_context, krb5_kdc_rep *);
void KRB5_CALLCONV krb5_free_last_req(krb5_context, krb5_last_req_entry **);
void KRB5_CALLCONV krb5_free_enc_kdc_rep_part(krb5_context,
                                              krb5_enc_kdc_rep_part *);
void KRB5_CALLCONV krb5_free_ap_req(krb5_context, krb5_ap_req *);
void KRB5_CALLCONV krb5_free_ap_rep(krb5_context, krb5_ap_rep *);
void KRB5_CALLCONV krb5_free_cred(krb5_context, krb5_cred *);
void KRB5_CALLCONV krb5_free_cred_enc_part(krb5_context, krb5_cred_enc_part *);
void KRB5_CALLCONV krb5_free_pa_data(krb5_context, krb5_pa_data **);
void KRB5_CALLCONV krb5_free_tkt_authent(krb5_context, krb5_tkt_authent *);
void KRB5_CALLCONV krb5_free_enc_data(krb5_context, krb5_enc_data *);
krb5_error_code krb5_set_config_files(krb5_context, const char **);

krb5_error_code KRB5_CALLCONV krb5_get_default_config_files(char ***filenames);

void KRB5_CALLCONV krb5_free_config_files(char **filenames);

krb5_error_code krb5_rd_req_decoded(krb5_context, krb5_auth_context *,
                                    const krb5_ap_req *, krb5_const_principal,
                                    krb5_keytab, krb5_flags *, krb5_ticket **);

krb5_error_code krb5_rd_req_decoded_anyflag(krb5_context, krb5_auth_context *,
                                            const krb5_ap_req *,
                                            krb5_const_principal, krb5_keytab,
                                            krb5_flags *, krb5_ticket **);

krb5_error_code KRB5_CALLCONV
krb5_cc_register(krb5_context, const krb5_cc_ops *, krb5_boolean );

krb5_error_code krb5_walk_realm_tree(krb5_context, const krb5_data *,
                                     const krb5_data *, krb5_principal **,
                                     int);

krb5_error_code
krb5_auth_con_set_safe_cksumtype(krb5_context, krb5_auth_context,
                                 krb5_cksumtype);

krb5_error_code krb5_auth_con_setivector(krb5_context, krb5_auth_context,
                                         krb5_pointer);

krb5_error_code krb5_auth_con_getivector(krb5_context, krb5_auth_context,
                                         krb5_pointer *);

krb5_error_code krb5_auth_con_setpermetypes(krb5_context, krb5_auth_context,
                                            const krb5_enctype *);

krb5_error_code krb5_auth_con_getpermetypes(krb5_context, krb5_auth_context,
                                            krb5_enctype **);

krb5_error_code krb5_auth_con_get_subkey_enctype(krb5_context context,
                                                 krb5_auth_context,
                                                 krb5_enctype *);

krb5_error_code
krb5_auth_con_get_authdata_context(krb5_context context,
                                   krb5_auth_context auth_context,
                                   krb5_authdata_context *ad_context);

krb5_error_code
krb5_auth_con_set_authdata_context(krb5_context context,
                                   krb5_auth_context auth_context,
                                   krb5_authdata_context ad_context);

krb5_error_code krb5_read_message(krb5_context, krb5_pointer, krb5_data *);
krb5_error_code krb5_write_message(krb5_context, krb5_pointer, krb5_data *);
int krb5_net_read(krb5_context, int , char *, int);
int krb5_net_write(krb5_context, int , const char *, int);

krb5_error_code KRB5_CALLCONV krb5_get_realm_domain(krb5_context,
                                                    const char *, char ** );

krb5_error_code krb5_gen_portaddr(krb5_context, const krb5_address *,
                                  krb5_const_pointer, krb5_address **);

krb5_error_code krb5_gen_replay_name(krb5_context, const krb5_address *,
                                     const char *, char **);
krb5_error_code krb5_make_fulladdr(krb5_context, krb5_address *,
                                   krb5_address *, krb5_address *);

krb5_error_code krb5_set_debugging_time(krb5_context, krb5_timestamp,
                                        krb5_int32);
krb5_error_code krb5_use_natural_time(krb5_context);
krb5_error_code krb5_set_time_offsets(krb5_context, krb5_timestamp,
                                      krb5_int32);

/* Some data comparison and conversion functions.  */
static inline int
data_eq(krb5_data d1, krb5_data d2)
{
    return (d1.length == d2.length && (d1.length == 0 ||
                                       !memcmp(d1.data, d2.data, d1.length)));
}

static inline int
data_eq_string (krb5_data d, const char *s)
{
    return (d.length == strlen(s) && (d.length == 0 ||
                                      !memcmp(d.data, s, d.length)));
}

static inline krb5_data
make_data(void *data, unsigned int len)
{
    krb5_data d;

    d.magic = KV5M_DATA;
    d.data = (char *) data;
    d.length = len;
    return d;
}

static inline krb5_data
empty_data()
{
    return make_data(NULL, 0);
}

static inline krb5_data
string2data(char *str)
{
    return make_data(str, strlen(str));
}

static inline krb5_error_code
alloc_data(krb5_data *data, unsigned int len)
{
    /* Allocate at least one byte since zero-byte allocs may return NULL. */
    char *ptr = (char *) calloc((len > 0) ? len : 1, 1);

    if (ptr == NULL)
        return ENOMEM;
    data->magic = KV5M_DATA;
    data->data = ptr;
    data->length = len;
    return 0;
}

static inline int
authdata_eq(krb5_authdata a1, krb5_authdata a2)
{
    return (a1.ad_type == a2.ad_type && a1.length == a2.length &&
            (a1.length == 0 || !memcmp(a1.contents, a2.contents, a1.length)));
}

/* Allocate zeroed memory; set *code to 0 on success or ENOMEM on failure. */
static inline void *
k5calloc(size_t nmemb, size_t size, krb5_error_code *code)
{
    void *ptr;

    /* Allocate at least one byte since zero-byte allocs may return NULL. */
    ptr = calloc(nmemb ? nmemb : 1, size ? size : 1);
    *code = (ptr == NULL) ? ENOMEM : 0;
    return ptr;
}

/* Allocate zeroed memory; set *code to 0 on success or ENOMEM on failure. */
static inline void *
k5alloc(size_t size, krb5_error_code *code)
{
    return k5calloc(1, size, code);
}

/* Return a copy of the len bytes of memory at in; set *code to 0 or ENOMEM. */
static inline void *
k5memdup(const void *in, size_t len, krb5_error_code *code)
{
    void *ptr = k5alloc(len, code);

    if (ptr != NULL && len > 0)
        memcpy(ptr, in, len);
    return ptr;
}

/* Like k5memdup, but add a final null byte. */
static inline void *
k5memdup0(const void *in, size_t len, krb5_error_code *code)
{
    void *ptr = k5alloc(len + 1, code);

    if (ptr != NULL && len > 0)
        memcpy(ptr, in, len);
    return ptr;
}

krb5_error_code KRB5_CALLCONV
krb5_get_credentials_for_user(krb5_context context, krb5_flags options,
                              krb5_ccache ccache,
                              krb5_creds *in_creds,
                              krb5_data *cert,
                              krb5_creds **out_creds);

krb5_error_code KRB5_CALLCONV
krb5_get_credentials_for_proxy(krb5_context context,
                               krb5_flags options,
                               krb5_ccache ccache,
                               krb5_creds *in_creds,
                               krb5_ticket *evidence_tkt,
                               krb5_creds **out_creds);

krb5_error_code KRB5_CALLCONV
krb5int_get_authdata_containee_types(krb5_context context,
                                     const krb5_authdata *container,
                                     unsigned int *nad_types,
                                     krb5_authdatatype **ad_types);

krb5_error_code krb5int_parse_enctype_list(krb5_context context,
                                           const char *profkey, char *profstr,
                                           krb5_enctype *default_list,
                                           krb5_enctype **result);

krb5_boolean k5_etypes_contains(const krb5_enctype *list, krb5_enctype etype);

void k5_change_error_message_code(krb5_context ctx, krb5_error_code oldcode,
                                  krb5_error_code newcode);

/* Define shorter internal names for setting error messages. */
#define k5_setmsg krb5_set_error_message
#define k5_prependmsg krb5_prepend_error_message
#define k5_wrapmsg krb5_wrap_error_message

#endif /* _KRB5_INT_H */
