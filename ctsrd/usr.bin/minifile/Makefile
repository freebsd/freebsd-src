# $FreeBSD$

PROG=	minifile

MAN=

WARNS=	0

.include <src.opts.mk>

.if ${MK_CHERI} != "no"
LDADD+=	-lcheri	-lvuln_magic
CFLAGS+=	-DCHERI
.else
DPADD+=	${LIBMAGIC}
LDADD+= -lmagic
.endif

.include <bsd.prog.mk>
