divert(-1)
#
# Copyright (c) 2000 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: preserve_luser_host.m4,v 1.2 2000/11/10 18:50:30 ca Exp $')
divert(-1)

ifdef(`LUSER_RELAY', `',
`errprint(`*** LUSER_RELAY should be defined before FEATURE(`preserve_luser_host')
    ')')
define(`_PRESERVE_LUSER_HOST_', `1')
