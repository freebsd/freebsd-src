/*
 * Implements Kerberos 4 authentication and ecryption
 */

#ifndef WINTEL_AUTH_H
#define WINTEL_AUTH_H

void auth_parse(kstream, unsigned char *, int);

int auth_init(kstream, kstream_ptr);

void auth_destroy(kstream);

int auth_encrypt(struct kstream_data_block *, struct kstream_data_block *,
		 kstream);

int auth_decrypt(struct kstream_data_block *, struct kstream_data_block *,
		 kstream);

extern BOOL forward_flag;
extern BOOL forwardable_flag;
extern BOOL forwarded_tickets;

#ifdef ENCRYPTION
extern BOOL encrypt_flag;
#endif

#endif /* WINTEL_AUTH_H */
