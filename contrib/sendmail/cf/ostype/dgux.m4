divert(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
# Copyright (c) 1983 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`@(#)dgux.m4	8.10 (Berkeley) 5/19/1998')
ifdef(`LOCAL_MAILER_FLAGS',, `define(`LOCAL_MAILER_FLAGS', m9)')dnl
define(`confTIME_ZONE', `USE_TZ')dnl
define(`confEBINDIR', `/usr/lib')dnl
LOCAL_CONFIG
E_FORCE_MAIL_LOCAL_=yes
