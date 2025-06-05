from k5test import *
import re

# KDC option test coverage notes:
#
# FORWARDABLE              here
# FORWARDED                no test
# PROXIABLE                here
# PROXY                    no test
# ALLOW_POSTDATE           no test
# POSTDATED                no test
# RENEWABLE                t_renew.py
# CNAME_IN_ADDL_TKT        gssapi/t_s4u.py
# CANONICALIZE             t_kdb.py and various other tests
# REQUEST_ANONYMOUS        t_pkinit.py
# DISABLE_TRANSITED_CHECK  no test
# RENEWABLE_OK             t_renew.py
# ENC_TKT_IN_SKEY          t_u2u.py
# RENEW                    t_renew.py
# VALIDATE                 no test

# Run klist -f and return the flags on the ticket for svcprinc.
def get_flags(realm, svcprinc):
    grab_flags = False
    for line in realm.run([klist, '-f']).splitlines():
        if grab_flags:
            return re.findall(r'Flags: ([a-zA-Z]*)', line)[0]
        grab_flags = line.endswith(svcprinc)


# Get the flags on the ticket for svcprinc, and check for an expected
# element and an expected-absent element, either of which can be None.
def check_flags(realm, svcprinc, expected_flag, expected_noflag):
    flags = get_flags(realm, svcprinc)
    if expected_flag is not None and not expected_flag in flags:
        fail('expected flag ' + expected_flag)
    if expected_noflag is not None and expected_noflag in flags:
        fail('did not expect flag ' + expected_noflag)


# Run kinit with the given flags, and check the flags on the resulting
# TGT.
def kinit_check_flags(realm, flags, expected_flag, expected_noflag):
    realm.kinit(realm.user_princ, password('user'), flags)
    check_flags(realm, realm.krbtgt_princ, expected_flag, expected_noflag)


# Run kinit with kflags.  Then get credentials for the host principal
# with gflags, and check the flags on the resulting ticket.
def gcred_check_flags(realm, kflags, gflags, expected_flag, expected_noflag):
    realm.kinit(realm.user_princ, password('user'), kflags)
    realm.run(['./gcred'] + gflags + ['unknown', realm.host_princ])
    check_flags(realm, realm.host_princ, expected_flag, expected_noflag)


realm = K5Realm()

mark('proxiable (AS)')
kinit_check_flags(realm, [], None, 'P')
kinit_check_flags(realm, ['-p'], 'P', None)
realm.run([kadminl, 'modprinc', '-allow_proxiable', realm.user_princ])
kinit_check_flags(realm, ['-p'], None, 'P')
realm.run([kadminl, 'modprinc', '+allow_proxiable', realm.user_princ])
realm.run([kadminl, 'modprinc', '-allow_proxiable', realm.krbtgt_princ])
kinit_check_flags(realm, ['-p'], None, 'P')
realm.run([kadminl, 'modprinc', '+allow_proxiable', realm.krbtgt_princ])

mark('proxiable (TGS)')
gcred_check_flags(realm, [], [], None, 'P')
gcred_check_flags(realm, ['-p'], [], 'P', None)

# Not tested: PROXIABLE option set with a non-proxiable TGT (because
# there is no krb5_get_credentials() flag to request this; would
# expect a non-proxiable ticket).

# Not tested: proxiable TGT but PROXIABLE flag not set (because we
# internally set the PROXIABLE option when using a proxiable TGT;
# would expect a non-proxiable ticket).

mark('forwardable (AS)')
kinit_check_flags(realm, [], None, 'F')
kinit_check_flags(realm, ['-f'], 'F', None)
realm.run([kadminl, 'modprinc', '-allow_forwardable', realm.user_princ])
kinit_check_flags(realm, ['-f'], None, 'F')
realm.run([kadminl, 'modprinc', '+allow_forwardable', realm.user_princ])
realm.run([kadminl, 'modprinc', '-allow_forwardable', realm.krbtgt_princ])
kinit_check_flags(realm, ['-f'], None, 'F')
realm.run([kadminl, 'modprinc', '+allow_forwardable', realm.krbtgt_princ])

mark('forwardable (TGS)')
realm.kinit(realm.user_princ, password('user'))
gcred_check_flags(realm, [], [], None, 'F')
gcred_check_flags(realm, [], ['-f'], None, 'F')
gcred_check_flags(realm, ['-f'], [], 'F', None)

# Not tested: forwardable TGT but FORWARDABLE flag not set (because we
# internally set the FORWARDABLE option when using a forwardable TGT;
# would expect a non-proxiable ticket).

success('KDC option tests')
