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
VERSIONID(`$Id: powerux.m4,v 8.13 1999/04/24 05:37:43 gshapiro Exp $')

define(`LOCAL_MAILER_PATH', `/usr/bin/rmail')dnl
_DEFIFNOT(`LOCAL_MAILER_FLAGS', `mn9')dnl
define(`LOCAL_MAILER_ARGS', `rmail $u')dnl
define(`LOCAL_SHELL_FLAGS', `ehuP')dnl
define(`UUCP_MAILER_ARGS', `uux - -r -a$g -gmedium $h!rmail ($u)')dnl
define(`confEBINDIR', `/usr/local/lib')dnl
