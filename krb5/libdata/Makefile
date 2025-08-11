# Copyright (c) 2025 Lexi Winter.
#
# SPDX-License-Identifier: BSD-2-Clause OR ISC

.include "../Makefile.inc"

.PATH: ${KRB5_DIR}/build-tools

PACKAGE=kerberos-lib

PCFILES=gssrpc.pc \
	kadm-client.pc \
	kadm-server.pc \
	kdb.pc \
	krb5.pc \
	krb5-gssapi.pc \
	mit-krb5.pc \
	mit-krb5-gssapi.pc

CLEANFILES+=	${PCFILES}

.SUFFIXES: .pc .pc.in

.pc.in.pc:
	sed -e 's,@prefix@,/usr,g ; \
		s,@exec_prefix@,$${prefix},g ; \
		s,@libdir@,${LIBDIR},g ; \
		s,@includedir@,${INCLUDEDIR},g ; \
		s,@KRB5_VERSION@,${KRB5_VERSION},g ; \
		s,@KDB5_DB_LIB@,,g ; \
		s,@DEFCCNAME@,FILE:/tmp/krb5cc_%{uid},g ; \
		s,@DEFKTNAME@,FILE:/etc/krb5.keytab,g ; \
		s,@DEFCKTNAME@,FILE:/var/krb5/user/%{euid}/client.keytab,g ; \
		s,@COM_ERR_LIB@,-lcom_err,g ;' \
		${.IMPSRC} > ${.TARGET}

all: ${PCFILES}
	@if ! grep -q "^PACKAGE_VERSION='${KRB5_VERSION}'$$" ${KRB5_DIR}/configure; then \
		echo "KRB5_VERSION ${KRB5_VERSION} does not match the source:"; \
		grep "^PACKAGE_VERSION=" ${KRB5_DIR}/configure; \
		exit 1; \
	fi >&2

.include <bsd.lib.mk>
