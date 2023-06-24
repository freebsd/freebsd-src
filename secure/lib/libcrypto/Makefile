# $FreeBSD$

SHLIBDIR?=	/lib
SUBDIR=		engines modules

.include <bsd.own.mk>
.include <src.opts.mk>

PACKAGE=	openssl
LIB=		crypto
SHLIB_MAJOR=	30
VERSION_MAP=	${.CURDIR}/Version.map

NO_LINT=
PCFILES=	libcrypto.pc

.include "Makefile.man"
.include "Makefile.inc"

SRCS=	asn1_dsa.c bsearch.c cpt_err.c context.c core_algorithm.c core_fetch.c core_namemap.c cpuid.c cryptlib.c ctype.c
SRCS+=	cversion.c der_writer.c ebcdic.c ex_data.c getenv.c info.c init.c initthread.c
SRCS+=	mem.c mem_sec.c o_dir.c o_fopen.c o_init.c o_str.c o_time.c packet.c
SRCS+=	param_build.c param_build_set.c params.c params_dup.c params_from_text.c
SRCS+=	passphrase.c provider.c provider_child.c provider_conf.c provider_core.c provider_predefined.c punycode.c self_test_core.c sparse_array.c threads_lib.c threads_none.c threads_pthread.c trace.c uid.c
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

# aes
SRCS+=	aes_cbc.c aes_cfb.c aes_ecb.c aes_ige.c aes_misc.c aes_ofb.c aes_wrap.c
.if defined(ASM_aarch64)
SRCS+=	aes_core.c aesv8-armx.S vpaes-armv8.S
ACFLAGS.aesv8-armx.S=	-march=armv8-a+crypto
.elif defined(ASM_amd64)
SRCS+=	aes_core.c aesni-mb-x86_64.S aesni-sha1-x86_64.S aesni-sha256-x86_64.S
SRCS+=	aesni-x86_64.S vpaes-x86_64.S
.elif defined(ASM_arm)
SRCS+=	aes-armv4.S aesv8-armx.S bsaes-armv7.S
.elif defined(ASM_i386)
SRCS+=	aes_core.c aesni-x86.S vpaes-x86.S
.elif defined(ASM_powerpc)
SRCS+=	aes_core.c aes-ppc.S vpaes-ppc.S aesp8-ppc.S
.elif defined(ASM_powerpc64)
SRCS+=	aes_core.c aes-ppc.S vpaes-ppc.S aesp8-ppc.S
.elif defined(ASM_powerpc64le)
SRCS+=	aes_core.c aes-ppc.S vpaes-ppc.S aesp8-ppc.S
.else
SRCS+=	aes_core.c
.endif

# asn1
SRCS+=	a_bitstr.c a_d2i_fp.c a_digest.c a_dup.c a_gentm.c a_i2d_fp.c
SRCS+=	a_int.c a_mbstr.c a_object.c a_octet.c a_print.c a_sign.c a_strex.c
SRCS+=	a_strnid.c a_time.c a_type.c a_utctm.c a_utf8.c a_verify.c
SRCS+=	ameth_lib.c asn1_err.c asn1_gen.c asn1_item_list.c asn1_lib.c
SRCS+=	asn1_parse.c asn_mime.c asn_moid.c asn_mstbl.c asn_pack.c bio_asn1.c
SRCS+=	bio_ndef.c d2i_pr.c d2i_pu.c evp_asn1.c f_int.c f_string.c i2d_evp.c
SRCS+=	n_pkey.c nsseq.c p5_pbe.c p5_pbev2.c p5_scrypt.c p8_pkey.c
SRCS+=	t_bitst.c t_pkey.c t_spki.c tasn_dec.c tasn_enc.c tasn_fre.c
SRCS+=	tasn_new.c tasn_prn.c tasn_scn.c tasn_typ.c tasn_utl.c x_algor.c
SRCS+=	x_bignum.c x_info.c x_int64.c x_long.c x_pkey.c x_sig.c x_spki.c
SRCS+=	x_val.c

# async
SRCS+=	async.c async_err.c async_posix.c async_wait.c

# bf
SRCS+=	bf_cfb64.c bf_ecb.c bf_ofb64.c bf_prefix.c bf_readbuff.c bf_skey.c
.if defined(ASM_i386)
SRCS+=	bf-586.S
.else
SRCS+=	bf_enc.c
.endif

# bio
SRCS+=	bio_addr.c bio_dump.c bio_print.c bio_sock.c bio_sock2.c bf_buff.c
SRCS+=	bf_lbuf.c bf_nbio.c bf_null.c bio_cb.c bio_err.c bio_lib.c bio_meth.c
SRCS+=	bss_acpt.c bss_bio.c bss_conn.c bss_core.c bss_dgram.c bss_fd.c
SRCS+=	bss_file.c bss_log.c bss_mem.c bss_null.c bss_sock.c ossl_core_bio.c

# bn
SRCS+=	bn_add.c bn_blind.c bn_const.c bn_conv.c bn_ctx.c bn_depr.c bn_dh.c
SRCS+=	bn_div.c bn_err.c bn_exp.c bn_exp2.c bn_gcd.c bn_gf2m.c bn_intern.c
SRCS+=	bn_kron.c bn_lib.c bn_mod.c bn_mont.c bn_mpi.c bn_mul.c bn_nist.c
SRCS+=	bn_prime.c bn_print.c bn_rand.c bn_recp.c bn_rsa_fips186_4.c bn_shift.c
SRCS+=	bn_sqr.c bn_sqrt.c bn_srp.c bn_word.c bn_x931p.c
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

# Full of strict aliasing violations that LLVM has been seen to break with
# optimisations, which can lead to ECDSA signatures not working. See
# https://github.com/openssl/openssl/issues/12247 for the upstream bug report.
CFLAGS.bn_nist.c+=	-fno-strict-aliasing

# buffer
SRCS+=	buf_err.c buffer.c

# camellia
SRCS+=	cmll_cfb.c cmll_ctr.c cmll_ecb.c cmll_ofb.c
.if defined(ASM_amd64)
SRCS+=	cmll-x86_64.S cmll_misc.c
.elif defined(ASM_i386)
SRCS+=	cmll-x86.S
.else
SRCS+=	camellia.c cmll_cbc.c cmll_misc.c
.endif

# cast
SRCS+=	c_cfb64.c c_ecb.c c_enc.c c_ofb64.c c_skey.c

# chacha
.if defined(ASM_aarch64)
SRCS+=	chacha-armv8.S
.elif defined(ASM_amd64)
SRCS+=	chacha-x86_64.S
.elif defined(ASM_arm)
SRCS+=	chacha-armv4.S
.elif defined(ASM_i386)
SRCS+=	chacha-x86.S
.elif defined(ASM_powerpc)
SRCS+=	chacha_ppc.c chacha-ppc.S
.elif defined(ASM_powerpc64)
SRCS+=	chacha_ppc.c chacha-ppc.S
.elif defined(ASM_powerpc64le)
SRCS+=	chacha_ppc.c chacha-ppc.S
.else
SRCS+=	chacha_enc.c
.endif

# cmac
SRCS+=	cmac.c

# cmp
SRCS+=	cmp_asn.c cmp_client.c cmp_ctx.c cmp_err.c cmp_hdr.c cmp_http.c
SRCS+=	cmp_msg.c cmp_protect.c cmp_server.c cmp_status.c cmp_util.c cmp_vfy.c

# cms
SRCS+=	cms_asn1.c cms_att.c cms_cd.c cms_dd.c cms_dh.c cms_ec.c cms_enc.c
SRCS+=	cms_env.c cms_err.c cms_ess.c cms_io.c cms_kari.c cms_lib.c cms_pwri.c
SRCS+=	cms_rsa.c cms_sd.c cms_smime.c

# comp
SRCS+=	c_zlib.c comp_err.c comp_lib.c

# conf
SRCS+=	conf_api.c conf_def.c conf_err.c conf_lib.c conf_mall.c conf_mod.c
SRCS+=	conf_sap.c conf_ssl.c

# crmf
SRCS+=	crmf_asn.c crmf_err.c crmf_lib.c crmf_pbm.c

# ct
SRCS+=	ct_b64.c ct_err.c ct_log.c ct_oct.c ct_policy.c ct_prn.c ct_sct.c
SRCS+=	ct_sct_ctx.c ct_vfy.c ct_x509v3.c

# des
SRCS+=	cbc_cksm.c cbc_enc.c cfb64ede.c cfb64enc.c cfb_enc.c ecb3_enc.c
SRCS+=	ecb_enc.c fcrypt.c ofb64ede.c ofb64enc.c ofb_enc.c pcbc_enc.c
SRCS+=	qud_cksm.c rand_key.c set_key.c str2key.c xcbc_enc.c
.if defined(ASM_i386)
SRCS+=	crypt586.S des-586.S
.else
SRCS+=	des_enc.c fcrypt_b.c
.endif

# dh
SRCS+=	dh_ameth.c dh_asn1.c dh_backend.c dh_check.c dh_err.c dh_gen.c
SRCS+=	dh_group_params.c dh_kdf.c dh_key.c dh_lib.c dh_meth.c dh_pmeth.c
SRCS+=	dh_prn.c dh_rfc5114.c

# dsa
SRCS+=	dsa_ameth.c dsa_asn1.c dsa_backend.c dsa_check.c dsa_err.c dsa_gen.c
SRCS+=	dsa_key.c dsa_lib.c dsa_meth.c dsa_ossl.c dsa_pmeth.c dsa_prn.c
SRCS+=	dsa_sign.c dsa_vrf.c

# dso
SRCS+=	dso_dlfcn.c dso_err.c dso_lib.c

# ec
SRCS+=	curve25519.c curve448.c curve448_tables.c ec2_oct.c ec2_smpl.c
SRCS+=	ec_ameth.c ec_asn1.c ec_backend.c ec_check.c ec_curve.c ec_cvt.c
SRCS+=	ec_deprecated.c ec_err.c ec_key.c ec_kmeth.c ec_lib.c ec_mult.c ec_oct.c
SRCS+=	ec_pmeth.c ec_print.c ecdh_kdf.c ecdh_ossl.c ecdsa_ossl.c ecdsa_sign.c
SRCS+=	ecdsa_vrf.c eck_prn.c ecp_mont.c ecp_nist.c
SRCS+=	ecp_oct.c ecp_smpl.c ecx_backend.c ecx_key.c ecx_meth.c eddsa.c
SRCS+=	f_generic.c f_impl32.c f_impl64.c scalar.c
# see OPENSSL_NO_EC_NISTP_64_GCC_128 in configuration.h
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

# encode_decode
SRCS+=	decoder_err.c decoder_lib.c decoder_meth.c decoder_pkey.c
SRCS+=	encoder_err.c encoder_lib.c encoder_meth.c encoder_pkey.c

# engine
SRCS+=	eng_all.c eng_cnf.c eng_ctrl.c eng_dyn.c eng_err.c
SRCS+=	eng_fat.c eng_init.c eng_lib.c eng_list.c eng_openssl.c eng_pkey.c
SRCS+=	eng_rdrand.c eng_table.c tb_asnmth.c tb_cipher.c tb_dh.c tb_digest.c
SRCS+=	tb_dsa.c tb_eckey.c tb_pkmeth.c tb_rand.c tb_rsa.c

# err
SRCS+=	err.c err_all.c err_blocks.c err_prn.c

# ess
SRCS+=	ess_asn1.c ess_err.c ess_lib.c

# evp
SRCS+=	asymcipher.c bio_b64.c bio_enc.c bio_md.c bio_ok.c c_allc.c c_alld.c cmeth_lib.c
SRCS+=	ctrl_params_translate.c dh_ctrl.c dh_support.c digest.c dsa_ctrl.c e_aes.c e_aes_cbc_hmac_sha1.c
SRCS+=	e_aes_cbc_hmac_sha256.c e_aria.c e_bf.c e_camellia.c e_cast.c
SRCS+=	e_chacha20_poly1305.c e_des.c e_des3.c e_idea.c e_null.c e_old.c
SRCS+=	e_rc2.c e_rc4.c e_rc4_hmac_md5.c e_rc5.c e_seed.c e_sm4.c e_xcbc_d.c ec_ctrl.c ec_support.c
SRCS+=	encode.c evp_cnf.c evp_enc.c evp_err.c evp_fetch.c evp_key.c evp_lib.c evp_pbe.c
SRCS+=	evp_pkey.c evp_rand.c evp_utils.c exchange.c kdf_lib.c kdf_meth.c kem.c keymgmt_lib.c keymgmt_meth.c
SRCS+=	legacy_blake2.c legacy_md4.c legacy_md5.c legacy_md5_sha1.c m_null.c
SRCS+=	legacy_ripemd.c legacy_sha.c legacy_wp.c m_sigver.c mac_lib.c mac_meth.c names.c p5_crpt.c
SRCS+=	p5_crpt2.c p_dec.c p_enc.c p_legacy.c p_lib.c  p_open.c p_seal.c p_sign.c
SRCS+=	p_verify.c pbe_scrypt.c pmeth_check.c pmeth_gn.c pmeth_lib.c signature.c

# ffc
SRCS+=	ffc_backend.c ffc_dh.c ffc_key_generate.c ffc_key_validate.c
SRCS+=	ffc_params.c ffc_params_generate.c ffc_params_validate.c

# hmac
SRCS+=	hmac.c

# http
SRCS+=	http_client.c http_err.c http_lib.c

# kdf
SRCS+=	kdf_err.c

# lhash
SRCS+=	lh_stats.c lhash.c

# md4
SRCS+=	md4_dgst.c md4_one.c

# md5
SRCS+=	md5_dgst.c md5_one.c md5_sha1.c
.if defined(ASM_amd64)
SRCS+=	md5-x86_64.S
.elif defined(ASM_i386)
SRCS+=	md5-586.S
.endif

# modes
SRCS+=	cbc128.c ccm128.c cfb128.c ctr128.c cts128.c gcm128.c ocb128.c
SRCS+=	ofb128.c siv128.c wrap128.c xts128.c
.if defined(ASM_aarch64)
SRCS+=	ghashv8-armx.S aes-gcm-armv8_64.S
ACFLAGS.ghashv8-armx.S=	-march=armv8-a+crypto
.elif defined(ASM_amd64)
SRCS+=	aesni-gcm-x86_64.S ghash-x86_64.S
.elif defined(ASM_arm)
SRCS+=	ghash-armv4.S ghashv8-armx.S
.elif defined(ASM_i386)
SRCS+=	ghash-x86.S
.elif defined(ASM_powerpc)
SRCS+=	ghashp8-ppc.S
.elif defined(ASM_powerpc64)
SRCS+=	ghashp8-ppc.S
.elif defined(ASM_powerpc64le)
SRCS+=	ghashp8-ppc.S
.endif

# objects
SRCS+=	o_names.c obj_dat.c obj_err.c obj_lib.c obj_xref.c

# ocsp
SRCS+=	ocsp_asn.c ocsp_cl.c ocsp_err.c ocsp_ext.c ocsp_http.c ocsp_lib.c
SRCS+=	ocsp_prn.c ocsp_srv.c ocsp_vfy.c v3_ocsp.c

# pem
SRCS+=	pem_all.c pem_err.c pem_info.c pem_lib.c pem_oth.c pem_pk8.c
SRCS+=	pem_pkey.c pem_sign.c pem_x509.c pem_xaux.c pvkfmt.c

# pkcs12
SRCS+=	p12_add.c p12_asn.c p12_attr.c p12_crpt.c p12_crt.c p12_decr.c
SRCS+=	p12_init.c p12_key.c p12_kiss.c p12_mutl.c p12_npas.c p12_p8d.c
SRCS+=	p12_p8e.c p12_sbag.c p12_utl.c pk12err.c

# pkcs7
SRCS+=	bio_pk7.c pk7_asn1.c pk7_attr.c pk7_doit.c pk7_lib.c pk7_mime.c
SRCS+=	pk7_smime.c pkcs7err.c

# poly1305
SRCS+=	poly1305.c
.if defined(ASM_aarch64)
SRCS+=	poly1305-armv8.S
.elif defined(ASM_amd64)
SRCS+=	poly1305-x86_64.S
.elif defined(ASM_arm)
SRCS+=	poly1305-armv4.S
.elif defined(ASM_i386)
SRCS+=	poly1305-x86.S
.elif defined(ASM_powerpc)
SRCS+=	poly1305_ppc.c poly1305-ppc.S poly1305-ppcfp.S
.elif defined(ASM_powerpc64)
SRCS+=	poly1305_ppc.c poly1305-ppc.S poly1305-ppcfp.S
.elif defined(ASM_powerpc64le)
SRCS+=	poly1305_ppc.c poly1305-ppc.S poly1305-ppcfp.S
.endif

# property
SRCS+=	defn_cache.c property.c property_err.c property_parse.c property_query.c
SRCS+=	property_string.c

# providers
SRCS+=	baseprov.c defltprov.c nullprov.c prov_running.c

# providers/common
SRCS+=	bio_prov.c capabilities.c digest_to_nid.c provider_ctx.c provider_err.c
SRCS+=	provider_seeding.c provider_util.c securitycheck.c
SRCS+=	securitycheck_default.c

# providers/common/der
SRCS+=	der_rsa_gen.c der_rsa_key.c der_rsa_sig.c
SRCS+=	der_digests_gen.c
SRCS+=	der_wrap_gen.c
SRCS+=	der_dsa_gen.c der_dsa_key.c der_dsa_sig.c
SRCS+=	der_ec_gen.c der_ec_key.c der_ec_sig.c
SRCS+=	der_ecx_gen.c der_ecx_key.c

# providers/implementations/asymciphers
SRCS+=	rsa_enc.c

# providers/implementations/ciphers
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
SRCS+=	cipher_aes_ocb.c cipher_aes_ocb_hw.c
SRCS+=	cipher_aes_xts_fips.c
SRCS+=	cipher_aes_siv.c cipher_aes_siv_hw.c
SRCS+=	cipher_blowfish.c cipher_blowfish_hw.c
SRCS+=	cipher_camellia.c cipher_camellia_hw.c
SRCS+=	cipher_cast5.c cipher_cast5_hw.c
SRCS+=	cipher_chacha20.c cipher_chacha20_hw.c
SRCS+=	cipher_chacha20_poly1305.c cipher_chacha20_poly1305_hw.c
SRCS+=	cipher_des.c cipher_des_hw.c
SRCS+=	cipher_desx.c cipher_desx_hw.c
SRCS+=	cipher_null.c
SRCS+=	cipher_rc4.c cipher_rc4_hw.c
SRCS+=	cipher_rc4_hmac_md5.c cipher_rc4_hmac_md5_hw.c
SRCS+=	cipher_seed.c cipher_seed_hw.c
SRCS+=	cipher_tdes.c cipher_tdes_common.c cipher_tdes_hw.c
SRCS+=	cipher_tdes_default.c cipher_tdes_default_hw.c \
	cipher_tdes_wrap.c cipher_tdes_wrap_hw.c

# providers/implementations/digests
SRCS+=	digestcommon.c
SRCS+=	blake2_prov.c blake2b_prov.c blake2s_prov.c
SRCS+=	md4_prov.c
SRCS+=	md5_prov.c md5_sha1_prov.c
SRCS+=	null_prov.c
SRCS+=	ripemd_prov.c
SRCS+=	sha2_prov.c sha3_prov.c
SRCS+=	wp_prov.c

# providers/implementations/encode_decode
SRCS+=	decode_der2key.c decode_epki2pki.c decode_msblob2key.c decode_pvk2key.c
SRCS+=	decode_pem2der.c decode_spki2typespki.c
SRCS+=	encode_key2any.c encode_key2blob.c encode_key2ms.c encode_key2text.c
SRCS+=	endecoder_common.c

# providers/implementations/exchange
SRCS+=	dh_exch.c
SRCS+=	ecx_exch.c ecdh_exch.c
SRCS+=	kdf_exch.c

# providers/implementations/kdfs
SRCS+=	hkdf.c kbkdf.c krb5kdf.c pbkdf1.c pbkdf2.c pbkdf2_fips.c
SRCS+=	pkcs12kdf.c scrypt.c sskdf.c sshkdf.c tls1_prf.c x942kdf.c

# providers/implementations/kem
SRCS+=	rsa_kem.c

# providers/implementations/keymgmt
SRCS+=	dh_kmgmt.c dsa_kmgmt.c ec_kmgmt.c ecx_kmgmt.c kdf_legacy_kmgmt.c
SRCS+=	mac_legacy_kmgmt.c rsa_kmgmt.c

# providers/implementations/macs
SRCS+=	gmac_prov.c hmac_prov.c kmac_prov.c
SRCS+=	blake2b_mac.c blake2s_mac.c
SRCS+=	cmac_prov.c
SRCS+=	poly1305_prov.c
SRCS+=	siphash_prov.c

# providers/implementations/rands
SRCS+=	crngt.c drbg.c drbg_ctr.c drbg_hash.c drbg_hmac.c test_rng.c
SRCS+=	seed_src.c

# providers/implementations/rands/seeding
SRCS+=	rand_cpu_x86.c rand_tsc.c rand_unix.c rand_win.c

# providers/implementations/signature
SRCS+=	dsa_sig.c eddsa_sig.c ecdsa_sig.c mac_legacy_sig.c rsa_sig.c

# providers/implementations/storemgmt
SRCS+=	file_store.c file_store_any2obj.c

# rand
SRCS+=	prov_seed.c rand_deprecated.c rand_egd.c rand_err.c rand_lib.c
SRCS+=	rand_meth.c rand_pool.c randfile.c

# rc2
SRCS+=	rc2_cbc.c rc2_ecb.c rc2_skey.c rc2cfb64.c rc2ofb64.c

# rc4
.if defined(ASM_amd64)
SRCS+=	rc4-md5-x86_64.S rc4-x86_64.S
.elif defined(ASM_i386)
SRCS+=	rc4-586.S
.else
SRCS+=	rc4_enc.c rc4_skey.c
.endif

# ripemd
SRCS+=	rmd_dgst.c rmd_one.c
.if defined(ASM_i386)
SRCS+=	rmd-586.S
.endif

# rsa
SRCS+=	rsa_ameth.c rsa_asn1.c rsa_backend.c rsa_chk.c rsa_crpt.c
SRCS+=	rsa_err.c rsa_gen.c rsa_lib.c rsa_meth.c rsa_mp.c rsa_mp_names.c
SRCS+=	rsa_none.c rsa_oaep.c rsa_ossl.c rsa_pk1.c rsa_pmeth.c rsa_prn.c
SRCS+=	rsa_pss.c rsa_saos.c rsa_schemes.c rsa_sign.c rsa_sp800_56b_check.c
SRCS+=	rsa_sp800_56b_gen.c rsa_x931.c rsa_x931g.c

# seed
SRCS+=	seed.c seed_cbc.c seed_cfb.c seed_ecb.c seed_ofb.c

# sha
SRCS+=	sha1_one.c sha1dgst.c sha256.c sha3.c sha512.c
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

# siphash
SRCS+=	siphash.c

# srp
SRCS+=	srp_lib.c srp_vfy.c

# ssl
SRCS+=	s3_cbc.c

# ssl/record
SRCS+=	tls_pad.c

# stack
SRCS+=	stack.c

# store
SRCS+=	store_err.c store_init.c store_lib.c store_meth.c store_register.c
SRCS+=	store_result.c store_strings.c

# ts
SRCS+=	ts_asn1.c ts_conf.c ts_err.c ts_lib.c ts_req_print.c ts_req_utils.c
SRCS+=	ts_rsp_print.c ts_rsp_sign.c ts_rsp_utils.c ts_rsp_verify.c
SRCS+=	ts_verify_ctx.c

# txt_db
SRCS+=	txt_db.c

# ui
SRCS+=	ui_err.c ui_lib.c ui_null.c ui_openssl.c ui_util.c

# whrlpool
SRCS+=	wp_dgst.c
.if defined(ASM_amd64)
SRCS+=	wp-x86_64.S
.elif defined(ASM_i386)
SRCS+=	wp-mmx.S wp_block.c
.else
SRCS+=	wp_block.c
.endif

# x509
SRCS+=	by_dir.c by_file.c by_store.c
SRCS+=	pcy_cache.c pcy_data.c pcy_lib.c pcy_map.c pcy_node.c pcy_tree.c
SRCS+=	t_crl.c t_req.c t_x509.c
SRCS+=	v3_addr.c v3_admis.c v3_akeya.c v3_akid.c v3_asid.c
SRCS+=	v3_bcons.c v3_bitst.c v3_conf.c v3_cpols.c v3_crld.c v3_enum.c
SRCS+=	v3_extku.c v3_genn.c v3_ia5.c v3_info.c v3_int.c v3_ist.c v3_lib.c v3_ncons.c
SRCS+=	v3_pci.c v3_pcia.c v3_pcons.c v3_pku.c v3_pmaps.c v3_prn.c v3_purp.c
SRCS+=	v3_san.c v3_skid.c v3_sxnet.c v3_tlsf.c v3_utf8.c v3_utl.c v3err.c
SRCS+=	x509_att.c x509_cmp.c x509_d2.c x509_def.c x509_err.c x509_ext.c x509_lu.c x509_meth.c
SRCS+=	x509_obj.c x509_r2x.c x509_req.c x509_set.c x509_trust.c x509_txt.c
SRCS+=	x509_v3.c x509_vfy.c x509_vpm.c x509cset.c x509name.c x509rset.c
SRCS+=	x509spki.c x509type.c x_all.c x_attrib.c x_crl.c x_exten.c x_name.c
SRCS+=	x_pubkey.c x_req.c x_x509.c x_x509a.c

INCS=	aes.h asn1.h asn1_mac.h asn1err.h asn1t.h async.h asyncerr.h bio.h
INCS+=	bioerr.h blowfish.h bn.h bnerr.h buffer.h buffererr.h camellia.h
INCS+=	cast.h cmac.h cmp.h cmp_util.h cmperr.h cms.h cmserr.h comp.h comperr.h conf.h conf_api.h
INCS+=	conferr.h configuration.h conftypes.h core.h core_dispatch.h core_names.h core_object.h
INCS+=	crmf.h crmferr.h crypto.h cryptoerr.h cryptoerr_legacy.h ct.h cterr.h
INCS+=	decoder.h decodererr.h des.h dh.h dherr.h dsa.h
INCS+=	dsaerr.h dtls1.h e_os2.h ebcdic.h ec.h ecdh.h ecdsa.h ecerr.h encoder.h encodererr.h
INCS+=	engine.h engineerr.h err.h ess.h esserr.h evp.h evperr.h fips_names.h fipskey.h hmac.h http.h httperr.h idea.h kdf.h
INCS+=	kdferr.h lhash.h macros.h md2.h md4.h md5.h mdc2.h modes.h obj_mac.h
INCS+=	objects.h objectserr.h ocsp.h ocsperr.h opensslconf.h opensslv.h
INCS+=	ossl_typ.h param_build.h params.h pem.h pem2.h pemerr.h pkcs12.h pkcs12err.h pkcs7.h
INCS+=	pkcs7err.h prov_ssl.h proverr.h provider.h rand.h randerr.h rc2.h rc4.h rc5.h ripemd.h
INCS+=	rsa.h rsaerr.h safestack.h seed.h self_test.h sha.h srp.h srtp.h ssl.h ssl2.h
INCS+=	ssl3.h sslerr.h sslerr_legacy.h stack.h store.h storeerr.h symhacks.h tls1.h trace.h ts.h
INCS+=	tserr.h txt_db.h types.h ui.h uierr.h whrlpool.h x509.h x509_vfy.h x509err.h
INCS+=	x509v3.h x509v3err.h

INCSDIR=	${INCLUDEDIR}/openssl

LIBADD=	pthread

CFLAGS+=	-I${LCRYPTO_SRC}/crypto
CFLAGS+=	-I${LCRYPTO_SRC}/crypto/ec/curve448
CFLAGS+=	-I${LCRYPTO_SRC}/crypto/ec/curve448/arch_32
CFLAGS+=	-I${LCRYPTO_SRC}/crypto/modes
CFLAGS+=	-I${.OBJDIR}

.if !empty(SRCS:M*.S)
ACFLAGS+=	-Wa,--noexecstack
.endif

SRCS+=	buildinf.h

CLEANDIRS=	openssl
CLEANFILES=	buildinf.h

.if defined(ASM_${MACHINE_CPUARCH}) || defined(ASM_${MACHINE_ARCH})
_cmd1=/%%NO_ASM%%/d
.else
_cmd1=s/%%NO_ASM%%//
.endif
.if ${MK_OPENSSL_KTLS} != "no"
_cmd2=/%%NO_KTLS%%/d
.else
_cmd2=s/%%NO_KTLS%%//
.endif

buildinf.h: Makefile
	( echo "/*"; \
	echo " * WARNING: do not edit!"; \
	echo " * Generated by ${.ALLSRC}"; \
	echo " */"; \
	echo "#define PLATFORM \"platform: FreeBSD-${MACHINE_ARCH}\""; \
	echo "#define DATE \"built on: reproducible build, date unspecified\""; \
	echo "static const char compiler_flags[] = \"compiler: ${COMPILER_TYPE}\";" ) \
	> ${.TARGET}

.include <bsd.lib.mk>

PICFLAG+=	-DOPENSSL_PIC

.if defined(ASM_${MACHINE_CPUARCH})
.PATH:	${SRCTOP}/secure/lib/libcrypto/arch/${MACHINE_CPUARCH}
.if defined(ASM_amd64)
.PATH:	${LCRYPTO_SRC}/crypto/bn/asm
.endif
.elif defined(ASM_${MACHINE_ARCH})
.PATH:	${SRCTOP}/secure/lib/libcrypto/arch/${MACHINE_ARCH}
.endif

.PATH:	${LCRYPTO_SRC}/crypto \
	${LCRYPTO_SRC}/crypto/aes \
	${LCRYPTO_SRC}/crypto/aria \
	${LCRYPTO_SRC}/crypto/asn1 \
	${LCRYPTO_SRC}/crypto/async \
	${LCRYPTO_SRC}/crypto/async/arch \
	${LCRYPTO_SRC}/crypto/bf \
	${LCRYPTO_SRC}/crypto/bio \
	${LCRYPTO_SRC}/crypto/bn \
	${LCRYPTO_SRC}/crypto/buffer \
	${LCRYPTO_SRC}/crypto/camellia \
	${LCRYPTO_SRC}/crypto/cast \
	${LCRYPTO_SRC}/crypto/chacha \
	${LCRYPTO_SRC}/crypto/cmac \
	${LCRYPTO_SRC}/crypto/cmp \
	${LCRYPTO_SRC}/crypto/cms \
	${LCRYPTO_SRC}/crypto/comp \
	${LCRYPTO_SRC}/crypto/conf \
	${LCRYPTO_SRC}/crypto/crmf \
	${LCRYPTO_SRC}/crypto/ct \
	${LCRYPTO_SRC}/crypto/des \
	${LCRYPTO_SRC}/crypto/dh \
	${LCRYPTO_SRC}/crypto/dsa \
	${LCRYPTO_SRC}/crypto/dso \
	${LCRYPTO_SRC}/crypto/ec \
	${LCRYPTO_SRC}/crypto/ec/curve448 \
	${LCRYPTO_SRC}/crypto/ec/curve448/arch_32 \
	${LCRYPTO_SRC}/crypto/ec/curve448/arch_64 \
	${LCRYPTO_SRC}/crypto/encode_decode \
	${LCRYPTO_SRC}/crypto/engine \
	${LCRYPTO_SRC}/crypto/err \
	${LCRYPTO_SRC}/crypto/ess \
	${LCRYPTO_SRC}/crypto/evp \
	${LCRYPTO_SRC}/crypto/ffc \
	${LCRYPTO_SRC}/crypto/hmac \
	${LCRYPTO_SRC}/crypto/http \
	${LCRYPTO_SRC}/crypto/idea \
	${LCRYPTO_SRC}/crypto/kdf \
	${LCRYPTO_SRC}/crypto/lhash \
	${LCRYPTO_SRC}/crypto/md4 \
	${LCRYPTO_SRC}/crypto/md5 \
	${LCRYPTO_SRC}/crypto/mdc2 \
	${LCRYPTO_SRC}/crypto/modes \
	${LCRYPTO_SRC}/crypto/objects \
	${LCRYPTO_SRC}/crypto/ocsp \
	${LCRYPTO_SRC}/crypto/pem \
	${LCRYPTO_SRC}/crypto/pkcs12 \
	${LCRYPTO_SRC}/crypto/pkcs7 \
	${LCRYPTO_SRC}/crypto/poly1305 \
	${LCRYPTO_SRC}/crypto/property \
	${LCRYPTO_SRC}/crypto/rand \
	${LCRYPTO_SRC}/crypto/rc2 \
	${LCRYPTO_SRC}/crypto/rc4 \
	${LCRYPTO_SRC}/crypto/rc5 \
	${LCRYPTO_SRC}/crypto/ripemd \
	${LCRYPTO_SRC}/crypto/rsa \
	${LCRYPTO_SRC}/crypto/seed \
	${LCRYPTO_SRC}/crypto/sha \
	${LCRYPTO_SRC}/crypto/siphash \
	${LCRYPTO_SRC}/crypto/sm2 \
	${LCRYPTO_SRC}/crypto/sm3 \
	${LCRYPTO_SRC}/crypto/sm4 \
	${LCRYPTO_SRC}/crypto/srp \
	${LCRYPTO_SRC}/crypto/stack \
	${LCRYPTO_SRC}/crypto/store \
	${LCRYPTO_SRC}/crypto/ts \
	${LCRYPTO_SRC}/crypto/txt_db \
	${LCRYPTO_SRC}/crypto/ui \
	${LCRYPTO_SRC}/crypto/whrlpool \
	${LCRYPTO_SRC}/crypto/x509 \
	${LCRYPTO_SRC}/crypto/x509v3 \
	${LCRYPTO_SRC}/include/openssl \
	${LCRYPTO_SRC}/providers \
	${LCRYPTO_SRC}/providers/common \
	${LCRYPTO_SRC}/providers/common/der \
	${LCRYPTO_SRC}/providers/implementations/asymciphers \
	${LCRYPTO_SRC}/providers/implementations/ciphers \
	${LCRYPTO_SRC}/providers/implementations/digests \
	${LCRYPTO_SRC}/providers/implementations/encode_decode \
	${LCRYPTO_SRC}/providers/implementations/exchange \
	${LCRYPTO_SRC}/providers/implementations/kdfs \
	${LCRYPTO_SRC}/providers/implementations/kem \
	${LCRYPTO_SRC}/providers/implementations/keymgmt \
	${LCRYPTO_SRC}/providers/implementations/macs \
	${LCRYPTO_SRC}/providers/implementations/rands \
	${LCRYPTO_SRC}/providers/implementations/rands/seeding \
	${LCRYPTO_SRC}/providers/implementations/signature \
	${LCRYPTO_SRC}/providers/implementations/storemgmt \
	${LCRYPTO_SRC}/ssl \
	${LCRYPTO_SRC}/ssl/record
