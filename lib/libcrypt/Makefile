#
# $FreeBSD$
#

SHLIB_MAJOR=	3
LIB=		crypt
SHLIBDIR?=	/lib

.PATH:		${.CURDIR}/../libmd
SRCS=		crypt.c misc.c \
		crypt-md5.c md5c.c \
		crypt-nthash.c md4c.c
MAN=		crypt.3
MLINKS=		crypt.3 crypt_get_format.3 crypt.3 crypt_set_format.3
CFLAGS+=	-I${.CURDIR}/../libmd -I${.CURDIR}/../libutil

# Pull in the strong crypto, if it is present.
.if exists(${.CURDIR}/../../secure/lib/libcrypt) && !defined(NO_CRYPT)
.PATH:		${.CURDIR}/../../secure/lib/libcrypt
SRCS+=		crypt-des.c crypt-blowfish.c blowfish.c
CFLAGS+=	-I${.CURDIR} -DHAS_DES -DHAS_BLOWFISH
.endif

# And the auth_getval() code and support.
.PATH:		${.CURDIR}/../libutil
SRCS+=		auth.c property.c
.for sym in auth_getval property_find properties_read properties_free \
	    MD4Init MD4Final MD4Update MD4Pad \
	    MD5Init MD5Final MD5Update MD5Pad
CFLAGS+=	-D${sym}=__${sym}
.endfor

PRECIOUSLIB=

.include <bsd.lib.mk>
