# $FreeBSD$

LIB=	hdb
LDFLAGS=	-Wl,--no-undefined
LDADD=	-lasn1 -lcom_err -lkrb5 -lroken
DPADD=	${LIBASN1} ${LIBCOM_ERR} ${LIBKRB5} ${LIBROKEN}

INCS=	hdb-private.h \
	hdb-protos.h \
	hdb.h \
	hdb_asn1.h \
	hdb_err.h

SRCS=	common.c \
	db.c \
	db3.c \
	dbinfo.c \
	ext.c \
	hdb-ldap.c \
	hdb.c \
	hdb_asn1.h \
	hdb_err.c \
	hdb_err.h \
	keys.c \
	keytab.c \
	mkey.c \
	ndbm.c \
	print.c \
	${GEN:S/.x$/.c/}

CFLAGS+=-I${KRB5DIR}/lib/hdb -I${KRB5DIR}/lib/asn1 \
	-I${KRB5DIR}/lib/roken -I. ${LDAPCFLAGS}
CFLAGS+=-DHDB_DB_DIR="\"/var/heimdal\""

GEN=	asn1_Salt.x \
	asn1_Key.x \
	asn1_Event.x \
	asn1_HDBFlags.x \
	asn1_GENERATION.x \
	asn1_HDB_Ext_PKINIT_acl.x \
	asn1_HDB_Ext_PKINIT_hash.x \
	asn1_HDB_Ext_Constrained_delegation_acl.x \
	asn1_HDB_Ext_Lan_Manager_OWF.x \
	asn1_HDB_Ext_Password.x \
	asn1_HDB_Ext_Aliases.x \
	asn1_HDB_extension.x \
	asn1_HDB_extensions.x \
	asn1_hdb_entry.x \
	asn1_hdb_entry_alias.x

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
