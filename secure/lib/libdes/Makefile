#	@(#)Makefile	5.4 (Berkeley) 5/7/91
#	$FreeBSD$

LIB=	des
SRCS=	cbc3_enc.c cbc_cksm.c cbc_enc.c  cfb64enc.c cfb_enc.c  \
	ecb3_enc.c ecb_enc.c  ede_enc.c  enc_read.c enc_writ.c \
	ncbc_enc.c new_rkey.c ofb64enc.c ofb_enc.c  pcbc_enc.c \
	qud_cksm.c rand_key.c read_pwd.c rpc_enc.c  set_key.c  \
	str2key.c  cfb64ede.c ofb64ede.c supp.c     xcbc_enc.c
HEADER=	des.h

SHLIB_MAJOR=	3
SHLIB_MINOR=	0

CFLAGS+=	-Wall
CLEANFILES+=	des_crypt.3

MAN3=	des_crypt.3

des_crypt.3: des_crypt.man
	cp ${.OODATE} ${.TARGET}

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

beforeinstall:
.for i in ${HEADER}
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/$i \
		${DESTDIR}/usr/include
.endfor

.include <bsd.lib.mk>
