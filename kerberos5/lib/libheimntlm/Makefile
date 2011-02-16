# $FreeBSD: src/kerberos5/lib/libheimntlm/Makefile,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

LIB=	heimntlm
SRCS=	ntlm.c
INCS=	heimntlm.h heimntlm-protos.h
CFLAGS+=-I${KRB5DIR}/lib/ntlm
VERSION_MAP= ${KRB5DIR}/lib/ntlm/version-script.map

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/ntlm
