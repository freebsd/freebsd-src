divert(-1)
#
# Copyright (c) 1999-2002, 2004, 2007, 2009 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: ldap_routing.m4,v 8.17 2009/06/26 21:11:08 ca Exp $')
divert(-1)

# Check first two arguments.  If they aren't set, may need to warn in proto.m4
ifelse(len(X`'_ARG1_), `1', `define(`_LDAP_ROUTING_WARN_', `yes')')
ifelse(len(X`'_ARG2_), `1', `define(`_LDAP_ROUTING_WARN_', `yes')')
ifelse(len(X`'_ARG5_), `1', `', `define(`_LDAP_ROUTE_NODOMAIN_', `yes')')

# Check for third argument to indicate how to deal with non-existant
# LDAP records
ifelse(len(X`'_ARG3_), `1', `define(`_LDAP_ROUTING_', `_PASS_THROUGH_')',
       _ARG3_, `passthru', `define(`_LDAP_ROUTING_', `_PASS_THROUGH_')',
       _ARG3_, `sendertoo', `define(`_LDAP_ROUTING_', `_MUST_EXIST_')define(`_LDAP_SENDER_MUST_EXIST_')',
       `define(`_LDAP_ROUTING_', `_MUST_EXIST_')')

# Check for fourth argument to indicate how to deal with +detail info
ifelse(len(X`'_ARG4_), `1', `',
       _ARG4_, `strip', `define(`_LDAP_ROUTE_DETAIL_', `_STRIP_')',
       _ARG4_, `preserve', `define(`_LDAP_ROUTE_DETAIL_', `_PRESERVE_')')

# Check for sixth argument to indicate how to deal with tempfails
ifelse(len(X`'_ARG6_), `1', `define(`_LDAP_ROUTE_MAPTEMP_', `_QUEUE_')',
       _ARG6_, `tempfail', `define(`_LDAP_ROUTE_MAPTEMP_', `_TEMPFAIL_')',
       _ARG6_, `queue', `define(`_LDAP_ROUTE_MAPTEMP_', `_QUEUE_')')

define(`_ATMPF_', `<TMPF>')dnl
dnl check whether arg contains -T`'_ATMPF_
dnl unless it is a sequence map or just LDAP
dnl note: this does not work if ARG1 begins with space(s), however, as
dnl we issue a warning, hopefully the user will fix it...
ifelse(defn(`_ARG1_'), `', `',
  defn(`_ARG1_'), `LDAP', `',
  `ifelse(index(_ARG1_, `sequence '), `0', `',
    `ifelse(index(_ARG1_, _ATMPF_), `-1',
      `errprint(`*** WARNING: missing -T'_ATMPF_` in first argument of FEATURE(`ldap_routing')
')
      define(`_ABP_', index(_ARG1_, ` '))
      define(`_NARG1_', `substr(_ARG1_, 0, _ABP_) -T'_ATMPF_` substr(_ARG1_, _ABP_)')
      ')
    ')
  ')
ifelse(defn(`_ARG2_'), `', `',
  defn(`_ARG2_'), `LDAP', `',
  `ifelse(index(_ARG2_, `sequence '), `0', `',
    `ifelse(index(_ARG2_, _ATMPF_), `-1',
      `errprint(`*** WARNING: missing -T'_ATMPF_` in second argument of FEATURE(`ldap_routing')
')
      define(`_ABP_', index(_ARG2_, ` '))
      define(`_NARG2_', `substr(_ARG2_, 0, _ABP_) -T'_ATMPF_` substr(_ARG2_, _ABP_)')
      ')
    ')
  ')

LOCAL_CONFIG
# LDAP routing maps
Kldapmh ifelse(len(X`'_ARG1_), `1',
	       `ldap -1 -T<TMPF> -v mailHost -k (&(objectClass=inetLocalMailRecipient)(mailLocalAddress=%0))',
	       defn(`_NARG1_'), `', `_ARG1_', `_NARG1_')

Kldapmra ifelse(len(X`'_ARG2_), `1',
		`ldap -1 -T<TMPF> -v mailRoutingAddress -k (&(objectClass=inetLocalMailRecipient)(mailLocalAddress=%0))',
		defn(`_NARG2_'), `', `_ARG2_', `_NARG2_')
