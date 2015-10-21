# $FreeBSD$

SHLIBDIR?=	/lib
SUBDIR=		engines

.include <bsd.own.mk>

LIB=		crypto
SHLIB_MAJOR=	7
ALLOW_SHARED_TEXTREL=

NO_LINT=

.if exists(Makefile.man)
.include "Makefile.man"
.endif
.if defined(NOTYET)
MAN+=	config.5 des_modes.7
.endif

.include "Makefile.inc"

# base sources
SRCS=	cpt_err.c cryptlib.c cversion.c ex_data.c mem.c mem_dbg.c o_dir.c \
	o_fips.c o_init.c o_str.c o_time.c uid.c
.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	x86_64cpuid.S
.elif ${MACHINE_CPUARCH} == "i386"
SRCS+=	x86cpuid.s
.else
SRCS+=	mem_clr.c
.endif
INCS+=	crypto.h ebcdic.h opensslv.h ossl_typ.h symhacks.h ../e_os2.h

# aes
SRCS+=	aes_cfb.c aes_ctr.c aes_ecb.c aes_ige.c aes_misc.c aes_ofb.c aes_wrap.c
.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	aes-x86_64.S aesni-sha1-x86_64.S aesni-x86_64.S bsaes-x86_64.S \
	vpaes-x86_64.S
.elif ${MACHINE_CPUARCH} == "i386"
SRCS+=	aes-586.s aesni-x86.s vpaes-x86.s
.else
SRCS+=	aes_cbc.c aes_core.c
.endif
INCS+=	aes.h

# asn1
SRCS+=	a_bitstr.c a_bool.c a_bytes.c a_d2i_fp.c a_digest.c a_dup.c a_enum.c \
	a_gentm.c a_i2d_fp.c a_int.c a_mbstr.c a_object.c a_octet.c a_print.c \
	a_set.c a_sign.c a_strex.c a_strnid.c a_time.c a_type.c a_utctm.c \
	a_utf8.c a_verify.c ameth_lib.c asn1_err.c asn1_gen.c asn1_lib.c \
	asn1_par.c asn_mime.c asn_moid.c asn_pack.c bio_asn1.c bio_ndef.c \
	d2i_pr.c d2i_pu.c evp_asn1.c f_enum.c f_int.c f_string.c i2d_pr.c \
	i2d_pu.c n_pkey.c nsseq.c p5_pbe.c p5_pbev2.c p8_pkey.c t_bitst.c \
	t_crl.c t_pkey.c t_req.c t_spki.c t_x509.c t_x509a.c tasn_dec.c \
	tasn_enc.c tasn_fre.c tasn_new.c tasn_prn.c tasn_typ.c tasn_utl.c \
	x_algor.c x_attrib.c x_bignum.c x_crl.c x_exten.c x_info.c x_long.c \
	x_name.c x_nx509.c x_pkey.c x_pubkey.c x_req.c x_sig.c x_spki.c \
	x_val.c x_x509.c x_x509a.c
INCS+=	asn1.h asn1_mac.h asn1t.h

# bf
SRCS+=	bf_cfb64.c bf_ecb.c bf_ofb64.c bf_skey.c
.if ${MACHINE_CPUARCH} == "i386"
.if ${MACHINE_CPU:Mi686}
SRCS+=	bf-686.s
.else
SRCS+=	bf-586.s
.endif
.else
SRCS+=	bf_enc.c
.endif
INCS+=	blowfish.h

# bio
SRCS+=	b_dump.c b_print.c b_sock.c bf_buff.c bf_nbio.c bf_null.c bio_cb.c \
	bio_err.c bio_lib.c bss_acpt.c bss_bio.c bss_conn.c bss_dgram.c \
	bss_fd.c bss_file.c bss_log.c bss_mem.c bss_null.c bss_sock.c
INCS+=	bio.h

# bn
SRCS+=	bn_add.c bn_blind.c bn_const.c bn_ctx.c bn_depr.c bn_div.c bn_err.c \
	bn_exp.c bn_exp2.c bn_gcd.c bn_gf2m.c bn_kron.c bn_lib.c bn_mod.c \
	bn_mont.c bn_mpi.c bn_mul.c bn_nist.c bn_prime.c bn_print.c bn_rand.c \
	bn_recp.c bn_shift.c bn_sqr.c bn_sqrt.c bn_word.c bn_x931p.c
.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	modexp512-x86_64.S x86_64-gcc.c x86_64-gf2m.S x86_64-mont.S \
	x86_64-mont5.S
.elif ${MACHINE_CPUARCH} == "i386"
SRCS+=	bn-586.s co-586.s x86-gf2m.s x86-mont.s
.else
SRCS+=	bn_asm.c
.endif
INCS+=	bn.h

# buffer
SRCS+=	buf_err.c buf_str.c buffer.c
INCS+=	buffer.h

# camellia
SRCS+=	cmll_cfb.c cmll_ctr.c cmll_ecb.c cmll_ofb.c cmll_utl.c
.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	cmll_misc.c cmll-x86_64.S
.elif ${MACHINE_CPUARCH} == "i386"
SRCS+=	cmll-x86.s
.else
SRCS+=	camellia.c cmll_cbc.c cmll_misc.c
.endif
INCS+=	camellia.h

# cast
SRCS+=	c_cfb64.c c_ecb.c c_enc.c c_ofb64.c c_skey.c
INCS+=	cast.h

# cmac
SRCS+=	cm_ameth.c cm_pmeth.c cmac.c
INCS+=	cmac.h

# cms
SRCS+=	cms_asn1.c cms_att.c cms_dd.c cms_enc.c cms_env.c cms_err.c \
	cms_ess.c cms_io.c cms_lib.c cms_pwri.c cms_sd.c cms_smime.c
INCS+=	cms.h

# comp
SRCS+=	c_rle.c c_zlib.c comp_err.c comp_lib.c
INCS+=	comp.h

# conf
SRCS+=	conf_api.c conf_def.c conf_err.c conf_lib.c conf_mall.c conf_mod.c \
	conf_sap.c
INCS+=	conf.h conf_api.h

# des
SRCS+=	cbc_cksm.c cbc_enc.c cfb64ede.c cfb64enc.c cfb_enc.c des_old.c \
	des_old2.c ecb3_enc.c ecb_enc.c ede_cbcm_enc.c enc_read.c enc_writ.c \
	fcrypt.c ofb64ede.c ofb64enc.c ofb_enc.c pcbc_enc.c qud_cksm.c \
	rand_key.c read2pwd.c rpc_enc.c set_key.c str2key.c xcbc_enc.c
.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	crypt586.s des-586.s
.else
SRCS+=	des_enc.c fcrypt_b.c
.endif
INCS+=	des.h des_old.h

# dh
SRCS+=	dh_ameth.c dh_asn1.c dh_check.c dh_depr.c dh_err.c dh_gen.c dh_key.c \
	dh_lib.c dh_pmeth.c dh_prn.c
INCS+=	dh.h

# dsa
SRCS+=	dsa_ameth.c dsa_asn1.c dsa_depr.c dsa_err.c dsa_gen.c dsa_key.c \
	dsa_lib.c dsa_ossl.c dsa_pmeth.c dsa_prn.c dsa_sign.c dsa_vrf.c
INCS+=	dsa.h

# dso
SRCS+=	dso_dlfcn.c dso_err.c dso_lib.c dso_openssl.c
INCS+=	dso.h

# ec
SRCS+=	ec2_mult.c ec2_oct.c ec2_smpl.c ec_ameth.c ec_asn1.c ec_check.c \
	ec_curve.c ec_cvt.c ec_err.c ec_key.c ec_lib.c ec_mult.c ec_oct.c \
	ec_pmeth.c ec_print.c eck_prn.c ecp_mont.c ecp_nist.c ecp_oct.c \
	ecp_smpl.c
INCS+=	ec.h

# ecdh
SRCS+=	ech_err.c ech_key.c ech_lib.c ech_ossl.c
INCS+=	ecdh.h

# ecdsa
SRCS+=	ecs_asn1.c ecs_err.c ecs_lib.c ecs_ossl.c ecs_sign.c ecs_vrf.c
INCS+=	ecdsa.h

# engine
SRCS+=	eng_all.c eng_cnf.c eng_cryptodev.c eng_ctrl.c eng_dyn.c eng_err.c \
	eng_fat.c eng_init.c eng_lib.c eng_list.c eng_openssl.c eng_pkey.c \
	eng_rdrand.c eng_rsax.c eng_table.c tb_asnmth.c tb_cipher.c tb_dh.c \
	tb_digest.c tb_dsa.c tb_ecdh.c tb_ecdsa.c tb_pkmeth.c tb_rand.c \
	tb_rsa.c tb_store.c
INCS+=	engine.h

# err
SRCS+=	err.c err_all.c err_prn.c
INCS+=	err.h

# evp
SRCS+=	bio_b64.c bio_enc.c bio_md.c bio_ok.c c_all.c c_allc.c c_alld.c \
	digest.c e_aes.c e_aes_cbc_hmac_sha1.c e_bf.c e_camellia.c e_cast.c \
	e_des.c e_des3.c e_idea.c e_null.c e_old.c e_rc2.c e_rc4.c \
	e_rc4_hmac_md5.c e_rc5.c e_seed.c e_xcbc_d.c encode.c evp_acnf.c \
	evp_cnf.c evp_enc.c evp_err.c evp_key.c evp_lib.c evp_pbe.c \
	evp_pkey.c m_dss.c m_dss1.c m_ecdsa.c m_md4.c m_md5.c m_mdc2.c \
	m_null.c m_ripemd.c m_sha.c m_sha1.c m_sigver.c m_wp.c names.c \
	p5_crpt.c p5_crpt2.c p_dec.c p_enc.c p_lib.c p_open.c p_seal.c \
	p_sign.c p_verify.c pmeth_fn.c pmeth_gn.c pmeth_lib.c
INCS+=	evp.h

# hmac
SRCS+=	hm_ameth.c hm_pmeth.c hmac.c
INCS+=	hmac.h

# idea
SRCS+=	i_cbc.c i_cfb64.c i_ecb.c i_ofb64.c i_skey.c
INCS+=	idea.h

# krb5
INCS+=	krb5_asn.h

# lhash
SRCS+=	lh_stats.c lhash.c
INCS+=	lhash.h

# md4
SRCS+=	md4_dgst.c md4_one.c
INCS+=	md4.h

# md5
SRCS+=	md5_dgst.c md5_one.c
.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	md5-x86_64.S
.elif ${MACHINE_CPUARCH} == "i386"
SRCS+=	md5-586.s
.endif
INCS+=	md5.h

# mdc2
SRCS+=	mdc2_one.c mdc2dgst.c
INCS+=	mdc2.h

# modes
SRCS+=	cbc128.c ccm128.c cfb128.c ctr128.c cts128.c gcm128.c ofb128.c xts128.c
.if ${MACHINE_CPUARCH} == "amd64" 
SRCS+=	ghash-x86_64.S
.elif ${MACHINE_CPUARCH} == "i386"
SRCS+=	ghash-x86.s
.endif
INCS+=	modes.h

# objects
SRCS+=	o_names.c obj_dat.c obj_err.c obj_lib.c obj_xref.c
INCS+=	obj_mac.h objects.h

# ocsp
SRCS+=	ocsp_asn.c ocsp_cl.c ocsp_err.c ocsp_ext.c ocsp_ht.c ocsp_lib.c \
	ocsp_prn.c ocsp_srv.c ocsp_vfy.c
INCS+=	ocsp.h

# pem
SRCS+=	pem_all.c pem_err.c pem_info.c pem_lib.c pem_oth.c pem_pk8.c \
	pem_pkey.c pem_seal.c pem_sign.c pem_x509.c pem_xaux.c pvkfmt.c
INCS+=	pem.h pem2.h

# pkcs12
SRCS+=	p12_add.c p12_asn.c p12_attr.c p12_crpt.c p12_crt.c p12_decr.c \
	p12_init.c p12_key.c p12_kiss.c p12_mutl.c p12_npas.c p12_p8d.c \
	p12_p8e.c p12_utl.c pk12err.c
INCS+=	pkcs12.h

# pkcs7
SRCS+=	bio_pk7.c pk7_asn1.c pk7_attr.c pk7_doit.c pk7_lib.c pk7_mime.c \
	pk7_smime.c pkcs7err.c
INCS+=	pkcs7.h

# pqueue
SRCS+=	pqueue.c
INCS+=	pqueue.h

# rand
SRCS+=	md_rand.c rand_egd.c rand_err.c rand_lib.c rand_unix.c randfile.c
INCS+=	rand.h

# rc2
SRCS+=	rc2_cbc.c rc2_ecb.c rc2_skey.c rc2cfb64.c rc2ofb64.c
INCS+=	rc2.h

# rc4
SRCS+=	rc4_utl.c
.if ${MACHINE_CPUARCH} == "amd64" 
SRCS+=	rc4-md5-x86_64.S rc4-x86_64.S
.elif ${MACHINE_CPUARCH} == "i386"
SRCS+=	rc4-586.s
.else
SRCS+=	rc4_enc.c rc4_skey.c
.endif
INCS+=	rc4.h

# rc5
SRCS+=	rc5_ecb.c rc5_skey.c rc5cfb64.c rc5ofb64.c
.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	rc5-586.s
.else
SRCS+=	rc5_enc.c
.endif
INCS+=	rc5.h

# ripemd
SRCS+=	rmd_dgst.c rmd_one.c
.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	rmd-586.s
.endif
INCS+=	ripemd.h

# rsa
SRCS+=	rsa_ameth.c rsa_asn1.c rsa_chk.c rsa_crpt.c rsa_depr.c rsa_eay.c \
	rsa_err.c rsa_gen.c rsa_lib.c rsa_none.c rsa_null.c rsa_oaep.c \
	rsa_pk1.c rsa_pmeth.c rsa_prn.c rsa_pss.c rsa_saos.c rsa_sign.c \
	rsa_ssl.c rsa_x931.c
INCS+=	rsa.h

# seed
SRCS+=	seed.c seed_cbc.c seed_cfb.c seed_ecb.c seed_ofb.c
INCS+=	seed.h

# sha
SRCS+=	sha1_one.c sha1dgst.c sha256.c sha512.c sha_dgst.c sha_one.c
.if ${MACHINE_CPUARCH} == "amd64" 
SRCS+=	sha1-x86_64.S sha256-x86_64.S sha512-x86_64.S
.elif ${MACHINE_CPUARCH} == "i386"
SRCS+=	sha1-586.s sha256-586.s sha512-586.s
.endif
INCS+=	sha.h

# srp
SRCS+=	srp_lib.c srp_vfy.c
INCS+=	srp.h

# stack
SRCS+=	stack.c
INCS+=	safestack.h stack.h

# ts
SRCS+=	ts_asn1.c ts_conf.c ts_err.c ts_lib.c ts_req_print.c ts_req_utils.c \
	ts_rsp_print.c ts_rsp_sign.c ts_rsp_utils.c ts_rsp_verify.c \
	ts_verify_ctx.c
INCS+=	ts.h

# txt_db
SRCS+=	txt_db.c
INCS+=	txt_db.h

# ui
SRCS+=	ui_compat.c ui_err.c ui_lib.c ui_openssl.c ui_util.c
INCS+=	ui.h ui_compat.h

# whrlpool
SRCS+=	wp_dgst.c
.if ${MACHINE_CPUARCH} == "amd64" 
SRCS+=	wp-x86_64.S
.elif ${MACHINE_CPUARCH} == "i386"
SRCS+=	wp-mmx.s wp_block.c
.else
SRCS+=	wp_block.c
.endif
INCS+=	whrlpool.h

# x509
SRCS+=	by_dir.c by_file.c x509_att.c x509_cmp.c x509_d2.c x509_def.c \
	x509_err.c x509_ext.c x509_lu.c x509_obj.c x509_r2x.c x509_req.c \
	x509_set.c x509_trs.c x509_txt.c x509_v3.c x509_vfy.c x509_vpm.c \
	x509cset.c x509name.c x509rset.c x509spki.c x509type.c x_all.c
INCS+=	x509.h x509_vfy.h

# x509v3
SRCS+=	pcy_cache.c pcy_data.c pcy_lib.c pcy_map.c pcy_node.c pcy_tree.c \
	v3_addr.c v3_akey.c v3_akeya.c v3_alt.c v3_asid.c v3_bcons.c \
	v3_bitst.c v3_conf.c v3_cpols.c v3_crld.c v3_enum.c v3_extku.c \
	v3_genn.c v3_ia5.c v3_info.c v3_int.c v3_lib.c v3_ncons.c v3_ocsp.c \
	v3_pci.c v3_pcia.c v3_pcons.c v3_pku.c v3_pmaps.c v3_prn.c v3_purp.c \
	v3_skey.c v3_sxnet.c v3_utl.c v3err.c
INCS+=	x509v3.h

SRCS+=	buildinf.h
INCS+=	opensslconf.h
INCSDIR=	${INCLUDEDIR}/openssl

CSTD=	gnu89

CFLAGS+=	-I${LCRYPTO_SRC}/crypto/asn1
CFLAGS+=	-I${LCRYPTO_SRC}/crypto/evp
CFLAGS+=	-I${LCRYPTO_SRC}/crypto/modes

.if !empty(SRCS:M*.s)
AFLAGS+=	--noexecstack
.endif
.if !empty(SRCS:M*.S)
ACFLAGS+=	-Wa,--noexecstack
.endif

CLEANFILES=	buildinf.h opensslconf.h

buildinf.h: ${.CURDIR}/Makefile
	( echo "#ifndef MK1MF_BUILD"; \
	echo "    /* auto-generated by util/mkbuildinf.pl for crypto/cversion.c */"; \
	echo "    #define CFLAGS \"compiler: ${COMPILER_TYPE}\""; \
	echo "    #define PLATFORM \"platform: FreeBSD-${MACHINE_ARCH}\""; \
	echo "#endif" ) > ${.TARGET}

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
opensslconf.h: opensslconf-x86.h
.else
opensslconf.h: opensslconf-${MACHINE_CPUARCH}.h
.endif
	${CP} ${.ALLSRC} ${.TARGET}

.include <bsd.lib.mk>

.if ${MACHINE_CPUARCH} == "amd64"
.PATH: ${.CURDIR}/amd64
.elif ${MACHINE_CPUARCH} == "i386"
.PATH: ${.CURDIR}/i386
.endif

.if ${MACHINE_CPUARCH} == "amd64"
_bn_asmpath=	${LCRYPTO_SRC}/crypto/bn/asm
.endif

.PATH:	${LCRYPTO_SRC}/crypto \
	${LCRYPTO_SRC}/crypto/aes \
	${LCRYPTO_SRC}/crypto/asn1 \
	${LCRYPTO_SRC}/crypto/bf \
	${LCRYPTO_SRC}/crypto/bio \
	${LCRYPTO_SRC}/crypto/bn \
	${_bn_asmpath} \
	${LCRYPTO_SRC}/crypto/buffer \
	${LCRYPTO_SRC}/crypto/camellia \
	${LCRYPTO_SRC}/crypto/cast \
	${LCRYPTO_SRC}/crypto/cmac \
	${LCRYPTO_SRC}/crypto/cms \
	${LCRYPTO_SRC}/crypto/comp \
	${LCRYPTO_SRC}/crypto/conf \
	${LCRYPTO_SRC}/crypto/des \
	${LCRYPTO_SRC}/crypto/dh \
	${LCRYPTO_SRC}/crypto/dsa \
	${LCRYPTO_SRC}/crypto/dso \
	${LCRYPTO_SRC}/crypto/ec \
	${LCRYPTO_SRC}/crypto/ecdh \
	${LCRYPTO_SRC}/crypto/ecdsa \
	${LCRYPTO_SRC}/crypto/engine \
	${LCRYPTO_SRC}/crypto/err \
	${LCRYPTO_SRC}/crypto/evp \
	${LCRYPTO_SRC}/crypto/hmac \
	${LCRYPTO_SRC}/crypto/idea \
	${LCRYPTO_SRC}/crypto/krb5 \
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
	${LCRYPTO_SRC}/crypto/pqueue \
	${LCRYPTO_SRC}/crypto/rand \
	${LCRYPTO_SRC}/crypto/rc2 \
	${LCRYPTO_SRC}/crypto/rc4 \
	${LCRYPTO_SRC}/crypto/rc5 \
	${LCRYPTO_SRC}/crypto/ripemd \
	${LCRYPTO_SRC}/crypto/rsa \
	${LCRYPTO_SRC}/crypto/seed \
	${LCRYPTO_SRC}/crypto/sha \
	${LCRYPTO_SRC}/crypto/srp \
	${LCRYPTO_SRC}/crypto/stack \
	${LCRYPTO_SRC}/crypto/ts \
	${LCRYPTO_SRC}/crypto/txt_db \
	${LCRYPTO_SRC}/crypto/ui \
	${LCRYPTO_SRC}/crypto/whrlpool \
	${LCRYPTO_SRC}/crypto/x509 \
	${LCRYPTO_SRC}/crypto/x509v3 \
	${.CURDIR}/man
