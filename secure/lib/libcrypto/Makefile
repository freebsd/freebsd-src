# $FreeBSD$

.include "Makefile.inc"

.PATH:	${LCRYPTO_SRC} ${LCRYPTO_SRC}/asn1 ${LCRYPTO_SRC}/bf \
	${LCRYPTO_SRC}/bio ${LCRYPTO_SRC}/bn ${LCRYPTO_SRC}/buffer \
	${LCRYPTO_SRC}/cast ${LCRYPTO_SRC}/comp ${LCRYPTO_SRC}/conf \
	${LCRYPTO_SRC}/des ${LCRYPTO_SRC}/dh ${LCRYPTO_SRC}/dsa \
	${LCRYPTO_SRC}/dso ${LCRYPTO_SRC}/err ${LCRYPTO_SRC}/evp \
	${LCRYPTO_SRC}/hmac ${LCRYPTO_SRC}/lhash ${LCRYPTO_SRC}/md2 \
	${LCRYPTO_SRC}/md4 ${LCRYPTO_SRC}/md5 ${LCRYPTO_SRC}/mdc2 \
	${LCRYPTO_SRC}/objects ${LCRYPTO_SRC}/pem ${LCRYPTO_SRC}/pkcs7 \
	${LCRYPTO_SRC}/pkcs12 ${LCRYPTO_SRC}/rand ${LCRYPTO_SRC}/rc2 \
	${LCRYPTO_SRC}/rc4 ${LCRYPTO_SRC}/rc5 ${LCRYPTO_SRC}/ripemd \
	${LCRYPTO_SRC}/rsa ${LCRYPTO_SRC}/../rsaref ${LCRYPTO_SRC}/sha \
	${LCRYPTO_SRC}/stack ${LCRYPTO_SRC}/txt_db ${LCRYPTO_SRC}/x509 \
	${LCRYPTO_SRC}/x509v3

.if !defined(NOPERL) && ${MACHINE_ARCH} == "i386"
.PATH:	${LCRYPTO_SRC}/rc4/asm ${LCRYPTO_SRC}/rc5/asm \
	${LCRYPTO_SRC}/des/asm ${LCRYPTO_SRC}/cast/asm \
	${LCRYPTO_SRC}/sha/asm ${LCRYPTO_SRC}/bn/asm \
	${LCRYPTO_SRC}/bf/asm ${LCRYPTO_SRC}/md5/asm \
	${LCRYPTO_SRC}/ripemd/asm
PERLPATH=	${LCRYPTO_SRC}/des/asm:${LCRYPTO_SRC}/perlasm
.endif

.if defined(MAKE_IDEA) && ${MAKE_IDEA} == YES
.PATH:  ${LCRYPTO_SRC}/idea
.endif

LIB=		crypto
SHLIB_MAJOR=	2

MAINTAINER=	kris

# base sources
SRCS+=	cpt_err.c cryptlib.c cversion.c ebcdic.c ex_data.c mem.c mem_dbg.c \
	tmdiff.c uid.c

# asn1

SRCS+=	a_bitstr.c a_bmp.c a_bool.c a_bytes.c a_d2i_fp.c a_digest.c \
	a_dup.c a_enum.c a_gentm.c a_hdr.c a_i2d_fp.c a_int.c \
	a_mbstr.c a_meth.c a_null.c a_object.c a_octet.c a_print.c \
	a_set.c a_sign.c a_strex.c a_strnid.c a_time.c a_type.c \
	a_utctm.c a_utf8.c a_verify.c a_vis.c asn1_err.c asn1_lib.c \
	asn1_par.c asn_pack.c d2i_dhp.c d2i_dsap.c d2i_pr.c d2i_pu.c \
	d2i_r_pr.c d2i_r_pu.c d2i_s_pr.c d2i_s_pu.c evp_asn1.c \
	f_enum.c f_int.c f_string.c i2d_dhp.c i2d_dsap.c i2d_pr.c \
	i2d_pu.c i2d_r_pr.c i2d_r_pu.c i2d_s_pr.c i2d_s_pu.c n_pkey.c \
	nsseq.c p5_pbe.c p5_pbev2.c p7_dgst.c p7_enc.c p7_enc_c.c \
	p7_evp.c p7_i_s.c p7_lib.c p7_recip.c p7_s_e.c p7_signd.c \
	p7_signi.c p8_pkey.c t_bitst.c t_crl.c t_pkey.c t_req.c \
	t_spki.c t_x509.c t_x509a.c x_algor.c x_attrib.c x_cinf.c \
	x_crl.c x_exten.c x_info.c x_name.c x_pkey.c x_pubkey.c \
	x_req.c x_sig.c x_spki.c x_val.c x_x509.c x_x509a.c

# blowfish
SRCS+=	bf_cfb64.c bf_ecb.c bf_ofb64.c bf_skey.c
.if !defined(NOPERL) && ${MACHINE_ARCH} == "i386"
.if ${MACHINE_CPU:Mi686}
SRCS+=	bf-686.pl
.else
SRCS+=	bf-586.pl
.endif
.else
SRCS+=	bf_enc.c
.endif

# bio
SRCS+=	b_dump.c b_print.c b_sock.c bf_buff.c bf_nbio.c bf_null.c \
	bio_cb.c bio_err.c bio_lib.c bss_acpt.c bss_bio.c bss_conn.c \
	bss_fd.c bss_file.c bss_log.c bss_mem.c bss_null.c bss_sock.c

# bn

SRCS+=	bn_add.c bn_blind.c bn_ctx.c bn_div.c bn_err.c \
	bn_exp.c bn_exp2.c bn_gcd.c bn_lib.c bn_mont.c bn_mpi.c \
	bn_mul.c bn_prime.c bn_print.c bn_rand.c bn_recp.c bn_shift.c \
	bn_sqr.c bn_word.c
.if !defined(NOPERL) && ${MACHINE_ARCH} == "i386"
SRCS+=	bn-586.pl co-586.pl
.else
SRCS+=	bn_asm.c
.endif

# buffer
SRCS+=	buf_err.c buffer.c 

# cast
SRCS+=	c_cfb64.c c_ecb.c c_ofb64.c c_skey.c
.if !defined(NOPERL) && ${MACHINE_ARCH} == "i386"
SRCS+=	cast-586.pl
.else
SRCS+=	c_enc.c
.endif

# comp
SRCS+=	c_rle.c c_zlib.c comp_lib.c

# conf
SRCS+=	conf_api.c conf_def.c conf_err.c conf_lib.c

# des
SRCS+=	cbc_cksm.c cbc_enc.c cfb64ede.c cfb64enc.c cfb_enc.c \
	ecb3_enc.c ecb_enc.c ede_cbcm_enc.c enc_read.c enc_writ.c \
	fcrypt.c ofb64ede.c ofb64enc.c ofb_enc.c pcbc_enc.c \
	qud_cksm.c rand_key.c read2pwd.c read_pwd.c rpc_enc.c \
	set_key.c str2key.c xcbc_enc.c rnd_keys.c
.if !defined(NOPERL) && ${MACHINE_ARCH} == "i386"
SRCS+=	des-586.pl crypt586.pl
.else
SRCS+=	des_enc.c fcrypt_b.c
.endif

# dh
SRCS+=	dh_check.c dh_err.c dh_gen.c dh_key.c dh_lib.c

# dsa 
SRCS+=	dsa_asn1.c dsa_err.c dsa_gen.c dsa_key.c dsa_lib.c dsa_ossl.c \
	dsa_sign.c dsa_vrf.c

# dso
SRCS+=	dso_dl.c dso_dlfcn.c dso_err.c dso_lib.c dso_null.c \
	dso_openssl.c

# err
SRCS+=	err.c err_all.c err_prn.c

# evp
SRCS+=	bio_b64.c bio_enc.c bio_md.c bio_ok.c c_all.c c_allc.c c_alld.c \
	digest.c e_bf.c e_cast.c e_des.c e_des3.c e_idea.c e_null.c \
	e_rc2.c e_rc4.c e_rc5.c e_xcbc_d.c encode.c evp_enc.c \
	evp_err.c evp_key.c evp_lib.c evp_pbe.c evp_pkey.c m_dss.c \
	m_dss1.c m_md2.c m_md4.c m_md5.c m_mdc2.c m_null.c m_ripemd.c \
	m_sha.c m_sha1.c names.c p5_crpt.c p5_crpt2.c p_dec.c p_enc.c \
	p_lib.c p_open.c p_seal.c p_sign.c p_verify.c

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

# md4
SRCS+=	md4_dgst.c md4_one.c

# md5
SRCS+=	md5_dgst.c md5_one.c
.if !defined(NOPERL) && ${MACHINE_ARCH} == "i386"
SRCS+=	md5-586.pl
.endif

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
SRCS+=	md_rand.c rand_egd.c rand_err.c rand_lib.c rand_win.c randfile.c

# rc2
SRCS+=	rc2_cbc.c rc2cfb64.c rc2_ecb.c rc2ofb64.c rc2_skey.c 

# rc4
SRCS+=	rc4_skey.c
.if !defined(NOPERL) && ${MACHINE_ARCH} == "i386"
SRCS+=	rc4-586.pl
.else
SRCS+=	rc4_enc.c
.endif

# rc5
SRCS+=	rc5cfb64.c rc5_ecb.c rc5ofb64.c rc5_skey.c
.if !defined(NOPERL) && ${MACHINE_ARCH} == "i386"
SRCS+=	rc5-586.pl
.else
SRCS+=	rc5_enc.c
.endif

# ripemd
SRCS+=	rmd_dgst.c rmd_one.c
.if !defined(NOPERL) && ${MACHINE_ARCH} == "i386"
SRCS+=	rmd-586.pl
.endif

# rsa
.if defined(WITH_RSA) && ${WITH_RSA} == YES
SRCS+=	rsa_chk.c rsa_eay.c rsa_err.c rsa_gen.c rsa_lib.c rsa_none.c \
	rsa_null.c rsa_oaep.c rsa_pk1.c rsa_saos.c rsa_sign.c rsa_ssl.c
.endif

# sha
SRCS+=	sha_dgst.c sha_one.c sha1_one.c sha1dgst.c
.if !defined(NOPERL) && ${MACHINE_ARCH} == "i386"
SRCS+=	sha1-586.pl
.endif

# stack
SRCS+=	stack.c

# txt_db
SRCS+=	txt_db.c

# x509
SRCS+=	by_dir.c by_file.c x509_att.c x509_cmp.c x509_d2.c x509_def.c \
	x509_err.c x509_ext.c x509_lu.c x509_obj.c x509_r2x.c \
	x509_req.c x509_set.c x509_trs.c x509_txt.c x509_v3.c \
	x509_vfy.c x509name.c x509rset.c x509spki.c x509type.c x_all.c 
# x509v3
SRCS+=	v3_akey.c v3_alt.c v3_bcons.c v3_bitst.c v3_conf.c v3_cpols.c \
	v3_crld.c v3_enum.c v3_extku.c v3_genn.c v3_ia5.c v3_info.c \
	v3_int.c v3_lib.c v3_pku.c v3_prn.c v3_purp.c v3_skey.c \
	v3_sxnet.c v3_utl.c v3err.c

POD1+=	apps/CA.pl.pod apps/asn1parse.pod apps/ca.pod \
	apps/ciphers.pod apps/config.pod apps/crl.pod \
	apps/crl2pkcs7.pod apps/dgst.pod apps/dhparam.pod apps/dsa.pod \
	apps/dsaparam.pod apps/enc.pod apps/gendsa.pod apps/genrsa.pod \
	apps/nseq.pod apps/openssl.pod apps/passwd.pod apps/pkcs12.pod \
	apps/pkcs7.pod apps/pkcs8.pod apps/rand.pod apps/req.pod \
	apps/rsa.pod apps/rsautl.pod apps/s_client.pod \
	apps/s_server.pod apps/sess_id.pod apps/smime.pod \
	apps/speed.pod apps/spkac.pod apps/verify.pod apps/version.pod \
	apps/x509.pod

POD3+=	crypto/BIO_ctrl.pod crypto/BIO_f_base64.pod \
	crypto/BIO_f_buffer.pod crypto/BIO_f_cipher.pod \
	crypto/BIO_f_md.pod crypto/BIO_f_null.pod crypto/BIO_f_ssl.pod \
	crypto/BIO_find_type.pod crypto/BIO_new.pod \
	crypto/BIO_new_bio_pair.pod crypto/BIO_push.pod \
	crypto/BIO_read.pod crypto/BIO_s_accept.pod \
	crypto/BIO_s_bio.pod crypto/BIO_s_connect.pod \
	crypto/BIO_s_fd.pod crypto/BIO_s_file.pod crypto/BIO_s_mem.pod \
	crypto/BIO_s_null.pod crypto/BIO_s_socket.pod \
	crypto/BIO_set_callback.pod crypto/BIO_should_retry.pod \
	crypto/BN_CTX_new.pod crypto/BN_CTX_start.pod \
	crypto/BN_add.pod crypto/BN_add_word.pod crypto/BN_bn2bin.pod \
	crypto/BN_cmp.pod crypto/BN_copy.pod \
	crypto/BN_generate_prime.pod crypto/BN_mod_inverse.pod \
	crypto/BN_mod_mul_montgomery.pod \
	crypto/BN_mod_mul_reciprocal.pod crypto/BN_new.pod \
	crypto/BN_num_bytes.pod crypto/BN_rand.pod \
	crypto/BN_set_bit.pod crypto/BN_zero.pod \
	crypto/CRYPTO_set_ex_data.pod crypto/DH_generate_key.pod \
	crypto/DH_generate_parameters.pod \
	crypto/DH_get_ex_new_index.pod crypto/DH_new.pod \
	crypto/DH_set_method.pod crypto/DH_size.pod \
	crypto/DSA_SIG_new.pod crypto/DSA_do_sign.pod \
	crypto/DSA_dup_DH.pod crypto/DSA_generate_key.pod \
	crypto/DSA_generate_parameters.pod \
	crypto/DSA_get_ex_new_index.pod crypto/DSA_new.pod \
	crypto/DSA_set_method.pod crypto/DSA_sign.pod \
	crypto/DSA_size.pod crypto/ERR_GET_LIB.pod \
	crypto/ERR_clear_error.pod crypto/ERR_error_string.pod \
	crypto/ERR_get_error.pod crypto/ERR_load_crypto_strings.pod \
	crypto/ERR_load_strings.pod crypto/ERR_print_errors.pod \
	crypto/ERR_put_error.pod crypto/ERR_remove_state.pod \
	crypto/EVP_DigestInit.pod crypto/EVP_EncryptInit.pod \
	crypto/EVP_OpenInit.pod crypto/EVP_SealInit.pod \
	crypto/EVP_SignInit.pod crypto/EVP_VerifyInit.pod \
	crypto/OPENSSL_VERSION_NUMBER.pod \
	crypto/OpenSSL_add_all_algorithms.pod crypto/RAND_add.pod \
	crypto/RAND_bytes.pod crypto/RAND_cleanup.pod \
	crypto/RAND_egd.pod crypto/RAND_load_file.pod \
	crypto/RAND_set_rand_method.pod crypto/RSA_blinding_on.pod \
	crypto/RSA_check_key.pod crypto/RSA_generate_key.pod \
	crypto/RSA_get_ex_new_index.pod crypto/RSA_new.pod \
	crypto/RSA_padding_add_PKCS1_type_1.pod crypto/RSA_print.pod \
	crypto/RSA_private_encrypt.pod crypto/RSA_public_encrypt.pod \
	crypto/RSA_set_method.pod crypto/RSA_sign.pod \
	crypto/RSA_sign_ASN1_OCTET_STRING.pod crypto/RSA_size.pod \
	crypto/bio.pod crypto/blowfish.pod crypto/bn.pod \
	crypto/bn_internal.pod crypto/buffer.pod crypto/crypto.pod \
	crypto/d2i_DHparams.pod crypto/d2i_RSAPublicKey.pod \
	crypto/des.pod crypto/des_modes.pod crypto/dh.pod \
	crypto/dsa.pod crypto/err.pod crypto/evp.pod crypto/hmac.pod \
	crypto/lh_stats.pod crypto/lhash.pod crypto/md5.pod \
	crypto/mdc2.pod crypto/rand.pod crypto/rc4.pod \
	crypto/ripemd.pod crypto/rsa.pod crypto/sha.pod \
	crypto/threads.pod

POD3+=	ssl/SSL_CIPHER_get_name.pod \
	ssl/SSL_CTX_add_extra_chain_cert.pod \
	ssl/SSL_CTX_add_session.pod ssl/SSL_CTX_flush_sessions.pod \
	ssl/SSL_CTX_free.pod ssl/SSL_CTX_get_ex_new_index.pod \
	ssl/SSL_CTX_get_verify_mode.pod \
	ssl/SSL_CTX_load_verify_locations.pod ssl/SSL_CTX_new.pod \
	ssl/SSL_CTX_sess_set_cache_size.pod \
	ssl/SSL_CTX_sess_set_get_cb.pod ssl/SSL_CTX_sessions.pod \
	ssl/SSL_CTX_set_cipher_list.pod \
	ssl/SSL_CTX_set_client_CA_list.pod \
	ssl/SSL_CTX_set_default_passwd_cb.pod \
	ssl/SSL_CTX_set_options.pod \
	ssl/SSL_CTX_set_session_cache_mode.pod \
	ssl/SSL_CTX_set_session_id_context.pod \
	ssl/SSL_CTX_set_ssl_version.pod ssl/SSL_CTX_set_timeout.pod \
	ssl/SSL_CTX_set_verify.pod ssl/SSL_CTX_use_certificate.pod \
	ssl/SSL_SESSION_free.pod ssl/SSL_SESSION_get_ex_new_index.pod \
	ssl/SSL_SESSION_get_time.pod ssl/SSL_accept.pod \
	ssl/SSL_clear.pod ssl/SSL_connect.pod ssl/SSL_free.pod \
	ssl/SSL_get_ciphers.pod ssl/SSL_get_client_CA_list.pod \
	ssl/SSL_get_current_cipher.pod ssl/SSL_get_error.pod \
	ssl/SSL_get_ex_data_X509_STORE_CTX_idx.pod \
	ssl/SSL_get_ex_new_index.pod ssl/SSL_get_fd.pod \
	ssl/SSL_get_peer_cert_chain.pod \
	ssl/SSL_get_peer_certificate.pod ssl/SSL_get_rbio.pod \
	ssl/SSL_get_session.pod ssl/SSL_get_verify_result.pod \
	ssl/SSL_library_init.pod ssl/SSL_load_client_CA_file.pod \
	ssl/SSL_new.pod ssl/SSL_pending.pod ssl/SSL_read.pod \
	ssl/SSL_set_bio.pod ssl/SSL_set_fd.pod ssl/SSL_set_session.pod \
	ssl/SSL_set_verify_result.pod ssl/SSL_shutdown.pod \
	ssl/SSL_write.pod ssl/d2i_SSL_SESSION.pod ssl/ssl.pod \
	ssl/SSL_CTX_sess_number.pod ssl/SSL_CTX_set_mode.pod \
	ssl/SSL_get_version.pod ssl/SSL_set_connect_state.pod \
	ssl/SSL_set_shutdown.pod

.if defined(WANT_OPENSSL_MANPAGES)
.for section in 1 3
.for pod in ${POD${section}}
.for target in ${pod:T:S/.pod/.${section}/g}
MAN+= ${target}
CLEANFILES+= ${target}
${target}: ${LCRYPTO_SRC}/../doc/${pod}
	pod2man ${LCRYPTO_SRC}/../doc/${pod} > ${target}
.endfor
.endfor
.endfor
.endif

MAN+=	des_crypt.3

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

INCS=		${HDRS} openssl/evp.h openssl/opensslconf.h
INCSDIR=	${INCLUDEDIR}/openssl
INCSLINKS=	openssl/des.h ${INCLUDEDIR}/des.h

afterinstall:
.if !defined(NOPIC)
SYMLINKS+=	lib${LIB}.so.${SHLIB_MAJOR} ${LIBDIR}/libdes.so.3
SYMLINKS+=	lib${LIB}.so.${SHLIB_MAJOR} ${LIBDIR}/libdes.so
.endif
SYMLINKS+=	lib${LIB}.a ${LIBDIR}/libdes.a
.if !defined(NOPROFILE)
SYMLINKS+=	lib${LIB}_p.a ${LIBDIR}/libdes_p.a
.endif

.include <bsd.lib.mk>

.if !defined(NOPERL) && ${MACHINE_ARCH} == "i386"
CLEANFILES+=	${SRCS:M*.pl:S/.pl$/.cmt/} ${SRCS:M*.pl:S/.pl$/.s/}
.SUFFIXES:	.pl .cmt
.pl.cmt:
	perl -I${PERLPATH} ${.ALLSRC} elf ${CPUTYPE:Mi386:S/i//} > ${.TARGET}

.cmt.s:
	tr -d "'" < ${.ALLSRC} > ${.TARGET}
.endif

