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
VERSIONID(`$Id: unspecified-domain.m4,v 8.10 1999/02/07 07:26:07 gshapiro Exp $')
errprint(`*** ERROR: You are trying to use the Berkeley sample configuration')
errprint(`	files outside of the Computer Science Division at Berkeley.')
errprint(`	The configuration (.mc) files must be customized to reference')
errprint(`	domain files appropriate for your environment.')
