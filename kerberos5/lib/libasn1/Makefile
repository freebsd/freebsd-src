# $FreeBSD$

LIB=	asn1
CFLAGS+=-I${KRB5DIR}/include \
	-I${KRB5DIR}/lib/asn1 \
	-I${KRB5DIR}/lib/roken \
	-I${INCLUDEOBJDIR} \
	-I${.OBJDIR}
SRCS=	der_get.c der_put.c der_free.c der_length.c der_copy.c \
	timegm.c asn1_err.c asn1_err.h asn1.h roken.h \
	${GEN:S/.x/.c/g}

GEN=	asn1_APOptions.x asn1_AP_REP.x asn1_AP_REQ.x asn1_AS_REP.x \
	asn1_AS_REQ.x asn1_Authenticator.x asn1_AuthorizationData.x \
	asn1_Checksum.x asn1_EncAPRepPart.x asn1_EncASRepPart.x \
	asn1_EncKDCRepPart.x asn1_EncKrbCredPart.x \
	asn1_EncKrbPrivPart.x asn1_EncTGSRepPart.x \
	asn1_EncTicketPart.x asn1_EncryptedData.x \
	asn1_EncryptionKey.x asn1_ETYPE_INFO.x asn1_ETYPE_INFO_ENTRY.x \
	asn1_HostAddress.x asn1_HostAddresses.x asn1_KDCOptions.x \
	asn1_KDC_REP.x asn1_KDC_REQ.x asn1_KDC_REQ_BODY.x \
	asn1_KRB_CRED.x asn1_KRB_ERROR.x asn1_KRB_PRIV.x \
	asn1_KRB_SAFE.x asn1_KRB_SAFE_BODY.x asn1_KerberosTime.x \
	asn1_KrbCredInfo.x asn1_LastReq.x asn1_METHOD_DATA.x \
	asn1_PA_DATA.x asn1_PA_ENC_TS_ENC.x asn1_Principal.x \
	asn1_PrincipalName.x asn1_Realm.x asn1_TGS_REP.x \
	asn1_TGS_REQ.x asn1_Ticket.x asn1_TicketFlags.x \
	asn1_TransitedEncoding.x

INCLUDES=asn1.h asn1_err.h 

.include <bsd.lib.mk>

.PATH:		${KRB5DIR}/lib/asn1
.PATH:		${KRB5DIR}/lib/roken

build-tools:	make-print-version asn1_compile

beforedepend all: roken.h

.for I in ${GEN}
${I:S/.x/.c/}:	${I}
	cmp -s ${.OODATE} ${.TARGET} 2> /dev/null || cp ${.OODATE} ${.TARGET}
.endfor

CLEANFILES+=	${GEN:S/.x/.c/g} asn1.h asn1_files

${GEN} asn1.h:		asn1_compile k5.asn1
	./asn1_compile ${KRB5DIR}/lib/asn1/k5.asn1

build-tools:	make-print-version asn1_compile

asn1_compile:	parse.o lex.o main.c hash.c symbol.c gen.c \
		gen_encode.c gen_decode.c gen_free.c gen_length.c \
		gen_copy.c gen_glue.c getarg.c warnerr.c print_version.o \
		get_window_size.c strupr.c
	${CC} ${CFLAGS} ${.OODATE} -o ${.TARGET}

parse.o:	parse.c roken.h

parse.h parse.c:	parse.y
	${YACC}	-d ${.OODATE}
	mv y.tab.c parse.c
	mv y.tab.h parse.h

lex.o:		lex.l parse.h

print_version.o: print_version.h print_version.c roken.h
	${CC} ${CFLAGS} -c -o ${.TARGET} ${KRB5DIR}/lib/roken/print_version.c

print_version.h: make-print-version
	./make-print-version print_version.h

make-print-version: make-print-version.c
	${CC} ${CFLAGS} -static -o ${.TARGET} ${.OODATE}

CLEANFILES+=	${GEN} asn1_compile lex.o parse.o parse.c parse.h \
		hdb_asn1.h make-print-version print_version.h print_version.o
