# $FreeBSD$

.include "Makefile.inc"

.PATH:  ${LCRYPTO_SRC} ${LCRYPTO_SRC}/asn1 ${LCRYPTO_SRC}/bf \
	${LCRYPTO_SRC}/bio ${LCRYPTO_SRC}/bn ${LCRYPTO_SRC}/buffer \
	${LCRYPTO_SRC}/cast ${LCRYPTO_SRC}/comp ${LCRYPTO_SRC}/conf \
	${LCRYPTO_SRC}/des ${LCRYPTO_SRC}/dh ${LCRYPTO_SRC}/dsa \
	${LCRYPTO_SRC}/err ${LCRYPTO_SRC}/evp ${LCRYPTO_SRC}/hmac \
	${LCRYPTO_SRC}/lhash ${LCRYPTO_SRC}/md2 ${LCRYPTO_SRC}/md5 \
	${LCRYPTO_SRC}/mdc2 ${LCRYPTO_SRC}/objects ${LCRYPTO_SRC}/pem \
	${LCRYPTO_SRC}/pkcs7 ${LCRYPTO_SRC}/pkcs12 ${LCRYPTO_SRC}/rand \
	${LCRYPTO_SRC}/rc2 ${LCRYPTO_SRC}/rc4 ${LCRYPTO_SRC}/rc5 \
	${LCRYPTO_SRC}/ripemd ${LCRYPTO_SRC}/rsa ${LCRYPTO_SRC}/../rsaref \
	${LCRYPTO_SRC}/sha ${LCRYPTO_SRC}/stack ${LCRYPTO_SRC}/txt_db \
	${LCRYPTO_SRC}/x509 ${LCRYPTO_SRC}/x509v3

.if defined(MAKE_IDEA) && ${MAKE_IDEA} == YES
.PATH:  ${LCRYPTO_SRC}/idea
.endif

LIB=		crypto
SHLIB_MAJOR=	1

MAINTAINER=	kris

# base sources
SRCS+=	cpt_err.c cryptlib.c cversion.c ebcdic.c ex_data.c mem.c mem_dbg.c \
	tmdiff.c

# asn1
SRCS+=	a_bitstr.c a_bmp.c a_bool.c a_bytes.c a_d2i_fp.c a_digest.c \
	a_dup.c a_enum.c a_gentm.c a_hdr.c a_i2d_fp.c a_int.c a_mbstr.c \
	a_meth.c a_null.c a_object.c a_octet.c a_print.c a_set.c a_sign.c \
	a_strnid.c a_time.c a_type.c a_utctm.c a_utf8.c a_verify.c a_vis.c \
	asn1_err.c asn1_lib.c asn1_par.c asn_pack.c d2i_dhp.c d2i_dsap.c \
	d2i_pr.c d2i_pu.c d2i_r_pr.c d2i_r_pu.c d2i_s_pr.c d2i_s_pu.c \
	evp_asn1.c f_enum.c f_int.c f_string.c i2d_dhp.c i2d_dsap.c i2d_pr.c \
	i2d_pu.c i2d_r_pr.c i2d_r_pu.c i2d_s_pr.c i2d_s_pu.c n_pkey.c nsseq.c \
	p5_pbe.c p5_pbev2.c p7_dgst.c p7_enc.c p7_enc_c.c p7_evp.c p7_i_s.c \
	p7_lib.c p7_recip.c p7_s_e.c p7_signd.c p7_signi.c p8_pkey.c \
	t_bitst.c t_crl.c t_pkey.c t_req.c t_spki.c t_x509.c t_x509a.c \
	x_algor.c x_attrib.c x_cinf.c x_crl.c x_exten.c x_info.c x_name.c \
	x_pkey.c x_pubkey.c x_req.c x_sig.c x_spki.c x_val.c x_x509.c \
	x_x509a.c \

# blowfish
SRCS+=	bf_cfb64.c bf_ecb.c bf_enc.c bf_ofb64.c bf_skey.c

# bio
SRCS+=	b_dump.c b_print.c b_sock.c bf_buff.c bf_nbio.c bf_null.c \
	bio_cb.c bio_err.c bio_lib.c bss_acpt.c bss_bio.c bss_conn.c \
	bss_fd.c bss_file.c bss_log.c bss_mem.c bss_null.c bss_sock.c

# bn
SRCS+=	bn_add.c bn_asm.c bn_blind.c bn_ctx.c bn_div.c bn_err.c bn_exp.c \
	bn_exp2.c bn_gcd.c bn_lib.c bn_mont.c bn_mpi.c bn_mul.c bn_prime.c \
	bn_print.c bn_rand.c bn_recp.c bn_shift.c bn_sqr.c bn_word.c

# buffer
SRCS+=	buf_err.c buffer.c 

# cast
SRCS+=	c_cfb64.c c_ecb.c c_enc.c c_ofb64.c c_skey.c

# comp
SRCS+=	c_rle.c c_zlib.c comp_lib.c

# conf
SRCS+=	conf.c conf_err.c

# des
SRCS+=	cbc_cksm.c cbc_enc.c cfb64ede.c cfb64enc.c cfb_enc.c des_enc.c \
	ecb3_enc.c ecb_enc.c ede_cbcm_enc.c enc_read.c enc_writ.c fcrypt.c \
	fcrypt_b.c ofb64ede.c ofb64enc.c ofb_enc.c pcbc_enc.c qud_cksm.c \
	rand_key.c read2pwd.c read_pwd.c rnd_keys.c rpc_enc.c set_key.c \
	str2key.c supp.c xcbc_enc.c 

# dh
SRCS+=	dh_check.c dh_err.c dh_gen.c dh_key.c dh_lib.c

# dsa 
SRCS+=	dsa_asn1.c dsa_err.c dsa_gen.c dsa_key.c dsa_lib.c dsa_ossl.c \
	dsa_sign.c dsa_vrf.c

# err
SRCS+=	err.c err_all.c err_prn.c

# evp
SRCS+=	bio_b64.c bio_enc.c bio_md.c bio_ok.c c_all.c c_allc.c c_alld.c \
	digest.c e_cbc_3d.c e_cbc_bf.c e_cbc_c.c e_cbc_d.c e_cbc_i.c \
	e_cbc_r2.c e_cbc_r5.c e_cfb_3d.c e_cfb_bf.c e_cfb_c.c e_cfb_d.c \
	e_cfb_i.c e_cfb_r2.c e_cfb_r5.c e_ecb_3d.c e_ecb_bf.c e_ecb_c.c \
	e_ecb_d.c e_ecb_i.c e_ecb_r2.c e_ecb_r5.c e_null.c e_ofb_3d.c \
	e_ofb_bf.c e_ofb_c.c e_ofb_d.c e_ofb_i.c e_ofb_r2.c e_ofb_r5.c \
	e_rc4.c e_xcbc_d.c encode.c evp_enc.c evp_err.c evp_key.c \
	evp_lib.c evp_pbe.c evp_pkey.c m_dss.c m_dss1.c m_md2.c m_md5.c \
	m_mdc2.c m_null.c m_ripemd.c m_sha.c m_sha1.c names.c p5_crpt.c \
	p5_crpt2.c p_dec.c p_enc.c p_lib.c p_open.c p_seal.c p_sign.c \
	p_verify.c

# hmac
SRCS+=	hmac.c

# idea
.if defined(MAKE_IDEA) && ${MAKE_IDEA} == YES
SRCS+=	i_ecb.c i_cbc.c i_cfb64.c i_ofb64.c i_skey.c
.endif

# lhash
SRCS+=	lh_stats.c lhash.c

# md2
SRCS+=	md2_dgst.c md2_one.c

# md5
SRCS+=	md5_dgst.c md5_one.c

# mdc2
SRCS+=	mdc2dgst.c mdc2_one.c

# objects
SRCS+=	o_names.c obj_dat.c obj_err.c obj_lib.c

# pem
SRCS+=	pem_all.c pem_err.c pem_info.c pem_lib.c pem_seal.c pem_sign.c

# pkcs7
SRCS+=	pk7_attr.c pk7_doit.c pk7_lib.c pk7_mime.c pk7_smime.c pkcs7err.c 

# pkcs12
SRCS+=	p12_add.c p12_attr.c p12_bags.c p12_crpt.c p12_crt.c p12_decr.c \
	p12_init.c p12_key.c p12_kiss.c p12_lib.c p12_mac.c p12_mutl.c \
	p12_npas.c p12_sbag.c p12_utl.c pk12err.c

# rand
SRCS+=	md_rand.c rand_egd.c rand_err.c rand_lib.c randfile.c

# rc2
SRCS+=	rc2_cbc.c rc2cfb64.c rc2_ecb.c rc2ofb64.c rc2_skey.c 

# rc4
SRCS+=	rc4_enc.c rc4_skey.c

# rc5
SRCS+=	rc5cfb64.c rc5_ecb.c rc5_enc.c rc5ofb64.c rc5_skey.c 

# ripemd
SRCS+=	rmd_dgst.c rmd_one.c

# rsa
.if defined(WITH_RSA) && ${WITH_RSA} == YES
SRCS+=	rsa_chk.c rsa_eay.c rsa_err.c rsa_gen.c rsa_lib.c rsa_none.c \
	rsa_null.c rsa_oaep.c rsa_pk1.c rsa_saos.c rsa_sign.c rsa_ssl.c
.endif

# sha
SRCS+=	sha_dgst.c sha_one.c sha1_one.c sha1dgst.c

# stack
SRCS+=	stack.c

# txt_db
SRCS+=	txt_db.c

# x509
SRCS+=	by_dir.c by_file.c x509_att.c x509_cmp.c x509_d2.c x509_def.c \
	x509_err.c x509_ext.c x509_lu.c x509_obj.c x509_r2x.c x509_req.c \
	x509_set.c x509_trs.c x509_txt.c x509_v3.c x509_vfy.c x509name.c \
	x509rset.c x509spki.c x509type.c x_all.c

# x509v3
SRCS+=	v3_akey.c v3_alt.c v3_bcons.c v3_bitst.c v3_conf.c v3_cpols.c \
	v3_crld.c v3_enum.c v3_extku.c v3_genn.c v3_ia5.c v3_info.c v3_int.c \
	v3_lib.c v3_pku.c v3_prn.c v3_purp.c v3_skey.c v3_sxnet.c v3_utl.c \
	v3err.c

POD1+=	apps/CA.pl.pod apps/asn1parse.pod apps/ca.pod apps/ciphers.pod \
	apps/config.pod apps/crl.pod apps/crl2pkcs7.pod apps/dgst.pod \
	apps/dhparam.pod apps/dsa.pod apps/dsaparam.pod apps/enc.pod \
	apps/gendsa.pod apps/genrsa.pod apps/nseq.pod apps/openssl.pod \
	apps/passwd.pod apps/pkcs12.pod apps/pkcs7.pod apps/pkcs8.pod \
	apps/req.pod apps/rsa.pod apps/s_client.pod apps/s_server.pod \
	apps/sess_id.pod apps/smime.pod apps/speed.pod apps/spkac.pod \
	apps/verify.pod apps/version.pod apps/x509.pod

POD3+=	crypto/BN_CTX_new.pod crypto/BN_CTX_start.pod crypto/BN_add.pod \
	crypto/BN_add_word.pod crypto/BN_bn2bin.pod crypto/BN_cmp.pod \
	crypto/BN_copy.pod crypto/BN_generate_prime.pod \
	crypto/BN_mod_inverse.pod crypto/BN_mod_mul_montgomery.pod \
	crypto/BN_mod_mul_reciprocal.pod crypto/BN_new.pod \
	crypto/BN_num_bytes.pod crypto/BN_rand.pod crypto/BN_set_bit.pod \
	crypto/BN_zero.pod crypto/CRYPTO_set_ex_data.pod \
	crypto/DH_generate_key.pod crypto/DH_generate_parameters.pod \
	crypto/DH_get_ex_new_index.pod crypto/DH_new.pod \
	crypto/DH_set_method.pod crypto/DH_size.pod crypto/DSA_SIG_new.pod \
	crypto/DSA_do_sign.pod crypto/DSA_dup_DH.pod \
	crypto/DSA_generate_key.pod crypto/DSA_generate_parameters.pod \
	crypto/DSA_get_ex_new_index.pod crypto/DSA_new.pod \
	crypto/DSA_set_method.pod crypto/DSA_sign.pod crypto/DSA_size.pod \
	crypto/ERR_GET_LIB.pod crypto/ERR_clear_error.pod \
	crypto/ERR_error_string.pod crypto/ERR_get_error.pod \
	crypto/ERR_load_crypto_strings.pod crypto/ERR_load_strings.pod \
	crypto/ERR_print_errors.pod crypto/ERR_put_error.pod \
	crypto/ERR_remove_state.pod crypto/EVP_DigestInit.pod \
	crypto/EVP_EncryptInit.pod crypto/OPENSSL_VERSION_NUMBER.pod \
	crypto/OpenSSL_add_all_algorithms.pod crypto/RAND_add.pod \
	crypto/RAND_bytes.pod crypto/RAND_cleanup.pod crypto/RAND_egd.pod \
	crypto/RAND_load_file.pod crypto/RAND_set_rand_method.pod \
	crypto/RSA_blinding_on.pod crypto/RSA_check_key.pod \
	crypto/RSA_generate_key.pod crypto/RSA_get_ex_new_index.pod \
	crypto/RSA_new.pod crypto/RSA_padding_add_PKCS1_type_1.pod \
	crypto/RSA_print.pod crypto/RSA_private_encrypt.pod \
	crypto/RSA_public_encrypt.pod crypto/RSA_set_method.pod \
	crypto/RSA_sign.pod crypto/RSA_sign_ASN1_OCTET_STRING.pod \
	crypto/RSA_size.pod crypto/blowfish.pod crypto/bn.pod \
	crypto/bn_internal.pod crypto/buffer.pod crypto/crypto.pod \
	crypto/d2i_DHparams.pod crypto/d2i_RSAPublicKey.pod \
	crypto/des_modes.pod crypto/dh.pod crypto/dsa.pod crypto/err.pod \
	crypto/hmac.pod crypto/lh_stats.pod crypto/lhash.pod crypto/md5.pod \
	crypto/mdc2.pod crypto/rand.pod crypto/rc4.pod crypto/ripemd.pod \
	crypto/rsa.pod crypto/sha.pod crypto/threads.pod

POD3+=	ssl/SSL_get_error.pod ssl/ssl.pod

#.for section in 1 3
#.for pod in ${POD${section}}
#.for target in ${pod:T:S/.pod/.${section}/g}
#MAN${section}+= ${target}
#CLEANFILES+= ${target}
#all-man: ${target}
#${target}: ${LCRYPTO_SRC}/../doc/${pod}
#	pod2man ${LCRYPTO_SRC}/../doc/${pod} > ${target}
#.endfor
#.endfor
#.endfor

MAN3=	des_crypt.3

MLINKS= des_crypt.3 des_read_password.3 \
	des_crypt.3 des_read_2password.3 des_crypt.3 des_string_to_key.3 \
	des_crypt.3 des_string_to_2key.3 des_crypt.3 des_read_pw_string.3 \
	des_crypt.3 des_random_key.3 des_crypt.3 des_set_key.3 \
	des_crypt.3 des_key_sched.3 des_crypt.3 des_ecb_encrypt.3 \
	des_crypt.3 des_3ecb_encrypt.3 des_crypt.3 des_cbc_encrypt.3 \
	des_crypt.3 des_3cbc_encrypt.3 des_crypt.3 des_pcbc_encrypt.3 \
	des_crypt.3 des_cfb_encrypt.3 des_crypt.3 des_ofb_encrypt.3 \
	des_crypt.3 des_cbc_cksum.3 des_crypt.3 des_quad_cksum.3 \
	des_crypt.3 des_enc_read.3 des_crypt.3 des_enc_write.3 \
	des_crypt.3 des_set_odd_parity.3 des_crypt.3 des_is_weak_key.3

beforeinstall:  openssl/opensslconf.h openssl/evp.h
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${CRYPTO_HDRS} openssl/evp.h openssl/opensslconf.h \
	    ${DESTDIR}/usr/include/openssl

.include <bsd.lib.mk>

afterinstall:
.if !defined(NOPIC)
	@cd ${DESTDIR}${SHLIBDIR}; \
	rm -f libdes.so.3; \
	ln -sf lib${LIB}.so.${SHLIB_MAJOR} libdes.so.3;
.endif
.if !defined(NOPIC) && ${OBJFORMAT} == elf
	@cd ${DESTDIR}${SHLIBDIR}; \
	rm -f libdes.so; \
	ln -sf lib${LIB}.so libdes.so
.endif
	@cd  ${DESTDIR}${LIBDIR}; \
	rm -f libdes.a; \
	ln -sf lib${LIB}.a libdes.a
	@cd  ${DESTDIR}${INCDIR}; \
	rm -f des.h; \
	ln -sf openssl/des.h
.if !defined(NOPROFILE)
	@cd  ${DESTDIR}${LIBDIR}; \
	rm -f libdes_p.a; \
	ln -sf lib${LIB}_p.a libdes_p.a
.endif
