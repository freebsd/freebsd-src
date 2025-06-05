from k5test import *

# Specify a supported_enctypes so the chpass tests know what to expect.
supported_enctypes = 'aes256-cts:normal aes128-cts:normal'
conf = {'realms': {'$realm': {'supported_enctypes': supported_enctypes}}}
realm = K5Realm(create_user=False, create_host=False, kdc_conf=conf)

with open(os.path.join(realm.testdir, 'acl'), 'w') as f:
    f.write('''
admin                   admcilse
admin/get               il
admin/modify            mc
admin/delete            d
admin/add               a
admin/rename            adil
''')

with open(os.path.join(realm.testdir, 'dictfile'), 'w') as f:
    f.write('''
Abyssinia
Discordianism
foo
''')

realm.start_kadmind()

realm.run([kadminl, 'addpol', '-maxlife', '10000s', '-minlength', '8',
           '-minclasses', '2', '-maxfailure', '2',
           '-failurecountinterval', '90s', '-lockoutduration', '180s',
           'test-pol'])
realm.run([kadminl, 'addpol', '-minlife', '10s', 'minlife-pol'])
realm.run([kadminl, 'addpol', 'dict-only-pol'])
realm.run([kadminl, 'addprinc', '-pw', 'admin', 'admin'])
realm.run([kadminl, 'addprinc', '-pw', 'admin', 'admin/get'])
realm.run([kadminl, 'addprinc', '-pw', 'admin', 'admin/modify'])
realm.run([kadminl, 'addprinc', '-pw', 'admin', 'admin/delete'])
realm.run([kadminl, 'addprinc', '-pw', 'admin', 'admin/add'])
realm.run([kadminl, 'addprinc', '-pw', 'admin', 'admin/rename'])
realm.run([kadminl, 'addprinc', '-pw', 'admin', 'admin/none'])
realm.run([kadminl, 'addprinc', '-pw', 'us3r', '-policy', 'minlife-pol',
           'user'])

realm.run(['./t_kadm5srv', 'srv'])
realm.run(['./t_kadm5clnt', 'clnt'])
success('kadm5 API tests')
