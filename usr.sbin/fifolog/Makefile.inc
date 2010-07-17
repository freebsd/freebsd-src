# $FreeBSD: src/usr.sbin/fifolog/Makefile.inc,v 1.2.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

LIBFIFOLOG= ${.OBJDIR}/../lib/libfifolog.a

WARNS?=	6

#LINT=	flint
#LINTFLAGS= ${.CURDIR}/../flint.lnt -I/usr/include

.include "../Makefile.inc"
