#-
# Copyright (c) 1998 Juniper Networks, Inc.
# All rights reserved.
# Copyright (c) 2002 Networks Associates Technology, Inc.
# All rights reserved.
#
# Portions of this software was developed for the FreeBSD Project by
# ThinkSec AS and NAI Labs, the Security Research Division of Network
# Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
# ("CBOSS"), as part of the DARPA CHATS research program.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote
#    products derived from this software without specific prior written
#    permission.
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
# $FreeBSD$
#

OPENPAM		 = ${.CURDIR}/../../../contrib/openpam

LIB		 = pam

CFLAGS		+= -I${.CURDIR} -I${OPENPAM}/include
CFLAGS		+= -DLIB_MAJ=${SHLIB_MAJOR}
NOPROFILE	 = YES
WARNS		?= 4

SRCS		 =
SRCS		+= openpam_borrow_cred.c
SRCS		+= openpam_dispatch.c
SRCS		+= openpam_dynamic.c
SRCS		+= openpam_findenv.c
SRCS		+= openpam_free_data.c
SRCS		+= openpam_get_option.c
SRCS		+= openpam_load.c
SRCS		+= openpam_log.c
SRCS		+= openpam_restore_cred.c
SRCS		+= openpam_set_option.c
SRCS		+= openpam_ttyconv.c
SRCS		+= pam_acct_mgmt.c
SRCS		+= pam_authenticate.c
SRCS		+= pam_chauthtok.c
SRCS		+= pam_close_session.c
SRCS		+= pam_end.c
SRCS		+= pam_error.c
SRCS		+= pam_get_authtok.c
SRCS		+= pam_get_data.c
SRCS		+= pam_get_item.c
SRCS		+= pam_get_user.c
SRCS		+= pam_getenv.c
SRCS		+= pam_getenvlist.c
SRCS		+= pam_info.c
SRCS		+= pam_open_session.c
SRCS		+= pam_prompt.c
SRCS		+= pam_putenv.c
SRCS		+= pam_set_data.c
SRCS		+= pam_set_item.c
SRCS		+= pam_setcred.c
SRCS		+= pam_setenv.c
SRCS		+= pam_start.c
SRCS		+= pam_strerror.c
SRCS		+= pam_verror.c
SRCS		+= pam_vinfo.c
SRCS		+= pam_vprompt.c

MAN		 =
MAN		+= openpam_borrow_cred.3
MAN		+= openpam_free_data.3
MAN		+= openpam_get_option.3
MAN		+= openpam_log.3
MAN		+= openpam_restore_cred.3
MAN		+= openpam_set_option.3
MAN		+= openpam_ttyconv.3
MAN		+= pam.3
MAN		+= pam_acct_mgmt.3
MAN		+= pam_authenticate.3
MAN		+= pam_chauthtok.3
MAN		+= pam_close_session.3
MAN		+= pam_end.3
MAN		+= pam_error.3
MAN		+= pam_get_authtok.3
MAN		+= pam_get_data.3
MAN		+= pam_get_item.3
MAN		+= pam_get_user.3
MAN		+= pam_getenv.3
MAN		+= pam_getenvlist.3
MAN		+= pam_info.3
MAN		+= pam_open_session.3
MAN		+= pam_prompt.3
MAN		+= pam_putenv.3
MAN		+= pam_set_data.3
MAN		+= pam_set_item.3
MAN		+= pam_setcred.3
MAN		+= pam_setenv.3
MAN		+= pam_sm_acct_mgmt.3
MAN		+= pam_sm_authenticate.3
MAN		+= pam_sm_chauthtok.3
MAN		+= pam_sm_close_session.3
MAN		+= pam_sm_open_session.3
MAN		+= pam_sm_setcred.3
MAN		+= pam_start.3
MAN		+= pam_strerror.3

# Local additions
SRCS		+= pam_debug_log.c
SRCS		+= pam_std_option.c

HEADERS		 =
HEADERS		+= security/openpam.h
HEADERS		+= security/pam_appl.h
HEADERS		+= security/pam_constants.h
HEADERS		+= security/pam_modules.h
HEADERS		+= security/pam_types.h

ADD_HEADERS	 = pam_mod_misc.h

# Static modules
MODULE_DIR	 = ../modules
.include "${.CURDIR}/${MODULE_DIR}/modules.inc"
STATIC_MODULES	 = ${MODULES:C/.*/${MODULE_DIR}\/&\/lib&.a/}
STATICOBJS	+= openpam_static_modules.o
CLEANFILES	+= openpam_static.o

_EXTRADEPEND: openpam_static.c
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
	    ${CFLAGS:M-nostdinc*} ${CFLAGS:M-[BID]*} \
	    ${.ALLSRC}

.if ${OBJFORMAT} == elf
CLEANFILES+=	setdef0.o openpam_static_modules.o setdef1.o \
		setdef0.c setdef1.c setdefs.h

openpam_static_modules.o: openpam_static.o ${STATIC_MODULES}
	${LD} -o ${.TARGET} -r --whole-archive ${.ALLSRC}
.else
openpam_static_modules.o: openpam_static.o ${STATIC_MODULES}
	${LD} -o ${.TARGET} -r -Bforcearchive ${.ALLSRC}
.endif

# Headers
beforeinstall:
.for HEADER in ${HEADERS}
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${OPENPAM}/include/${HEADER} ${DESTDIR}/usr/include/${HEADER}
.endfor
.for HEADER in ${ADD_HEADERS}
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/security/${HEADER} ${DESTDIR}/usr/include/security/${HEADER}
.endfor

.include <bsd.lib.mk>
.PATH: ${OPENPAM}/include
.PATH: ${OPENPAM}/lib
.PATH: ${OPENPAM}/doc/man
