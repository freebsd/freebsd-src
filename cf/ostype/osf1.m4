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
VERSIONID(`$Id: osf1.m4,v 8.16 1999/10/11 18:45:43 gshapiro Exp $')
ifdef(`STATUS_FILE',, `define(`STATUS_FILE', `/usr/adm/sendmail/sendmail.st')')dnl
define(`confDEF_USER_ID', `daemon')
define(`confEBINDIR', `/usr/lbin')dnl
