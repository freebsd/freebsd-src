# $FreeBSD$

LIB=	ulog
SHLIB_MAJOR= 0
INCS=	ulog.h
SRCS=	ulog.h ulog_getutxent.c ulog_internal.h \
	ulog_login.c ulog_login_pseudo.c

MAN=	ulog_getutxent.3 ulog_login.3
MLINKS+=ulog_getutxent.3 ulog_endutxent.3 \
	ulog_getutxent.3 ulog_setutxent.3 \
	ulog_login.3 ulog_login_pseudo.3 \
	ulog_login.3 ulog_logout.3 \
	ulog_login.3 ulog_logout_pseudo.3

WARNS?=	6

VERSION_DEF= ${.CURDIR}/../libc/Versions.def
SYMBOL_MAPS= ${.CURDIR}/Symbol.map

.include <bsd.lib.mk>
