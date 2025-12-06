SHLIBDIR?=	/lib

.include <src.opts.mk>

PACKAGE=	runtime

SHLIB_MAJOR=	5
LIB=		crypt

SRCS=		crypt.c misc.c \
		crypt-md5.c \
		crypt-nthash.c \
		crypt-sha256.c \
		crypt-sha512.c
MAN=		crypt.3
MLINKS=		crypt.3 crypt_get_format.3 crypt.3 crypt_r.3 \
		crypt.3 crypt_set_format.3
CFLAGS+=	-I${SRCTOP}/lib/libmd \
		-I${SRCTOP}/sys/crypto/sha2

# Pull in the strong crypto, if it is present.
.if exists(${SRCTOP}/secure/lib/libcrypt) && ${MK_CRYPT} != "no"
.PATH:		${SRCTOP}/secure/lib/libcrypt
SRCS+=		crypt-des.c crypt-blowfish.c blowfish.c
CFLAGS+=	-I${.CURDIR} -DHAS_DES -DHAS_BLOWFISH
.endif

WARNS?=		2

PRECIOUSLIB=

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

LIBADD+=	md
SHLIB_LDSCRIPT=	libcrypt.ldscript
STATIC_LDSCRIPT= libcrypt.aldscript
CLEANFILES+=	libcrypt.ald

libcrypt.ald: ${.CURDIR}/${STATIC_LDSCRIPT}
	sed -e 's,@@LIB@@,${LIB},g' \
	    -e 's,@@STATICLIB_SUFFIX@@,${_STATICLIB_SUFFIX},g' \
	    ${.ALLSRC} > ${.TARGET}

all: ${STATIC_LDSCRIPT} libcrypt.ald

install-libcrypt.a: libcrypt.ald
	${INSTALL} ${DEV_TAG_ARGS} -S -C -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} libcrypt.ald ${DESTDIR}${_LIBDIR}/lib${LIB}.a

realinstall: install-libcrypt.a

.include <bsd.lib.mk>
