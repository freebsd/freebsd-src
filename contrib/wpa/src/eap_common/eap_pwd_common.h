/*
 * EAP server/peer: EAP-pwd shared definitions
 * Copyright (c) 2009, Dan Harkins <dharkins@lounge.org>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef EAP_PWD_COMMON_H
#define EAP_PWD_COMMON_H

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>

/*
 * definition of a finite cyclic group
 * TODO: support one based on a prime field
 */
typedef struct group_definition_ {
	u16 group_num;
	EC_GROUP *group;
	EC_POINT *pwe;
	BIGNUM *order;
	BIGNUM *prime;
} EAP_PWD_group;

/*
 * EAP-pwd header, included on all payloads
 * L(1 bit) | M(1 bit) | exch(6 bits) | total_length(if L is set)
 */
#define EAP_PWD_HDR_SIZE                1

#define EAP_PWD_OPCODE_ID_EXCH          1
#define EAP_PWD_OPCODE_COMMIT_EXCH      2
#define EAP_PWD_OPCODE_CONFIRM_EXCH     3
#define EAP_PWD_GET_LENGTH_BIT(x)       ((x) & 0x80)
#define EAP_PWD_SET_LENGTH_BIT(x)       ((x) |= 0x80)
#define EAP_PWD_GET_MORE_BIT(x)         ((x) & 0x40)
#define EAP_PWD_SET_MORE_BIT(x)         ((x) |= 0x40)
#define EAP_PWD_GET_EXCHANGE(x)         ((x) & 0x3f)
#define EAP_PWD_SET_EXCHANGE(x,y)       ((x) |= (y))

/* EAP-pwd-ID payload */
struct eap_pwd_id {
	be16 group_num;
	u8 random_function;
#define EAP_PWD_DEFAULT_RAND_FUNC       1
	u8 prf;
#define EAP_PWD_DEFAULT_PRF             1
	u8 token[4];
	u8 prep;
#define EAP_PWD_PREP_NONE               0
#define EAP_PWD_PREP_MS                 1
	u8 identity[0];     /* length inferred from payload */
} STRUCT_PACKED;

/* common routines */
int compute_password_element(EAP_PWD_group *, u16, u8 *, int, u8 *, int, u8 *,
			     int, u8 *);
int compute_keys(EAP_PWD_group *, BN_CTX *, BIGNUM *, BIGNUM *, BIGNUM *,
		 u8 *, u8 *, u32 *, u8 *, u8 *, u8 *);
struct crypto_hash * eap_pwd_h_init(void);
void eap_pwd_h_update(struct crypto_hash *hash, const u8 *data, size_t len);
void eap_pwd_h_final(struct crypto_hash *hash, u8 *digest);

#endif  /* EAP_PWD_COMMON_H */
