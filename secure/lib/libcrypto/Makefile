# $FreeBSD$

LIB=		crypto
SHLIB_MAJOR=	3

NOLINT=		true

.include "Makefile.inc"

# base sources
SRCS+=	cpt_err.c cryptlib.c cversion.c ebcdic.c ex_data.c mem.c mem_clr.c \
	mem_dbg.c o_time.c tmdiff.c uid.c

# aes
SRCS+=	aes_cbc.c aes_cfb.c aes_core.c aes_ctr.c aes_ecb.c aes_misc.c aes_ofb.c

# asn1
SRCS+=	a_bitstr.c a_bool.c a_bytes.c a_d2i_fp.c a_digest.c \
	a_dup.c a_enum.c a_gentm.c a_hdr.c a_i2d_fp.c a_int.c \
	a_mbstr.c a_meth.c a_object.c a_octet.c a_print.c \
	a_set.c a_sign.c a_strex.c a_strnid.c a_time.c a_type.c \
	a_utctm.c a_utf8.c a_verify.c asn1_err.c asn1_lib.c \
	asn1_par.c asn_moid.c asn_pack.c d2i_pr.c d2i_pu.c \
	evp_asn1.c f_enum.c f_int.c f_string.c i2d_pr.c i2d_pu.c \
	n_pkey.c nsseq.c p5_pbe.c p5_pbev2.c p8_pkey.c t_bitst.c \
	t_crl.c t_pkey.c t_req.c t_spki.c t_x509.c t_x509a.c \
	tasn_dec.c tasn_enc.c tasn_fre.c tasn_new.c tasn_typ.c \
	tasn_utl.c x_algor.c x_attrib.c x_bignum.c x_crl.c \
	x_exten.c x_info.c x_long.c x_name.c x_pkey.c x_pubkey.c \
	x_req.c x_sig.c x_spki.c x_val.c x_x509.c x_x509a.c

# bf
SRCS+=	bf_cfb64.c bf_ecb.c bf_enc.c bf_ofb64.c bf_skey.c

# bio
SRCS+=	b_dump.c b_print.c b_sock.c bf_buff.c bf_lbuf.c bf_nbio.c \
	bf_null.c bio_cb.c bio_err.c bio_lib.c bss_acpt.c bss_bio.c \
	bss_conn.c bss_fd.c bss_file.c bss_log.c bss_mem.c \
	bss_null.c bss_sock.c

# bn
SRCS+=	bn_add.c bn_asm.c bn_blind.c bn_ctx.c bn_div.c bn_err.c bn_exp.c \
	bn_exp2.c bn_gcd.c bn_kron.c bn_lib.c bn_mod.c bn_mont.c \
	bn_mpi.c bn_mul.c bn_prime.c bn_print.c bn_rand.c bn_recp.c \
	bn_shift.c bn_sqr.c bn_sqrt.c bn_word.c

# buffer
SRCS+=	buf_err.c buffer.c

# cast
SRCS+=	c_cfb64.c c_ecb.c c_enc.c c_ofb64.c c_skey.c

# comp
SRCS+=	c_rle.c c_zlib.c comp_err.c comp_lib.c

# conf
SRCS+=	conf_api.c conf_def.c conf_err.c conf_lib.c conf_mall.c conf_mod.c conf_sap.c

# des
SRCS+=	cbc3_enc.c cbc_cksm.c cbc_enc.c cfb64ede.c cfb64enc.c cfb_enc.c \
	des_enc.c des_old.c des_old2.c ecb3_enc.c ecb_enc.c ede_cbcm_enc.c \
	enc_read.c enc_writ.c fcrypt.c fcrypt_b.c ofb64ede.c ofb64enc.c \
	ofb_enc.c pcbc_enc.c qud_cksm.c rand_key.c read2pwd.c rnd_keys.c \
	rpc_enc.c set_key.c str2key.c xcbc_enc.c

# dh
SRCS+=	dh_asn1.c dh_check.c dh_err.c dh_gen.c dh_key.c dh_lib.c

# dsa
SRCS+=	dsa_asn1.c dsa_err.c dsa_gen.c dsa_key.c dsa_lib.c dsa_ossl.c dsa_sign.c dsa_vrf.c

# dso
SRCS+=	dso_dl.c dso_dlfcn.c dso_err.c dso_lib.c dso_null.c dso_openssl.c

# ec
SRCS+=	ec_cvt.c ec_err.c ec_lib.c ec_mult.c ecp_mont.c ecp_nist.c \
	ecp_recp.c ecp_smpl.c

# engine
SRCS+=	eng_all.c eng_cnf.c eng_ctrl.c eng_dyn.c eng_err.c eng_fat.c \
	eng_init.c eng_lib.c eng_list.c eng_openssl.c eng_pkey.c \
	eng_table.c hw_4758_cca.c hw_4758_cca_err.c hw_aep.c hw_aep_err.c \
	hw_atalla.c hw_atalla_err.c hw_cryptodev.c hw_cswift.c \
	hw_cswift_err.c hw_ncipher.c hw_ncipher_err.c hw_nuron.c \
	hw_nuron_err.c hw_sureware.c hw_sureware_err.c hw_ubsec.c \
	hw_ubsec_err.c tb_cipher.c tb_dh.c tb_digest.c tb_dsa.c tb_rand.c \
	tb_rsa.c

# err
SRCS+=	err.c err_all.c err_prn.c

# evp
SRCS+=	bio_b64.c bio_enc.c bio_md.c bio_ok.c c_all.c c_allc.c c_alld.c \
	digest.c e_aes.c e_bf.c e_cast.c e_des.c e_des3.c e_idea.c \
	e_null.c e_rc2.c e_rc4.c e_rc5.c e_xcbc_d.c encode.c evp_acnf.c \
	evp_enc.c evp_err.c evp_key.c evp_lib.c evp_pbe.c evp_pkey.c \
	m_dss.c m_dss1.c m_md2.c m_md4.c m_md5.c m_mdc2.c m_null.c \
	m_ripemd.c m_sha.c m_sha1.c names.c openbsd_hw.c p5_crpt.c \
	p5_crpt2.c p_dec.c p_enc.c p_lib.c p_open.c p_seal.c p_sign.c \
	p_verify.c

# hmac
SRCS+=	hmac.c

# idea
.if defined(MAKE_IDEA) && ${MAKE_IDEA} == YES
SRCS+=	i_cbc.c i_cfb64.c i_ecb.c i_ofb64.c i_skey.c
.endif

# krb5
#SRCS+=	krb5_asn.c

# lhash
SRCS+=	lh_stats.c lhash.c

# md2
SRCS+=	md2_dgst.c md2_one.c

# md4
SRCS+=	md4_dgst.c md4_one.c

# md5
SRCS+=	md5_dgst.c md5_one.c

# mdc2
SRCS+=	mdc2_one.c mdc2dgst.c

# objects
SRCS+=	o_names.c obj_dat.c obj_err.c obj_lib.c

# ocsp
SRCS+=	ocsp_asn.c ocsp_cl.c ocsp_err.c ocsp_ext.c ocsp_ht.c \
	ocsp_lib.c ocsp_prn.c ocsp_srv.c ocsp_vfy.c

# pem
SRCS+=	pem_all.c pem_err.c pem_info.c pem_lib.c pem_oth.c pem_pk8.c \
	pem_pkey.c pem_seal.c pem_sign.c pem_x509.c pem_xaux.c

# pkcs12
SRCS+=	p12_add.c p12_asn.c p12_attr.c p12_crpt.c p12_crt.c \
	p12_decr.c p12_init.c p12_key.c p12_kiss.c p12_mutl.c \
	p12_npas.c p12_p8d.c p12_p8e.c p12_utl.c pk12err.c

# pkcs7
SRCS+=	example.c pk7_asn1.c pk7_attr.c pk7_dgst.c pk7_doit.c \
	pk7_lib.c pk7_mime.c pk7_smime.c pkcs7err.c

# rand
SRCS+=	md_rand.c rand_egd.c rand_err.c rand_lib.c rand_unix.c randfile.c

# rc2
SRCS+=	rc2_cbc.c rc2_ecb.c rc2_skey.c rc2cfb64.c rc2ofb64.c

# rc4
SRCS+=	rc4_enc.c rc4_skey.c

# rc5
SRCS+=	rc5_ecb.c rc5_enc.c rc5_skey.c rc5cfb64.c rc5ofb64.c

# ripemd
SRCS+=	rmd_dgst.c rmd_one.c

# rsa
SRCS+=	rsa_asn1.c rsa_chk.c rsa_eay.c rsa_err.c rsa_gen.c rsa_lib.c \
	rsa_none.c rsa_null.c rsa_oaep.c rsa_pk1.c rsa_saos.c \
	rsa_sign.c rsa_ssl.c

# sha
SRCS+=	sha1_one.c sha1dgst.c sha_dgst.c sha_one.c

# stack
SRCS+=	stack.c

# threads
SRCS+=	th-lock.c

# txt_db
SRCS+=	txt_db.c

# ui
SRCS+=	ui_compat.c ui_err.c ui_lib.c ui_openssl.c ui_util.c

# x509
SRCS+=	by_dir.c by_file.c x509_att.c x509_cmp.c x509_d2.c \
	x509_def.c x509_err.c x509_ext.c x509_lu.c x509_obj.c \
	x509_r2x.c x509_req.c x509_set.c x509_trs.c x509_txt.c \
	x509_v3.c x509_vfy.c x509cset.c x509name.c x509rset.c \
	x509spki.c x509type.c x_all.c

# x509v3
SRCS+=	v3_akey.c v3_akeya.c v3_alt.c v3_bcons.c v3_bitst.c \
	v3_conf.c v3_cpols.c v3_crld.c v3_enum.c v3_extku.c \
	v3_genn.c v3_ia5.c v3_info.c v3_int.c v3_lib.c v3_ocsp.c \
	v3_pku.c v3_prn.c v3_purp.c v3_skey.c v3_sxnet.c v3_utl.c v3err.c

MAN3=	ASN1_OBJECT_new.3 ASN1_STRING_length.3 ASN1_STRING_new.3 \
	ASN1_STRING_print_ex.3 BIO_ctrl.3 BIO_f_base64.3 BIO_f_buffer.3 \
	BIO_f_cipher.3 BIO_f_md.3 BIO_f_null.3 BIO_f_ssl.3 BIO_find_type.3 \
	BIO_new.3 BIO_push.3 BIO_read.3 BIO_s_accept.3 BIO_s_bio.3 \
	BIO_s_connect.3 BIO_s_fd.3 BIO_s_file.3 BIO_s_mem.3 BIO_s_null.3 \
	BIO_s_socket.3 BIO_set_callback.3 BIO_should_retry.3 BN_CTX_new.3 \
	BN_CTX_start.3 BN_add.3 BN_add_word.3 BN_bn2bin.3 BN_cmp.3 \
	BN_copy.3 BN_generate_prime.3 BN_mod_inverse.3 BN_mod_mul_montgomery.3 \
	BN_mod_mul_reciprocal.3 BN_new.3 BN_num_bytes.3 BN_rand.3 \
	BN_set_bit.3 BN_swap.3 BN_zero.3 CRYPTO_set_ex_data.3 \
	DH_generate_key.3 DH_generate_parameters.3 DH_get_ex_new_index.3 \
	DH_new.3 DH_set_method.3 DH_size.3 DSA_SIG_new.3 DSA_do_sign.3 \
	DSA_dup_DH.3 DSA_generate_key.3 DSA_generate_parameters.3 \
	DSA_get_ex_new_index.3 DSA_new.3 DSA_set_method.3 DSA_sign.3 \
	DSA_size.3 ERR_GET_LIB.3 ERR_clear_error.3 ERR_error_string.3 \
	ERR_get_error.3 ERR_load_crypto_strings.3 ERR_load_strings.3 \
	ERR_print_errors.3 ERR_put_error.3 ERR_remove_state.3 \
	EVP_BytesToKey.3 EVP_DigestInit.3 EVP_EncryptInit.3 EVP_OpenInit.3 \
	EVP_PKEY_new.3 EVP_PKEY_set1_RSA.3 EVP_SealInit.3 EVP_SignInit.3 \
	EVP_VerifyInit.3 OBJ_nid2obj.3 OPENSSL_VERSION_NUMBER.3 \
	OpenSSL_add_all_algorithms.3 PKCS12_create.3 PKCS12_parse.3 \
	PKCS7_decrypt.3 PKCS7_encrypt.3 PKCS7_sign.3 PKCS7_verify.3 \
	RAND_add.3 RAND_bytes.3 RAND_cleanup.3 RAND_egd.3 RAND_load_file.3 \
	RAND_set_rand_method.3 RSA_blinding_on.3 RSA_check_key.3 \
	RSA_generate_key.3 RSA_get_ex_new_index.3 RSA_new.3 \
	RSA_padding_add_PKCS1_type_1.3 RSA_print.3 RSA_private_encrypt.3 \
	RSA_public_encrypt.3 RSA_set_method.3 RSA_sign.3 \
	RSA_sign_ASN1_OCTET_STRING.3 RSA_size.3 SMIME_read_PKCS7.3 \
	SMIME_write_PKCS7.3 X509_NAME_ENTRY_get_object.3 \
	X509_NAME_add_entry_by_txt.3 X509_NAME_get_index_by_NID.3 \
	X509_NAME_print_ex.3 X509_new.3 bio.3 blowfish.3 bn.3 bn_internal.3 \
	buffer.3 crypto.3 d2i_ASN1_OBJECT.3 d2i_DHparams.3 d2i_DSAPublicKey.3 \
	d2i_PKCS8PrivateKey.3 d2i_RSAPublicKey.3 d2i_X509.3 d2i_X509_ALGOR.3 \
	d2i_X509_CRL.3 d2i_X509_NAME.3 d2i_X509_REQ.3 d2i_X509_SIG.3 \
	des.3 des_modes.3 dh.3 dsa.3 engine.3 err.3 evp.3 hmac.3 \
	lh_stats.3 lhash.3 md5.3 mdc2.3 pem.3 rand.3 rc4.3 ripemd.3 \
	rsa.3 sha.3 threads.3 ui.3 ui_compat.3

INCS=		${HDRS} openssl/evp.h openssl/opensslconf.h
INCSDIR=	${INCLUDEDIR}/openssl

.include <bsd.lib.mk>

.if defined(MAKE_IDEA) && ${MAKE_IDEA} == YES
_ideapath=	${LCRYPTO_SRC}/crypto/idea
.endif

.PATH: \
	${LCRYPTO_SRC}/crypto \
	${LCRYPTO_SRC}/crypto/aes \
	${LCRYPTO_SRC}/crypto/asn1 \
	${LCRYPTO_SRC}/crypto/bf \
	${LCRYPTO_SRC}/crypto/bio \
	${LCRYPTO_SRC}/crypto/bn \
	${LCRYPTO_SRC}/crypto/buffer \
	${LCRYPTO_SRC}/crypto/cast \
	${LCRYPTO_SRC}/crypto/comp \
	${LCRYPTO_SRC}/crypto/conf \
	${LCRYPTO_SRC}/crypto/des \
	${LCRYPTO_SRC}/crypto/dh \
	${LCRYPTO_SRC}/crypto/dsa \
	${LCRYPTO_SRC}/crypto/dso \
	${LCRYPTO_SRC}/crypto/ec \
	${LCRYPTO_SRC}/crypto/engine \
	${LCRYPTO_SRC}/crypto/err \
	${LCRYPTO_SRC}/crypto/evp \
	${LCRYPTO_SRC}/crypto/hmac \
	${_ideapath} \
	${LCRYPTO_SRC}/crypto/krb5 \
	${LCRYPTO_SRC}/crypto/lhash \
	${LCRYPTO_SRC}/crypto/md2 \
	${LCRYPTO_SRC}/crypto/md4 \
	${LCRYPTO_SRC}/crypto/md5 \
	${LCRYPTO_SRC}/crypto/mdc2 \
	${LCRYPTO_SRC}/crypto/objects \
	${LCRYPTO_SRC}/crypto/ocsp \
	${LCRYPTO_SRC}/crypto/pem \
	${LCRYPTO_SRC}/crypto/pkcs12 \
	${LCRYPTO_SRC}/crypto/pkcs7 \
	${LCRYPTO_SRC}/crypto/rand \
	${LCRYPTO_SRC}/crypto/rc2 \
	${LCRYPTO_SRC}/crypto/rc4 \
	${LCRYPTO_SRC}/crypto/rc5 \
	${LCRYPTO_SRC}/crypto/ripemd \
	${LCRYPTO_SRC}/crypto/rsa \
	${LCRYPTO_SRC}/crypto/sha \
	${LCRYPTO_SRC}/crypto/stack \
	${LCRYPTO_SRC}/crypto/threads \
	${LCRYPTO_SRC}/crypto/txt_db \
	${LCRYPTO_SRC}/crypto/ui \
	${LCRYPTO_SRC}/crypto/x509 \
	${LCRYPTO_SRC}/crypto/x509v3 \
	${LCRYPTO_SRC} \
	${.CURDIR}/man
