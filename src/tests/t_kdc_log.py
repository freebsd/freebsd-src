from k5test import *

# Make a TGS request with an expired ticket.
realm = K5Realm()
realm.stop()
realm.start_kdc(['-T', '3600'])
realm.run([kvno, realm.host_princ], expected_code=1)

kdc_logfile = os.path.join(realm.testdir, 'kdc.log')
f = open(kdc_logfile, 'r')
found_skew = False
for line in f:
    if 'Clock skew too great' in line:
        found_skew = True
        if realm.user_princ not in line:
            fail('Client principal not logged in expired-ticket TGS request')
f.close()
if not found_skew:
    fail('Did not find KDC log line for expired-ticket TGS request')

success('KDC logging tests')
