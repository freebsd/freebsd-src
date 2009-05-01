# $FreeBSD: src/usr.sbin/fifolog/Makefile.inc,v 1.1.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

LIBFIFOLOG =	${.OBJDIR}/../lib/libfifolog.a


WARNS ?=	6

# CFLAGS	+=	-O0 -g

# LINT =		flint
# LINTFLAGS =	${.CURDIR}/../flint.lnt -I/usr/include 

.include "../Makefile.inc"
