#
# $FreeBSD: src/gnu/usr.bin/perl/miniperl/Makefile,v 1.11 2000/01/29 14:18:02 markm Exp $
#

PROG=	miniperl
NOMAN=	true
CFLAGS+=-I${PERL5SRC} -I${.OBJDIR}
SRCS=	miniperlmain.c config.h
# Miniperl _must_ be static!!
NOSHARED= yes
DPADD=	${LIBPERL} ${LIBM} ${LIBCRYPT}
LDADD=	-L${.OBJDIR}/../libperl -lperl -lm -lcrypt

# Trick the bootstrap tools into thinking that miniperl is perl.
# This gets overwritten.
LINKS=	${BINDIR}/${PROG} ${BINDIR}/perl

.include <bsd.prog.mk>

.PATH:	${PERL5SRC}
