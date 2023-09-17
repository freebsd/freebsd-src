divert(-1)
#
# Copyright (c) 2020 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#

errprint(`*** ERROR: FEATURE(tls_failures) has been replaced by confTLS_FALLBACK_TO_CLEAR
')
define(`confTLS_FALLBACK_TO_CLEAR', `true')
