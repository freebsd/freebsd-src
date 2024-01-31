divert(-1)
#
# Copyright (c) 2023 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
define(`confOPENSSL_CNF', dnl
ifelse(defn(`_ARG_'), `', `/etc/mail/fips.ossl', `_ARG_'))dnl
ifelse(len(X`'_ARG2_),`1',`',`LOCAL_CONFIG
EOPENSSL_MODULES=_ARG2_')
