from k5test import *
from datetime import datetime
import re

conf = {'realms': {'$realm': {'max_life': '20h', 'max_renewable_life': '20h'}}}
realm = K5Realm(create_host=False, get_creds=False, kdc_conf=conf)

# We will be scraping timestamps from klist to compute lifetimes, so
# use a time zone with no daylight savings time.
realm.env['TZ'] = 'UTC'

def test(testname, life, rlife, exp_life, exp_rlife, env=None):
    global realm
    flags = ['-l', life]
    if rlife is not None:
        flags += ['-r', rlife]
    realm.kinit(realm.user_princ, password('user'), flags=flags, env=env)
    out = realm.run([klist, '-f'])

    if ('Default principal: %s\n' % realm.user_princ) not in out:
        fail('%s: did not get tickets' % testname)

    # Extract flags and check the renewable flag against expectations.
    flags = re.findall(r'Flags: ([a-zA-Z]*)', out)[0]
    if exp_rlife is None and 'R' in flags:
        fail('%s: ticket unexpectedly renewable' % testname)
    if exp_rlife is not None and 'R' not in flags:
        fail('%s: ticket unexpectedly non-renewable' % testname)

    # Extract the start time, end time, and renewable end time if present.
    times = re.findall(r'\d\d/\d\d/\d\d \d\d:\d\d:\d\d', out)
    times = [datetime.strptime(t, '%m/%d/%y %H:%M:%S') for t in times]
    starttime = times[0]
    endtime = times[1]
    rtime = times[2] if len(times) >= 3 else None

    # Check the ticket lifetime against expectations.  If the lifetime
    # was determined by the request, there may be a small error
    # because KDC requests contain an end time rather than a lifetime.
    life = (endtime - starttime).seconds
    if abs(life - exp_life) > 5:
        fail('%s: expected life %d, got %d' % (testname, exp_life, life))

    # Check the ticket renewable lifetime against expectations.
    if exp_rlife is None and rtime is not None:
        fail('%s: ticket has unexpected renew_till' % testname)
    if exp_rlife is not None and rtime is None:
        fail('%s: ticket is renewable but has no renew_till' % testname)
    if rtime is not None:
        rlife = (rtime - starttime).seconds
        if abs(rlife - exp_rlife) > 5:
            fail('%s: expected rlife %d, got %d' %
                 (testname, exp_rlife, rlife))

# Get renewable tickets.
test('simple', '1h', '2h', 3600, 7200)

# Renew twice, to test that renewed tickets are renewable.
mark('renew twice')
realm.kinit(realm.user_princ, flags=['-R'])
realm.kinit(realm.user_princ, flags=['-R'])
realm.klist(realm.user_princ)

# Make sure we can use a renewed ticket.
realm.run([kvno, realm.user_princ])

# Make sure we can't renew non-renewable tickets.
mark('non-renewable')
test('non-renewable', '1h', None, 3600, None)
realm.kinit(realm.user_princ, flags=['-R'], expected_code=1,
            expected_msg="KDC can't fulfill requested option")

# Test that -allow_renewable on the client principal works.
mark('allow_renewable (client)')
realm.run([kadminl, 'modprinc', '-allow_renewable', 'user'])
test('disallowed client', '1h', '2h', 3600, None)
realm.run([kadminl, 'modprinc', '+allow_renewable', 'user'])

# Test that -allow_renewable on the server principal works.
mark('allow_renewable (server)')
realm.run([kadminl, 'modprinc', '-allow_renewable',  realm.krbtgt_princ])
test('disallowed server', '1h', '2h', 3600, None)
realm.run([kadminl, 'modprinc', '+allow_renewable', realm.krbtgt_princ])

# Test that trivially renewable tickets are issued if renew_till <=
# till.  (Our client code bumps up the requested renewable life to the
# requested life.)
mark('trivially renewable')
test('short', '2h', '1h', 7200, 7200)

# Test that renewable tickets are issued if till > max life by
# default, but not if we configure away the RENEWABLE-OK option.
mark('renewable-ok')
no_opts_conf = {'libdefaults': {'kdc_default_options': '0'}}
no_opts = realm.special_env('no_opts', False, krb5_conf=no_opts_conf)
realm.run([kadminl, 'modprinc', '-maxlife', '10 hours', 'user'])
test('long', '15h', None, 10 * 3600, 15 * 3600)
test('long noopts', '15h', None, 10 * 3600, None, env=no_opts)
realm.run([kadminl, 'modprinc', '-maxlife', '20 hours', 'user'])

# Test maximum renewable life on the client principal.
mark('maxrenewlife (client)')
realm.run([kadminl, 'modprinc', '-maxrenewlife', '5 hours', 'user'])
test('maxrenewlife client 1', '4h', '5h', 4 * 3600, 5 * 3600)
test('maxrenewlife client 2', '6h', '10h', 6 * 3600, 5 * 3600)

# Test maximum renewable life on the server principal.
mark('maxrenewlife (server)')
realm.run([kadminl, 'modprinc', '-maxrenewlife', '3 hours',
           realm.krbtgt_princ])
test('maxrenewlife server 1', '2h', '3h', 2 * 3600, 3 * 3600)
test('maxrenewlife server 2', '4h', '8h', 4 * 3600, 3 * 3600)

# Test realm maximum life.
mark('realm maximum life')
realm.run([kadminl, 'modprinc', '-maxrenewlife', '40 hours', 'user'])
realm.run([kadminl, 'modprinc', '-maxrenewlife', '40 hours',
           realm.krbtgt_princ])
test('maxrenewlife realm 1', '10h', '20h', 10 * 3600, 20 * 3600)
test('maxrenewlife realm 2', '21h', '40h', 20 * 3600, 20 * 3600)

success('Renewing credentials')
