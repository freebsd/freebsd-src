divert(-1)
#
# Copyright (c) 2001 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: mpeix.m4,v 1.1 2001/12/13 23:56:40 gshapiro Exp $')

ifdef(`LOCAL_MAILER_PATH',, `define(`LOCAL_MAILER_PATH', `/bin/tsmail')')dnl
_DEFIFNOT(`LOCAL_MAILER_FLAGS', `mu9')dnl
ifdef(`LOCAL_MAILER_ARGS',, `define(`LOCAL_MAILER_ARGS', `tsmail $u')')dnl
ifdef(`LOCAL_SHELL_PATH',, `define(`LOCAL_SHELL_PATH', `/bin/sh')')dnl
ifdef(`confDEF_USER_ID',, `define(`confDEF_USER_ID', `SERVER.SENDMAIL')')dnl
ifdef(`confTRUSTED_USER',, `define(`confTRUSTED_USER', `SERVER.SENDMAIL')')dnl
define(`confTIME_ZONE', `USE_TZ')dnl
define(`confDONT_BLAME_SENDMAIL', `ForwardFileInGroupWritableDirPath')dnl
