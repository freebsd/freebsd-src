#!/usr/bin/python
from k5test import *

rollover_krb5_conf = {'libdefaults': {'allow_weak_crypto': 'true'}}

realm = K5Realm(krbtgt_keysalt='des-cbc-crc:normal',
                krb5_conf=rollover_krb5_conf)

princ1 = 'host/test1@%s' % (realm.realm,)
princ2 = 'host/test2@%s' % (realm.realm,)
realm.addprinc(princ1)
realm.addprinc(princ2)

realm.run([kvno, realm.host_princ])

# Change key for TGS, keeping old key.
realm.run([kadminl, 'cpw', '-randkey', '-e', 'aes256-cts', '-keepold',
           realm.krbtgt_princ])

# Ensure that kvno still works with an old TGT.
realm.run([kvno, princ1])

realm.run([kadminl, 'purgekeys', realm.krbtgt_princ])
# Make sure an old TGT fails after purging old TGS key.
realm.run([kvno, princ2], expected_code=1)
output = realm.run([klist, '-e'])

expected = 'krbtgt/%s@%s\n\tEtype (skey, tkt): des-cbc-crc, des-cbc-crc' % \
    (realm.realm, realm.realm)

if expected not in output:
    fail('keyrollover: expected TGS enctype not found')

# Check that new key actually works.
realm.kinit(realm.user_princ, password('user'))
realm.run([kvno, realm.host_princ])
output = realm.run([klist, '-e'])

expected = 'krbtgt/%s@%s\n\tEtype (skey, tkt): ' \
    'aes256-cts-hmac-sha1-96, aes256-cts-hmac-sha1-96' % \
    (realm.realm, realm.realm)

if expected not in output:
    fail('keyrollover: expected TGS enctype not found after change')

# Test that the KDC only accepts the first enctype for a kvno, for a
# local-realm TGS request.  To set this up, we abuse an edge-case
# behavior of modprinc -kvno.  First, set up a DES3 krbtgt entry at
# kvno 1 and cache a krbtgt ticket.
realm.run([kadminl, 'cpw', '-randkey', '-e', 'des3-cbc-sha1',
           realm.krbtgt_princ])
realm.run([kadminl, 'modprinc', '-kvno', '1', realm.krbtgt_princ])
realm.kinit(realm.user_princ, password('user'))
# Add an AES krbtgt entry at kvno 2, and then reset it to kvno 1
# (modprinc -kvno sets the kvno on all entries without deleting any).
realm.run([kadminl, 'cpw', '-randkey', '-keepold', '-e', 'aes256-cts',
           realm.krbtgt_princ])
realm.run([kadminl, 'modprinc', '-kvno', '1', realm.krbtgt_princ])
out = realm.run([kadminl, 'getprinc', realm.krbtgt_princ])
if 'vno 1, aes256' not in out or 'vno 1, des3' not in out:
    fail('keyrollover: setup for TGS enctype test failed')
# Now present the DES3 ticket to the KDC and make sure it's rejected.
realm.run([kvno, realm.host_princ], expected_code=1)

realm.stop()

# Test a cross-realm TGT key rollover scenario where realm 1 mimics
# the Active Directory behavior of always using kvno 0 when issuing
# cross-realm TGTs.  The first kvno invocation caches a cross-realm
# TGT with the old key, and the second kvno invocation sends it to
# r2's KDC with no kvno to identify it, forcing the KDC to try
# multiple keys.
r1, r2 = cross_realms(2)
crosstgt_princ = 'krbtgt/%s@%s' % (r2.realm, r1.realm)
r1.run([kadminl, 'modprinc', '-kvno', '0', crosstgt_princ])
r1.run([kvno, r2.host_princ])
r2.run([kadminl, 'cpw', '-pw', 'newcross', '-keepold', crosstgt_princ])
r1.run([kadminl, 'cpw', '-pw', 'newcross', crosstgt_princ])
r1.run([kadminl, 'modprinc', '-kvno', '0', crosstgt_princ])
r1.run([kvno, r2.user_princ])

success('keyrollover')
