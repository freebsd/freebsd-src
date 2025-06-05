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

SUBDIR=	krb5_private krb5 gssrpc gssapi

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>

INCSGROUPS=	INCS COM_ERR VERTO GSSAPI_KRB5

INCSDIR=	${INCLUDEDIR}

.PATH:	${KRB5_DIR}/include

INCS=	kdb.h \
	krad.h \
	krb5.h

GSSAPI_KRB5DIR=	${INCLUDEDIR}/gssapi_krb5/gssapi
GSSAPI_KRB5=	gssapi.h

CLEANFILES=	osconf.new

# XXX The output of this needs to be verified against the osconf.h I created
# It is only used to verify osconf.h has not changed
osconf.h:
	cat ./osconf.hin | sed -e "s\"@KRB5RCTMPDIR\"${KRB5RCTMPDIR}\"" -e "s\"@PREFIX\"${KRB5PREFIX}\"" -e "s\"@EXEC_PREFIX\"${KRB5_EXEC_PREFIX}\"" -e "s\"@BINDIR\"${KRB5_BINDIR}\"" -e "s\"@LIBDIR\"${KRB5_LIBDIR}\"" -e "s\"@SBINDIR\"${KRB5_SBINDIR}\"" -e "s\"@MODULEDIR\"${KRB5_MODULEDIR}\"" -e "s\"@GSSMODULEDIR\"${KRB5_GSSMODULEDIR}\"" -e "s\"@LOCALSTATEDIR\"${KRB5_LOCALSTATEDIR}\"" -e "s\"@RUNSTATEDIR\"${KRB5_RUNSTATEDIR}\"" -e "s\"@SYSCONFDIR\"${KRB5_SYSCONFDIR}\"" -e "s\"@DYNOBJEXT\"${KRB5_DYNOBJEXT}\"" -e "s\"@SYSCONFCONF\":${KRB5_SYSCONFCONF}\"" > osconf.new
	sh ${MOVEIFCHANGED} osconf.new osconf.h

rebuild-error-tables: ${K5_ET_HEADERS}

force:

stage_includes: copies
installincludes: copies
SDESTDIR?=	${SYSROOT:U${DESTDIR}}

.PHONY: force rebuild-error-tables osconf.h

.PATH:	${KRB5_DIR}/util/et
COM_ERRDIR=	${INCLUDEDIR}
COM_ERR=	com_err.h

.PATH:	${KRB5_DIR}/util/verto
VERTODIR=	${INCLUDEDIR}
VERTO=		verto-module.h \
		verto.h

.PATH:	${KRB5_DIR}/include/krb5

copies:
	cd ${KRB5_DIR}/include; \
	${INSTALL} -C ${TAG_ARGS:D${TAG_ARGS},dev} -o ${BINOWN} -g ${BINGRP} -m444 ${INCS} ${SDESTDIR}${INCLUDEDIR}
	cd ${KRB5_DIR}/util/et; \
	${INSTALL} -C ${TAG_ARGS:D${TAG_ARGS},dev} -o ${BINOWN} -g ${BINGRP} -m444 ${COM_ERR} ${SDESTDIR}${INCLUDEDIR}
	cd ${KRB5_DIR}/util/verto; \
	${INSTALL} -C ${TAG_ARGS:D${TAG_ARGS},dev} -o ${BINOWN} -g ${BINGRP} -m444 ${VERTO} ${SDESTDIR}${INCLUDEDIR}


.include <bsd.prog.mk>
