#-
# Copyright (c) 2002 Networks Associates Technology, Inc.
# All rights reserved.
#
# This software was developed for the FreeBSD Project by ThinkSec AS and
# NAI Labs, the Security Research Division of Network Associates, Inc.
# under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
# DARPA CHATS research program.
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
# $P4: //depot/projects/openpam/lib/Makefile#14 $
#

LIB		 = pam
SHLIB_MAJOR	 = 2
SHLIB_MINOR	 = 0

WARNS		?= 4
NO_WERROR	 = yes
CFLAGS		+= -I${.CURDIR}/../include
CFLAGS		+= -DLIB_MAJ=${SHLIB_MAJOR}

SRCS		 =
SRCS		+= openpam_borrow_cred.c
SRCS		+= openpam_configure.c
SRCS		+= openpam_dispatch.c
SRCS		+= openpam_dynamic.c
SRCS		+= openpam_findenv.c
SRCS		+= openpam_free_data.c
SRCS		+= openpam_get_option.c
SRCS		+= openpam_load.c
SRCS		+= openpam_log.c
SRCS		+= openpam_restore_cred.c
SRCS		+= openpam_set_option.c
SRCS		+= openpam_static.c
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

.if 0
SRCS		+= pam_authenticate_secondary.c
SRCS		+= pam_get_mapped_authtok.c
SRCS		+= pam_get_mapped_username.c
SRCS		+= pam_set_mapped_authtok.c
SRCS		+= pam_set_mapped_username.c
.endif

.include <bsd.lib.mk>
