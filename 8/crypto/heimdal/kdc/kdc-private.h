/* This is a generated file */
#ifndef __kdc_private_h__
#define __kdc_private_h__

#include <stdarg.h>

krb5_error_code
_kdc_add_KRB5SignedPath (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	hdb_entry_ex */*krbtgt*/,
	krb5_enctype /*enctype*/,
	krb5_const_principal /*server*/,
	KRB5SignedPathPrincipals */*principals*/,
	EncTicketPart */*tkt*/);

krb5_error_code
_kdc_add_inital_verified_cas (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	pk_client_params */*params*/,
	EncTicketPart */*tkt*/);

krb5_error_code
_kdc_as_rep (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	KDC_REQ */*req*/,
	const krb5_data */*req_buffer*/,
	krb5_data */*reply*/,
	const char */*from*/,
	struct sockaddr */*from_addr*/,
	int /*datagram_reply*/);

krb5_boolean
_kdc_check_addresses (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	HostAddresses */*addresses*/,
	const struct sockaddr */*from*/);

krb5_error_code
_kdc_check_flags (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	hdb_entry_ex */*client_ex*/,
	const char */*client_name*/,
	hdb_entry_ex */*server_ex*/,
	const char */*server_name*/,
	krb5_boolean /*is_as_req*/);

krb5_error_code
_kdc_db_fetch (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	krb5_const_principal /*principal*/,
	unsigned /*flags*/,
	HDB **/*db*/,
	hdb_entry_ex **/*h*/);

krb5_error_code
_kdc_db_fetch4 (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	const char */*name*/,
	const char */*instance*/,
	const char */*realm*/,
	unsigned /*flags*/,
	hdb_entry_ex **/*ent*/);

krb5_error_code
_kdc_do_524 (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	const Ticket */*t*/,
	krb5_data */*reply*/,
	const char */*from*/,
	struct sockaddr */*addr*/);

krb5_error_code
_kdc_do_digest (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	const DigestREQ */*req*/,
	krb5_data */*reply*/,
	const char */*from*/,
	struct sockaddr */*addr*/);

krb5_error_code
_kdc_do_kaserver (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	unsigned char */*buf*/,
	size_t /*len*/,
	krb5_data */*reply*/,
	const char */*from*/,
	struct sockaddr_in */*addr*/);

krb5_error_code
_kdc_do_kx509 (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	const Kx509Request */*req*/,
	krb5_data */*reply*/,
	const char */*from*/,
	struct sockaddr */*addr*/);

krb5_error_code
_kdc_do_version4 (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	unsigned char */*buf*/,
	size_t /*len*/,
	krb5_data */*reply*/,
	const char */*from*/,
	struct sockaddr_in */*addr*/);

krb5_error_code
_kdc_encode_reply (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	KDC_REP */*rep*/,
	const EncTicketPart */*et*/,
	EncKDCRepPart */*ek*/,
	krb5_enctype /*etype*/,
	int /*skvno*/,
	const EncryptionKey */*skey*/,
	int /*ckvno*/,
	const EncryptionKey */*ckey*/,
	const char **/*e_text*/,
	krb5_data */*reply*/);

krb5_error_code
_kdc_encode_v4_ticket (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	void */*buf*/,
	size_t /*len*/,
	const EncTicketPart */*et*/,
	const PrincipalName */*service*/,
	size_t */*size*/);

krb5_error_code
_kdc_find_etype (
	krb5_context /*context*/,
	const hdb_entry_ex */*princ*/,
	krb5_enctype */*etypes*/,
	unsigned /*len*/,
	Key **/*ret_key*/,
	krb5_enctype */*ret_etype*/);

const PA_DATA*
_kdc_find_padata (
	const KDC_REQ */*req*/,
	int */*start*/,
	int /*type*/);

void
_kdc_fix_time (time_t **/*t*/);

void
_kdc_free_ent (
	krb5_context /*context*/,
	hdb_entry_ex */*ent*/);

krb5_error_code
_kdc_get_des_key (
	krb5_context /*context*/,
	hdb_entry_ex */*principal*/,
	krb5_boolean /*is_server*/,
	krb5_boolean /*prefer_afs_key*/,
	Key **/*ret_key*/);

krb5_error_code
_kdc_get_preferred_key (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	hdb_entry_ex */*h*/,
	const char */*name*/,
	krb5_enctype */*enctype*/,
	Key **/*key*/);

void
_kdc_log_timestamp (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	const char */*type*/,
	KerberosTime /*authtime*/,
	KerberosTime */*starttime*/,
	KerberosTime /*endtime*/,
	KerberosTime */*renew_till*/);

krb5_error_code
_kdc_make_anonymous_principalname (PrincipalName */*pn*/);

int
_kdc_maybe_version4 (
	unsigned char */*buf*/,
	int /*len*/);

krb5_error_code
_kdc_pac_generate (
	krb5_context /*context*/,
	hdb_entry_ex */*client*/,
	krb5_pac */*pac*/);

krb5_error_code
_kdc_pac_verify (
	krb5_context /*context*/,
	const krb5_principal /*client_principal*/,
	hdb_entry_ex */*client*/,
	hdb_entry_ex */*server*/,
	krb5_pac */*pac*/);

krb5_error_code
_kdc_pk_check_client (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	const hdb_entry_ex */*client*/,
	pk_client_params */*client_params*/,
	char **/*subject_name*/);

void
_kdc_pk_free_client_param (
	krb5_context /*context*/,
	pk_client_params */*client_params*/);

krb5_error_code
_kdc_pk_initialize (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	const char */*user_id*/,
	const char */*anchors*/,
	char **/*pool*/,
	char **/*revoke_list*/);

krb5_error_code
_kdc_pk_mk_pa_reply (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	pk_client_params */*client_params*/,
	const hdb_entry_ex */*client*/,
	const KDC_REQ */*req*/,
	const krb5_data */*req_buffer*/,
	krb5_keyblock **/*reply_key*/,
	METHOD_DATA */*md*/);

krb5_error_code
_kdc_pk_rd_padata (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	const KDC_REQ */*req*/,
	const PA_DATA */*pa*/,
	pk_client_params **/*ret_params*/);

krb5_error_code
_kdc_tgs_rep (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	KDC_REQ */*req*/,
	krb5_data */*data*/,
	const char */*from*/,
	struct sockaddr */*from_addr*/,
	int /*datagram_reply*/);

krb5_error_code
_kdc_tkt_add_if_relevant_ad (
	krb5_context /*context*/,
	EncTicketPart */*tkt*/,
	int /*type*/,
	const krb5_data */*data*/);

krb5_error_code
_kdc_try_kx509_request (
	void */*ptr*/,
	size_t /*len*/,
	Kx509Request */*req*/,
	size_t */*size*/);

krb5_error_code
_kdc_windc_client_access (
	krb5_context /*context*/,
	struct hdb_entry_ex */*client*/,
	KDC_REQ */*req*/);

#endif /* __kdc_private_h__ */
