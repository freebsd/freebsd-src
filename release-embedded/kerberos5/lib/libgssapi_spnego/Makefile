# $FreeBSD$

LIB=	gssapi_spnego
LDFLAGS= -Wl,-Bsymbolic -Wl,--no-undefined
LDADD=	-lgssapi -lheimbase -lasn1 -lroken
DPADD=	${LIBGSSAPI} ${LIBHEIMBASE} ${LIBASN1} ${LIBROKEN}
SHLIB_MAJOR=	10

SRCS=	accept_sec_context.c \
	compat.c \
	context_stubs.c \
	cred_stubs.c \
	external.c \
	init_sec_context.c \
	freebsd_compat.c \
	${GEN:S/.x$/.c/:S/.hx$/.h/} \
	gss_oid.c

GEN=	asn1_ContextFlags.x \
	asn1_MechType.x \
	asn1_MechTypeList.x \
	asn1_NegotiationToken.x \
	asn1_NegotiationTokenWin.x \
	asn1_NegHints.x \
	asn1_NegTokenInit.x \
	asn1_NegTokenInitWin.x \
	asn1_NegTokenResp.x \
	spnego_asn1.hx \
	spnego_asn1-priv.hx

CFLAGS+=-I${KRB5DIR}/lib/gssapi
CFLAGS+=-I${KRB5DIR}/lib/gssapi/gssapi
CFLAGS+=-I${KRB5DIR}/lib/gssapi/spnego
CFLAGS+=-I${KRB5DIR}/lib/asn1
CFLAGS+=-I${.CURDIR}/../../../lib/libgssapi
CFLAGS+=-I${KRB5DIR}/lib/roken -I.

CLEANFILES=	${GEN} ${GEN:S/.x$/.c/:S/.hx$/.h/} \
		spnego_asn1_files spnego_asn1-template.c

${GEN}: spnego.asn1 spnego.opt
	${ASN1_COMPILE} --option-file=${.ALLSRC:M*.opt} \
		${.ALLSRC:M*.asn1} spnego_asn1

.SUFFIXES: .h .c .x .hx

.x.c:   
	cmp -s ${.IMPSRC} ${.TARGET} 2> /dev/null || cp ${.IMPSRC} ${.TARGET}

.hx.h:
	cmp -s ${.IMPSRC} ${.TARGET} 2> /dev/null || cp ${.IMPSRC} ${.TARGET}

.include <bsd.lib.mk>

.SUFFIXES: .h .c .x .hx

.PATH: ${KRB5DIR}/lib/gssapi/spnego ${.CURDIR}/../../../lib/libgssapi ${.CURDIR}/../libgssapi_krb5
