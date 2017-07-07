#ifndef _PRINC_XDR_H
#define _PRINC_XDR_H 1

#include <sys/types.h>
#include <krb5.h>
#include <kdb.h>
#include <gssrpc/rpc.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif

#define OSA_ADB_PRINC_VERSION_1  0x12345C01
#define KADM5_XDR_FAILURE                        (43787575L)

typedef struct _osa_pw_hist_t {
  int n_key_data;
  krb5_key_data *key_data;
} osa_pw_hist_ent, *osa_pw_hist_t;

typedef struct _osa_princ_ent_t {
  int                         version;
  char                        *policy;
  long                        aux_attributes;
  unsigned int                old_key_len;
  unsigned int                old_key_next;
  krb5_kvno                   admin_history_kvno;
  osa_pw_hist_ent             *old_keys;
} osa_princ_ent_rec, *osa_princ_ent_t;

bool_t
ldap_xdr_krb5_ui_2(XDR *xdrs, krb5_ui_2 *objp);

bool_t
ldap_xdr_krb5_int16(XDR *xdrs, krb5_int16 *objp);

bool_t
ldap_xdr_nullstring(XDR *xdrs, char **objp);

bool_t
ldap_xdr_krb5_kvno(XDR *xdrs, krb5_kvno *objp);

bool_t
ldap_xdr_krb5_key_data(XDR *xdrs, krb5_key_data *objp);

bool_t
ldap_xdr_osa_pw_hist_ent(XDR *xdrs, osa_pw_hist_ent *objp);

bool_t
ldap_xdr_osa_princ_ent_rec(XDR *xdrs, osa_princ_ent_t objp);

void
ldap_osa_free_princ_ent(osa_princ_ent_t val);

krb5_error_code
krb5_lookup_tl_kadm_data(krb5_tl_data *tl_data, osa_princ_ent_rec *princ_entry);

krb5_error_code
krb5_update_tl_kadm_data(krb5_context context, krb5_db_entry *entry,
                         osa_princ_ent_rec *princ_entry);

#endif
