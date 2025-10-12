# $NetBSD: Makefile,v 1.1.1.1 2020/06/15 01:52:53 christos Exp $

.include <bsd.own.mk>

USE_SHLIBDIR=   yes

CPPFLAGS+=-D_REENTRANT
#LIBDPLIBS+=pthread ${NETBSDSRCDIR}/lib/libpthread
LIB=blocklist
SRCS=bl.c blocklist.c
MAN=libblocklist.3
MLINKS+=libblocklist.3 blocklist_open.3
MLINKS+=libblocklist.3 blocklist_close.3
MLINKS+=libblocklist.3 blocklist.3
MLINKS+=libblocklist.3 blocklist_r.3
MLINKS+=libblocklist.3 blocklist_sa.3
MLINKS+=libblocklist.3 blocklist_sa_r.3

.include <bsd.lib.mk>
