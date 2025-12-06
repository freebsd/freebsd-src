#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.include "../Makefile.inc"

.PATH:	${KRB5_DIR}/include/krb5 \
	${KRB5_DIR}/lib/gssapi/krb5 \
	${KRB5_DIR}/lib/gssapi/generic \
	${KRB5_DIR}/include


INCSDIR=	${INCLUDEDIR}/krb5
INCS=	ccselect_plugin.h \
	certauth_plugin.h \
	clpreauth_plugin.h \
	hostrealm_plugin.h \
	kadm5_auth_plugin.h \
	kadm5_hook_plugin.h \
	kdcpolicy_plugin.h \
	kdcpreauth_plugin.h \
	localauth_plugin.h \
	locate_plugin.h \
	plugin.h \
	preauth_plugin.h \
	pwqual_plugin.h \
	krb5.h ${GGENI} ${K5GENI}

.include "${KRB5_SRCTOP}/lib/krb5/error_tables/Makefile.inc"
PFX=	include
.include "${KRB5_SRCTOP}/lib/gssapi/krb5/Makefile.et"
.include "${KRB5_SRCTOP}/lib/gssapi/generic/Makefile.et"

CLEANFILES+=	krb5.h

krb5.h:	krb5.hin ${GENI_ET}
	@echo "/* This file is generated, please don't edit it directly.  */" > ${.TARGET}
	@echo "#ifndef KRB5_KRB5_H_INCLUDED" >> ${.TARGET}
	@echo "#define KRB5_KRB5_H_INCLUDED" >> ${.TARGET}
	@cat ${.ALLSRC} >> ${.TARGET}
	@echo "#endif /* KRB5_KRB5_H_INCLUDED */" >> ${.TARGET}

.include <bsd.prog.mk>
