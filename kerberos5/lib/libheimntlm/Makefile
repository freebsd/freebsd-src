# $FreeBSD$

LIB=	heimntlm
SRCS=	ntlm.c
INCS=	heimntlm.h heimntlm-protos.h
CFLAGS+=-I${KRB5DIR}/lib/ntlm
VERSION_MAP= ${KRB5DIR}/lib/ntlm/version-script.map

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/ntlm
