/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/rc_dfl.h */
/*
 * This file of the Kerberos V5 software is derived from public-domain code
 * contributed by Daniel J. Bernstein, <brnstnd@acf10.nyu.edu>.
 *
 */

/*
 * Declarations for the default replay cache implementation.
 */

#ifndef KRB5_RC_DFL_H
#define KRB5_RC_DFL_H

krb5_error_code KRB5_CALLCONV
krb5_rc_dfl_init(krb5_context, krb5_rcache, krb5_deltat);

krb5_error_code KRB5_CALLCONV
krb5_rc_dfl_recover(krb5_context, krb5_rcache);

krb5_error_code KRB5_CALLCONV
krb5_rc_dfl_recover_or_init(krb5_context, krb5_rcache, krb5_deltat);

krb5_error_code KRB5_CALLCONV
krb5_rc_dfl_destroy(krb5_context, krb5_rcache);

krb5_error_code KRB5_CALLCONV
krb5_rc_dfl_close(krb5_context, krb5_rcache);

krb5_error_code KRB5_CALLCONV
krb5_rc_dfl_store(krb5_context, krb5_rcache, krb5_donot_replay *);

krb5_error_code KRB5_CALLCONV
krb5_rc_dfl_expunge(krb5_context, krb5_rcache);

krb5_error_code KRB5_CALLCONV
krb5_rc_dfl_get_span(krb5_context, krb5_rcache, krb5_deltat *);

char * KRB5_CALLCONV
krb5_rc_dfl_get_name(krb5_context, krb5_rcache);

krb5_error_code KRB5_CALLCONV
krb5_rc_dfl_resolve(krb5_context, krb5_rcache, char *);

krb5_error_code krb5_rc_dfl_close_no_free(krb5_context, krb5_rcache);
void krb5_rc_free_entry(krb5_context, krb5_donot_replay **);
#endif
