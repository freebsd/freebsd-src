/* This file is in the public domain. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdbool.h>
#include <unistd.h>

#include <openssl/ssl.h>

static inline void
__SSLv3_dummy_method_impl(void)
{
	static const char warning[] = "SSLv3 use is deprecated.\n";
	static bool once = false;

	if (once)
		return;

	once = true;
	write(STDERR_FILENO, warning, sizeof(warning) - 1);
}

const SSL_METHOD *
__SSLv3_method_fbsd12(void)
{
	__SSLv3_dummy_method_impl();
	return (NULL);
}
__sym_compat(SSLv3_method, __SSLv3_method_fbsd12, OPENSSL_1_1_0);

const SSL_METHOD *
__SSLv3_client_method_fbsd12(void)
{
	__SSLv3_dummy_method_impl();
	return (NULL);
}
__sym_compat(SSLv3_client_method, __SSLv3_client_method_fbsd12, OPENSSL_1_1_0);

const SSL_METHOD *
__SSLv3_server_method_fbsd12(void)
{
	__SSLv3_dummy_method_impl();
	return (NULL);
}
__sym_compat(SSLv3_server_method, __SSLv3_server_method_fbsd12, OPENSSL_1_1_0);
