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
VERSIONID(`$Id: use_ct_file.m4,v 8.9 1999/02/07 07:26:13 gshapiro Exp $')
divert(-1)

# if defined, the sendmail.cf will read the /etc/sendmail.ct file
# to find the names of trusted users.  There should only be a few
# of these, and normally this is done directly in the .cf file.

define(`_USE_CT_FILE_', `')

divert(0)
