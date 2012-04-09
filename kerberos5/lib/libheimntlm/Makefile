# $FreeBSD: src/kerberos5/lib/libheimntlm/Makefile,v 1.1.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

LIB=	heimntlm
SRCS=	ntlm.c
INCS=	heimntlm.h heimntlm-protos.h
CFLAGS+=-I${KRB5DIR}/lib/ntlm
VERSION_MAP= ${KRB5DIR}/lib/ntlm/version-script.map

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/ntlm
