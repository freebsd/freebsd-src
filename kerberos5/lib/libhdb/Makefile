# $FreeBSD$

LIB=	hdb
CFLAGS+=-I${KRB5DIR}/include \
	-I${INCLUDEOBJDIR} \
	-I${KRB5DIR}/lib/krb5 \
	-I${KRB5DIR}/lib/hdb \
	-I${KRB5DIR}/lib/asn1 \
	-I${KRB5DIR}/lib/roken \
	-I${.OBJDIR} \
	-I${ASN1OBJDIR}

SRCS=	\
	common.c				\
	db.c					\
	db3.c					\
	hdb-ldap.c				\
	hdb.c					\
	keytab.c				\
	mkey.c					\
	ndbm.c					\
	print.c					\
	hdb_err.c				\
	hdb_err.h				\
	hdb_asn1.h				\
	krb5_err.h				\
	heim_err.h				\
	${GEN:S/.x/.c/g}

GEN=						\
	asn1_Key.x				\
	asn1_Event.x				\
	asn1_HDBFlags.x				\
	asn1_hdb_entry.x			\
	asn1_Salt.x

.include <bsd.lib.mk>

.PATH:	${KRB5DIR}/lib/hdb
.PATH:	${KRB5DIR}/lib/asn1
.PATH:	${KRB5DIR}/lib/vers
.PATH:	${KRB5DIR}/lib/roken

build-tools:	make-print-version asn1_compile

.for I in ${GEN}
${I:S/.x/.c/}:  ${I}
	cmp -s ${.OODATE} ${.TARGET} 2> /dev/null || cp ${.OODATE} ${.TARGET}
.endfor

CLEANFILES+=	${GEN:S/.x/.c/g} hdb_asn1.h asn1_files

${GEN} hdb_asn1.h:	asn1_compile hdb.asn1
	./asn1_compile ${KRB5DIR}/lib/hdb/hdb.asn1 hdb_asn1

asn1_compile:	parse.o lex.o main.c hash.c symbol.c gen.c \
		gen_encode.c gen_decode.c gen_free.c gen_length.c \
		gen_copy.c gen_glue.c getarg.c warnerr.c print_version.o \
		get_window_size.c strupr.c
	${CC} ${CFLAGS} ${.OODATE} -o ${.TARGET}

parse.o:	parse.c

parse.h parse.c:	parse.y
	${YACC}	-d ${.OODATE}
	mv y.tab.c parse.c
	mv y.tab.h parse.h

lex.o:		lex.l

print_version.o: print_version.h print_version.c
	${CC} ${CFLAGS} -c -o ${.TARGET} ${KRB5DIR}/lib/vers/print_version.c

print_version.h: make-print-version
	./make-print-version print_version.h

make-print-version: make-print-version.c
	${CC} ${CFLAGS} -static -o ${.TARGET} ${.OODATE}

CLEANFILES+=	${GEN} asn1_compile lex.o parse.o parse.c parse.h \
		hdb_asn1.h make-print-version print_version.h print_version.o

