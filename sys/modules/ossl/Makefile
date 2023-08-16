
.PATH:	${SRCTOP}/sys/crypto/openssl
.PATH:	${SRCTOP}/sys/crypto/openssl/${MACHINE_CPUARCH}

KMOD=	ossl
OBJS+=	${OBJS.${MACHINE_CPUARCH}}
SRCS=	bus_if.h \
	cryptodev_if.h \
	device_if.h \
	ossl.c \
	ossl_aes.c \
	ossl_chacha20.c \
	ossl_poly1305.c \
	ossl_sha1.c \
	ossl_sha256.c \
	ossl_sha512.c \
	${SRCS.${MACHINE_CPUARCH}}

SRCS.aarch64= \
	chacha-armv8.S \
	poly1305-armv8.S \
	sha1-armv8.S \
	sha256-armv8.S \
	sha512-armv8.S \
	vpaes-armv8.S \
	ossl_aarch64.c

SRCS.amd64= \
	aes-gcm-avx512.S \
	aesni-x86_64.S \
	aesni-gcm-x86_64.S \
	chacha-x86_64.S \
	ghash-x86_64.S \
	poly1305-x86_64.S \
	sha1-x86_64.S \
	sha256-x86_64.S \
	sha512-x86_64.S \
	ossl_aes_gcm.c \
	ossl_x86.c

SRCS.i386= \
	aesni-x86.S \
	chacha-x86.S \
	poly1305-x86.S \
	sha1-586.S \
	sha256-586.S \
	sha512-586.S \
	ossl_x86.c

# For arm64, we are forced to rewrite the compiler invocation for the assembly
# files, to remove -mgeneral-regs-only.
${SRCS.aarch64:M*.S:S/S/o/}: ${.TARGET:R}.S
	${CC} -c ${CFLAGS:N-mgeneral-regs-only} ${WERROR} ${PROF} ${.IMPSRC}
	${CTFCONVERT_CMD}

# Based on modules/armv8crypto/Makefile.
# Clang doesn't recognize "aes*" instructions without -march set.
aesv8-armx.o: aesv8-armx.S
	${CC} -c ${CFLAGS:N-mgeneral-regs-only} ${WERROR} ${PROF} \
	    -march=armv8-a+crypto ${.IMPSRC}
	${CTFCONVERT_CMD}

OBJS.aarch64= aesv8-armx.o

.include <bsd.kmod.mk>
