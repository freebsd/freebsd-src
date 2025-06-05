from k5test import *
from datetime import datetime
import re

testpreauth = os.path.join(buildtop, 'plugins', 'preauth', 'test', 'test.so')
testpolicy = os.path.join(buildtop, 'plugins', 'kdcpolicy', 'test',
                          'kdcpolicy_test.so')
krb5_conf = {'plugins': {'kdcpreauth': {'module': 'test:' + testpreauth},
                         'clpreauth': {'module': 'test:' + testpreauth},
                         'kdcpolicy': {'module': 'test:' + testpolicy}}}
kdc_conf = {'realms': {'$realm': {'default_principal_flags': '+preauth',
                                  'max_renewable_life': '1d'}}}
realm = K5Realm(krb5_conf=krb5_conf, kdc_conf=kdc_conf)

# We will be scraping timestamps from klist to compute lifetimes, so
# use a time zone with no daylight savings time.
realm.env['TZ'] = 'UTC'

realm.run([kadminl, 'addprinc', '-pw', password('fail'), 'fail'])

def verify_time(out, target_time):
    times = re.findall(r'\d\d/\d\d/\d\d \d\d:\d\d:\d\d', out)
    times = [datetime.strptime(t, '%m/%d/%y %H:%M:%S') for t in times]
    divisor = 1
    while len(times) > 0:
        starttime = times.pop(0)
        endtime = times.pop(0)
        renewtime = times.pop(0)

        if str((endtime - starttime) * divisor) != target_time:
            fail('unexpected lifetime value')
        if str((renewtime - endtime) * divisor) != target_time:
            fail('unexpected renewable value')

        # Service tickets should have half the lifetime of initial
        # tickets.
        divisor = 2

rflags = ['-r', '1d', '-l', '12h']

# Test AS+TGS success path.
realm.kinit(realm.user_princ, password('user'),
            rflags + ['-X', 'indicators=SEVEN_HOURS'])
realm.run([kvno, realm.host_princ])
realm.run(['./adata', realm.host_princ], expected_msg='+97: [SEVEN_HOURS]')
out = realm.run([klist, '-e', realm.ccache])
verify_time(out, '7:00:00')

# Test AS+TGS success path with different values.
realm.kinit(realm.user_princ, password('user'),
            rflags + ['-X', 'indicators=ONE_HOUR'])
realm.run([kvno, realm.host_princ])
realm.run(['./adata', realm.host_princ], expected_msg='+97: [ONE_HOUR]')
out = realm.run([klist, '-e', realm.ccache])
verify_time(out, '1:00:00')

# Test TGS failure path (using previous creds).
realm.run([kvno, 'fail@%s' % realm.realm], expected_code=1,
          expected_msg='KDC policy rejects request')

# Test AS failure path.
realm.kinit('fail@%s' % realm.realm, password('fail'),
            expected_code=1, expected_msg='KDC policy rejects request')

success('kdcpolicy tests')
