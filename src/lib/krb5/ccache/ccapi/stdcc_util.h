/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* stdcc_util.h
 *
 * Frank Dabek, July 1998
 */

#if defined(_WIN32) || defined(USE_CCAPI)

#include "autoconf.h"

#if USE_CCAPI_V3
#include <CredentialsCache.h>
#else
#if defined(_WIN32)
#include "cacheapi.h"
#else
#include <CredentialsCache2.h>
#endif
#endif

#include "krb5.h"

/* protoypes for private functions declared in stdcc_util.c */
#ifdef USE_CCAPI_V3
krb5_error_code
copy_cc_cred_union_to_krb5_creds (krb5_context in_context,
                                  const cc_credentials_union *in_cred_union,
                                  krb5_creds *out_creds);
krb5_error_code
copy_krb5_creds_to_cc_cred_union (krb5_context in_context,
                                  krb5_creds *in_creds,
                                  cc_credentials_union **out_cred_union);

krb5_error_code
cred_union_release (cc_credentials_union *in_cred_union);
#else
int copyCCDataArrayToK5(cc_creds *cc, krb5_creds *kc, char whichArray);
int copyK5DataArrayToCC(krb5_creds *kc, cc_creds *cc, char whichArray);
void dupCCtoK5(krb5_context context, cc_creds *src, krb5_creds *dest);
void dupK5toCC(krb5_context context, krb5_creds *creds, cred_union **cu);
cc_int32 krb5int_free_cc_cred_union (cred_union** creds);
#endif
int stdccCredsMatch(krb5_context context, krb5_creds *base, krb5_creds *match, int whichfields);
int bitTst(int var, int mask);

#define kAddressArray 4
#define kAuthDataArray 5

#endif /* defined(_WIN32) || defined(USE_CCAPI) */
