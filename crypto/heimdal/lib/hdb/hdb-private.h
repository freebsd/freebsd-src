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

krb5_error_code
_hdb_store __P((
	krb5_context context,
	HDB *db,
	unsigned flags,
	hdb_entry *entry));

#endif /* __hdb_private_h__ */
