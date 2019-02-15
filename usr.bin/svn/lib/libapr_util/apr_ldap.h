/* $FreeBSD$ */
/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * apr_ldap.h is generated from apr_ldap.h.in by configure -- do not edit apr_ldap.h
 */
/**
 * @file apr_ldap.h
 * @brief  APR-UTIL LDAP 
 */
#ifndef APU_LDAP_H
#define APU_LDAP_H

/**
 * @defgroup APR_Util_LDAP LDAP
 * @ingroup APR_Util
 * @{
 */

/* this will be defined if LDAP support was compiled into apr-util */
#define APR_HAS_LDAP		  0

/* identify the LDAP toolkit used */
#define APR_HAS_NETSCAPE_LDAPSDK  0
#define APR_HAS_SOLARIS_LDAPSDK   0
#define APR_HAS_NOVELL_LDAPSDK    0
#define APR_HAS_MOZILLA_LDAPSDK   0
#define APR_HAS_OPENLDAP_LDAPSDK  0
#define APR_HAS_MICROSOFT_LDAPSDK 0
#define APR_HAS_ZOS_LDAPSDK       0
#define APR_HAS_OTHER_LDAPSDK     0


/*
 * Handle the case when LDAP is enabled
 */
#if APR_HAS_LDAP

/*
 * The following #defines are DEPRECATED and should not be used for
 * anything. They remain to maintain binary compatibility.
 * The original code defined the OPENLDAP SDK as present regardless
 * of what really was there, which was way bogus. In addition, the
 * apr_ldap_url_parse*() functions have been rewritten specifically for
 * APR, so the APR_HAS_LDAP_URL_PARSE macro is forced to zero.
 */
#define APR_HAS_LDAP_SSL 1
#define APR_HAS_LDAP_URL_PARSE      0

#if APR_HAS_OPENLDAP_LDAPSDK && !defined(LDAP_DEPRECATED) 
/* Ensure that the "deprecated" interfaces are still exposed
 * with OpenLDAP >= 2.3; these were exposed by default in earlier
 * releases. */
#define LDAP_DEPRECATED 1
#endif

/*
 * Include the standard LDAP header files.
 */






/*
 * Detected standard functions
 */
#define APR_HAS_LDAPSSL_CLIENT_INIT 0
#define APR_HAS_LDAPSSL_CLIENT_DEINIT 0
#define APR_HAS_LDAPSSL_ADD_TRUSTED_CERT 0
#define APR_HAS_LDAP_START_TLS_S 0
#define APR_HAS_LDAP_SSLINIT 0
#define APR_HAS_LDAPSSL_INIT 0
#define APR_HAS_LDAPSSL_INSTALL_ROUTINES 0

/*
 * Make sure the secure LDAP port is defined
 */
#ifndef LDAPS_PORT
#define LDAPS_PORT 636  /* ldaps:/// default LDAP over TLS port */
#endif

/*
 * For ldap function calls that input a size limit on the number of returned elements
 * Some SDKs do not have the define for LDAP_DEFAULT_LIMIT (-1) or LDAP_NO_LIMIT (0)
 */
#if APR_HAS_ZOS_LDAPSDK
#define APR_LDAP_SIZELIMIT LDAP_NO_LIMIT
#else
#ifdef LDAP_DEFAULT_LIMIT
#define APR_LDAP_SIZELIMIT LDAP_DEFAULT_LIMIT
#else
#define APR_LDAP_SIZELIMIT -1 /* equivalent to LDAP_DEFAULT_LIMIT */
#endif
#endif

/*
 * z/OS is missing some defines
 */
#ifndef LDAP_VERSION_MAX
#define LDAP_VERSION_MAX  LDAP_VERSION
#endif
#if APR_HAS_ZOS_LDAPSDK
#define LDAP_VENDOR_NAME "IBM z/OS"
#endif

/* Note: Macros defining const casting has been removed in APR v1.0,
 * pending real support for LDAP v2.0 toolkits.
 *
 * In the mean time, please use an LDAP v3.0 toolkit.
 */
#if LDAP_VERSION_MAX <= 2
#error Support for LDAP v2.0 toolkits has been removed from apr-util. Please use an LDAP v3.0 toolkit.
#endif 

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * This structure allows the C LDAP API error codes to be returned
 * along with plain text error messages that explain to us mere mortals
 * what really happened.
 */
typedef struct apr_ldap_err_t {
    const char *reason;
    const char *msg;
    int rc;
} apr_ldap_err_t;

#ifdef __cplusplus
}
#endif

#include "apr_ldap_url.h"
#include "apr_ldap_init.h"
#include "apr_ldap_option.h"

/** @} */
#endif /* APR_HAS_LDAP */
#endif /* APU_LDAP_H */
