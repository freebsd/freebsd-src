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
VERSIONID(`@(#)dynix3.2.m4	8.10 (Berkeley) 10/6/1998')
define(`ALIAS_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/aliases', `/usr/lib/aliases'))dnl
ifdef(`QUEUE_DIR',, `define(`QUEUE_DIR', /usr/spool/mqueue)')dnl
define(`confEBINDIR', `/usr/lib')dnl
