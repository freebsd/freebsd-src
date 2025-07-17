from k5test import *

# These tests will become much less important after the y2038 boundary
# has elapsed, and may start exhibiting problems around the year 2075.

if runenv.sizeof_time_t <= 4:
    skip_rest('y2038 timestamp tests', 'platform has 32-bit time_t')

# Start a KDC running roughly 21 years in the future, after the y2038
# boundary.  Set long maximum lifetimes for later tests.
conf = {'realms': {'$realm': {'max_life': '9000d',
                              'max_renewable_life': '9000d'}}}
realm = K5Realm(start_kdc=False, kdc_conf=conf)
realm.start_kdc(['-T', '662256000'])

# kinit without preauth should succeed with clock skew correction, but
# will result in an expired ticket, because we sent an absolute end
# time and didn't get a chance to correct it..
mark('kinit, no preauth')
realm.kinit(realm.user_princ, password('user'))
realm.run([kvno, realm.host_princ], expected_code=1,
          expected_msg='Ticket expired')

# kinit with preauth should succeed and result in a valid ticket, as
# we get a chance to correct the end time based on the KDC time.  Try
# with encrypted timestamp and encrypted challenge.
mark('kinit, with preauth')
realm.run([kadminl, 'modprinc', '+requires_preauth', 'user'])
realm.kinit(realm.user_princ, password('user'))
realm.run([kvno, realm.host_princ])
realm.kinit(realm.user_princ, password('user'), flags=['-T', realm.ccache])
realm.run([kvno, realm.host_princ])

# Test that expiration warning works after y2038, by setting a
# password expiration time ten minutes after the KDC time.
mark('expiration warning')
realm.run([kadminl, 'modprinc', '-pwexpire', '662256600 seconds', 'user'])
out = realm.kinit(realm.user_princ, password('user'))
if 'will expire in less than one hour' not in out:
    fail('password expiration message')
year = int(out.split()[-1])
if year < 2038 or year > 9999:
    fail('password expiration year')

realm.stop_kdc()
realm.start_kdc()
realm.start_kadmind()
realm.prep_kadmin()

# Test getdate parsing of absolute timestamps after 2038 and
# marshalling over the kadmin protocol.  The local time zone will
# affect the display time by a little bit, so just look for the year.
mark('kadmin marshalling')
realm.run_kadmin(['modprinc', '-pwexpire', '2040-02-03', realm.host_princ])
realm.run_kadmin(['getprinc', realm.host_princ], expected_msg=' 2040\n')

# Get a ticket whose lifetime crosses the y2038 boundary and
# range-check the expiration year as reported by klist.
mark('ticket lifetime across y2038')
realm.kinit(realm.user_princ, password('user'),
            flags=['-l', '8000d', '-r', '8500d'])
realm.run([kvno, realm.host_princ])
out = realm.run([klist])
if int(out.split('\n')[4].split()[2].split('/')[2]) < 39:
    fail('unexpected tgt expiration year')
if int(out.split('\n')[5].split()[2].split('/')[2]) < 40:
    fail('unexpected tgt rtill year')
if int(out.split('\n')[6].split()[2].split('/')[2]) < 39:
    fail('unexpected service ticket expiration year')
if int(out.split('\n')[7].split()[2].split('/')[2]) < 40:
    fail('unexpected service ticket rtill year')
realm.kinit(realm.user_princ, None, ['-R'])
out = realm.run([klist])
if int(out.split('\n')[4].split()[2].split('/')[2]) < 39:
    fail('unexpected renewed tgt expiration year')
if int(out.split('\n')[5].split()[2].split('/')[2]) < 40:
    fail('unexpected renewed tgt rtill year')

success('y2038 tests')
