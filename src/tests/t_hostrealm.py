#!/usr/bin/python
from k5test import *

plugin = os.path.join(buildtop, "plugins", "hostrealm", "test",
                      "hostrealm_test.so")

# Disable the "dns" module (we can't easily test TXT lookups) and
# arrange the remaining modules in an order which makes sense for most
# tests.
conf = {'plugins': {'hostrealm': {'module': ['test1:' + plugin,
                                             'test2:' + plugin],
                                  'enable_only': ['test2', 'profile',
                                                  'domain', 'test1']}},
        'domain_realm': {'.x': 'DOTMATCH', 'x': 'MATCH', '.1': 'NUMMATCH'}}
realm = K5Realm(krb5_conf=conf, create_kdb=False)

def test(realm, args, expected_realms, msg, env=None):
    out = realm.run(['./hrealm'] + args, env=env)
    if out.split('\n') != expected_realms + ['']:
        fail(msg)

def test_error(realm, args, expected_error, msg, env=None):
    out = realm.run(['./hrealm'] + args, env=env, expected_code=1)
    if expected_error not in out:
        fail(msg)

def testh(realm, host, expected_realms, msg, env=None):
    test(realm, ['-h', host], expected_realms, msg, env=env)
def testf(realm, host, expected_realms, msg, env=None):
    test(realm, ['-f', host], expected_realms, msg, env=env)
def testd(realm, expected_realm, msg, env=None):
    test(realm, ['-d'], [expected_realm], msg, env=env)
def testh_error(realm, host, expected_error, msg, env=None):
    test_error(realm, ['-h', host], expected_error, msg, env=env)
def testf_error(realm, host, expected_error, msg, env=None):
    test_error(realm, ['-f', host], expected_error, msg, env=env)
def testd_error(realm, expected_error, msg, env=None):
    test_error(realm, ['-d'], expected_error, msg, env=env)

###
### krb5_get_host_realm tests
###

# The test2 module returns a fatal error on hosts beginning with 'z',
# and an answer on hosts begining with 'a'.
testh_error(realm, 'zoo', 'service not available', 'host_realm test2 z')
testh(realm, 'abacus', ['a'], 'host_realm test2 a')

# The profile module gives answers for hostnames equal to or ending in
# 'X', due to [domain_realms].  There is also an entry for hostnames
# ending in '1', but hostnames which appear to be IP or IPv6 addresses
# should instead fall through to test1.
testh(realm, 'x', ['MATCH'], 'host_realm profile x')
testh(realm, '.x', ['DOTMATCH'], 'host_realm profile .x')
testh(realm, 'b.x', ['DOTMATCH'], 'host_realm profile b.x')
testh(realm, '.b.c.x', ['DOTMATCH'], 'host_realm profile .b.c.x')
testh(realm, 'b.1', ['NUMMATCH'], 'host_realm profile b.1')
testh(realm, '4.3.2.1', ['4', '3', '2', '1'], 'host_realm profile 4.3.2.1')
testh(realm, 'b:c.x', ['b:c', 'x'], 'host_realm profile b:c.x')
# hostname cleaning should convert "X." to "x" before matching.
testh(realm, 'X.', ['MATCH'], 'host_realm profile X.')

# The test1 module returns a list of the hostname components.
testh(realm, 'b.c.d', ['b', 'c', 'd'], 'host_realm test1')

# If no module returns a result, we should get the referral realm.
testh(realm, '', [''], 'host_realm referral realm')

###
### krb5_get_fallback_host_realm tests
###

# Return a special environment with realm_try_domains set to n.
def try_env(realm, testname, n):
    conf = {'libdefaults': {'realm_try_domains': str(n)}}
    return realm.special_env(testname, False, krb5_conf=conf)

# The domain module will answer with the uppercased parent domain,
# with no special configuration.
testf(realm, 'a.b.c', ['B.C'], 'fallback_realm domain a.b.c')

# With realm_try_domains = 0, the hostname itself will be looked up as
# a realm and returned if found.
try0 = try_env(realm, 'try0', 0)
testf(realm, 'krbtest.com', ['KRBTEST.COM'], 'fallback_realm try0', env=try0)
testf(realm, 'a.b.krbtest.com', ['B.KRBTEST.COM'],
      'fallback_realm try0 grandparent', env=try0)
testf(realm, 'a.b.c', ['B.C'], 'fallback_realm try0 nomatch', env=try0)

# With realm_try_domains = 2, the parent and grandparent will be
# checked as well, but it stops there.
try2 = try_env(realm, 'try2', 2)
testf(realm, 'krbtest.com', ['KRBTEST.COM'], 'fallback_realm try2', env=try2)
testf(realm, 'a.b.krbtest.com', ['KRBTEST.COM'],
      'fallback_realm try2 grandparent', env=try2)
testf(realm, 'a.b.c.krbtest.com', ['B.C.KRBTEST.COM'],
      'fallback_realm try2 great-grandparent', env=try2)

# The test1 module answers with a list of components.  Use an IPv4
# address to bypass the domain module.
testf(realm, '1.2.3.4', ['1', '2', '3', '4'], 'fallback_realm test1')

# If no module answers, the default realm is returned.  The test2
# module returns an error when we try to look that up.
testf_error(realm, '', 'service not available', 'fallback_realm default')

###
### krb5_get_default_realm tests
###

# The test2 module returns an error.
testd_error(realm, 'service not available', 'default_realm test2')

# The profile module returns the default realm from the profile.
# Disable test2 to expose this behavior.
disable_conf = {'plugins': {'hostrealm': {'disable': 'test2'}}}
notest2 = realm.special_env('notest2', False, krb5_conf=disable_conf)
testd(realm, 'KRBTEST.COM', 'default_realm profile', env=notest2)

# The test1 module returns a list of two realms, of which we can only
# see the first.  Remove the profile default_realm setting to expose
# this behavior.
remove_default = {'libdefaults': {'default_realm': None}}
nodefault_conf = dict(disable_conf.items() + remove_default.items())
nodefault = realm.special_env('nodefault', False, krb5_conf=nodefault_conf)
testd(realm, 'one', 'default_realm test1', env=nodefault)

success('hostrealm interface tests')
