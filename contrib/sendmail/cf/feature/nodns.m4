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
VERSIONID(`@(#)nodns.m4	8.9 (Berkeley) 10/6/1998')
divert(-1)

undefine(`confBIND_OPTS')dnl
errprint(`FEATURE(nodns) is no-op.
Use ServiceSwitchFile ('ifdef(`confSERVICE_SWITCH_FILE',confSERVICE_SWITCH_FILE,ifdef(`_USE_ETC_MAIL_',`/etc/mail/service.switch',`/etc/service.switch'))`) if your OS does not provide its own instead.
')
