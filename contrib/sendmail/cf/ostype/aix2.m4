divert(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
# Copyright (c) 1995 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`@(#)aix2.m4	8.8 (Berkeley) 5/19/1998')
define(`LOCAL_MAILER_PATH', /bin/bellmail)dnl
define(`LOCAL_MAILER_ARGS', mail $u)dnl
define(`LOCAL_MAILER_FLAGS', `mn9')dnl
define(`confEBINDIR', `/usr/lib')dnl
define(`confTIME_ZONE', `USE_TZ')dnl
