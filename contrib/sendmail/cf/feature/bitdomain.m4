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
VERSIONID(`@(#)bitdomain.m4	8.13 (Berkeley) 5/19/98')
divert(-1)

define(`BITDOMAIN_TABLE', ifelse(_ARG_, `', DATABASE_MAP_TYPE` -o /etc/bitdomain', `_ARG_'))dnl
