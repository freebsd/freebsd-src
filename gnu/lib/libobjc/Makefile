# Makefile for libobjc
# $Id: Makefile,v 1.5 1999/04/04 16:36:30 obrien Exp $

GCCDIR=	${.CURDIR}/../../../contrib/egcs/gcc

.PATH:	${GCCDIR}/objc

LIB=    objc
NOMAN=	sorry
NOPIC=	works but method lookup slowdown is significant

SRCS=   my_archive.c class.c encoding.c hash.c init.c misc.c nil_method.c \
	my_objects.c sarray.c selector.c my_sendmsg.c \
	thr.c thr-single.c \
	NXConstStr.m Object.m Protocol.m linking.m

HDRS=	encoding.h hash.h objc-act.h objc-api.h objc-list.h objc.h runtime.h \
	sarray.h thr.h typedstream.h NXConstStr.h Object.h Protocol.h

CFLAGS+= -I${GCCDIR}/objc -I${GCCDIR}

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${HDRS:S;^;${GCCDIR}/objc/;} ${DESTDIR}/usr/include/objc

my_objects.c:	objects.c
	sed -e '/\.\.\/tconfig.h/d' < ${GCCDIR}/objc/objects.c > my_objects.c

my_sendmsg.c:	sendmsg.c  runtime-info.h
	sed -e '/\.\.\/tconfig.h/d' < ${GCCDIR}/objc/sendmsg.c > my_sendmsg.c

my_archive.c:	archive.c
	sed -e '/config.h/d' < ${GCCDIR}/objc/archive.c > my_archive.c

runtime-info.h:
	`${CC} --print-prog-name=cc1obj` -print-objc-runtime-info \
	    < /dev/null > ${.TARGET}

CLEANFILES+= my_objects.c my_sendmsg.c my_archive.c runtime-info.h

.include <bsd.lib.mk>
