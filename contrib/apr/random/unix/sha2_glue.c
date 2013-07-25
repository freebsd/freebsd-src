#include <apr.h>
#include <apr_random.h>
#include <apr_pools.h>
#include "sha2.h"

static void sha256_init(apr_crypto_hash_t *h)
    {
    apr__SHA256_Init(h->data);
    }

static void sha256_add(apr_crypto_hash_t *h,const void *data,
			  apr_size_t bytes)
    {
    apr__SHA256_Update(h->data,data,bytes);
    }

static void sha256_finish(apr_crypto_hash_t *h,unsigned char *result)
    {
    apr__SHA256_Final(result,h->data);
    }

APR_DECLARE(apr_crypto_hash_t *) apr_crypto_sha256_new(apr_pool_t *p)
    {
    apr_crypto_hash_t *h=apr_palloc(p,sizeof *h);

    h->data=apr_palloc(p,sizeof(SHA256_CTX));
    h->init=sha256_init;
    h->add=sha256_add;
    h->finish=sha256_finish;
    h->size=256/8;

    return h;
    }
