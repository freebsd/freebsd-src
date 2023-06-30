# $FreeBSD$

SHLIB_NAME?=	fips.so

CFLAGS+=	-DFIPS_MODULE

SRCS+=	fips_entry.c fipsprov.c self_test.c self_test_kats.c

.include "../../Makefile.common"

# crypto
SRCS+=	provider_core.c provider_predefined.c \
	core_fetch.c core_algorithm.c core_namemap.c self_test_core.c

SRCS+=	cpuid.c ctype.c
.if defined(ASM_aarch64)
SRCS+=	arm64cpuid.S armcap.c
ACFLAGS.arm64cpuid.S=	-march=armv8-a+crypto
.elif defined(ASM_amd64)
SRCS+=	x86_64cpuid.S
.elif defined(ASM_arm)
SRCS+=	armv4cpuid.S armcap.c
.elif defined(ASM_i386)
SRCS+=	x86cpuid.S
.elif defined(ASM_powerpc)
SRCS+=	ppccpuid.S ppccap.c
.elif defined(ASM_powerpc64)
SRCS+=	ppccpuid.S ppccap.c
.elif defined(ASM_powerpc64le)
SRCS+=	ppccpuid.S ppccap.c
.else
SRCS+=	mem_clr.c
.endif

# crypto/bn
SRCS+=	bn_add.c bn_div.c bn_exp.c bn_lib.c bn_ctx.c bn_mul.c \
	bn_mod.c bn_conv.c bn_rand.c bn_shift.c bn_word.c bn_blind.c \
	bn_kron.c bn_sqrt.c bn_gcd.c bn_prime.c bn_sqr.c \
	bn_recp.c bn_mont.c bn_mpi.c bn_exp2.c bn_gf2m.c bn_nist.c \
	bn_intern.c bn_dh.c bn_rsa_fips186_4.c bn_const.c
.if defined(ASM_aarch64)
SRCS+=	armv8-mont.S bn_asm.c
.elif defined(ASM_amd64)
SRCS+=	rsaz-avx2.S rsaz-avx512.S rsaz-x86_64.S rsaz_exp.c rsaz_exp_x2.c
SRCS+=	x86_64-gcc.c x86_64-gf2m.S x86_64-mont.S x86_64-mont5.S
.elif defined(ASM_arm)
SRCS+=	armv4-gf2m.S armv4-mont.S bn_asm.c
.elif defined(ASM_i386)
SRCS+=	bn-586.S co-586.S x86-gf2m.S x86-mont.S
.elif defined(ASM_powerpc)
SRCS+=	bn_ppc.c bn-ppc.S ppc-mont.S
.elif defined(ASM_powerpc64)
SRCS+=	bn_ppc.c bn-ppc.S ppc-mont.S
.elif defined(ASM_powerpc64le)
SRCS+=	bn_ppc.c bn-ppc.S ppc-mont.S
.else
SRCS+=	bn_asm.c
.endif

# crypto/dh
SRCS+=	dh_lib.c dh_key.c dh_group_params.c dh_check.c dh_backend.c dh_gen.c \
	dh_kdf.c

# crypto/dsa
SRCS+=	dsa_sign.c dsa_vrf.c dsa_lib.c dsa_ossl.c dsa_check.c \
	dsa_key.c dsa_backend.c dsa_gen.c

# crypto/ec
SRCS+=	ec_lib.c ecp_smpl.c ecp_mont.c ecp_nist.c ec_cvt.c ec_mult.c \
	ec_curve.c ec_check.c ec_key.c ec_kmeth.c ecx_key.c ec_asn1.c \
	ec2_smpl.c \
	ecp_oct.c ec2_oct.c ec_oct.c ecdh_ossl.c \
	ecdsa_ossl.c ecdsa_sign.c ecdsa_vrf.c curve25519.c \
	curve448/f_generic.c curve448/scalar.c \
	curve448/curve448_tables.c curve448/eddsa.c curve448/curve448.c \
	ec_backend.c ecx_backend.c ecdh_kdf.c curve448/arch_64/f_impl64.c \
	curve448/arch_32/f_impl32.c
SRCS+=	cryptlib.c params.c params_from_text.c bsearch.c ex_data.c o_str.c \
	threads_pthread.c threads_none.c initthread.c \
	context.c sparse_array.c asn1_dsa.c packet.c param_build.c \
	param_build_set.c der_writer.c threads_lib.c params_dup.c

.include <bsd.opts.mk>
.if ${MACHINE_ABI:Mlittle-endian} && ${MACHINE_ABI:Mlong64}
SRCS+=	ecp_nistp224.c ecp_nistp256.c ecp_nistp521.c ecp_nistputil.c
.endif
.if defined(ASM_aarch64)
SRCS+=	ecp_nistz256-armv8.S ecp_nistz256.c
.elif defined(ASM_amd64)
SRCS+=	ecp_nistz256-x86_64.S ecp_nistz256.c x25519-x86_64.S
.elif defined(ASM_arm)
SRCS+=	ecp_nistz256-armv4.S ecp_nistz256.c
.elif defined(ASM_i386)
SRCS+=	ecp_nistz256-x86.S ecp_nistz256.c
.elif defined(ASM_powerpc64)
SRCS+=	ecp_nistp521-ppc64.S ecp_nistz256-ppc64.S ecp_nistz256.c ecp_ppc.c x25519-ppc64.S
.elif defined(ASM_powerpc64le)
SRCS+=	ecp_nistp521-ppc64.S ecp_nistz256-ppc64.S ecp_nistz256.c ecp_ppc.c x25519-ppc64.S
.endif

# crypto/evp
SRCS+=	digest.c evp_enc.c evp_lib.c evp_fetch.c evp_utils.c \
	mac_lib.c mac_meth.c keymgmt_meth.c keymgmt_lib.c kdf_lib.c kdf_meth.c \
	m_sigver.c pmeth_lib.c signature.c p_lib.c pmeth_gn.c exchange.c \
	evp_rand.c asymcipher.c kem.c dh_support.c ec_support.c pmeth_check.c

# crypto/ffc
SRCS+=	ffc_params.c ffc_params_generate.c ffc_key_generate.c \
	ffc_params_validate.c ffc_key_validate.c ffc_backend.c \
	ffc_dh.c

# crypto/lhash
SRCS+=	lhash.c

# crypto/property
SRCS+=	property_string.c property_parse.c property_query.c property.c defn_cache.c

# crypto/rsa
SRCS+=	rsa_ossl.c rsa_gen.c rsa_lib.c rsa_sign.c rsa_pk1.c \
	rsa_none.c rsa_oaep.c rsa_chk.c rsa_pss.c rsa_x931.c rsa_crpt.c \
	rsa_sp800_56b_gen.c rsa_sp800_56b_check.c rsa_backend.c \
	rsa_mp_names.c rsa_schemes.c
SRCS+=	rsa_acvp_test_params.c

# crypto/sha
SRCS+=	sha1dgst.c sha256.c sha512.c sha3.c
.if defined(ASM_aarch64)
SRCS+=	keccak1600-armv8.S sha1-armv8.S sha256-armv8.S sha512-armv8.S
.elif defined(ASM_amd64)
SRCS+=	keccak1600-x86_64.S sha1-mb-x86_64.S sha1-x86_64.S
SRCS+=	sha256-mb-x86_64.S sha256-x86_64.S sha512-x86_64.S
.elif defined(ASM_arm)
SRCS+=	keccak1600-armv4.S sha1-armv4-large.S sha256-armv4.S sha512-armv4.S
.elif defined(ASM_i386)
SRCS+=	keccak1600.c sha1-586.S sha256-586.S sha512-586.S
.elif defined(ASM_powerpc)
SRCS+=	keccak1600.c sha_ppc.c sha1-ppc.S sha256-ppc.S sha512-ppc.S sha256p8-ppc.S sha512p8-ppc.S
.elif defined(ASM_powerpc64)
SRCS+=	keccak1600-ppc64.S sha_ppc.c sha1-ppc.S sha256-ppc.S sha512-ppc.S sha256p8-ppc.S sha512p8-ppc.S
.elif defined(ASM_powerpc64le)
SRCS+=	keccak1600-ppc64.S sha_ppc.c sha1-ppc.S sha256-ppc.S sha512-ppc.S sha256p8-ppc.S sha512p8-ppc.S
.else
SRCS+=	keccak1600.c
.endif

# common
SRCS+=	capabilities.c bio_prov.c digest_to_nid.c \
	securitycheck.c provider_seeding.c
SRCS+=	securitycheck_fips.c

# common/der
SRCS+=	der_rsa_gen.c der_rsa_key.c
SRCS+=	der_rsa_sig.c

SRCS+=	der_dsa_gen.c der_dsa_key.c
SRCS+=	der_dsa_sig.c

SRCS+=	der_ec_gen.c der_ec_key.c
SRCS+=	der_ec_sig.c

SRCS+=	der_ecx_gen.c der_ecx_key.c

SRCS+=	der_wrap_gen.c

# asymciphers
SRCS+=	rsa_enc.c

# ciphers
SRCS+=	ciphercommon.c ciphercommon_hw.c ciphercommon_block.c \
	ciphercommon_gcm.c ciphercommon_gcm_hw.c \
	ciphercommon_ccm.c ciphercommon_ccm_hw.c
SRCS+=	cipher_aes.c cipher_aes_hw.c \
	cipher_aes_xts.c cipher_aes_xts_hw.c \
	cipher_aes_gcm.c cipher_aes_gcm_hw.c \
	cipher_aes_ccm.c cipher_aes_ccm_hw.c \
	cipher_aes_wrp.c \
	cipher_aes_cbc_hmac_sha.c \
	cipher_aes_cbc_hmac_sha256_hw.c cipher_aes_cbc_hmac_sha1_hw.c \
	cipher_cts.c
SRCS+=	cipher_aes_xts_fips.c
SRCS+=	cipher_tdes.c cipher_tdes_common.c cipher_tdes_hw.c

# digests
SRCS+=	digestcommon.c
SRCS+=	sha2_prov.c
SRCS+=	sha3_prov.c

# exchange
SRCS+=	dh_exch.c
SRCS+=	ecx_exch.c
SRCS+=	ecdh_exch.c
SRCS+=	kdf_exch.c

# kdfs
SRCS+=	tls1_prf.c
SRCS+=	hkdf.c
SRCS+=	kbkdf.c
SRCS+=	pbkdf2.c
SRCS+=	pbkdf2_fips.c
SRCS+=	sskdf.c
SRCS+=	sshkdf.c
SRCS+=	x942kdf.c

# kem
SRCS+=	rsa_kem.c

# keymgmt
SRCS+=	dh_kmgmt.c
SRCS+=	dsa_kmgmt.c
SRCS+=	ec_kmgmt.c
SRCS+=	ecx_kmgmt.c
SRCS+=	kdf_legacy_kmgmt.c
SRCS+=	mac_legacy_kmgmt.c
SRCS+=	rsa_kmgmt.c

# macs
SRCS+=	gmac_prov.c
SRCS+=	hmac_prov.c
SRCS+=	kmac_prov.c
SRCS+=	cmac_prov.c

# rands
SRCS+=	drbg.c test_rng.c drbg_ctr.c drbg_hash.c drbg_hmac.c crngt.c

# signature
SRCS+=	dsa_sig.c
SRCS+=	eddsa_sig.c ecdsa_sig.c
SRCS+=	mac_legacy_sig.c
SRCS+=	rsa_sig.c

# ssl
SRCS+=	record/tls_pad.c s3_cbc.c

.include <bsd.lib.mk>

.if defined(ASM_${MACHINE_CPUARCH})
.PATH:	${SRCTOP}/secure/lib/libcrypto/arch/${MACHINE_CPUARCH}
.if defined(ASM_amd64)
.PATH:	${LCRYPTO_SRC}/crypto/bn/asm
.endif
.elif defined(ASM_${MACHINE_ARCH})
.PATH:	${SRCTOP}/secure/lib/libcrypto/arch/${MACHINE_ARCH}
.endif

.PATH:	${LCRYPTO_SRC}/crypto \
	${LCRYPTO_SRC}/crypto/bio \
	${LCRYPTO_SRC}/crypto/bn \
	${LCRYPTO_SRC}/crypto/dh \
	${LCRYPTO_SRC}/crypto/dsa \
	${LCRYPTO_SRC}/crypto/ec \
	${LCRYPTO_SRC}/crypto/evp \
	${LCRYPTO_SRC}/crypto/ffc \
	${LCRYPTO_SRC}/crypto/lhash \
	${LCRYPTO_SRC}/crypto/property \
	${LCRYPTO_SRC}/crypto/rsa \
	${LCRYPTO_SRC}/crypto/sha \
	${LCRYPTO_SRC}/providers/fips \
	${LCRYPTO_SRC}/providers/common/der \
	${LCRYPTO_SRC}/providers/implementations/asymciphers \
	${LCRYPTO_SRC}/providers/implementations/ciphers \
	${LCRYPTO_SRC}/providers/implementations/digests \
	${LCRYPTO_SRC}/providers/implementations/exchange \
	${LCRYPTO_SRC}/providers/implementations/kdfs \
	${LCRYPTO_SRC}/providers/implementations/kem \
	${LCRYPTO_SRC}/providers/implementations/keymgmt \
	${LCRYPTO_SRC}/providers/implementations/macs \
	${LCRYPTO_SRC}/providers/implementations/rands \
	${LCRYPTO_SRC}/providers/implementations/signature \
	${LCRYPTO_SRC}/ssl
