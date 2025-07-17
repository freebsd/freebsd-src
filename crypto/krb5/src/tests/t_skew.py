from k5test import *

# Create a realm with the KDC one hour in the past.
realm = K5Realm(start_kdc=False)
realm.start_kdc(['-T', '-3600'])

# kinit (no preauth) should work, and should set a clock skew allowing
# kvno to work, with or without FAST.
mark('kdc_timesync enabled, no preauth')
realm.kinit(realm.user_princ, password('user'))
realm.run([kvno, realm.host_princ])
realm.kinit(realm.user_princ, password('user'), flags=['-T', realm.ccache])
realm.run([kvno, realm.host_princ])
realm.run([kdestroy])

# kinit (with preauth) should work, with or without FAST.
mark('kdc_timesync enabled, with preauth')
realm.run([kadminl, 'modprinc', '+requires_preauth', 'user'])
realm.kinit(realm.user_princ, password('user'))
realm.run([kvno, realm.host_princ])
realm.kinit(realm.user_princ, password('user'), flags=['-T', realm.ccache])
realm.run([kvno, realm.host_princ])
realm.run([kdestroy])

realm.stop()

# Repeat the above tests with kdc_timesync disabled.
conf = {'libdefaults': {'kdc_timesync': '0'}}
realm = K5Realm(start_kdc=False, krb5_conf=conf)
realm.start_kdc(['-T', '-3600'])

# Get tickets to use for FAST kinit tests.  The start time offset is
# ignored by the KDC since we aren't getting postdatable tickets, but
# serves to suppress the client clock skew check on the KDC reply.
fast_cache = realm.ccache + '.fast'
realm.kinit(realm.user_princ, password('user'),
            flags=['-s', '-3600s', '-c', fast_cache])

# kinit should detect too much skew in the KDC response.  kinit with
# FAST should fail from the KDC since the armor AP-REQ won't be valid.
mark('KDC timesync disabled, no preauth')
realm.kinit(realm.user_princ, password('user'), expected_code=1,
            expected_msg='Clock skew too great in KDC reply')
realm.kinit(realm.user_princ, None, flags=['-T', fast_cache], expected_code=1,
            expected_msg='Clock skew too great while')

# kinit (with preauth) should fail from the KDC, with or without FAST.
mark('KDC timesync disabled, with preauth')
realm.run([kadminl, 'modprinc', '+requires_preauth', 'user'])
realm.kinit(realm.user_princ, password('user'), expected_code=1,
            expected_msg='Clock skew too great while')
realm.kinit(realm.user_princ, None, flags=['-T', fast_cache], expected_code=1,
            expected_msg='Clock skew too great while')

success('Clock skew tests')
