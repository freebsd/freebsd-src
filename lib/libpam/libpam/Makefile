# Copyright 1998 Juniper Networks, Inc.
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
#	$FreeBSD$

PAMDIR=		${.CURDIR}/../../../contrib/libpam
MODOBJDIR=	../modules

.PATH:		${PAMDIR}/libpam ${PAMDIR}/libpam_misc ${PAMDIR}/doc/man

LIB=		pam
CFLAGS+=	-I${PAMDIR}/libpam/include -I.
CFLAGS+=	-DDEFAULT_MODULE_PATH=\"${SHLIBDIR}/\"
NOPROFILE=	yes
CLEANFILES+=	security

# Files from ${PAMDIR}/libpam:
SRCS=		pam_account.c pam_auth.c pam_data.c pam_delay.c \
		pam_dispatch.c pam_end.c pam_env.c pam_handlers.c \
		pam_item.c pam_log.c pam_misc.c pam_password.c \
		pam_second.c pam_session.c pam_start.c \
		pam_strerror.c
HDRS1=		_pam_compat.h _pam_macros.h _pam_types.h \
		pam_appl.h pam_malloc.h pam_modules.h
MAN+=		pam_authenticate.3 pam_chauthtok.3 pam_fail_delay.3 \
		pam_open_session.3 pam_setcred.3 pam_start.3 \
		pam_strerror.3
MLINKS+=	pam_open_session.3 pam_close_session.3 \
		pam_start.3 pam_end.3
MAN+=		pam.8
MLINKS+=	pam.8 pam.conf.5 pam.8 pam.d.5

# Files from ${PAMDIR}/libpam_misc:
SRCS+=		help_env.c misc_conv.c xstrdup.c
HDRS2=		pam_misc.h

# Files from ${.CURDIR}:
SRCS+=		pam_get_pass.c pam_prompt.c pam_std_option.c
HDRS3=		pam_mod_misc.h

# Static PAM modules:
STATIC_MODULES+= ${MODOBJDIR}/pam_cleartext_pass_ok/libpam_cleartext_pass_ok.a
STATIC_MODULES+= ${MODOBJDIR}/pam_deny/libpam_deny.a
.if defined(MAKE_KERBEROS4) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
STATIC_MODULES+= ${MODOBJDIR}/pam_kerberosIV/libpam_kerberosIV.a
.endif
.if defined(MAKE_KERBEROS5__) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
STATIC_MODULES+= ${MODOBJDIR}/pam_kerberos5/libpam_kerberos5.a
.endif
STATIC_MODULES+= ${MODOBJDIR}/pam_opie/libpam_opie.a
STATIC_MODULES+= ${MODOBJDIR}/pam_permit/libpam_permit.a
STATIC_MODULES+= ${MODOBJDIR}/pam_radius/libpam_radius.a
STATIC_MODULES+= ${MODOBJDIR}/pam_skey/libpam_skey.a
.if exists(${.CURDIR}/../../../crypto/openssh) && !defined(NOCRYPT) && !defined(NO_OPENSSL) && !defined(NO_OPENSSH)
STATIC_MODULES+= ${MODOBJDIR}/pam_ssh/libpam_ssh.a
.endif
STATIC_MODULES+= ${MODOBJDIR}/pam_tacplus/libpam_tacplus.a
STATIC_MODULES+= ${MODOBJDIR}/pam_unix/libpam_unix.a

STATICOBJS+=	pam_static_modules.o
CLEANFILES+=	pam_static.o

_EXTRADEPEND:	pam_static.c
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
	    ${CFLAGS:M-nostdinc*} ${CFLAGS:M-[BID]*} \
	    ${.ALLSRC}

.if ${OBJFORMAT} == elf
CLEANFILES+=	setdef0.o _pam_static_modules.o setdef1.o \
		setdef0.c setdef1.c setdefs.h

pam_static_modules.o:	setdef0.o pam_static.o _pam_static_modules.o setdef1.o
	${LD} -o ${.TARGET} -r ${.ALLSRC}

setdef0.o:	setdef0.c setdefs.h
setdef1.o:	setdef1.c setdefs.h

setdef0.c setdef1.c setdefs.h:	_pam_static_modules.o
	gensetdefs ${.ALLSRC}

_pam_static_modules.o:	${STATIC_MODULES}
	${LD} -o ${.TARGET} -r --whole-archive ${.ALLSRC}
.else
pam_static_modules.o:	pam_static.o ${STATIC_MODULES}
	${LD} -o ${.TARGET} -r -Bforcearchive ${.ALLSRC}
.endif

all:		security

beforedepend:	security

beforeinstall:
	cd ${PAMDIR}/libpam/include/security; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${HDRS1} ${DESTDIR}/usr/include/security
	cd ${PAMDIR}/libpam_misc; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${HDRS2} ${DESTDIR}/usr/include/security
	cd ${.CURDIR}; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${HDRS3} ${DESTDIR}/usr/include/security

security:
	ln -sf ${PAMDIR}/libpam_misc security

.include <bsd.lib.mk>
