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
VERSIONID(`@(#)unknown.m4	8.6 (Berkeley) 5/19/98')
errprint(`*** ERROR: You have not specified a valid operating system type.')
errprint(`	Use the OSTYPE macro to select a valid system type.  This')
errprint(`	is necessary in order to get the proper pathnames and flags')
errprint(`	appropriate for your environment.')
