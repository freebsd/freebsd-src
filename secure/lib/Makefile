# $FreeBSD$

SUBDIR= libcipher libdes
.if !defined(NOCRYPT) && !defined(MAKE_KERBEROS4)
SUBDIR+=libtelnet
.endif

.include <bsd.subdir.mk>
