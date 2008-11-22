# $FreeBSD$

LIB=	hdb

INCS=	hdb-private.h \
	hdb-protos.h \
	hdb.h \
	hdb_asn1.h \
	hdb_err.h

SRCS=	common.c \
	db.c \
	db3.c \
	hdb-ldap.c \
	hdb.c \
	hdb_asn1.h \
	hdb_err.c \
	hdb_err.h \
	keytab.c \
	mkey.c \
	ndbm.c \
	print.c \
	${GEN:S/.x$/.c/}

CFLAGS+=-I${KRB5DIR}/lib/hdb -I${KRB5DIR}/lib/asn1 \
	-I${KRB5DIR}/lib/roken -I. ${LDAPCFLAGS}

GEN=	asn1_Event.x \
	asn1_GENERATION.x \
	asn1_HDBFlags.x \
	asn1_Key.x \
	asn1_Salt.x \
	asn1_hdb_entry.x

CLEANFILES= ${GEN} ${GEN:S/.x$/.c/} hdb_asn1.h asn1_files

.ORDER: ${GEN} hdb_asn1.h
${GEN} hdb_asn1.h: hdb.asn1 ../../tools/asn1_compile/asn1_compile
	../../tools/asn1_compile/asn1_compile ${.ALLSRC:M*.asn1} hdb_asn1

../../tools/asn1_compile/asn1_compile:
	cd ${.CURDIR}/../../tools/asn1_compile && ${MAKE}

.for I in ${GEN}
${I:R}.c: ${I}
	cat ${.ALLSRC} > ${.TARGET}
.endfor

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/hdb
