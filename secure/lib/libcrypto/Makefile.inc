# $FreeBSD$

LCRYPTO_SRC=	${.CURDIR}/../../../crypto/openssl/crypto
CFLAGS+= -DTERMIOS -DANSI_SOURCE -DNO_IDEA -I${LCRYPTO_SRC} -I${.OBJDIR}

.if ${MACHINE_ARCH} == "i386"
CFLAGS+= -DL_ENDIAN
# -DBN_ASM -DSHA1_ASM -DMD5_ASM -DRMD160_ASM - XXX notyet
CFLAGS+= -DDEVRANDOM=\"/dev/urandom\"
.else
.if ${MACHINE_ARCH} == "alpha"
# no ENDIAN stuff defined for alpha (64-bit)
.endif
.endif

WITH_RSA?= YES
