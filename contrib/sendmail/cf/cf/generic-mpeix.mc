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

#
#  This is a generic configuration file for HP MPE/iX.
#  It has support for local and SMTP mail only.  If you want to
#  customize it, copy it to a name appropriate for your environment
#  and do the modifications there.
#

divert(0)dnl
VERSIONID(`$Id: generic-mpeix.mc,v 8.1 2001/12/13 23:56:37 gshapiro Exp $')
OSTYPE(mpeix)dnl
DOMAIN(generic)dnl
define(`confFORWARD_PATH', `$z/.forward')dnl
MAILER(local)dnl
MAILER(smtp)dnl
