/* This is a generated file */
#ifndef __hdb_private_h__
#define __hdb_private_h__

#ifdef __STDC__
#include <stdarg.h>
#ifndef __P
#define __P(x) x
#endif
#else
#ifndef __P
#define __P(x) ()
#endif
#endif

krb5_error_code
_hdb_fetch __P((
	krb5_context context,
	HDB *db,
	unsigned flags,
	hdb_entry *entry));

krb5_error_code
_hdb_remove __P((
	krb5_context context,
	HDB *db,
	hdb_entry *entry));

void
_hdb_seal_keys_int __P((
	hdb_entry *ent,
	int key_version,
	krb5_data schedule));

krb5_error_code
_hdb_store __P((
	krb5_context context,
	HDB *db,
	unsigned flags,
	hdb_entry *entry));

void
_hdb_unseal_keys_int __P((
	hdb_entry *ent,
	int key_version,
	krb5_data schedule));

#endif /* __hdb_private_h__ */
