# $FreeBSD$

SUBDIR=	lib add create delete info version

.if !defined(NOCRYPT) && !defined(NOSECURE) && !defined(NO_OPENSSL)
DISTRIBUTION=	crypto
SUBDIR+= sign
.endif

.include <bsd.subdir.mk>
