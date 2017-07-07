#!/usr/bin/python
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
        args = pw + ks
        kadmin_as(all_changepw, ['cpw'] + args + ['unselected'])
        kadmin_as(some_changepw, ['cpw'] + args + ['selected'])
        out = kadmin_as(none, ['cpw'] + args + ['selected'], expected_code=1)
        if 'Operation requires ``change-password\'\' privilege' not in out:
            fail('cpw failure (no perms)')
        out = kadmin_as(some_changepw, ['cpw'] + args + ['unselected'],
                        expected_code=1)
        if 'Operation requires ``change-password\'\' privilege' not in out:
            fail('cpw failure (target)')
        out = kadmin_as(none, ['cpw'] + args + ['none'])
        realm.run([kadminl, 'modprinc', '-policy', 'minlife', 'none'])
        out = kadmin_as(none, ['cpw'] + args + ['none'], expected_code=1)
        if 'Current password\'s minimum life has not expired' not in out:
            fail('cpw failure (minimum life)')
        realm.run([kadminl, 'modprinc', '-clearpolicy', 'none'])
realm.run([kadminl, 'delprinc', 'selected'])
realm.run([kadminl, 'delprinc', 'unselected'])

kadmin_as(all_add, ['addpol', 'policy'])
realm.run([kadminl, 'delpol', 'policy'])
out = kadmin_as(none, ['addpol', 'policy'], expected_code=1)
if 'Operation requires ``add\'\' privilege' not in out:
    fail('addpol failure (no perms)')

# addprinc can generate two different RPC calls depending on options.
for ks in ([], ['-e', 'aes256-cts']):
    args = ['-pw', 'pw'] + ks
    kadmin_as(all_add, ['addprinc'] + args + ['unselected'])
    realm.run([kadminl, 'delprinc', 'unselected'])
    kadmin_as(some_add, ['addprinc'] + args + ['selected'])
    realm.run([kadminl, 'delprinc', 'selected'])
    kadmin_as(restricted_add, ['addprinc'] + args + ['unselected'])
    out = realm.run([kadminl, 'getprinc', 'unselected'])
    if 'REQUIRES_PRE_AUTH' not in out:
        fail('addprinc success (restrictions) -- restriction check')
    realm.run([kadminl, 'delprinc', 'unselected'])
    out = kadmin_as(none, ['addprinc'] + args + ['selected'], expected_code=1)
    if 'Operation requires ``add\'\' privilege' not in out:
        fail('addprinc failure (no perms)')
    out = kadmin_as(some_add, ['addprinc'] + args + ['unselected'],
                    expected_code=1)
    if 'Operation requires ``add\'\' privilege' not in out:
        fail('addprinc failure (target)')

realm.addprinc('unselected', 'pw')
kadmin_as(all_delete, ['delprinc', 'unselected'])
realm.addprinc('selected', 'pw')
kadmin_as(some_delete, ['delprinc', 'selected'])
realm.addprinc('unselected', 'pw')
out = kadmin_as(none, ['delprinc', 'unselected'], expected_code=1)
if 'Operation requires ``delete\'\' privilege' not in out:
    fail('delprinc failure (no perms)')
out = kadmin_as(some_delete, ['delprinc', 'unselected'], expected_code=1)
if 'Operation requires ``delete\'\' privilege' not in out:
    fail('delprinc failure (no target)')
realm.run([kadminl, 'delprinc', 'unselected'])

out = kadmin_as(all_inquire, ['getpol', 'minlife'])
if 'Policy: minlife' not in out:
    fail('getpol success (acl)')
out = kadmin_as(none, ['getpol', 'minlife'], expected_code=1)
if 'Operation requires ``get\'\' privilege' not in out:
    fail('getpol failure (no perms)')
realm.run([kadminl, 'modprinc', '-policy', 'minlife', 'none'])
out = kadmin_as(none, ['getpol', 'minlife'])
if 'Policy: minlife' not in out:
    fail('getpol success (self policy exemption)')
realm.run([kadminl, 'modprinc', '-clearpolicy', 'none'])

realm.addprinc('selected', 'pw')
realm.addprinc('unselected', 'pw')
out = kadmin_as(all_inquire, ['getprinc', 'unselected'])
if 'Principal: unselected@KRBTEST.COM' not in out:
    fail('getprinc success (acl)')
out = kadmin_as(some_inquire, ['getprinc', 'selected'])
if 'Principal: selected@KRBTEST.COM' not in out:
    fail('getprinc success (target)')
out = kadmin_as(none, ['getprinc', 'selected'], expected_code=1)
if 'Operation requires ``get\'\' privilege' not in out:
    fail('getprinc failure (no perms)')
out = kadmin_as(some_inquire, ['getprinc', 'unselected'], expected_code=1)
if 'Operation requires ``get\'\' privilege' not in out:
    fail('getprinc failure (target)')
out = kadmin_as(none, ['getprinc', 'none'])
if 'Principal: none@KRBTEST.COM' not in out:
    fail('getprinc success (self exemption)')
realm.run([kadminl, 'delprinc', 'selected'])
realm.run([kadminl, 'delprinc', 'unselected'])

out = kadmin_as(all_list, ['listprincs'])
if 'K/M@KRBTEST.COM' not in out:
    fail('listprincs success (acl)')
out = kadmin_as(none, ['listprincs'], expected_code=1)
if 'Operation requires ``list\'\' privilege' not in out:
    fail('listprincs failure (no perms)')

realm.addprinc('selected', 'pw')
realm.addprinc('unselected', 'pw')
realm.run([kadminl, 'setstr', 'selected', 'key', 'value'])
realm.run([kadminl, 'setstr', 'unselected', 'key', 'value'])
out = kadmin_as(all_inquire, ['getstrs', 'unselected'])
if 'key: value' not in out:
    fail('getstrs success (acl)')
out = kadmin_as(some_inquire, ['getstrs', 'selected'])
if 'key: value' not in out:
    fail('getstrs success (target)')
out = kadmin_as(none, ['getstrs', 'selected'], expected_code=1)
if 'Operation requires ``get\'\' privilege' not in out:
    fail('getstrs failure (no perms)')
out = kadmin_as(some_inquire, ['getstrs', 'unselected'], expected_code=1)
if 'Operation requires ``get\'\' privilege' not in out:
    fail('getstrs failure (target)')
out = kadmin_as(none, ['getstrs', 'none'])
if '(No string attributes.)' not in out:
    fail('getstrs success (self exemption)')
realm.run([kadminl, 'delprinc', 'selected'])
realm.run([kadminl, 'delprinc', 'unselected'])

out = kadmin_as(all_modify, ['modpol', '-maxlife', '1 hour', 'policy'],
                expected_code=1)
if 'Operation requires' in out:
    fail('modpol success (acl)')
out = kadmin_as(none, ['modpol', '-maxlife', '1 hour', 'policy'],
                expected_code=1)
if 'Operation requires ``modify\'\' privilege' not in out:
    fail('modpol failure (no perms)')

realm.addprinc('selected', 'pw')
realm.addprinc('unselected', 'pw')
kadmin_as(all_modify, ['modprinc', '-maxlife', '1 hour',  'unselected'])
kadmin_as(some_modify, ['modprinc', '-maxlife', '1 hour', 'selected'])
kadmin_as(restricted_modify, ['modprinc', '-maxlife', '1 hour', 'unselected'])
out = realm.run([kadminl, 'getprinc', 'unselected'])
if 'REQUIRES_PRE_AUTH' not in out:
    fail('addprinc success (restrictions) -- restriction check')
out = kadmin_as(all_inquire, ['modprinc', '-maxlife', '1 hour', 'selected'],
                expected_code=1)
if 'Operation requires ``modify\'\' privilege' not in out:
    fail('addprinc failure (no perms)')
out = kadmin_as(some_modify, ['modprinc', '-maxlife', '1 hour', 'unselected'],
                expected_code=1)
if 'Operation requires' not in out:
    fail('modprinc failure (target)')
realm.run([kadminl, 'delprinc', 'selected'])
realm.run([kadminl, 'delprinc', 'unselected'])

realm.addprinc('selected', 'pw')
realm.addprinc('unselected', 'pw')
kadmin_as(all_modify, ['purgekeys', 'unselected'])
kadmin_as(some_modify, ['purgekeys', 'selected'])
out = kadmin_as(none, ['purgekeys', 'selected'], expected_code=1)
if 'Operation requires ``modify\'\' privilege' not in out:
    fail('purgekeys failure (no perms)')
out = kadmin_as(some_modify, ['purgekeys', 'unselected'], expected_code=1)
if 'Operation requires ``modify\'\' privilege' not in out:
    fail('purgekeys failure (target)')
kadmin_as(none, ['purgekeys', 'none'])
realm.run([kadminl, 'delprinc', 'selected'])
realm.run([kadminl, 'delprinc', 'unselected'])

realm.addprinc('from', 'pw')
kadmin_as(all_rename, ['renprinc', 'from', 'to'])
realm.run([kadminl, 'renprinc', 'to', 'from'])
kadmin_as(some_rename, ['renprinc', 'from', 'to'])
realm.run([kadminl, 'renprinc', 'to', 'from'])
out = kadmin_as(all_add, ['renprinc', 'from', 'to'], expected_code=1)
if 'Operation requires ``delete\'\' privilege' not in out:
    fail('renprinc failure (no delete perms)')
out = kadmin_as(all_delete, ['renprinc', 'from', 'to'], expected_code=1)
if 'Operation requires ``add\'\' privilege' not in out:
    fail('renprinc failure (no add perms)')
out = kadmin_as(some_rename, ['renprinc', 'from', 'notto'], expected_code=1)
if 'Operation requires ``add\'\' privilege' not in out:
    fail('renprinc failure (new target)')
realm.run([kadminl, 'renprinc', 'from', 'notfrom'])
out = kadmin_as(some_rename, ['renprinc', 'notfrom', 'to'], expected_code=1)
if 'Operation requires ``delete\'\' privilege' not in out:
    fail('renprinc failure (old target)')
out = kadmin_as(restricted_rename, ['renprinc', 'notfrom', 'to'],
                expected_code=1)
if 'Operation requires ``add\'\' privilege' not in out:
    fail('renprinc failure (restrictions)')
realm.run([kadminl, 'delprinc', 'notfrom'])

realm.addprinc('selected', 'pw')
realm.addprinc('unselected', 'pw')
kadmin_as(all_modify, ['setstr', 'unselected', 'key', 'value'])
kadmin_as(some_modify, ['setstr', 'selected', 'key', 'value'])
out = kadmin_as(none, ['setstr', 'selected',  'key', 'value'], expected_code=1)
if 'Operation requires ``modify\'\' privilege' not in out:
    fail('addprinc failure (no perms)')
out = kadmin_as(some_modify, ['setstr', 'unselected', 'key', 'value'],
                expected_code=1)
if 'Operation requires' not in out:
    fail('modprinc failure (target)')
realm.run([kadminl, 'delprinc', 'selected'])
realm.run([kadminl, 'delprinc', 'unselected'])

kadmin_as(admin, ['addprinc', '-pw', 'pw', 'anytarget'])
realm.run([kadminl, 'delprinc', 'anytarget'])
kadmin_as(wctarget, ['addprinc', '-pw', 'pw', 'wild/card'])
realm.run([kadminl, 'delprinc', 'wild/card'])
out = kadmin_as(wctarget, ['addprinc', '-pw', 'pw', 'wild/card/extra'],
                expected_code=1)
if 'Operation requires' not in out:
    fail('addprinc failure (target wildcard extra component)')
realm.addprinc('admin/user', 'pw')
kadmin_as(admin, ['delprinc', 'admin/user'])
out = kadmin_as(admin, ['delprinc', 'none'], expected_code=1)
if 'Operation requires' not in out:
    fail('delprinc failure (wildcard backreferences not matched)')
realm.addprinc('four/one/three', 'pw')
kadmin_as(onetwothreefour, ['delprinc', 'four/one/three'])

kadmin_as(restrictions, ['addprinc', '-pw', 'pw', 'type1'])
out = realm.run([kadminl, 'getprinc', 'type1'])
if 'Policy: minlife' not in out:
    fail('restriction (policy)')
realm.run([kadminl, 'delprinc', 'type1'])
kadmin_as(restrictions, ['addprinc', '-pw', 'pw', '-policy', 'minlife',
                         'type2'])
out = realm.run([kadminl, 'getprinc', 'type2'])
if 'Policy: [none]' not in out:
    fail('restriction (clearpolicy)')
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
out = realm.run([kadminl, 'getprinc', 'type3'])
if 'Maximum renewable life: 0 days 02:00:00' not in out:
    fail('restriction (maxrenewlife high)')

realm.run([kadminl, 'addprinc', '-pw', 'pw', 'extractkeys'])
out = kadmin_as(all_wildcard, ['ktadd', '-norandkey', 'extractkeys'],
                expected_code=1)
if 'Operation requires ``extract-keys\'\' privilege' not in out:
    fail('extractkeys failure (all_wildcard)')
kadmin_as(all_extract, ['ktadd', '-norandkey', 'extractkeys'])
realm.kinit('extractkeys', flags=['-k'])
os.remove(realm.keytab)

kadmin_as(all_modify, ['modprinc', '+lockdown_keys', 'extractkeys'])
out = kadmin_as(all_changepw, ['cpw', '-pw', 'newpw', 'extractkeys'],
                expected_code=1)
if 'Operation requires ``change-password\'\' privilege' not in out:
    fail('extractkeys failure (all_changepw)')
kadmin_as(all_changepw, ['cpw', '-randkey', 'extractkeys'])
out = kadmin_as(all_extract, ['ktadd', '-norandkey', 'extractkeys'],
                expected_code=1)
if 'Operation requires ``extract-keys\'\' privilege' not in out:
    fail('extractkeys failure (all_extract)')
out = kadmin_as(all_delete, ['delprinc', 'extractkeys'], expected_code=1)
if 'Operation requires ``delete\'\' privilege' not in out:
    fail('extractkeys failure (all_delete)')
out = kadmin_as(all_rename, ['renprinc', 'extractkeys', 'renamedprinc'],
                expected_code=1)
if 'Operation requires ``delete\'\' privilege' not in out:
    fail('extractkeys failure (all_rename)')
out = kadmin_as(all_modify, ['modprinc', '-lockdown_keys', 'extractkeys'],
                expected_code=1)
if 'Operation requires ``modify\'\' privilege' not in out:
    fail('extractkeys failure (all_modify)')
realm.run([kadminl, 'modprinc', '-lockdown_keys', 'extractkeys'])
kadmin_as(all_extract, ['ktadd', '-norandkey', 'extractkeys'])
realm.kinit('extractkeys', flags=['-k'])
os.remove(realm.keytab)

success('kadmin ACL enforcement')
