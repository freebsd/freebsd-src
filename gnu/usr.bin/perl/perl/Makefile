# $FreeBSD$

PROG=	perl
NOMAN=	true
CFLAGS+=-I${PERL5SRC} -I${.OBJDIR} -DPERL_CORE
SRCS=	perlmain.c config.h
NOSHARED= no
LDFLAGS=-Wl,-E
DPADD=	${DYNALOADER} ${LIBPERL} ${LIBM} ${LIBCRYPT} ${LIBMD}
LDADD=	${DYNALOADER} -lperl -lm -lcrypt -lmd
LINKS=	${BINDIR}/${PROG} ${BINDIR}/perl5 \
	${BINDIR}/${PROG} ${BINDIR}/perl${VERSION}

CLEANFILES=	Config.pm perlmain.c \
		autosplit ext.libs

.include <bsd.prog.mk>

beforedepend all:	scripts

${PROG}: scripts autosplit ${DYNALOADER}

perlmain.c: scripts autosplit
	sh writemain ${DYNALOADER} > ${.TARGET}

.PATH:	${PERL5SRC}
