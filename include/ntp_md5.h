/*
 * ntp_md5.h: deal with md5.h headers
 *
 * Use the system MD5 if available, otherwise libisc's.
 */
#if defined HAVE_MD5_H && defined HAVE_MD5INIT
# include <md5.h>
#else
# include "isc/md5.h"
  typedef isc_md5_t		MD5_CTX;
# define MD5Init(c)		isc_md5_init(c)
# define MD5Update(c, p, s)	isc_md5_update(c, p, s)
# define MD5Final(d, c)		isc_md5_final((c), (d))	/* swapped */
#endif

/*
 * Provide OpenSSL-alike MD5 API if we're not using OpenSSL
 */
#ifndef OPENSSL
  typedef MD5_CTX			EVP_MD_CTX;
# define EVP_get_digestbynid(t)		NULL
# define EVP_DigestInit(c, dt)		MD5Init(c)
# define EVP_DigestUpdate(c, p, s)	MD5Update(c, p, s)
# define EVP_DigestFinal(c, d, pdl)	\
	do {				\
		MD5Final((d), (c));	\
		*(pdl) = 16;		\
	} while (0)
#endif
