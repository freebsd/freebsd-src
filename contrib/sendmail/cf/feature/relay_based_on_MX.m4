divert(-1)
#
# Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: relay_based_on_MX.m4,v 8.11 1999/04/02 02:25:13 gshapiro Exp $')
divert(-1)

define(`_RELAY_MX_SERVED_', 1)

LOCAL_CONFIG
# MX map (to allow relaying to hosts that we MX for)
Kmxserved bestmx -z: -T<TEMP>

