# $FreeBSD$

SUBDIR= bdes
.if !defined(NOCRYPT) && !defined(MAKE_KERBEROS4)
SUBDIR+=telnet
.endif
.if !defined(NO_OPENSSL)
SUBDIR+=openssl
.endif

.include <bsd.subdir.mk>
