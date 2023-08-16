#include <sys/cdefs.h>
#include <opencrypto/cbc_mac.h>
#include <opencrypto/xform_auth.h>

/* Authentication instances */
const struct auth_hash auth_hash_ccm_cbc_mac_128 = {
	.type = CRYPTO_AES_CCM_CBC_MAC,
	.name = "CBC-CCM-AES-128",
	.keysize = AES_128_CBC_MAC_KEY_LEN,
	.hashsize = AES_CBC_MAC_HASH_LEN,
	.ctxsize = sizeof(struct aes_cbc_mac_ctx),
	.blocksize = CCM_CBC_BLOCK_LEN,
	.Init = AES_CBC_MAC_Init,
	.Setkey = AES_CBC_MAC_Setkey,
	.Reinit = AES_CBC_MAC_Reinit,
	.Update = AES_CBC_MAC_Update,
	.Final = AES_CBC_MAC_Final,
};
const struct auth_hash auth_hash_ccm_cbc_mac_192 = {
	.type = CRYPTO_AES_CCM_CBC_MAC,
	.name = "CBC-CCM-AES-192",
	.keysize = AES_192_CBC_MAC_KEY_LEN,
	.hashsize = AES_CBC_MAC_HASH_LEN,
	.ctxsize = sizeof(struct aes_cbc_mac_ctx),
	.blocksize = CCM_CBC_BLOCK_LEN,
	.Init = AES_CBC_MAC_Init,
	.Setkey = AES_CBC_MAC_Setkey,
	.Reinit = AES_CBC_MAC_Reinit,
	.Update = AES_CBC_MAC_Update,
	.Final = AES_CBC_MAC_Final,
};
const struct auth_hash auth_hash_ccm_cbc_mac_256 = {
	.type = CRYPTO_AES_CCM_CBC_MAC,
	.name = "CBC-CCM-AES-256",
	.keysize = AES_256_CBC_MAC_KEY_LEN,
	.hashsize = AES_CBC_MAC_HASH_LEN,
	.ctxsize = sizeof(struct aes_cbc_mac_ctx),
	.blocksize = CCM_CBC_BLOCK_LEN,
	.Init = AES_CBC_MAC_Init,
	.Setkey = AES_CBC_MAC_Setkey,
	.Reinit = AES_CBC_MAC_Reinit,
	.Update = AES_CBC_MAC_Update,
	.Final = AES_CBC_MAC_Final,
};
