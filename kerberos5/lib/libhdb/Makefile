# $FreeBSD$

LIB=	hdb

INCS=	\
	hdb-private.h	\
	hdb-protos.h	\
	hdb.h		\
	${.OBJDIR}/hdb_asn1.h	\
	${.OBJDIR}/hdb_err.h

SRCS=	\
	common.c			\
	db.c				\
	db3.c				\
	hdb-ldap.c			\
	hdb.c				\
	keytab.c			\
	mkey.c				\
	ndbm.c				\
	print.c				\
	hdb_err.c			\
	${.OBJDIR}/hdb_err.h		\
	${.OBJDIR}/hdb_asn1.h		\
	${.OBJDIR}/k524_err.h		\
	${.OBJDIR}/krb5_err.h		\
	${.OBJDIR}/heim_err.h		\
	${GEN:S/.x/.c/g}

CFLAGS+=-I${KRB5DIR}/include		\
	-I${INCLUDEOBJDIR}		\
	-I${KRB5DIR}/lib/krb5		\
	-I${KRB5DIR}/lib/hdb		\
	-I${KRB5DIR}/lib/asn1		\
	-I${KRB5DIR}/lib/roken		\
	-I${.OBJDIR}			\
	-I${ASN1OBJDIR}			\
	${LDAPCFLAGS}

GEN=					\
	asn1_Key.x			\
	asn1_GENERATION.x		\
	asn1_Event.x			\
	asn1_HDBFlags.x			\
	asn1_hdb_entry.x		\
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

${GEN} ${.OBJDIR}/hdb_asn1.h:	asn1_compile hdb.asn1
	./asn1_compile ${KRB5DIR}/lib/hdb/hdb.asn1 hdb_asn1

asn1_compile:					\
	gen.c					\
	gen_copy.c				\
	gen_decode.c				\
	gen_encode.c				\
	gen_free.c				\
	gen_glue.c				\
	gen_length.c				\
	hash.c					\
	emalloc.c				\
	main.c					\
	symbol.c				\
	getarg.c				\
	warnerr.c				\
	${.OBJDIR}/lex.o			\
	${.OBJDIR}/parse.o			\
	${.OBJDIR}/print_version.o		\
	get_window_size.c			\
	strupr.c
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.OODATE} ${LDADD}

.ORDER: ${.OBJDIR}/roken.h ${.OBJDIR}/parse.c ${.OBJDIR}/parse.h ${.OBJDIR}/lex.c

${.OBJDIR}/parse.o:	${.OBJDIR}/parse.c ${.OBJDIR}/roken.h

${.OBJDIR}/lex.o:	${.OBJDIR}/lex.c ${.OBJDIR}/parse.h ${.OBJDIR}/roken.h

${.OBJDIR}/parse.h ${.OBJDIR}/parse.c:	parse.y
	${YACC}	-d ${.OODATE}
	cp y.tab.c parse.c
	cp y.tab.h parse.h

${.OBJDIR}/lex.c:	lex.l
	${LEX} -t ${LFLAGS} ${.OODATE} > ${.TARGET}

${.OBJDIR}/print_version.o: ${.OBJDIR}/print_version.h print_version.c \
				${.OBJDIR}/roken.h
	${CC} ${CFLAGS} -c -o ${.TARGET} ${KRB5DIR}/lib/vers/print_version.c

${.OBJDIR}/print_version.h: make-print-version
	./make-print-version print_version.h

make-print-version: make-print-version.c
	${CC} ${CFLAGS} -static -o ${.TARGET} ${.OODATE}

CLEANFILES+=	${GEN} asn1_compile lex.o parse.o parse.c parse.h \
		hdb_asn1.h make-print-version print_version.h print_version.o \
		y.tab.c y.tab.h
