# $FreeBSD$

.include <src.opts.mk>

.if ${MK_CHERI_USE_LLD} == "no"
# linker error
WANT_CHERI=none
.endif

PROG=	kinit
CFLAGS+=-I${KRB5DIR}/lib/roken
LIBADD=	kafs5 krb5 heimntlm roken crypto vers

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
