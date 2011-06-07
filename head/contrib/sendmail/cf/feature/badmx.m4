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

divert(0)
VERSIONID(`$Id: badmx.m4,v 1.1 2006/12/16 00:56:32 ca Exp $')
divert(-1)

define(`_BADMX_CHK_', 1)

LOCAL_CONFIG
Kmxlist bestmx -z: -T<TEMP>
Kbadmx regex -a<BADMX> ^(([0-9]{1,3}\.){3}[0-9]){0,1}\.$
KdnsA dns -R A -a. -T<TEMP>
KBadMXIP regex -a<BADMXIP> ifelse(defn(`_ARG_'), `', `^(127\.|10\.|0\.0\.0\.0)', `_ARG_')
