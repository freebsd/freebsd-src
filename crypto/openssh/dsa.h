#ifndef DSA_H
#define DSA_H

Key	*dsa_key_from_blob(char *blob, int blen);
int	dsa_make_key_blob(Key *key, unsigned char **blobp, unsigned int *lenp);

int
dsa_sign(
    Key *key,
    unsigned char **sigp, int *lenp,
    unsigned char *data, int datalen);

int
dsa_verify(
    Key *key,
    unsigned char *signature, int signaturelen,
    unsigned char *data, int datalen);

Key *
dsa_generate_key(unsigned int bits);

#endif
