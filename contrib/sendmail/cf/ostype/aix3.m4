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
VERSIONID(`@(#)aix3.m4	8.12 (Berkeley) 5/19/98')
ifdef(`LOCAL_MAILER_PATH',, `define(`LOCAL_MAILER_PATH', /bin/bellmail)')dnl
ifdef(`LOCAL_MAILER_ARGS',, `define(`LOCAL_MAILER_ARGS', mail $u)')dnl
ifdef(`LOCAL_MAILER_FLAGS',, `define(`LOCAL_MAILER_FLAGS', `mn9')')dnl
define(`confEBINDIR', `/usr/lib')dnl
define(`confTIME_ZONE', `USE_TZ')dnl
