#!/usr/bin/python

# Copyright (C) 2011 by the Massachusetts Institute of Technology.
# All rights reserved.

# Export of this software from the United States of America may
#   require a specific license from the United States Government.
#   It is the responsibility of any person or organization contemplating
#   export to obtain such a license before exporting.
#
# WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
# distribute this software and its documentation for any purpose and
# without fee is hereby granted, provided that the above copyright
# notice appear in all copies and that both that copyright notice and
# this permission notice appear in supporting documentation, and that
# the name of M.I.T. not be used in advertising or publicity pertaining
# to distribution of the software without specific, written prior
# permission.  Furthermore if you modify this software you must label
# your software as modified software and not distribute it in such a
# fashion that it might be confused with the original M.I.T. software.
# M.I.T. makes no representations about the suitability of
# this software for any purpose.  It is provided "as is" without express
# or implied warranty.

from k5test import *

# Create two independent realms (no cross-realm TGTs).
r1 = K5Realm(create_user=False)
r2 = K5Realm(create_user=False, realm='KRBTEST2.COM', portbase=62000,
             testdir=os.path.join(r1.testdir, 'r2'))

host1 = 'p:' + r1.host_princ
host2 = 'p:' + r2.host_princ

# gsserver specifies the target as a GSS name.  The resulting
# principal will have the host-based type, but the realm won't be
# known before the client cache is selected (since k5test realms have
# no domain-realm mapping by default).
gssserver = 'h:host@' + hostname

# refserver specifies the target as a principal in the referral realm.
# The principal won't be treated as a host principal by the
# .k5identity rules since it has unknown type.
refserver = 'p:host/' + hostname + '@'

# Verify that we can't get initiator creds with no credentials in the
# collection.
output = r1.run(['./t_ccselect', host1, '-'], expected_code=1)
if 'No Kerberos credentials available' not in output:
    fail('Expected error not seen in output when no credentials available')

# Make a directory collection and use it for client commands in both realms.
ccdir = os.path.join(r1.testdir, 'cc')
ccname = 'DIR:' + ccdir
r1.env['KRB5CCNAME'] = ccname
r2.env['KRB5CCNAME'] = ccname

# Use .k5identity from testdir and not from the tester's homedir.
r1.env['HOME'] = r1.testdir
r2.env['HOME'] = r1.testdir

# Create two users in r1 and one in r2.
alice='alice@KRBTEST.COM'
bob='bob@KRBTEST.COM'
zaphod='zaphod@KRBTEST2.COM'
r1.addprinc(alice, password('alice'))
r1.addprinc(bob, password('bob'))
r2.addprinc(zaphod, password('zaphod'))

# Get tickets for one user in each realm (zaphod will be primary).
r1.kinit(alice, password('alice'))
r2.kinit(zaphod, password('zaphod'))

# Check that we can find a cache for a specified client principal.
output = r1.run(['./t_ccselect', host1, 'p:' + alice])
if output != (alice + '\n'):
    fail('alice not chosen when specified')
output = r2.run(['./t_ccselect', host2, 'p:' + zaphod])
if output != (zaphod + '\n'):
    fail('zaphod not chosen when specified')

# Check that we can guess a cache based on the service realm.
output = r1.run(['./t_ccselect', host1])
if output != (alice + '\n'):
    fail('alice not chosen as default initiator cred for server in r1')
output = r1.run(['./t_ccselect', host1, '-'])
if output != (alice + '\n'):
    fail('alice not chosen as default initiator name for server in r1')
output = r2.run(['./t_ccselect', host2])
if output != (zaphod + '\n'):
    fail('zaphod not chosen as default initiator cred for server in r1')
output = r2.run(['./t_ccselect', host2, '-'])
if output != (zaphod + '\n'):
    fail('zaphod not chosen as default initiator name for server in r1')

# Check that primary cache is used if server realm is unknown.
output = r2.run(['./t_ccselect', gssserver])
if output != (zaphod + '\n'):
    fail('zaphod not chosen via primary cache for unknown server realm')
r1.run(['./t_ccselect', gssserver], expected_code=1)

# Get a second cred in r1 (bob will be primary).
r1.kinit(bob, password('bob'))

# Try some cache selections using .k5identity.
k5id = open(os.path.join(r1.testdir, '.k5identity'), 'w')
k5id.write('%s realm=%s\n' % (alice, r1.realm))
k5id.write('%s service=ho*t host=%s\n' % (zaphod, hostname))
k5id.write('noprinc service=bogus')
k5id.close()
output = r1.run(['./t_ccselect', host1])
if output != (alice + '\n'):
    fail('alice not chosen via .k5identity realm line.')
output = r2.run(['./t_ccselect', gssserver])
if output != (zaphod + '\n'):
    fail('zaphod not chosen via .k5identity service/host line.')
output = r1.run(['./t_ccselect', refserver])
if output != (bob + '\n'):
    fail('bob not chosen via primary cache when no .k5identity line matches.')
output = r1.run(['./t_ccselect', 'h:bogus@' + hostname], expected_code=1)
if 'Can\'t find client principal noprinc' not in output:
    fail('Expected error not seen when k5identity selects bad principal.')

success('GSSAPI credential selection tests')
