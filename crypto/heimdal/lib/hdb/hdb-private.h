/* This is a generated file */
#ifndef __hdb_private_h__
#define __hdb_private_h__

#include <stdarg.h>

krb5_error_code
_hdb_fetch (
	krb5_context /*context*/,
	HDB */*db*/,
	unsigned /*flags*/,
	hdb_entry */*entry*/);

krb5_error_code
_hdb_remove (
	krb5_context /*context*/,
	HDB */*db*/,
	hdb_entry */*entry*/);

krb5_error_code
_hdb_store (
	krb5_context /*context*/,
	HDB */*db*/,
	unsigned /*flags*/,
	hdb_entry */*entry*/);

#endif /* __hdb_private_h__ */
