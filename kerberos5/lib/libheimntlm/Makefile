# $FreeBSD$

LIB=	heimntlm
LDFLAGS=	-Wl,--no-undefined
LDADD=	-lcrypto -lkrb5
DPADD=	${LIBCRYPTO} ${LIBKRB5}
SRCS=	ntlm.c
INCS=	heimntlm.h heimntlm-protos.h
CFLAGS+=-I${KRB5DIR}/lib/ntlm
VERSION_MAP= ${KRB5DIR}/lib/ntlm/version-script.map

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/ntlm
