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

OPENPAM=	${.CURDIR}/../../../contrib/openpam
.PATH: ${OPENPAM}/include ${OPENPAM}/lib ${OPENPAM}/doc/man

LIB=	pam
NO_PROFILE=

SRCS=	openpam_borrow_cred.c \
	openpam_configure.c \
	openpam_dispatch.c \
	openpam_dynamic.c \
	openpam_findenv.c \
	openpam_free_data.c \
	openpam_free_envlist.c \
	openpam_get_option.c \
	openpam_load.c \
	openpam_log.c \
	openpam_nullconv.c \
	openpam_readline.c \
	openpam_restore_cred.c \
	openpam_set_option.c \
	openpam_ttyconv.c \
	pam_acct_mgmt.c \
	pam_authenticate.c \
	pam_chauthtok.c \
	pam_close_session.c \
	pam_end.c \
	pam_error.c \
	pam_get_authtok.c \
	pam_get_data.c \
	pam_get_item.c \
	pam_get_user.c \
	pam_getenv.c \
	pam_getenvlist.c \
	pam_info.c \
	pam_open_session.c \
	pam_prompt.c \
	pam_putenv.c \
	pam_set_data.c \
	pam_set_item.c \
	pam_setcred.c \
	pam_setenv.c \
	pam_start.c \
	pam_strerror.c \
	pam_verror.c \
	pam_vinfo.c \
	pam_vprompt.c
# Local additions
SRCS+=	pam_debug_log.c

MAN=	openpam.3 \
	openpam_borrow_cred.3 \
	openpam_free_data.3 \
	openpam_free_envlist.3 \
	openpam_get_option.3 \
	openpam_log.3 \
	openpam_nullconv.3 \
	openpam_readline.3 \
	openpam_restore_cred.3 \
	openpam_set_option.3 \
	openpam_ttyconv.3 \
	pam.3 \
	pam_acct_mgmt.3 \
	pam_authenticate.3 \
	pam_chauthtok.3 \
	pam_close_session.3 \
	pam_conv.3 \
	pam_end.3 \
	pam_error.3 \
	pam_get_authtok.3 \
	pam_get_data.3 \
	pam_get_item.3 \
	pam_get_user.3 \
	pam_getenv.3 \
	pam_getenvlist.3 \
	pam_info.3 \
	pam_open_session.3 \
	pam_prompt.3 \
	pam_putenv.3 \
	pam_set_data.3 \
	pam_set_item.3 \
	pam_setcred.3 \
	pam_setenv.3 \
	pam_sm_acct_mgmt.3 \
	pam_sm_authenticate.3 \
	pam_sm_chauthtok.3 \
	pam_sm_close_session.3 \
	pam_sm_open_session.3 \
	pam_sm_setcred.3 \
	pam_start.3 \
	pam_strerror.3 \
	pam_verror.3 \
	pam_vinfo.3 \
	pam_vprompt.3

CSTD?= c99
WARNS?=	3
CFLAGS+= -I${.CURDIR} -I${OPENPAM}/include
CFLAGS+= -DLIB_MAJ=${SHLIB_MAJOR}

HEADERS=	security/openpam.h \
		security/openpam_version.h \
		security/pam_appl.h \
		security/pam_constants.h \
		security/pam_modules.h \
		security/pam_types.h \

ADD_HEADERS=	security/pam_mod_misc.h

# Static modules
MODULE_DIR=	../modules
.include "${.CURDIR}/${MODULE_DIR}/modules.inc"
STATIC_MODULES=	${MODULES:C/.*/${MODULE_DIR}\/&\/lib&.a/}
STATICOBJS+=	openpam_static_modules.o
CLEANFILES+=	openpam_static.o \
		openpam_static_modules.o

openpam_static_modules.o: openpam_static.o ${STATIC_MODULES}
	${LD} -o ${.TARGET} -r --whole-archive ${.ALLSRC}

# Can't put openpam_static.c in SRCS but want .o in .depend.
DPSRCS=		openpam_static.c

# Headers
INCS=		${HEADERS} ${ADD_HEADERS}
INCSDIR=	${INCLUDEDIR}/security

.include <bsd.lib.mk>
