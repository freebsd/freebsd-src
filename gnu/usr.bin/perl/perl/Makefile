#
# $FreeBSD$
#

PROG=	perl
NOMAN=	true
CFLAGS+=-I${PERL5SRC} -I${.OBJDIR}
SRCS=	perlmain.c config.h
NOSHARED= no
DYNALOADER= lib/auto/DynaLoader/DynaLoader.a
DPADD=	${DYNALOADER} ${LIBPERL} ${LIBM} ${LIBCRYPT} ${LIBMD}
LDADD=	-Wl,-E ${DYNALOADER} -lperl -lm -lcrypt -lmd
LINKS=	${BINDIR}/${PROG} ${BINDIR}/perl5 \
	${BINDIR}/${PROG} ${BINDIR}/perl${VERSION}

CLEANFILES=	Config.pm perlmain.c \
		autosplit ext.libs

.include <bsd.prog.mk>

beforedepend all: scripts

${PROG}: scripts autosplit ${DYNALOADER}

perlmain.c: scripts autosplit
	sh writemain ${DYNALOADER} > ${.TARGET}

.PATH:	${PERL5SRC}
