# Copyright 2001 FreeBSD, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

.include <src.opts.mk>

.if ${MK_MITKRB5} != "no"
SRCDIR= ${SRCTOP}/contrib/pam-krb5
.PATH:	${SRCDIR}/module \
	${SRCDIR}/portable \
	${SRCDIR}/pam-util \
	${SRCDIR}/docs \
	${SRCDIR}

PACKAGE=	kerberos
LIB=	pam_krb5
LIBADD=	com_err krb5

SRCS=	account.c \
	alt-auth.c \
	args.c \
	auth.c \
	cache.c \
	context.c \
	dummy.c \
	fast.c \
	krb5-extra.c \
	logging.c \
	pam-util_options.c \
	module_options.c \
	pam_syslog.c \
	pam_vsyslog.c \
	password.c \
	prompting.c \
	public.c \
	setcred.c \
	support.c \
	vector.c

MANNODEV=	pam_krb5.8
MANSRC.pam_krb5.8=pam-krb5.8

CFLAGS=	-I${SRCDIR} \
	-I${.CURDIR} \
	-fno-strict-aliasing \
	-DHAVE_CONFIG_H

WARNS?=	3

CLEANFILES=	pam-util_options.c module_options.c

pam-util_options.c:	.PHONY
	cp ${SRCDIR}/pam-util/options.c pam-util_options.c

module_options.c:	.PHONY
	cp ${SRCDIR}/module/options.c module_options.c

.ifdef VENDOR_IMPORT
# Regenerate the manual page from the pod source after vendor import
pam-krb5.8: pam_krb5.pod
	sed -e 's/pam(7)/pam.conf(5)/' <${.ALLSRC} | \
	    pod2mdoc -n pam_krb5 -s 8 >${.CURDIR}/${.TARGET}
.endif
.else
PACKAGE=	kerberos

LIB=		pam_krb5
SRCS=		pam_krb5.c
MANNODEV=	pam_krb5.8
.if defined(_FREEFALL_CONFIG)
CFLAGS+=-D_FREEFALL_CONFIG
WARNS?=	3
.endif

LIBADD+=	krb5

.endif

.include <bsd.lib.mk>
