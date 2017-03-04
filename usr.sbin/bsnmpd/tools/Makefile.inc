# $FreeBSD$
# Author: Shteryana Shopova <syrinx@FreeBSD.org>

BINDIR?= /usr/bin

CFLAGS+= -I. -I${.CURDIR}

.if exists(${.OBJDIR:H}/libbsnmptools)
LIBBSNMPTOOLSDIR=      ${.OBJDIR:H}/libbsnmptools
.else
LIBBSNMPTOOLSDIR=      ${.CURDIR:H}/libbsnmptools
.endif
LIBBSNMPTOOLS=         ${LIBBSNMPTOOLSDIR}/libbsnmptools.a

WARNS?=		6

.include "../Makefile.inc"
