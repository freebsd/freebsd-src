/* This is a generated file */
#ifndef __hdb_protos_h__
#define __hdb_protos_h__

#include <stdarg.h>

krb5_error_code
hdb_add_master_key (
	krb5_context /*context*/,
	krb5_keyblock */*key*/,
	hdb_master_key */*inout*/);

krb5_error_code
hdb_check_db_format (
	krb5_context /*context*/,
	HDB */*db*/);

krb5_error_code
hdb_clear_master_key (
	krb5_context /*context*/,
	HDB */*db*/);

krb5_error_code
hdb_create (
	krb5_context /*context*/,
	HDB **/*db*/,
	const char */*filename*/);

krb5_error_code
hdb_db_create (
	krb5_context /*context*/,
	HDB **/*db*/,
	const char */*filename*/);

krb5_error_code
hdb_enctype2key (
	krb5_context /*context*/,
	hdb_entry */*e*/,
	krb5_enctype /*enctype*/,
	Key **/*key*/);

krb5_error_code
hdb_entry2string (
	krb5_context /*context*/,
	hdb_entry */*ent*/,
	char **/*str*/);

int
hdb_entry2value (
	krb5_context /*context*/,
	hdb_entry */*ent*/,
	krb5_data */*value*/);

krb5_error_code
hdb_foreach (
	krb5_context /*context*/,
	HDB */*db*/,
	unsigned /*flags*/,
	hdb_foreach_func_t /*func*/,
	void */*data*/);

void
hdb_free_entry (
	krb5_context /*context*/,
	hdb_entry */*ent*/);

void
hdb_free_key (Key */*key*/);

void
hdb_free_master_key (
	krb5_context /*context*/,
	hdb_master_key /*mkey*/);

krb5_error_code
hdb_init_db (
	krb5_context /*context*/,
	HDB */*db*/);

int
hdb_key2principal (
	krb5_context /*context*/,
	krb5_data */*key*/,
	krb5_principal /*p*/);

krb5_error_code
hdb_ldap_create (
	krb5_context /*context*/,
	HDB ** /*db*/,
	const char */*arg*/);

krb5_error_code
hdb_lock (
	int /*fd*/,
	int /*operation*/);

krb5_error_code
hdb_ndbm_create (
	krb5_context /*context*/,
	HDB **/*db*/,
	const char */*filename*/);

krb5_error_code
hdb_next_enctype2key (
	krb5_context /*context*/,
	const hdb_entry */*e*/,
	krb5_enctype /*enctype*/,
	Key **/*key*/);

int
hdb_principal2key (
	krb5_context /*context*/,
	krb5_principal /*p*/,
	krb5_data */*key*/);

krb5_error_code
hdb_print_entry (
	krb5_context /*context*/,
	HDB */*db*/,
	hdb_entry */*entry*/,
	void */*data*/);

krb5_error_code
hdb_process_master_key (
	krb5_context /*context*/,
	int /*kvno*/,
	krb5_keyblock */*key*/,
	krb5_enctype /*etype*/,
	hdb_master_key */*mkey*/);

krb5_error_code
hdb_read_master_key (
	krb5_context /*context*/,
	const char */*filename*/,
	hdb_master_key */*mkey*/);

krb5_error_code
hdb_seal_keys (
	krb5_context /*context*/,
	HDB */*db*/,
	hdb_entry */*ent*/);

krb5_error_code
hdb_seal_keys_mkey (
	krb5_context /*context*/,
	hdb_entry */*ent*/,
	hdb_master_key /*mkey*/);

krb5_error_code
hdb_set_master_key (
	krb5_context /*context*/,
	HDB */*db*/,
	krb5_keyblock */*key*/);

krb5_error_code
hdb_set_master_keyfile (
	krb5_context /*context*/,
	HDB */*db*/,
	const char */*keyfile*/);

krb5_error_code
hdb_unlock (int /*fd*/);

krb5_error_code
hdb_unseal_keys (
	krb5_context /*context*/,
	HDB */*db*/,
	hdb_entry */*ent*/);

krb5_error_code
hdb_unseal_keys_mkey (
	krb5_context /*context*/,
	hdb_entry */*ent*/,
	hdb_master_key /*mkey*/);

int
hdb_value2entry (
	krb5_context /*context*/,
	krb5_data */*value*/,
	hdb_entry */*ent*/);

krb5_error_code
hdb_write_master_key (
	krb5_context /*context*/,
	const char */*filename*/,
	hdb_master_key /*mkey*/);

#endif /* __hdb_protos_h__ */
