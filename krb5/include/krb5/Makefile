#
# SPDX-License-Idendifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.include "../Makefile.inc"

.PATH: ${KRB5_DIR}/include/krb5

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
	pwqual_plugin.h

.include <bsd.prog.mk>
