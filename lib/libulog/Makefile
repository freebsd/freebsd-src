# $FreeBSD$

LIB=	ulog
SHLIB_MAJOR= 0
INCS=	ulog.h
SRCS=	ulog.h ulog_getutxent.c ulog_internal.h ulog_login.c \
	ulog_login_pseudo.c ulog_pututxline.c ulog_util.c

MAN=	ulog_getutxent.3 ulog_login.3 ulog_setutxfile.3
MLINKS+=ulog_getutxent.3 ulog_endutxent.3 \
	ulog_getutxent.3 ulog_getutxline.3 \
	ulog_getutxent.3 ulog_pututxline.3 \
	ulog_getutxent.3 ulog_setutxent.3 \
	ulog_login.3 ulog_login_pseudo.3 \
	ulog_login.3 ulog_logout.3 \
	ulog_login.3 ulog_logout_pseudo.3 \
	ulog_setutxfile.3 ulog_getutxuser.3

# Add links to <utmpx.h>-style functions.
MLINKS+=ulog_endutxent.3 endutxent.3 \
	ulog_getutxent.3 getutxent.3 \
	ulog_getutxline.3 getutxline.3 \
	ulog_pututxline.3 pututxline.3 \
	ulog_setutxent.3 setutxent.3

WARNS?=	6

VERSION_DEF= ${.CURDIR}/../libc/Versions.def
SYMBOL_MAPS= ${.CURDIR}/Symbol.map

.include <bsd.lib.mk>
