#
# $FreeBSD$
#

PROG=	suidperl
NOMAN=	true
CFLAGS+=-I${PERL5SRC} -I${.OBJDIR} -DIAMSUID
SRCS=	perlmain.c sperl.c config.h
NOSHARED= no
DYNALOADER= lib/auto/DynaLoader/DynaLoader.a
DPADD=	${DYNALOADER} ${LIBPERL} ${LIBM} ${LIBCRYPT} ${LIBMD}
LDADD=	-Wl,-E ${DYNALOADER} -lperl -lm -lcrypt -lmd
LINKS=	${BINDIR}/${PROG} ${BINDIR}/sperl5 \
	${BINDIR}/${PROG} ${BINDIR}/sperl${VERSION}
BINOWN=	root
.if defined(ENABLE_SUIDPERL) && ${ENABLE_SUIDPERL} == "true"
BINMODE=4511
.else
BINMODE=511
.endif

CLEANFILES=	Config.pm perlmain.c \
		autosplit ext.libs
CLEANFILES+=	sperl.c

.include <bsd.prog.mk>

beforedepend all: scripts

${PROG}: scripts autosplit ${DYNALOADER}

perlmain.c: scripts autosplit
	sh writemain ${DYNALOADER} > ${.TARGET}

.PATH:	${PERL5SRC}

sperl.c: ${PERL5SRC}/perl.c
	@ln -sf ${.OODATE} ${.TARGET}
