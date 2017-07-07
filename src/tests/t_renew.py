#!/usr/bin/python
from k5test import *

conf = {'realms': {'$realm': {'max_life': '20h', 'max_renewable_life': '20h'}}}
realm = K5Realm(create_host=False, get_creds=False, kdc_conf=conf)

def test(testname, life, rlife, expect_renewable, env=None):
    global realm
    flags = ['-l', life]
    if rlife is not None:
        flags += ['-r', rlife]
    realm.kinit(realm.user_princ, password('user'), flags=flags, env=env)
    out = realm.run([klist])
    if ('Default principal: %s\n' % realm.user_princ) not in out:
        fail('%s: did not get tickets' % testname)
    renewable = 'renew until' in out
    if renewable and not expect_renewable:
        fail('%s: tickets unexpectedly renewable' % testname)
    elif not renewable and expect_renewable:
        fail('%s: tickets unexpectedly non-renewable' % testname)

# Get renewable tickets.
test('simple', '1h', '2h', True)

# Renew twice, to test that renewed tickets are renewable.
realm.kinit(realm.user_princ, flags=['-R'])
realm.kinit(realm.user_princ, flags=['-R'])
realm.klist(realm.user_princ)

# Make sure we can use a renewed ticket.
realm.run([kvno, realm.user_princ])

# Make sure we can't renew non-renewable tickets.
test('non-renewable', '1h', '1h', False)
out = realm.kinit(realm.user_princ, flags=['-R'], expected_code=1)
if "KDC can't fulfill requested option" not in out:
    fail('expected error not seen renewing non-renewable ticket')

# Test that -allow_renewable on the client principal works.
realm.run([kadminl, 'modprinc', '-allow_renewable', 'user'])
test('disallowed client', '1h', '2h', False)
realm.run([kadminl, 'modprinc', '+allow_renewable', 'user'])

# Test that -allow_renewable on the server principal works.
realm.run([kadminl, 'modprinc', '-allow_renewable',  realm.krbtgt_princ])
test('disallowed server', '1h', '2h', False)
realm.run([kadminl, 'modprinc', '+allow_renewable', realm.krbtgt_princ])

# Test that non-renewable tickets are issued if renew_till < till.
test('short', '2h', '1h', False)

# Test that renewable tickets are issued if till > max life by
# default, but not if we configure away the RENEWABLE-OK option.
no_opts_conf = {'libdefaults': {'kdc_default_options': '0'}}
no_opts = realm.special_env('no_opts', False, krb5_conf=no_opts_conf)
realm.run([kadminl, 'modprinc', '-maxlife', '10 hours', 'user'])
test('long', '15h', None, True)
test('long noopts', '15h', None, False, env=no_opts)
realm.run([kadminl, 'modprinc', '-maxlife', '20 hours', 'user'])

# Test maximum renewable life on the client principal.
realm.run([kadminl, 'modprinc', '-maxrenewlife', '5 hours', 'user'])
test('maxrenewlife client yes', '4h', '5h', True)
test('maxrenewlife client no', '6h', '10h', False)

# Test maximum renewable life on the server principal.
realm.run([kadminl, 'modprinc', '-maxrenewlife', '3 hours',
           realm.krbtgt_princ])
test('maxrenewlife server yes', '2h', '3h', True)
test('maxrenewlife server no', '4h', '8h', False)

# Test realm maximum life.
realm.run([kadminl, 'modprinc', '-maxrenewlife', '40 hours', 'user'])
realm.run([kadminl, 'modprinc', '-maxrenewlife', '40 hours',
           realm.krbtgt_princ])
test('maxrenewlife realm yes', '10h', '20h', True)
test('maxrenewlife realm no', '21h', '40h', False)

success('Renewing credentials')
