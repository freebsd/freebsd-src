from k5test import *

plugin = os.path.join(buildtop, "plugins", "kadm5_hook", "test",
                      "kadm5_hook_test.so")

hook_krb5_conf = {'plugins': {'kadm5_hook': { 'module': 'test:' + plugin}}}

realm = K5Realm(krb5_conf=hook_krb5_conf, create_user=False, create_host=False)
realm.run([kadminl, 'addprinc', '-randkey', 'test'],
          expected_msg='create: stage precommit')

realm.run([kadminl, 'renprinc', 'test', 'test2'],
          expected_msg='rename: stage precommit')

success('kadm5_hook')
