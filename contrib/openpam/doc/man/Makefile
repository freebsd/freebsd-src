#-
# Copyright (c) 2002 Networks Associates Technologies, Inc.
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
# $Id$
#

MAN		 =
MAN		+= openpam_get_option.3
MAN		+= openpam_log.3
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
MAN		+= pam_verror.3
MAN		+= pam_vinfo.3
MAN		+= pam_vprompt.3

generate:
	(cd ${.CURDIR} && perl -w ../../misc/gendoc.pl ../../lib/*.c >pam.3)

.include <bsd.prog.mk>
