# Makefile for libobjc
# $FreeBSD$

GCCDIR=	${.CURDIR}/../../../contrib/gcc

.PATH:	${GCCDIR}/objc

LIB=    objc
NOMAN=	sorry
NOPIC=	does not work

SRCS=   archive.c class.c encoding.c hash.c init.c misc.c \
	my_objects.c sarray.c selector.c my_sendmsg.c \
	NXConstStr.m Object.m Protocol.m

HDRS=	encoding.h hash.h list.h objc-api.h objc.h sarray.h typedstream.h \
	NXConstStr.h Object.h Protocol.h

CFLAGS+= -I${GCCDIR}/objc -I${GCCDIR}

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${HDRS:S;^;${GCCDIR}/objc/;} ${DESTDIR}/usr/include/objc

my_objects.c:	objects.c
	sed -e '/\.\.\/tconfig.h/d' < ${GCCDIR}/objc/objects.c > my_objects.c

my_sendmsg.c:	sendmsg.c
	sed -e '/\.\.\/tconfig.h/d' < ${GCCDIR}/objc/sendmsg.c > my_sendmsg.c

CLEANFILES+= my_objects.c my_sendmsg.c

.include <bsd.lib.mk>
