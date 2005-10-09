#ifndef RC4_H
#define RC4_H

void rc4_skip(u8 *key, size_t keylen, size_t skip, u8 *data, size_t data_len);
void rc4(u8 *buf, size_t len, u8 *key, size_t key_len);

#endif /* RC4_H */
