# $FreeBSD$

SHLIB_NAME=libthread_db.so.1
NOPROFILE=yes
CFLAGS+=-Wall -I${.CURDIR}/../../../include -I${.CURDIR}/../include
LDADD=../pthread/libpthread_db.a

PRECIOUSLIB=yes

.PATH: ${.CURDIR}

SRCS+=thread_db.c
INCS=../include/thread_db.h

.include <bsd.lib.mk>
