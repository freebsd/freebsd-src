# $FreeBSD$
# Author: Shteryana Shopova <syrinx@FreeBSD.org>

BINDIR?= /usr/bin

CFLAGS+= -I. -I${.CURDIR}

.if exists(${.OBJDIR}/../libbsnmptools)
LIBBSNMPTOOLSDIR=      ${.OBJDIR}/../libbsnmptools
.else
LIBBSNMPTOOLSDIR=      ${.CURDIR}/../libbsnmptools
.endif
LIBBSNMPTOOLS=         ${LIBBSNMPTOOLSDIR}/libbsnmptools.a
