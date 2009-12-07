# $FreeBSD$

SHLIBDIR?=/lib

.include <bsd.own.mk>

LIB=	ulog
SHLIB_MAJOR= 0
INCS=	ulog.h utempter.h
SRCS=	ulog.h ulog_getutxent.c ulog_internal.h ulog_login.c \
	ulog_login_pseudo.c ulog_pututxline.c ulog_util.c utempter.c

MAN=	ulog_getutxent.3 ulog_login.3 ulog_setutxfile.3 \
	utempter_add_record.3
MLINKS+=ulog_getutxent.3 ulog_endutxent.3 \
	ulog_getutxent.3 ulog_getutxline.3 \
	ulog_getutxent.3 ulog_pututxline.3 \
	ulog_getutxent.3 ulog_setutxent.3 \
	ulog_login.3 ulog_login_pseudo.3 \
	ulog_login.3 ulog_logout.3 \
	ulog_login.3 ulog_logout_pseudo.3 \
	ulog_setutxfile.3 ulog_getutxuser.3 \
	utempter_add_record.3 utempter_remove_added_record.3 \
	utempter_add_record.3 utempter_remove_record.3 \
	utempter_add_record.3 addToUtmp.3 \
	utempter_remove_added_record.3 removeFromUtmp.3 \
	utempter_remove_record.3 removeLineFromUtmp.3

# Add links to <utmpx.h>-style functions.
MLINKS+=ulog_endutxent.3 endutxent.3 \
	ulog_getutxent.3 getutxent.3 \
	ulog_getutxline.3 getutxline.3 \
	ulog_pututxline.3 pututxline.3 \
	ulog_setutxent.3 setutxent.3

WARNS?=	6

VERSION_DEF= ${.CURDIR}/../libc/Versions.def
SYMBOL_MAPS= ${.CURDIR}/Symbol.map

.if ${MK_INSTALLLIB} != "no"
SYMLINKS+=libulog.a ${LIBDIR}/libutempter.a
.endif
.if !defined(NO_PIC)
SYMLINKS+=libulog.so ${LIBDIR}/libutempter.so
.endif
.if ${MK_PROFILE} != "no"
SYMLINKS+=libulog_p.a ${LIBDIR}/libutempter_p.a
.endif

.include <bsd.lib.mk>
