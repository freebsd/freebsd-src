from k5test import *

realm = K5Realm(create_user=False)

alice = 'alice@' + realm.realm
bob = 'bob@' + realm.realm
cc_alice = realm.ccache + '.alice'
cc_bob = realm.ccache + '.bob'
realm.addprinc(alice)
realm.addprinc(bob)
realm.extract_keytab(alice, realm.keytab)
realm.extract_keytab(bob, realm.keytab)
realm.kinit(alice, flags=['-k', '-c', cc_alice])
realm.kinit(bob, flags=['-k', '-c', cc_bob])

mark('FILE, default output ccache')
realm.run(['./t_store_cred', cc_alice])
realm.klist(alice)
# Overwriting should fail by default, whether or not the principal matches.
realm.run(['./t_store_cred', cc_alice], expected_code=1,
          expected_msg='The requested credential element already exists')
realm.run(['./t_store_cred', cc_bob], expected_code=1,
          expected_msg='The requested credential element already exists')
# Overwriting should succeed with overwrite_cred set.
realm.run(['./t_store_cred', '-o', cc_bob])
realm.klist(bob)
# default_cred has no effect without a collection.
realm.run(['./t_store_cred', '-d', '-o', cc_alice])
realm.klist(alice)

mark('FILE, gss_krb5_ccache_name()')
cc_alternate = realm.ccache + '.alternate'
realm.run(['./t_store_cred', cc_alice, cc_alternate])
realm.klist(alice, ccache=cc_alternate)
realm.run(['./t_store_cred', cc_bob, cc_alternate], expected_code=1,
          expected_msg='The requested credential element already exists')

mark('FILE, gss_store_cred_into()')
os.remove(cc_alternate)
realm.run(['./t_store_cred', '-i', cc_alice, cc_alternate])
realm.klist(alice, ccache=cc_alternate)
realm.run(['./t_store_cred', '-i', cc_bob, cc_alternate], expected_code=1,
          expected_msg='The requested credential element already exists')

mark('DIR, gss_krb5_ccache_name()')
cc_dir = 'DIR:' + os.path.join(realm.testdir, 'cc')
realm.run(['./t_store_cred', cc_alice, cc_dir])
realm.run([klist, '-c', cc_dir], expected_code=1,
          expected_msg='No credentials cache found')
realm.run([klist, '-l', '-c', cc_dir], expected_msg=alice)
realm.run(['./t_store_cred', cc_alice, cc_dir], expected_code=1,
          expected_msg='The requested credential element already exists')
realm.run(['./t_store_cred', '-o', cc_alice, cc_dir])
realm.run([klist, '-c', cc_dir], expected_code=1,
          expected_msg='No credentials cache found')
realm.run([klist, '-l', cc_dir], expected_msg=alice)
realm.run(['./t_store_cred', '-d', cc_bob, cc_dir])
# The k5test klist method does not currently work with a collection name.
realm.run([klist, cc_dir], expected_msg=bob)
realm.run([klist, '-l', cc_dir], expected_msg=alice)
realm.run(['./t_store_cred', '-o', '-d', cc_alice, cc_dir])
realm.run([klist, cc_dir], expected_msg=alice)
realm.run([kdestroy, '-A', '-c', cc_dir])

mark('DIR, gss_store_cred_into()')
realm.run(['./t_store_cred', '-i', cc_alice, cc_dir])
realm.run(['./t_store_cred', '-i', '-d', cc_bob, cc_dir])
realm.run([klist, cc_dir], expected_msg=bob)
realm.run([klist, '-l', cc_dir], expected_msg=alice)
realm.run([kdestroy, '-A', '-c', cc_dir])

mark('DIR, default output ccache')
realm.ccache = cc_dir
realm.env['KRB5CCNAME'] = cc_dir
realm.run(['./t_store_cred', '-i', cc_alice, cc_dir])
realm.run(['./t_store_cred', '-i', '-d', cc_bob, cc_dir])
realm.run([klist], expected_msg=bob)
realm.run([klist, '-l'], expected_msg=alice)

success('gss_store_cred() tests')
