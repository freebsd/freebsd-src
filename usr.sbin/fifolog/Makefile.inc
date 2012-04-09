# $FreeBSD: src/usr.sbin/fifolog/Makefile.inc,v 1.2.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

LIBFIFOLOG= ${.OBJDIR}/../lib/libfifolog.a

WARNS?=	6

#LINT=	flint
#LINTFLAGS= ${.CURDIR}/../flint.lnt -I/usr/include

.include "../Makefile.inc"
