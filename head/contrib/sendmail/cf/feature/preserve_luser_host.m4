divert(-1)
#
# Copyright (c) 2000, 2002 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: preserve_luser_host.m4,v 1.3 2002/04/14 13:22:58 ca Exp $')
divert(-1)

ifdef(`LUSER_RELAY', `',
`errprint(`*** LUSER_RELAY should be defined before FEATURE(`preserve_luser_host')
    ')')
define(`_PRESERVE_LUSER_HOST_', `1')
define(`_NEED_MACRO_MAP_', `1')
