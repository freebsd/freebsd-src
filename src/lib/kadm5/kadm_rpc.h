/* -*- mode: c; c-file-style: "bsd"; indent-tabs-mode: t -*- */
#ifndef __KADM_RPC_H__
#define __KADM_RPC_H__

#include <gssrpc/types.h>

#include	<krb5.h>
#include	<kadm5/admin.h>

struct cprinc_arg {
	krb5_ui_4 api_version;
	kadm5_principal_ent_rec rec;
	long mask;
	char *passwd;
};
typedef struct cprinc_arg cprinc_arg;

struct cprinc3_arg {
	krb5_ui_4 api_version;
	kadm5_principal_ent_rec rec;
	long mask;
	int n_ks_tuple;
	krb5_key_salt_tuple *ks_tuple;
	char *passwd;
};
typedef struct cprinc3_arg cprinc3_arg;

struct generic_ret {
	krb5_ui_4 api_version;
	kadm5_ret_t code;
};
typedef struct generic_ret generic_ret;

struct dprinc_arg {
	krb5_ui_4 api_version;
	krb5_principal princ;
};
typedef struct dprinc_arg dprinc_arg;

struct mprinc_arg {
	krb5_ui_4 api_version;
	kadm5_principal_ent_rec rec;
	long mask;
};
typedef struct mprinc_arg mprinc_arg;

struct rprinc_arg {
	krb5_ui_4 api_version;
	krb5_principal src;
	krb5_principal dest;
};
typedef struct rprinc_arg rprinc_arg;

struct gprincs_arg {
	krb5_ui_4 api_version;
	char *exp;
};
typedef struct gprincs_arg gprincs_arg;

struct gprincs_ret {
	krb5_ui_4 api_version;
	kadm5_ret_t code;
	char **princs;
	int count;
};
typedef struct gprincs_ret gprincs_ret;

struct chpass_arg {
	krb5_ui_4 api_version;
	krb5_principal princ;
	char *pass;
};
typedef struct chpass_arg chpass_arg;

struct chpass3_arg {
	krb5_ui_4 api_version;
	krb5_principal princ;
	krb5_boolean keepold;
	int n_ks_tuple;
	krb5_key_salt_tuple *ks_tuple;
	char *pass;
};
typedef struct chpass3_arg chpass3_arg;

struct setkey_arg {
	krb5_ui_4 api_version;
	krb5_principal princ;
	krb5_keyblock *keyblocks;
	int n_keys;
};
typedef struct setkey_arg setkey_arg;

struct setkey3_arg {
	krb5_ui_4 api_version;
	krb5_principal princ;
	krb5_boolean keepold;
	int n_ks_tuple;
	krb5_key_salt_tuple *ks_tuple;
	krb5_keyblock *keyblocks;
	int n_keys;
};
typedef struct setkey3_arg setkey3_arg;

struct setkey4_arg {
	krb5_ui_4 api_version;
	krb5_principal princ;
	krb5_boolean keepold;
	kadm5_key_data *key_data;
	int n_key_data;
};
typedef struct setkey4_arg setkey4_arg;

struct chrand_arg {
	krb5_ui_4 api_version;
	krb5_principal princ;
};
typedef struct chrand_arg chrand_arg;

struct chrand3_arg {
	krb5_ui_4 api_version;
	krb5_principal princ;
	krb5_boolean keepold;
	int n_ks_tuple;
	krb5_key_salt_tuple *ks_tuple;
};
typedef struct chrand3_arg chrand3_arg;

struct chrand_ret {
	krb5_ui_4 api_version;
	kadm5_ret_t code;
	krb5_keyblock key;
	krb5_keyblock *keys;
	int n_keys;
};
typedef struct chrand_ret chrand_ret;

struct gprinc_arg {
	krb5_ui_4 api_version;
	krb5_principal princ;
	long mask;
};
typedef struct gprinc_arg gprinc_arg;

struct gprinc_ret {
	krb5_ui_4 api_version;
	kadm5_ret_t code;
	kadm5_principal_ent_rec rec;
};
typedef struct gprinc_ret gprinc_ret;

struct cpol_arg {
	krb5_ui_4 api_version;
	kadm5_policy_ent_rec rec;
	long mask;
};
typedef struct cpol_arg cpol_arg;

struct dpol_arg {
	krb5_ui_4 api_version;
	char *name;
};
typedef struct dpol_arg dpol_arg;

struct mpol_arg {
	krb5_ui_4 api_version;
	kadm5_policy_ent_rec rec;
	long mask;
};
typedef struct mpol_arg mpol_arg;

struct gpol_arg {
	krb5_ui_4 api_version;
	char *name;
};
typedef struct gpol_arg gpol_arg;

struct gpol_ret {
	krb5_ui_4 api_version;
	kadm5_ret_t code;
	kadm5_policy_ent_rec rec;
};
typedef struct gpol_ret gpol_ret;

struct gpols_arg {
	krb5_ui_4 api_version;
	char *exp;
};
typedef struct gpols_arg gpols_arg;

struct gpols_ret {
	krb5_ui_4 api_version;
	kadm5_ret_t code;
	char **pols;
	int count;
};
typedef struct gpols_ret gpols_ret;

struct getprivs_ret {
	krb5_ui_4 api_version;
	kadm5_ret_t code;
	long privs;
};
typedef struct getprivs_ret getprivs_ret;

struct purgekeys_arg {
	krb5_ui_4 api_version;
	krb5_principal princ;
	int keepkvno;
};
typedef struct purgekeys_arg purgekeys_arg;

struct gstrings_arg {
	krb5_ui_4 api_version;
	krb5_principal princ;
};
typedef struct gstrings_arg gstrings_arg;

struct gstrings_ret {
	krb5_ui_4 api_version;
	kadm5_ret_t code;
	krb5_string_attr *strings;
	int count;
};
typedef struct gstrings_ret gstrings_ret;

struct sstring_arg {
	krb5_ui_4 api_version;
	krb5_principal princ;
	char *key;
	char *value;
};
typedef struct sstring_arg sstring_arg;

struct getpkeys_arg {
	krb5_ui_4 api_version;
	krb5_principal princ;
	krb5_kvno kvno;
};
typedef struct getpkeys_arg getpkeys_arg;

struct getpkeys_ret {
	krb5_ui_4 api_version;
	kadm5_ret_t code;
	kadm5_key_data *key_data;
	int n_key_data;
};
typedef struct getpkeys_ret getpkeys_ret;

#define KADM 2112
#define KADMVERS 2
#define CREATE_PRINCIPAL 1
extern  enum clnt_stat create_principal_2(cprinc_arg *, generic_ret *,
					  CLIENT *);
extern  bool_t create_principal_2_svc(cprinc_arg *, generic_ret *,
				      struct svc_req *);
#define DELETE_PRINCIPAL 2
extern  enum clnt_stat delete_principal_2(dprinc_arg *, generic_ret *,
					  CLIENT *);
extern  bool_t delete_principal_2_svc(dprinc_arg *, generic_ret *,
				      struct svc_req *);
#define MODIFY_PRINCIPAL 3
extern  enum clnt_stat modify_principal_2(mprinc_arg *, generic_ret *,
					  CLIENT *);
extern  bool_t modify_principal_2_svc(mprinc_arg *, generic_ret *,
				      struct svc_req *);
#define RENAME_PRINCIPAL 4
extern  enum clnt_stat rename_principal_2(rprinc_arg *, generic_ret *,
					  CLIENT *);
extern  bool_t rename_principal_2_svc(rprinc_arg *, generic_ret *,
				      struct svc_req *);
#define GET_PRINCIPAL 5
extern  enum clnt_stat get_principal_2(gprinc_arg *, gprinc_ret *, CLIENT *);
extern  bool_t get_principal_2_svc(gprinc_arg *, gprinc_ret *,
				   struct svc_req *);
#define CHPASS_PRINCIPAL 6
extern  enum clnt_stat chpass_principal_2(chpass_arg *, generic_ret *,
					  CLIENT *);
extern  bool_t chpass_principal_2_svc(chpass_arg *, generic_ret *,
				      struct svc_req *);
#define CHRAND_PRINCIPAL 7
extern  enum clnt_stat chrand_principal_2(chrand_arg *, chrand_ret *,
					  CLIENT *);
extern  bool_t chrand_principal_2_svc(chrand_arg *, chrand_ret *,
				      struct svc_req *);
#define CREATE_POLICY 8
extern  enum clnt_stat create_policy_2(cpol_arg *, generic_ret *, CLIENT *);
extern  bool_t create_policy_2_svc(cpol_arg *, generic_ret *,
				   struct svc_req *);
#define DELETE_POLICY 9
extern  enum clnt_stat delete_policy_2(dpol_arg *, generic_ret *, CLIENT *);
extern  bool_t delete_policy_2_svc(dpol_arg *, generic_ret *,
				   struct svc_req *);
#define MODIFY_POLICY 10
extern  enum clnt_stat modify_policy_2(mpol_arg *, generic_ret *, CLIENT *);
extern  bool_t modify_policy_2_svc(mpol_arg *, generic_ret *,
				   struct svc_req *);
#define GET_POLICY 11
extern  enum clnt_stat get_policy_2(gpol_arg *, gpol_ret *, CLIENT *);
extern  bool_t get_policy_2_svc(gpol_arg *, gpol_ret *, struct svc_req *);
#define GET_PRIVS 12
extern  enum clnt_stat get_privs_2(void *, getprivs_ret *, CLIENT *);
extern  bool_t get_privs_2_svc(krb5_ui_4 *, getprivs_ret *, struct svc_req *);
#define INIT 13
extern  enum clnt_stat init_2(void *, generic_ret *, CLIENT *);
extern  bool_t init_2_svc(krb5_ui_4 *, generic_ret *, struct svc_req *);
#define GET_PRINCS 14
extern  enum clnt_stat get_princs_2(gprincs_arg *, gprincs_ret *, CLIENT *);
extern  bool_t get_princs_2_svc(gprincs_arg *, gprincs_ret *,
				struct svc_req *);
#define GET_POLS 15
extern  enum clnt_stat get_pols_2(gpols_arg *, gpols_ret *, CLIENT *);
extern  bool_t get_pols_2_svc(gpols_arg *, gpols_ret *, struct svc_req *);
#define SETKEY_PRINCIPAL 16
extern  enum clnt_stat setkey_principal_2(setkey_arg *, generic_ret *,
					  CLIENT *);
extern  bool_t setkey_principal_2_svc(setkey_arg *, generic_ret *,
				      struct svc_req *);

/* 17 was SETV4KEY_PRINCIPAL (removed in 1.18). */

#define CREATE_PRINCIPAL3 18
extern  enum clnt_stat create_principal3_2(cprinc3_arg *, generic_ret *,
					   CLIENT *);
extern  bool_t create_principal3_2_svc(cprinc3_arg *, generic_ret *,
				       struct svc_req *);
#define CHPASS_PRINCIPAL3 19
extern  enum clnt_stat chpass_principal3_2(chpass3_arg *, generic_ret *,
					   CLIENT *);
extern  bool_t chpass_principal3_2_svc(chpass3_arg *, generic_ret *,
				       struct svc_req *);
#define CHRAND_PRINCIPAL3 20
extern  enum clnt_stat chrand_principal3_2(chrand3_arg *, chrand_ret *,
					   CLIENT *);
extern  bool_t chrand_principal3_2_svc(chrand3_arg *, chrand_ret *,
				       struct svc_req *);
#define SETKEY_PRINCIPAL3 21
extern  enum clnt_stat setkey_principal3_2(setkey3_arg *, generic_ret *,
					   CLIENT *);
extern  bool_t setkey_principal3_2_svc(setkey3_arg *, generic_ret *,
				       struct svc_req *);
#define PURGEKEYS 22
extern  enum clnt_stat purgekeys_2(purgekeys_arg *, generic_ret *, CLIENT *);
extern  bool_t purgekeys_2_svc(purgekeys_arg *, generic_ret *,
			       struct svc_req *);
#define GET_STRINGS 23
extern  enum clnt_stat get_strings_2(gstrings_arg *, gstrings_ret *, CLIENT *);
extern  bool_t get_strings_2_svc(gstrings_arg *, gstrings_ret *,
				 struct svc_req *);
#define SET_STRING 24
extern  enum clnt_stat set_string_2(sstring_arg *, generic_ret *, CLIENT *);
extern  bool_t set_string_2_svc(sstring_arg *, generic_ret *,
				struct svc_req *);
#define SETKEY_PRINCIPAL4 25
extern  enum clnt_stat setkey_principal4_2(setkey4_arg *, generic_ret *,
					   CLIENT *);
extern  bool_t setkey_principal4_2_svc(setkey4_arg *, generic_ret *,
				       struct svc_req *);
#define EXTRACT_KEYS 26
extern enum clnt_stat get_principal_keys_2(getpkeys_arg *, getpkeys_ret *,
					   CLIENT *);
extern  bool_t get_principal_keys_2_svc(getpkeys_arg *, getpkeys_ret *,
					struct svc_req *);

extern bool_t xdr_cprinc_arg ();
extern bool_t xdr_cprinc3_arg ();
extern bool_t xdr_generic_ret ();
extern bool_t xdr_dprinc_arg ();
extern bool_t xdr_mprinc_arg ();
extern bool_t xdr_rprinc_arg ();
extern bool_t xdr_gprincs_arg ();
extern bool_t xdr_gprincs_ret ();
extern bool_t xdr_chpass_arg ();
extern bool_t xdr_chpass3_arg ();
extern bool_t xdr_setkey_arg ();
extern bool_t xdr_setkey3_arg ();
extern bool_t xdr_setkey4_arg ();
extern bool_t xdr_chrand_arg ();
extern bool_t xdr_chrand3_arg ();
extern bool_t xdr_chrand_ret ();
extern bool_t xdr_gprinc_arg ();
extern bool_t xdr_gprinc_ret ();
extern bool_t xdr_kadm5_ret_t ();
extern bool_t xdr_kadm5_principal_ent_rec ();
extern bool_t xdr_kadm5_policy_ent_rec ();
extern bool_t	xdr_krb5_keyblock ();
extern bool_t	xdr_krb5_principal ();
extern bool_t	xdr_krb5_enctype ();
extern bool_t	xdr_krb5_octet ();
extern bool_t	xdr_krb5_int32 ();
extern bool_t	xdr_u_int32 ();
extern bool_t xdr_cpol_arg ();
extern bool_t xdr_dpol_arg ();
extern bool_t xdr_mpol_arg ();
extern bool_t xdr_gpol_arg ();
extern bool_t xdr_gpol_ret ();
extern bool_t xdr_gpols_arg ();
extern bool_t xdr_gpols_ret ();
extern bool_t xdr_getprivs_ret ();
extern bool_t xdr_purgekeys_arg ();
extern bool_t xdr_gstrings_arg ();
extern bool_t xdr_gstrings_ret ();
extern bool_t xdr_sstring_arg ();
extern bool_t xdr_krb5_string_attr ();
extern bool_t xdr_kadm5_key_data ();
extern bool_t xdr_getpkeys_arg ();
extern bool_t xdr_getpkeys_ret ();

#endif /* __KADM_RPC_H__ */
