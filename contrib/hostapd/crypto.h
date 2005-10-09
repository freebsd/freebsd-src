#ifndef CRYPTO_H
#define CRYPTO_H

void md4_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac);
void md4(const u8 *addr, size_t len, u8 *mac);
void des_encrypt(const u8 *clear, const u8 *key, u8 *cypher);

#endif /* CRYPTO_H */
