from k5test import *

# Unfortunately, we can't reliably test the k5login module.  We can control
# the directory where k5login files are read, but we can't suppress the UID
# validity check, which might fail in some filesystems for a .k5login file
# we create.
conf = {'plugins': {'localauth': { 'disable': 'k5login'}}}
realm = K5Realm(create_kdb=False, krb5_conf=conf)

def test_an2ln(env, aname, result, msg):
    out = realm.run(['./localauth', aname], env=env)
    if out != result + '\n':
        fail(msg)

def test_an2ln_err(env, aname, err, msg):
    realm.run(['./localauth', aname], env=env, expected_code=1,
              expected_msg=err)

def test_userok(env, aname, lname, ok, msg):
    out = realm.run(['./localauth', aname, lname], env=env)
    if ((ok and out != 'yes\n') or
        (not ok and out != 'no\n')):
        fail(msg)

# The default an2ln method works only in the default realm, and works
# for a single-component principal or a two-component principal where
# the second component is the default realm.
mark('default')
test_an2ln(None, 'user@KRBTEST.COM', 'user', 'default rule 1')
test_an2ln(None, 'user/KRBTEST.COM@KRBTEST.COM', 'user', 'default rule 2')
test_an2ln_err(None, 'user/KRBTEST.COM/x@KRBTEST.COM', 'No translation',
               'default rule (3)')
test_an2ln_err(None, 'user/X@KRBTEST.COM', 'No translation',
               'default rule comp mismatch')
test_an2ln_err(None, 'user@X', 'No translation', 'default rule realm mismatch')

# auth_to_local_names matches ignore the realm but are case-sensitive.
mark('auth_to_local_names')
conf_names1 = {'realms': {'$realm': {'auth_to_local_names': {'user': 'abcd'}}}}
names1 = realm.special_env('names1', False, conf_names1)
test_an2ln(names1, 'user@KRBTEST.COM', 'abcd', 'auth_to_local_names match')
test_an2ln(names1, 'user@X', 'abcd', 'auth_to_local_names out-of-realm match')
test_an2ln(names1, 'x@KRBTEST.COM', 'x', 'auth_to_local_names mismatch')
test_an2ln(names1, 'User@KRBTEST.COM', 'User', 'auth_to_local_names case')

# auth_to_local_names values must be in the default realm's section.
conf_names2 = {'realms': {'X': {'auth_to_local_names': {'user': 'abcd'}}}}
names2 = realm.special_env('names2', False, conf_names2)
test_an2ln_err(names2, 'user@X', 'No translation',
               'auth_to_local_names section mismatch')

# Return a realm environment containing an auth_to_local value (or list).
def a2l_realm(name, values):
    conf = {'realms': {'$realm': {'auth_to_local': values}}}
    return realm.special_env(name, False, conf)

# Test explicit use of default method.
mark('explicit default')
auth1 = a2l_realm('auth1', 'DEFAULT')
test_an2ln(auth1, 'user@KRBTEST.COM', 'user', 'default rule')

# Test some invalid auth_to_local values.
mark('auth_to_local invalid')
auth2 = a2l_realm('auth2', 'RULE')
test_an2ln_err(auth2, 'user@X', 'Improper format', 'null rule')
auth3 = a2l_realm('auth3', 'UNRECOGNIZED:stuff')
test_an2ln_err(auth3, 'user@X', 'Improper format', 'null rule')

# An empty rule has the default selection string (unparsed principal
# without realm) and no match or substitutions.
mark('rule (empty)')
rule1 = a2l_realm('rule1', 'RULE:')
test_an2ln(rule1, 'user@KRBTEST.COM', 'user', 'empty rule')
test_an2ln(rule1, 'user@X', 'user', 'empty rule (foreign realm)')
test_an2ln(rule1, 'a/b/c@X', 'a/b/c', 'empty rule (multi-component)')

# Test explicit selection string.  Also test that the default method
# is suppressed when auth_to_local values are present.
mark('rule (selection string)')
rule2 = a2l_realm('rule2', 'RULE:[2:$$0.$$2.$$1]')
test_an2ln(rule2, 'aaron/burr@REALM', 'REALM.burr.aaron', 'selection string')
test_an2ln_err(rule2, 'user@KRBTEST.COM', 'No translation', 'suppress default')

# Test match string.
mark('rule (match string)')
rule3 = a2l_realm('rule3', 'RULE:(.*tail)')
test_an2ln(rule3, 'withtail@X', 'withtail', 'rule match 1')
test_an2ln(rule3, 'x/withtail@X', 'x/withtail', 'rule match 2')
test_an2ln_err(rule3, 'tails@X', 'No translation', 'rule anchor mismatch')

# Test substitutions.
mark('rule (substitutions)')
rule4 = a2l_realm('rule4', 'RULE:s/birds/bees/')
test_an2ln(rule4, 'thebirdsbirdsbirds@X', 'thebeesbirdsbirds', 'subst 1')
rule5 = a2l_realm('rule4', 'RULE:s/birds/bees/g  s/bees/birds/')
test_an2ln(rule4, 'the/birdsbirdsbirds@x', 'the/birdsbeesbees', 'subst 2')

# Test a bunch of auth_to_local values and rule features in combination.
mark('rule (combo)')
combo = a2l_realm('combo', ['RULE:[1:$$1-$$0](fred.*)s/-/ /g',
                            'DEFAULT',
                            'RULE:[3:$$1](z.*z)'])
test_an2ln(combo, 'fred@X', 'fred X', 'combo 1')
test_an2ln(combo, 'fred-too@X', 'fred too X', 'combo 2')
test_an2ln(combo, 'fred@KRBTEST.COM', 'fred KRBTEST.COM', 'combo 3')
test_an2ln(combo, 'user@KRBTEST.COM', 'user', 'combo 4')
test_an2ln(combo, 'zazz/b/c@X', 'zazz', 'combo 5')
test_an2ln_err(combo, 'a/b@KRBTEST.COM', 'No translation', 'combo 6')

# Test the an2ln userok method with the combo environment.
mark('userok (an2ln)')
test_userok(combo, 'fred@X', 'fred X', True, 'combo userok 1')
test_userok(combo, 'user@KRBTEST.COM', 'user', True, 'combo userok 2')
test_userok(combo, 'user@KRBTEST.COM', 'X', False, 'combo userok 3')
test_userok(combo, 'a/b@KRBTEST.COM', 'a/b', False, 'combo userok 4')

mark('test modules')

# Register the two test modules and set up some auth_to_local and
# auth_to_local_names entries.
modpath = os.path.join(buildtop, 'plugins', 'localauth', 'test',
                       'localauth_test.so')
conf = {'plugins': {'localauth': { 'module': [
                'test1:' + modpath,
                'test2:' + modpath]}},
        'realms': {'$realm': {'auth_to_local': [
                'RULE:(test/rulefirst)s/.*/rule/',
                'TYPEA',
                'DEFAULT',
                'TYPEB:resid']},
                   'auth_to_local_names': {'test/a/b': 'name'}}}
mod = realm.special_env('mod', False, conf)

# test1's untyped an2ln method should come before the names method, mapping
# test/a/b@X to its realm name (superseding auth_to_local_names).
test_an2ln(mod, 'test/a/b@X', 'X', 'mod untyped an2ln')

# Match the auth_to_local values in order.  test2's TYPEA should map
# test/notrule to its second component, and its TYPEB should map
# anything which gets there to the residual string.
test_an2ln(mod, 'test/rulefirst@X', 'rule', 'mod auth_to_local 1')
test_an2ln(mod, 'test/notrule', 'notrule', 'mod auth_to_local 2')
test_an2ln(mod, 'user@KRBTEST.COM', 'user', 'mod auth_to_local 3')
test_an2ln(mod, 'xyz@X', 'resid', 'mod auth_to_local 4')

# test2's userok module should succeed when the number of components
# is equal to the length of the local name, should pass if the first
# component is 'pass', and should reject otherwise.
test_userok(mod, 'a/b/c/d@X', 'four', True, 'mod userok 1')
test_userok(mod, 'x/y/z@X', 'four', False, 'mod userok 2')
test_userok(mod, 'pass@KRBTEST.COM', 'pass', True, 'mod userok 3')
test_userok(mod, 'user@KRBTEST.COM', 'user', False, 'mod userok 4')

success('krb5_kuserok and krb5_aname_to_localname tests')
