/*
 * pam_krb5.h
 *
 * $Id: pam_krb5.h,v 1.5 1999/01/19 23:43:10 fcusack Exp $
 * $FreeBSD$
 */

int get_user_info(pam_handle_t *, char *, int, char **);
int verify_krb_v5_tgt(krb5_context, krb5_ccache, char *, int);
void cleanup_cache(pam_handle_t *, void *, int);

krb5_prompter_fct pam_prompter;

const char	*compat_princ_component(krb5_context, krb5_principal, int);
void		 compat_free_data_contents(krb5_context, krb5_data *);

#ifndef ENCTYPE_DES_CBC_MD5
#define ENCTYPE_DES_CBC_MD5	ETYPE_DES_CBC_MD5
#endif


