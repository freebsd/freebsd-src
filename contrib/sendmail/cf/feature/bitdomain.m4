divert(-1)
#
# Copyright (c) 1998, 1999, 2001 Sendmail, Inc. and its suppliers.
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
VERSIONID(`$Id: bitdomain.m4,v 8.28 2001/03/16 00:51:25 gshapiro Exp $')
divert(-1)

define(`_BITDOMAIN_TABLE_', `')

LOCAL_CONFIG
# BITNET mapping table
Kbitdomain ifelse(defn(`_ARG_'), `', DATABASE_MAP_TYPE MAIL_SETTINGS_DIR`bitdomain',
		  defn(`_ARG_'), `LDAP', `ldap -1 -v sendmailMTAMapValue -k (&(objectClass=sendmailMTAMapObject)(|(sendmailMTACluster=${sendmailMTACluster})(sendmailMTAHost=$j))(sendmailMTAMapName=bitdomain)(sendmailMTAKey=%0))',
		  `_ARG_')
