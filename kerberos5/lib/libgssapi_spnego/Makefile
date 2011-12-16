# $FreeBSD$

LIB=	gssapi_spnego
LDFLAGS= -Wl,-Bsymbolic -Wl,--no-undefined
LDADD=	-lgssapi -lasn1 -lroken
DPADD=	${LIBGSSAPI} ${LIBASN1} ${LIBROKEN}

SRCS=	accept_sec_context.c \
	compat.c \
	context_stubs.c \
	cred_stubs.c \
	external.c \
	init_sec_context.c \
	prefix.c \
	spnego_asn1.h \
	${GEN:S/.x$/.c/}

GEN=	asn1_ContextFlags.x \
	asn1_MechType.x \
	asn1_MechTypeList.x \
	asn1_NegotiationToken.x \
	asn1_NegotiationTokenWin.x \
	asn1_NegHints.x \
	asn1_NegTokenInit.x \
	asn1_NegTokenInitWin.x \
	asn1_NegTokenResp.x

CFLAGS+=-I${KRB5DIR}/lib/gssapi
CFLAGS+=-I${KRB5DIR}/lib/asn1
CFLAGS+=-I${KRB5DIR}/lib/roken -I.

CLEANFILES= ${GEN} ${GEN:S/.x$/.c/} spnego_asn1.h asn1_files

.ORDER: ${GEN} spnego_asn1.h
${GEN} spnego_asn1.h: spnego.asn1 ../../tools/asn1_compile/asn1_compile
	../../tools/asn1_compile/asn1_compile --sequence=MechTypeList ${.ALLSRC:M*.asn1} spnego_asn1

../../tools/asn1_compile/asn1_compile:
	cd ${.CURDIR}/../../tools/asn1_compile && ${MAKE}

.for I in ${GEN}
${I:R}.c: ${I}
	cat ${.ALLSRC} > ${.TARGET}
.endfor

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/gssapi/spnego ${.CURDIR}/../../../lib/libgssapi
