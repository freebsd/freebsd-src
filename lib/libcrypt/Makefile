#
# $FreeBSD$
#

SHLIB_MAJOR=	2
LIB=		crypt

.PATH:		${.CURDIR}/../libmd
SRCS=		crypt.c crypt-md5.c md5c.c misc.c
MAN=		crypt.3
MLINKS=		crypt.3 crypt_get_format.3 crypt.3 crypt_set_format.3
CFLAGS+=	-I${.CURDIR}/../libmd -I${.CURDIR}/../libutil
CFLAGS+=	-DLIBC_SCCS -Wall
# Pull in the crypt-des.c source, assuming it is present.
.if exists(${.CURDIR}/../../secure/lib/libcrypt/crypt-des.c) && \
    !defined(NOCRYPT)
.PATH:		${.CURDIR}/../../secure/lib/libcrypt
SRCS+=		crypt-des.c crypt-blowfish.c blowfish.c
CFLAGS+=	-I${.CURDIR} -DHAS_DES -DHAS_BLOWFISH
.endif
# And the auth_getval() code and support.
.PATH:		${.CURDIR}/../libutil
SRCS+=		auth.c property.c
.for sym in MD5Init MD5Final MD5Update MD5Pad auth_getval \
	    property_find properties_read properties_free
CFLAGS+=	-D${sym}=__${sym}
.endfor
PRECIOUSLIB=	yes

.include <bsd.lib.mk>
