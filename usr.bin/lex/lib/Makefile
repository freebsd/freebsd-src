.include <src.opts.mk>

.PATH:	${SRCTOP}/contrib/flex/src

LIB=    ln
SRCS=   libmain.c libyywrap.c
NO_PIC=

.if ${MK_INSTALLLIB} != "no"
LINKS=  ${LIBDIR}/libln.a ${LIBDIR}/libl.a
LINKS+=	${LIBDIR}/libln.a ${LIBDIR}/libfl.a
.endif

.include <bsd.lib.mk>

