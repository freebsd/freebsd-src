divert(-1)
#
# Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
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
VERSIONID(`$Id: bsdi1.0.m4,v 8.11 1999/11/19 05:18:14 gshapiro Exp $')
errprint(`NOTE: OSTYPE(bsdi1.0) is deprecated.  Use OSTYPE(bsdi) instead.')
include(_CF_DIR_`'ostype/bsdi.m4)dnl
