from k5test import *
import os

realm = K5Realm(create_host=False, create_user=False)

def make_client(name):
    global realm
    realm.addprinc(name, password(name))
    ccache = os.path.join(realm.testdir,
                          'kadmin_ccache_' + name.replace('/', '_'))
    realm.kinit(name, password(name),
                flags=['-S', 'kadmin/admin', '-c', ccache])
    return ccache

def kadmin_as(client, query, **kwargs):
    global realm
    return realm.run([kadmin, '-c', client] + query, **kwargs)

all_add = make_client('all_add')
all_changepw = make_client('all_changepw')
all_delete = make_client('all_delete')
all_inquire = make_client('all_inquire')
all_list = make_client('all_list')
all_modify = make_client('all_modify')
all_rename = make_client('all_rename')
all_wildcard = make_client('all_wildcard')
all_extract = make_client('all_extract')
some_add = make_client('some_add')
some_changepw = make_client('some_changepw')
some_delete = make_client('some_delete')
some_inquire = make_client('some_inquire')
some_modify = make_client('some_modify')
some_rename = make_client('some_rename')
restricted_add = make_client('restricted_add')
restricted_modify = make_client('restricted_modify')
restricted_rename = make_client('restricted_rename')
wctarget = make_client('wctarget')
admin = make_client('user/admin')
none = make_client('none')
restrictions = make_client('restrictions')
onetwothreefour = make_client('one/two/three/four')

realm.run([kadminl, 'addpol', '-minlife', '1 day', 'minlife'])

f = open(os.path.join(realm.testdir, 'acl'), 'w')
f.write('''
all_add            a
all_changepw       c
all_delete         d
all_inquire        i
all_list           l
all_modify         im
all_rename         ad
all_wildcard       x
all_extract        ie
some_add           a   selected
some_changepw      c   selected
some_delete        d   selected
some_inquire       i   selected
some_modify        im  selected
some_rename        d   from
some_rename        a   to
restricted_add     a   *         +preauth
restricted_modify  im  *         +preauth
restricted_rename  ad  *         +preauth

*/*                d   *2/*1
# The next line is a regression test for #8154; it is not used directly.
one/*/*/five       l
*/two/*/*          d   *3/*1/*2
*/admin            a
wctarget           a   wild/*
restrictions       a   type1     -policy minlife
restrictions       a   type2     -clearpolicy
restrictions       a   type3     -maxlife 1h -maxrenewlife 2h
''')
f.close()

realm.start_kadmind()

# cpw can generate four different RPC calls depending on options.
realm.addprinc('selected', 'oldpw')
realm.addprinc('unselected', 'oldpw')
for pw in (['-pw', 'newpw'], ['-randkey']):
    for ks in ([], ['-e', 'aes256-cts']):
        mark('cpw: %s %s' % (repr(pw), repr(ks)))
        args = pw + ks
        kadmin_as(all_changepw, ['cpw'] + args + ['unselected'])
        kadmin_as(some_changepw, ['cpw'] + args + ['selected'])
        msg = "Operation requires ``change-password'' privilege"
        kadmin_as(none, ['cpw'] + args + ['selected'], expected_code=1,
                  expected_msg=msg)
        kadmin_as(some_changepw, ['cpw'] + args + ['unselected'],
                  expected_code=1, expected_msg=msg)
        kadmin_as(none, ['cpw'] + args + ['none'])
        realm.run([kadminl, 'modprinc', '-policy', 'minlife', 'none'])
        msg = "Current password's minimum life has not expired"
        kadmin_as(none, ['cpw'] + args + ['none'], expected_code=1,
                  expected_msg=msg)
        realm.run([kadminl, 'modprinc', '-clearpolicy', 'none'])
realm.run([kadminl, 'delprinc', 'selected'])
realm.run([kadminl, 'delprinc', 'unselected'])

mark('addpol')
kadmin_as(all_add, ['addpol', 'policy'])
realm.run([kadminl, 'delpol', 'policy'])
kadmin_as(none, ['addpol', 'policy'], expected_code=1,
          expected_msg="Operation requires ``add'' privilege")

# addprinc can generate two different RPC calls depending on options.
for ks in ([], ['-e', 'aes256-cts']):
    mark('addprinc: %s' % repr(ks))
    args = ['-pw', 'pw'] + ks
    kadmin_as(all_add, ['addprinc'] + args + ['unselected'])
    realm.run([kadminl, 'delprinc', 'unselected'])
    kadmin_as(some_add, ['addprinc'] + args + ['selected'])
    realm.run([kadminl, 'delprinc', 'selected'])
    kadmin_as(restricted_add, ['addprinc'] + args + ['unselected'])
    realm.run([kadminl, 'getprinc', 'unselected'],
              expected_msg='REQUIRES_PRE_AUTH')
    realm.run([kadminl, 'delprinc', 'unselected'])
    kadmin_as(none, ['addprinc'] + args + ['selected'], expected_code=1,
              expected_msg="Operation requires ``add'' privilege")
    kadmin_as(some_add, ['addprinc'] + args + ['unselected'], expected_code=1,
              expected_msg="Operation requires ``add'' privilege")

mark('delprinc')
realm.addprinc('unselected', 'pw')
kadmin_as(all_delete, ['delprinc', 'unselected'])
realm.addprinc('selected', 'pw')
kadmin_as(some_delete, ['delprinc', 'selected'])
realm.addprinc('unselected', 'pw')
kadmin_as(none, ['delprinc', 'unselected'], expected_code=1,
          expected_msg="Operation requires ``delete'' privilege")
kadmin_as(some_delete, ['delprinc', 'unselected'], expected_code=1,
          expected_msg="Operation requires ``delete'' privilege")
realm.run([kadminl, 'delprinc', 'unselected'])

mark('getpol')
kadmin_as(all_inquire, ['getpol', 'minlife'], expected_msg='Policy: minlife')
kadmin_as(none, ['getpol', 'minlife'], expected_code=1,
          expected_msg="Operation requires ``get'' privilege")
realm.run([kadminl, 'modprinc', '-policy', 'minlife', 'none'])
kadmin_as(none, ['getpol', 'minlife'], expected_msg='Policy: minlife')
realm.run([kadminl, 'modprinc', '-clearpolicy', 'none'])

mark('getprinc')
realm.addprinc('selected', 'pw')
realm.addprinc('unselected', 'pw')
kadmin_as(all_inquire, ['getprinc', 'unselected'],
          expected_msg='Principal: unselected@KRBTEST.COM')
kadmin_as(some_inquire, ['getprinc', 'selected'],
          expected_msg='Principal: selected@KRBTEST.COM')
kadmin_as(none, ['getprinc', 'selected'], expected_code=1,
          expected_msg="Operation requires ``get'' privilege")
kadmin_as(some_inquire, ['getprinc', 'unselected'], expected_code=1,
          expected_msg="Operation requires ``get'' privilege")
kadmin_as(none, ['getprinc', 'none'],
          expected_msg='Principal: none@KRBTEST.COM')
realm.run([kadminl, 'delprinc', 'selected'])
realm.run([kadminl, 'delprinc', 'unselected'])

mark('listprincs')
kadmin_as(all_list, ['listprincs'], expected_msg='K/M@KRBTEST.COM')
kadmin_as(none, ['listprincs'], expected_code=1,
          expected_msg="Operation requires ``list'' privilege")

mark('getstrs')
realm.addprinc('selected', 'pw')
realm.addprinc('unselected', 'pw')
realm.run([kadminl, 'setstr', 'selected', 'key', 'value'])
realm.run([kadminl, 'setstr', 'unselected', 'key', 'value'])
kadmin_as(all_inquire, ['getstrs', 'unselected'], expected_msg='key: value')
kadmin_as(some_inquire, ['getstrs', 'selected'], expected_msg='key: value')
kadmin_as(none, ['getstrs', 'selected'], expected_code=1,
          expected_msg="Operation requires ``get'' privilege")
kadmin_as(some_inquire, ['getstrs', 'unselected'], expected_code=1,
          expected_msg="Operation requires ``get'' privilege")
kadmin_as(none, ['getstrs', 'none'], expected_msg='(No string attributes.)')
realm.run([kadminl, 'delprinc', 'selected'])
realm.run([kadminl, 'delprinc', 'unselected'])

mark('modpol')
out = kadmin_as(all_modify, ['modpol', '-maxlife', '1 hour', 'policy'],
                expected_code=1)
if 'Operation requires' in out:
    fail('modpol success (acl)')
kadmin_as(none, ['modpol', '-maxlife', '1 hour', 'policy'], expected_code=1,
          expected_msg="Operation requires ``modify'' privilege")

mark('modprinc')
realm.addprinc('selected', 'pw')
realm.addprinc('unselected', 'pw')
kadmin_as(all_modify, ['modprinc', '-maxlife', '1 hour',  'unselected'])
kadmin_as(some_modify, ['modprinc', '-maxlife', '1 hour', 'selected'])
kadmin_as(restricted_modify, ['modprinc', '-maxlife', '1 hour', 'unselected'])
realm.run([kadminl, 'getprinc', 'unselected'],
          expected_msg='REQUIRES_PRE_AUTH')
kadmin_as(all_inquire, ['modprinc', '-maxlife', '1 hour', 'selected'],
          expected_code=1,
          expected_msg="Operation requires ``modify'' privilege")
kadmin_as(some_modify, ['modprinc', '-maxlife', '1 hour', 'unselected'],
          expected_code=1, expected_msg='Operation requires')
realm.run([kadminl, 'delprinc', 'selected'])
realm.run([kadminl, 'delprinc', 'unselected'])

mark('purgekeys')
realm.addprinc('selected', 'pw')
realm.addprinc('unselected', 'pw')
kadmin_as(all_modify, ['purgekeys', 'unselected'])
kadmin_as(some_modify, ['purgekeys', 'selected'])
kadmin_as(none, ['purgekeys', 'selected'], expected_code=1,
          expected_msg="Operation requires ``modify'' privilege")
kadmin_as(some_modify, ['purgekeys', 'unselected'], expected_code=1,
          expected_msg="Operation requires ``modify'' privilege")
kadmin_as(none, ['purgekeys', 'none'])
realm.run([kadminl, 'delprinc', 'selected'])
realm.run([kadminl, 'delprinc', 'unselected'])

mark('renprinc')
realm.addprinc('from', 'pw')
kadmin_as(all_rename, ['renprinc', 'from', 'to'])
realm.run([kadminl, 'renprinc', 'to', 'from'])
kadmin_as(some_rename, ['renprinc', 'from', 'to'])
realm.run([kadminl, 'renprinc', 'to', 'from'])
kadmin_as(all_add, ['renprinc', 'from', 'to'], expected_code=1,
          expected_msg="Insufficient authorization for operation")
kadmin_as(all_delete, ['renprinc', 'from', 'to'], expected_code=1,
          expected_msg="Insufficient authorization for operation")
kadmin_as(some_rename, ['renprinc', 'from', 'notto'], expected_code=1,
          expected_msg="Insufficient authorization for operation")
realm.run([kadminl, 'renprinc', 'from', 'notfrom'])
kadmin_as(some_rename, ['renprinc', 'notfrom', 'to'], expected_code=1,
          expected_msg="Insufficient authorization for operation")
kadmin_as(restricted_rename, ['renprinc', 'notfrom', 'to'], expected_code=1,
          expected_msg="Insufficient authorization for operation")
realm.run([kadminl, 'delprinc', 'notfrom'])

mark('setstr')
realm.addprinc('selected', 'pw')
realm.addprinc('unselected', 'pw')
kadmin_as(all_modify, ['setstr', 'unselected', 'key', 'value'])
kadmin_as(some_modify, ['setstr', 'selected', 'key', 'value'])
kadmin_as(none, ['setstr', 'selected',  'key', 'value'], expected_code=1,
          expected_msg="Operation requires ``modify'' privilege")
kadmin_as(some_modify, ['setstr', 'unselected', 'key', 'value'],
          expected_code=1, expected_msg='Operation requires')
realm.run([kadminl, 'delprinc', 'selected'])
realm.run([kadminl, 'delprinc', 'unselected'])

mark('addprinc/delprinc (wildcard)')
kadmin_as(admin, ['addprinc', '-pw', 'pw', 'anytarget'])
realm.run([kadminl, 'delprinc', 'anytarget'])
kadmin_as(wctarget, ['addprinc', '-pw', 'pw', 'wild/card'])
realm.run([kadminl, 'delprinc', 'wild/card'])
kadmin_as(wctarget, ['addprinc', '-pw', 'pw', 'wild/card/extra'],
          expected_code=1, expected_msg='Operation requires')
realm.addprinc('admin/user', 'pw')
kadmin_as(admin, ['delprinc', 'admin/user'])
kadmin_as(admin, ['delprinc', 'none'], expected_code=1,
          expected_msg='Operation requires')
realm.addprinc('four/one/three', 'pw')
kadmin_as(onetwothreefour, ['delprinc', 'four/one/three'])

mark('addprinc (restrictions)')
kadmin_as(restrictions, ['addprinc', '-pw', 'pw', 'type1'])
realm.run([kadminl, 'getprinc', 'type1'], expected_msg='Policy: minlife')
realm.run([kadminl, 'delprinc', 'type1'])
kadmin_as(restrictions, ['addprinc', '-pw', 'pw', '-policy', 'minlife',
                         'type2'])
realm.run([kadminl, 'getprinc', 'type2'], expected_msg='Policy: [none]')
realm.run([kadminl, 'delprinc', 'type2'])
kadmin_as(restrictions, ['addprinc', '-pw', 'pw', '-maxlife', '1 minute',
                         'type3'])
out = realm.run([kadminl, 'getprinc', 'type3'])
if ('Maximum ticket life: 0 days 00:01:00' not in out or
    'Maximum renewable life: 0 days 02:00:00' not in out):
    fail('restriction (maxlife low, maxrenewlife unspec)')
realm.run([kadminl, 'delprinc', 'type3'])
kadmin_as(restrictions, ['addprinc', '-pw', 'pw', '-maxrenewlife', '1 day',
                         'type3'])
realm.run([kadminl, 'getprinc', 'type3'],
          expected_msg='Maximum renewable life: 0 days 02:00:00')

mark('extract')
realm.run([kadminl, 'addprinc', '-pw', 'pw', 'extractkeys'])
kadmin_as(all_wildcard, ['ktadd', '-norandkey', 'extractkeys'],
          expected_code=1,
          expected_msg="Operation requires ``extract-keys'' privilege")
kadmin_as(all_extract, ['ktadd', '-norandkey', 'extractkeys'])
realm.kinit('extractkeys', flags=['-k'])
os.remove(realm.keytab)

mark('lockdown_keys')
kadmin_as(all_modify, ['modprinc', '+lockdown_keys', 'extractkeys'])
kadmin_as(all_changepw, ['cpw', '-pw', 'newpw', 'extractkeys'],
          expected_code=1,
          expected_msg="Operation requires ``change-password'' privilege")
kadmin_as(all_changepw, ['cpw', '-randkey', 'extractkeys'])
kadmin_as(all_extract, ['ktadd', '-norandkey', 'extractkeys'], expected_code=1,
          expected_msg="Operation requires ``extract-keys'' privilege")
kadmin_as(all_delete, ['delprinc', 'extractkeys'], expected_code=1,
          expected_msg="Operation requires ``delete'' privilege")
kadmin_as(all_rename, ['renprinc', 'extractkeys', 'renamedprinc'],
          expected_code=1,
          expected_msg="Operation requires ``delete'' privilege")
kadmin_as(all_modify, ['modprinc', '-lockdown_keys', 'extractkeys'],
          expected_code=1,
          expected_msg="Operation requires ``modify'' privilege")
realm.run([kadminl, 'modprinc', '-lockdown_keys', 'extractkeys'])
kadmin_as(all_extract, ['ktadd', '-norandkey', 'extractkeys'])
realm.kinit('extractkeys', flags=['-k'])
os.remove(realm.keytab)

# Verify that self-service key changes require an initial ticket.
mark('self-service initial ticket')
realm.run([kadminl, 'cpw', '-pw', password('none'), 'none'])
realm.run([kadminl, 'modprinc', '+allow_tgs_req', 'kadmin/admin'])
realm.kinit('none', password('none'))
realm.run([kvno, 'kadmin/admin'])
msg = 'Operation requires initial ticket'
realm.run([kadmin, '-c', realm.ccache, 'cpw', '-pw', 'newpw', 'none'],
          expected_code=1, expected_msg=msg)
realm.run([kadmin, '-c', realm.ccache, 'cpw', '-pw', 'newpw',
           '-e', 'aes256-cts', 'none'], expected_code=1, expected_msg=msg)
realm.run([kadmin, '-c', realm.ccache, 'cpw', '-randkey', 'none'],
          expected_code=1, expected_msg=msg)
realm.run([kadmin, '-c', realm.ccache, 'cpw', '-randkey', '-e', 'aes256-cts',
           'none'], expected_code=1, expected_msg=msg)

# Test authentication to kadmin/hostname.
mark('authentication to kadmin/hostname')
kadmin_hostname = 'kadmin/' + hostname
realm.addprinc(kadmin_hostname)
realm.run([kadminl, 'delprinc', 'kadmin/admin'])
msgs = ('Getting initial credentials for user/admin@KRBTEST.COM',
        'Setting initial creds service to kadmin/admin',
        '/Server not found in Kerberos database',
        'Getting initial credentials for user/admin@KRBTEST.COM',
        'Setting initial creds service to ' + kadmin_hostname,
        'Decrypted AS reply')
realm.run([kadmin, '-p', 'user/admin', 'listprincs'], expected_code=1,
          expected_msg="Operation requires ``list'' privilege",
          input=password('user/admin'), expected_trace=msgs)

# Test operations disallowed at the libkadm5 layer.
realm.run([kadminl, 'delprinc', 'K/M'],
          expected_code=1, expected_msg='Cannot change protected principal')
realm.run([kadminl, 'cpw', '-pw', 'pw', 'kadmin/history'],
          expected_code=1, expected_msg='Cannot change protected principal')

success('kadmin ACL enforcement')
