#
# SPDX-License-Idendifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PACKAGE=	krb5

.include <src.opts.mk>

.include "../Makefile.inc"

LIBDIR=		${PLUGINSDIR}/kdb
SHLIBDIR=	${LIBDIR}
LIB=		db2
LDFLAGS=-Wl,--no-undefined
LIBADD=	krb5profile krb5 com_err k5crypto kadm5srv_mit kdb5 gssrpc gssapi_krb5 krb5support
VERSION_MAP=	${.CURDIR}/version.map

SRCS=	\
	adb_openclose.c \
	adb_policy.c \
	db2_exp.c \
	kdb_db2.c \
	kdb_xdr.c \
	lockout.c \
	pol_xdr.c

.include "${KRB5_SRCTOP}/plugins/kdb/db2/libdb2/Makefile.inc"

MAN=	db_btree.3 \
	db_hash.3 \
	db_lock.3 \
	db_log.3 \
	db_mpool.3 \
	db_open.3 \
	db_recno.3 \
	db_txn.3

CFLAGS+=-I${KRB5_DIR}/plugins/kdb/db2 \
	-I${KRB5_DIR}/plugins/kdb/db2/libdb2 \
	-I${KRB5_DIR}/plugins/kdb/db2/libdb2/include \
	-I${KRB5_DIR}/plugins/kdb/db2/libdb2/btree \
	-I${KRB5_DIR}/plugins/kdb/db2/libdb2/db \
	-I${KRB5_DIR}/plugins/kdb/db2/libdb2/hash \
	-I${KRB5_DIR}/plugins/kdb/db2/libdb2/mpool \
	-I${KRB5_DIR}/plugins/kdb/db2/libdb2/recno \
	-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include \
	-I${KRB5_DIR}/lib/kdb \
	-I${KRB5_OBJTOP}/plugins/kdb/db2 \
	-I${KRB5_OBJTOP}/lib/kdb

.include <bsd.lib.mk>

.SUFFIXES: .h .c .man .3

.man.3:
	@cp ${.ALLSRC} ${.TARGET}

.PATH:	${KRB5_DIR}/plugins/kdb/db2 \
	${KRB5_DIR}/plugins/kdb/db2/libdb2 \
	${KRB5_DIR}/plugins/kdb/db2/libdb2/man
