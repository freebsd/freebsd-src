divert(-1)
#
# Copyright (c) 2020 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#

divert(-1)
define(`_MTA_STS_', `')
define(`_NEED_MACRO_MAP_', `1')
ifelse(_ARG2_,`NO_SAN_TST',`',`define(`_STS_SAN', `1')')
LOCAL_CONFIG
O StrictTransportSecurity=true
ifelse(_ARG2_,`NO_SAN_TST',`',`O SetCertAltnames=true')
Ksts ifelse(defn(`_ARG_'), `', socket -d5 -T<TMPF> inet:5461@127.0.0.1,
	       defn(`_NARG_'), `', `_ARG_', `_NARG_')
