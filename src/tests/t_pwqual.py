#!/usr/bin/python
from k5test import *

plugin = os.path.join(buildtop, "plugins", "pwqual", "test", "pwqual_test.so")

dictfile = os.path.join(os.getcwd(), 'testdir', 'dict')

pconf = {'plugins': {'pwqual': {'module': 'combo:' + plugin}}}
dconf = {'realms': {'$realm': {'dict_file': dictfile}}}
realm = K5Realm(krb5_conf=pconf, kdc_conf=dconf, create_user=False,
                create_host=False)

# Write a short dictionary file.
f = open(dictfile, 'w')
f.write('birds\nbees\napples\noranges\n')
f.close()

realm.run([kadminl, 'addpol', 'pol'])

# The built-in "empty" module rejects empty passwords even without a policy.
out = realm.run([kadminl, 'addprinc', '-pw', '', 'p1'], expected_code=1)
if 'Empty passwords are not allowed' not in out:
    fail('Expected error not seen for empty password')

# The built-in "dict" module rejects dictionary words, but only with a policy.
realm.run([kadminl, 'addprinc', '-pw', 'birds', 'p2'])
out = realm.run([kadminl, 'addprinc', '-pw', 'birds', '-policy', 'pol', 'p3'],
                expected_code=1)
if 'Password is in the password dictionary' not in out:
    fail('Expected error not seen from dictionary password')

# The built-in "princ" module rejects principal components, only with a policy.
realm.run([kadminl, 'addprinc', '-pw', 'p4', 'p4'])
out = realm.run([kadminl, 'addprinc', '-pw', 'p5', '-policy', 'pol', 'p5'],
                expected_code=1)
if 'Password may not match principal name' not in out:
    fail('Expected error not seen from principal component')

# The dynamic "combo" module rejects pairs of dictionary words.
out = realm.run([kadminl, 'addprinc', '-pw', 'birdsoranges', 'p6'],
                expected_code=1)
if 'Password may not be a pair of dictionary words' not in out:
    fail('Expected error not seen from combo module')

# These plugin ordering tests aren't specifically related to the
# password quality interface, but are convenient to put here.

def test_order(realm, testname, conf, expected):
    conf = {'plugins': {'pwqual': conf}}
    env = realm.special_env(testname, False, krb5_conf=conf)
    out = realm.run(['./plugorder'], env=env)
    if out.split() != expected:
        fail('order test: ' + testname)

realm.stop()
realm = K5Realm(create_kdb=False)

# Check the test harness with no special configuration.
test_order(realm, 'noconf', {}, ['blt1', 'blt2', 'blt3'])

# Test the basic order: dynamic modules, then built-in modules, each
# in registration order.
conf = {'module': ['dyn3:' + plugin, 'dyn1:' + plugin, 'dyn2:' + plugin]}
test_order(realm, 'basic', conf,
           ['dyn3', 'dyn1', 'dyn2', 'blt1', 'blt2', 'blt3'])

# Disabling modules should not affect the order of other modules.
conf['disable'] = ['dyn1', 'blt3']
test_order(realm, 'disable', conf, ['dyn3', 'dyn2', 'blt1', 'blt2'])

# enable_only should reorder the modules, but can't resurrect disabled
# modules or create ones from thin air.
conf['enable_only'] = ['dyn2', 'blt3', 'blt2', 'dyn1', 'dyn3', 'xxx']
test_order(realm, 'enable_only', conf, ['dyn2', 'blt2', 'dyn3'])

# Duplicate modules should be pruned by preferring earlier entries.
conf = {'module': ['dyn3:' + plugin, 'dyn1:' + plugin, 'dyn3:' + plugin]}
test_order(realm, 'duplicate', conf, ['dyn3', 'dyn1', 'blt1', 'blt2', 'blt3'])

success('Password quality interface tests')
