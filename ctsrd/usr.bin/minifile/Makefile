# $FreeBSD$

PROG=	minifile

MAN=

WARNS=	0

.include <src.opts.mk>

.if ${MK_CHERI} != "no"
LDADD+=		-lvuln_magic
LIBADD+=	cheri
CFLAGS+=	-DCHERI
.else
LIBADD+=	magic
.endif

.include <bsd.prog.mk>
