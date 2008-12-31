# $FreeBSD: src/usr.sbin/fifolog/Makefile.inc,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

LIBFIFOLOG =	${.OBJDIR}/../lib/libfifolog.a


WARNS ?=	6

# CFLAGS	+=	-O0 -g

# LINT =		flint
# LINTFLAGS =	${.CURDIR}/../flint.lnt -I/usr/include 

.include "../Makefile.inc"
