#ifndef HMAC_H
#define HMAC_H

unsigned char *
hmac(
    EVP_MD *evp_md,
    unsigned int seqno,
    unsigned char *data, int datalen,
    unsigned char *key, int len);

#endif
