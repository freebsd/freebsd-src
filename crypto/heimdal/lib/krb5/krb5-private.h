/* This is a generated file */
#ifndef __krb5_private_h__
#define __krb5_private_h__

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

void
_krb5_crc_init_table __P((void));

u_int32_t
_krb5_crc_update __P((
	const char *p,
	size_t len,
	u_int32_t res));

int
_krb5_extract_ticket __P((
	krb5_context context,
	krb5_kdc_rep *rep,
	krb5_creds *creds,
	krb5_keyblock *key,
	krb5_const_pointer keyseed,
	krb5_key_usage key_usage,
	krb5_addresses *addrs,
	unsigned nonce,
	krb5_boolean allow_server_mismatch,
	krb5_boolean ignore_cname,
	krb5_decrypt_proc decrypt_proc,
	krb5_const_pointer decryptarg));

ssize_t
_krb5_get_int __P((
	void *buffer,
	unsigned long *value,
	size_t size));

void
_krb5_n_fold __P((
	const void *str,
	size_t len,
	void *key,
	size_t size));

ssize_t
_krb5_put_int __P((
	void *buffer,
	unsigned long value,
	size_t size));

#endif /* __krb5_private_h__ */
