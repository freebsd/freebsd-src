divert(-1)
#
# Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
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
VERSIONID(`$Id: nodns.m4,v 8.14 1999/07/22 17:55:35 gshapiro Exp $')
divert(-1)

undefine(`confBIND_OPTS')dnl
errprint(`FEATURE(nodns) is no-op.
Use ServiceSwitchFile ('ifdef(`confSERVICE_SWITCH_FILE',confSERVICE_SWITCH_FILE,MAIL_SETTINGS_DIR`service.switch')`) if your OS does not provide its own instead.
')
