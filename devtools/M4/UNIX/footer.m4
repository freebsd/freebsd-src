divert(-1)
#
# Copyright (c) 1999 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#  Definitions for Makefile construction for sendmail
#
#	$Id: footer.m4,v 8.3 2013-11-22 20:51:22 ca Exp $
#
divert(0)dnl
################  Dependency scripts
include(confBUILDTOOLSDIR/M4/depend/ifdef(`confDEPEND_TYPE', `confDEPEND_TYPE',
`generic').m4)dnl
################  End of dependency scripts
