# $FreeBSD$

.include <src.opts.mk>

.if ${MK_CHERI_USE_LLD} == "no"
# linker error
WANT_CHERI=none
.endif

BINDIR= ${LIBEXECDIR}/bsdinstall
PROG=	distextract
LIBADD=	archive dpv figpar ncursesw dialog m

WARNS?=	6
MAN=

.include <bsd.prog.mk>
