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
VERSIONID(`$Id: CS.Berkeley.EDU.m4,v 1.1.1.3 2000/08/12 21:55:36 gshapiro Exp $')
DOMAIN(Berkeley.EDU)dnl
HACK(cssubdomain)dnl
define(`confUSERDB_SPEC',
	`/usr/sww/share/lib/users.cs.db,/usr/sww/share/lib/users.eecs.db')dnl
