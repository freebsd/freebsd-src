#ifndef __crypto_headers_h__
#define __crypto_headers_h__

#include <openssl/evp.h>
#include <openssl/des.h>
#include <openssl/rc4.h>
#include <openssl/rc2.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/ui.h>
#include <openssl/rand.h>
#include <openssl/engine.h>
#include <openssl/pkcs12.h>
#include <openssl/pem.h>
#include <openssl/hmac.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/ecdh.h>
#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
#include <openssl/provider.h>
#include "fbsd_ossl_provider.h"
#endif

#endif /* __crypto_headers_h__ */
