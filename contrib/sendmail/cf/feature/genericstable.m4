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
VERSIONID(`@(#)genericstable.m4	8.7 (Berkeley) 5/19/98')
divert(-1)

define(`GENERICS_TABLE', ifelse(_ARG_, `', DATABASE_MAP_TYPE` -o /etc/genericstable', `_ARG_'))dnl
