# Copyright (c) 1996 WIDE Project. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modifications, are permitted provided that the above copyright notice
# and this paragraph are duplicated in all such forms and that any
# documentation, advertising materials, and other materials related to
# such distribution and use acknowledge that the software was developed
# by the WIDE Project, Japan. The name of the Project may not be used to
# endorse or promote products derived from this software without
# specific prior written permission. THIS SOFTWARE IS PROVIDED ``AS IS''
# AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
# LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE.
# $FreeBSD$

PROG=	rrenumd
SRCS=	rrenumd.c  parser.y lexer.l
YFLAGS+=	-d

CFLAGS+= -DINET6 -DIPSEC -I. -I${.CURDIR}
LDADD=	-lipsec -lcompat -ll -ly
DPADD=	${LIBIPSEC} ${LIBCOMPAT} ${LIBL} ${LIBY}

MAN5=	rrenumd.conf.5
MAN8=	rrenumd.8

SRCS+=y.tab.h
y.tab.h: parser.y

.if defined(YACCDEBUG)
CFLAGS+= -DYYDEBUG
YFLAGS+= -t -v
.endif

.include <bsd.prog.mk>

CLEANFILES+=	y.tab.h
