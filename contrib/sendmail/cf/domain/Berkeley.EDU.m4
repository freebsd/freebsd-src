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
VERSIONID(`@(#)Berkeley.EDU.m4	8.14 (Berkeley) 5/19/98')
DOMAIN(berkeley-only)dnl
define(`BITNET_RELAY', `bitnet-relay.Berkeley.EDU')dnl
define(`UUCP_RELAY', `uucp-relay.Berkeley.EDU')dnl
define(`confFORWARD_PATH', `$z/.forward.$w:$z/.forward')dnl
define(`confCW_FILE', `-o /etc/sendmail.cw')dnl
define(`confDONT_INIT_GROUPS', True)dnl
FEATURE(redirect)dnl
FEATURE(use_cw_file)dnl
FEATURE(stickyhost)dnl
