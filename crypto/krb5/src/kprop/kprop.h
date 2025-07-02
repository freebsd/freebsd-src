/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kprop/kprop.h */
/*
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#define KPROP_SERVICE_NAME "host"
#define TGT_SERVICE_NAME "krbtgt"
#define KPROP_SERVICE "krb5_prop"
#define KPROP_PORT 754

#define KPROP_PROT_VERSION "kprop5_01"

#define KPROP_BUFSIZ 32768
#define KPROP_DBSIZE_MAX_BUFSIZ 12  /* max length of an encoded DB size */

/* pathnames are in osconf.h, included via k5-int.h */

int sockaddr2krbaddr(krb5_context context, int family, struct sockaddr *sa,
                     krb5_address **dest);

krb5_error_code
sn2princ_realm(krb5_context context, const char *hostname, const char *sname,
               const char *realm, krb5_principal *princ_out);

/*
 * Encode size in four bytes (for backward compatibility) if it fits; otherwise
 * use the larger encoding.  buf must be allocated with at least
 * KPROP_DBSIZE_MAX_BUFSIZ bytes.
 */
void encode_database_size(uint64_t size, krb5_data *buf);

/* Decode a database size.  Return KRB5KRB_ERR_GENERIC if buf has an invalid
 * length or did not encode a 32-bit size compactly. */
krb5_error_code decode_database_size(const krb5_data *buf, uint64_t *size_out);
