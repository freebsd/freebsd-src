# $FreeBSD$

SUBDIR= bdes
.if !defined(NOCRYPT) && !defined(MAKE_KERBEROS4)
SUBDIR+=telnet
.endif

.include <bsd.subdir.mk>
