divert(-1)
#
# Copyright (c) 2006 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)dnl
VERSIONID(`$Id: block_bad_helo.m4,v 1.1 2006/06/15 22:49:30 ca Exp $')
divert(-1)

define(`_BLOCK_BAD_HELO_', `')dnl
RELAY_DOMAIN(`127.0.0.1')dnl
LOCAL_DOMAIN(`[127.0.0.1]')dnl
