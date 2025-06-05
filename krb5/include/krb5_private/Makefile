#
# SPDX-License-Idendifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.include "../Makefile.inc"

# This is a bit of a hack but the reason we have to do this is
# ${KRB5_DIR}/include/krb5/krb5.h is installed in /usr/include
# while ${KRB5_DIR}/include/krb5/krb5.hin (generates to krb5.h)
# installs to /usr/include/krb5/krb5.h. The conflicting files
# makes it impossible to generate krb5.h when it already exists.
# The MIT Makefile creates a krb5.h.new file, comparing it and
# replacing it if different. Then regenerating the original source
# to install it. Look at the MIT Makefile yourself to fully understand.

.PATH:	${KRB5_DIR}/include/krb5 \
	${KRB5_DIR}/lib/gssapi/krb5 \
	${KRB5_DIR}/lib/gssapi/generic \
	${KRB5_DIR}/include

INCSDIR=	${INCLUDEDIR}/krb5
INCS=	krb5.h ${GGENI} ${K5GENI}

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
