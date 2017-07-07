#!/usr/bin/python
from k5test import *

plugin = os.path.join(buildtop, "plugins", "kadm5_hook", "test",
                      "kadm5_hook_test.so")

hook_krb5_conf = {'plugins': {'kadm5_hook': { 'module': 'test:' + plugin}}}

realm = K5Realm(krb5_conf=hook_krb5_conf, create_user=False, create_host=False)
output = realm.run([kadminl, 'addprinc', '-randkey', 'test'])
if "create: stage precommit" not in output:
    fail('kadm5_hook test output not found')

output = realm.run([kadminl, 'renprinc', 'test', 'test2'])
if "rename: stage precommit" not in output:
    fail('kadm5_hook test output not found')

success('kadm5_hook')
