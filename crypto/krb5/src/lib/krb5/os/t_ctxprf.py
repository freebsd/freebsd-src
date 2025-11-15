from k5test import *

realm = K5Realm(create_kdb=False)

mark('initially empty profile')
realm.run(['./t_ctxprf', 'empty'])

mark('modified single-file profile')
realm.run(['./t_ctxprf'])
with open(os.path.join(realm.testdir, 'krb5.conf')) as f:
    contents = f.read()
    if 'ctx.prf.test' in contents:
        fail('profile changes unexpectedly flushed')

success('krb5_init_context_profile() tests')
