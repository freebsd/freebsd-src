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
VERSIONID(`$Id: hpux11.m4,v 8.1 1999/11/19 05:22:59 gshapiro Exp $')

ifdef(`LOCAL_MAILER_PATH',, `define(`LOCAL_MAILER_PATH', /usr/bin/rmail)')dnl
_DEFIFNOT(`LOCAL_MAILER_FLAGS', `m9')dnl
ifdef(`LOCAL_MAILER_ARGS',, `define(`LOCAL_MAILER_ARGS', `rmail -d $u')')dnl
ifdef(`LOCAL_SHELL_PATH',, `define(`LOCAL_SHELL_PATH', /usr/bin/sh)')dnl
ifdef(`UUCP_MAILER_ARGS',, `define(`UUCP_MAILER_ARGS', `uux - -r -a$g -gC $h!rmail ($u)')')dnl
define(`confTIME_ZONE', `USE_TZ')dnl
