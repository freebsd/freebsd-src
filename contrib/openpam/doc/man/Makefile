#-
# Copyright (c) 2002-2003 Networks Associates Technology, Inc.
# All rights reserved.
#
# This software was developed for the FreeBSD Project by ThinkSec AS
# and Network Associates Laboratories, the Security Research Division
# of Network Associates, Inc.  under DARPA/SPAWAR contract
# N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS research
# program.
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
# $P4: //depot/projects/openpam/doc/man/Makefile#13 $
#

GENDOC		 = ${.CURDIR}/../../misc/gendoc.pl

# Standard PAM API
PMAN		 =
PMAN		+= pam_acct_mgmt.3
PMAN		+= pam_authenticate.3
PMAN		+= pam_chauthtok.3
PMAN		+= pam_close_session.3
PMAN		+= pam_end.3
PMAN		+= pam_get_data.3
PMAN		+= pam_get_item.3
PMAN		+= pam_get_user.3
PMAN		+= pam_getenv.3
PMAN		+= pam_getenvlist.3
PMAN		+= pam_open_session.3
PMAN		+= pam_putenv.3
PMAN		+= pam_set_data.3
PMAN		+= pam_set_item.3
PMAN		+= pam_setcred.3
PMAN		+= pam_start.3
PMAN		+= pam_strerror.3

# Standard module API
MMAN		 =
MMAN		+= pam_sm_acct_mgmt.3
MMAN		+= pam_sm_authenticate.3
MMAN		+= pam_sm_chauthtok.3
MMAN		+= pam_sm_close_session.3
MMAN		+= pam_sm_open_session.3
MMAN		+= pam_sm_setcred.3

# OpenPAM extensions
OMAN		 =
OMAN		+= openpam_borrow_cred.3
OMAN		+= openpam_free_data.3
OMAN		+= openpam_get_option.3
OMAN		+= openpam_log.3
OMAN		+= openpam_nullconv.3
OMAN		+= openpam_readline.3
OMAN		+= openpam_restore_cred.3
OMAN		+= openpam_set_option.3
OMAN		+= openpam_ttyconv.3
OMAN		+= pam_error.3
OMAN		+= pam_get_authtok.3
OMAN		+= pam_info.3
OMAN		+= pam_prompt.3
OMAN		+= pam_setenv.3
OMAN		+= pam_verror.3
OMAN		+= pam_vinfo.3
OMAN		+= pam_vprompt.3

MAN		 = ${PMAN} ${OMAN} ${MMAN}
MAN		+= openpam.3
MAN		+= pam.3
MAN		+= pam_conv.3

CLEANFILES	+= ${PMAN} ${OMAN} ${MMAN} openpam.3 pam.3

.for man in ${PMAN} ${OMAN} ${MMAN}
${man}: ${.CURDIR}/../../lib/${man:R}.c ${GENDOC}
	perl -w ${GENDOC} ${.CURDIR}/../../lib/${man:R}.c
.endfor

openpam.3: ${OMAN} ${GENDOC} openpam.man
	perl -w ${GENDOC} -o ${OMAN} <${.CURDIR}/openpam.man

pam.3: ${PMAN} ${GENDOC} pam.man
	perl -w ${GENDOC} -p ${PMAN} <${.CURDIR}/pam.man

.include <bsd.prog.mk>
