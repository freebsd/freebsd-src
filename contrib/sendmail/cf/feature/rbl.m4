divert(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`@(#)rbl.m4	8.8 (Berkeley) 5/19/98')
divert(-1)

define(`_RBL_', ifelse(_ARG_, `', `rbl.maps.vix.com', `_ARG_'))dnl
