divert(-1)
#
# Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: ldap_routing.m4,v 8.5.4.1 2000/07/15 18:05:05 gshapiro Exp $')
divert(-1)

# Check first two arguments.  If they aren't set, may need to warn in proto.m4
ifelse(len(X`'_ARG1_), `1', `define(`_LDAP_ROUTING_WARN_', `yes')')
ifelse(len(X`'_ARG2_), `1', `define(`_LDAP_ROUTING_WARN_', `yes')')

# Check for third argument to indicate how to deal with non-existant
# LDAP records
ifelse(len(X`'_ARG3_), `1', `define(`_LDAP_ROUTING_', `_PASS_THROUGH_')',
       _ARG3_, `passthru', `define(`_LDAP_ROUTING_', `_PASS_THROUGH_')',
       `define(`_LDAP_ROUTING_', `_MUST_EXIST_')')

LOCAL_CONFIG
# LDAP routing maps
Kldapmh ifelse(len(X`'_ARG1_), `1',
	       `ldap -1 -v mailHost -k (&(objectClass=inetLocalMailRecipient)(mailLocalAddress=%0))',
	       `_ARG1_')

Kldapmra ifelse(len(X`'_ARG2_), `1',
		`ldap -1 -v mailRoutingAddress -k (&(objectClass=inetLocalMailRecipient)(mailLocalAddress=%0))',
		`_ARG2_')
