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

# Create two independent realms (no cross-realm TGTs).  For the
# fallback realm tests we need to control the precise server hostname,
# so turn off DNS canonicalization and shortname qualification.
conf = {'libdefaults': {'dns_canonicalize_hostname': 'false',
                        'qualify_shortname': ''}}
r1 = K5Realm(create_user=False, krb5_conf=conf)
r2 = K5Realm(create_user=False, krb5_conf=conf, realm='KRBTEST2.COM',
             portbase=62000, testdir=os.path.join(r1.testdir, 'r2'))

host1 = 'p:' + r1.host_princ
host2 = 'p:' + r2.host_princ
foo = 'foo.krbtest.com'
foo2 = 'foo.krbtest2.com'
foobar = "foo.bar.krbtest.com"

# These strings specify the target as a GSS name.  The resulting
# principal will have the host-based type, with the referral realm
# (since k5test realms have no domain-realm mapping by default).
# krb5_cc_select() will use the fallback realm, which is either the
# uppercased parent domain, or the default realm if the hostname is a
# single component.
gssserver = 'h:host@' + foo
gssserver2 = 'h:host@' + foo2
gssserver_bar = 'h:host@' + foobar
gsslocal = 'h:host@localhost'

# refserver specifies the target as a principal in the referral realm.
# The principal won't be treated as a host principal by the
# .k5identity rules since it has unknown type.
refserver = 'p:host/' + hostname + '@'

# Verify that we can't get initiator creds with no credentials in the
# collection.
r1.run(['./t_ccselect', host1, '-'], expected_code=1,
       expected_msg='No Kerberos credentials available')

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

# Create host principals and keytabs for fallback realm tests.
if hostname != 'localhost':
    r1.addprinc('host/localhost')
    r2.addprinc('host/localhost')
r1.addprinc('host/' + foo)
r2.addprinc('host/' + foo2)
r1.addprinc('host/' + foobar)
r1.extract_keytab('host/localhost', r1.keytab)
r2.extract_keytab('host/localhost', r2.keytab)
r1.extract_keytab('host/' + foo, r1.keytab)
r2.extract_keytab('host/' + foo2, r2.keytab)
r1.extract_keytab('host/' + foobar, r1.keytab)

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
output = r2.run(['./t_ccselect', refserver])
if output != (zaphod + '\n'):
    fail('zaphod not chosen via primary cache for unknown server realm')
r1.run(['./t_ccselect', gssserver2], expected_code=1)
# Check ccache selection using a fallback realm.
output = r1.run(['./t_ccselect', gssserver])
if output != (alice + '\n'):
    fail('alice not chosen via parent domain fallback')
output = r2.run(['./t_ccselect', gssserver2])
if output != (zaphod + '\n'):
    fail('zaphod not chosen via parent domain fallback')
# Check ccache selection using a fallback realm (default realm).
output = r1.run(['./t_ccselect', gsslocal])
if output != (alice + '\n'):
    fail('alice not chosen via default realm fallback')
output = r2.run(['./t_ccselect', gsslocal])
if output != (zaphod + '\n'):
    fail('zaphod not chosen via default realm fallback')

# Check that realm ccselect fallback works correctly
r1.run(['./t_ccselect', gssserver_bar], expected_msg=alice)
r2.kinit(zaphod, password('zaphod'))
r1.run(['./t_ccselect', gssserver_bar], expected_msg=alice)

# Get a second cred in r1 (bob will be primary).
r1.kinit(bob, password('bob'))

# Try some cache selections using .k5identity.
k5id = open(os.path.join(r1.testdir, '.k5identity'), 'w')
k5id.write('%s realm=%s\n' % (alice, r1.realm))
k5id.write('%s service=ho*t host=localhost\n' % zaphod)
k5id.write('noprinc service=bogus')
k5id.close()
output = r1.run(['./t_ccselect', host1])
if output != (alice + '\n'):
    fail('alice not chosen via .k5identity realm line.')
output = r2.run(['./t_ccselect', gsslocal])
if output != (zaphod + '\n'):
    fail('zaphod not chosen via .k5identity service/host line.')
output = r1.run(['./t_ccselect', refserver])
if output != (bob + '\n'):
    fail('bob not chosen via primary cache when no .k5identity line matches.')
r1.run(['./t_ccselect', 'h:bogus@' + foo2], expected_code=1,
       expected_msg="Can't find client principal noprinc")

success('GSSAPI credential selection tests')
