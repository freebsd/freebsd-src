#!/usr/bin/python
from k5test import *
from filecmp import cmp

# Make sure we can dump and load an ordinary database, and that
# principals and policies survive a dump/load cycle.

realm = K5Realm(start_kdc=False)
realm.run([kadminl, 'addpol', 'fred'])

# Create a dump file.
dumpfile = os.path.join(realm.testdir, 'dump')
realm.run([kdb5_util, 'dump', dumpfile])

# Write additional policy records to the dump.  Use the 1.8 format for
# one of them, to test retroactive compatibility (for issue #8213).
f = open('testdir/dump', 'a')
f.write('policy	compat	0	0	3	4	5	0	'
        '0	0	0\n')
f.write('policy	barney	0	0	1	1	1	0	'
        '0	0	0	0	0	0	-	1	'
        '2	28	'
        'fd100f5064625f6372656174696f6e404b5242544553542e434f4d00\n')
f.close()

# Destroy and load the database; check that the policies exist.
# Spot-check principal and policy fields.
realm.run([kdb5_util, 'destroy', '-f'])
realm.run([kdb5_util, 'load', dumpfile])
out = realm.run([kadminl, 'getprincs'])
if realm.user_princ not in out or realm.host_princ not in out:
    fail('Missing principal after load')
out = realm.run([kadminl, 'getprinc', realm.user_princ])
if 'Expiration date: [never]' not in out or 'MKey: vno 1' not in out:
    fail('Principal has wrong value after load')
out = realm.run([kadminl, 'getpols'])
if 'fred\n' not in out or 'barney\n' not in out:
    fail('Missing policy after load')
out = realm.run([kadminl, 'getpol', 'compat'])
if 'Number of old keys kept: 5' not in out:
    fail('Policy (1.8 format) has wrong value after load')
out = realm.run([kadminl, 'getpol', 'barney'])
if 'Number of old keys kept: 1' not in out:
    fail('Policy has wrong value after load')

# Dump/load again, and make sure everything is still there.
realm.run([kdb5_util, 'dump', dumpfile])
realm.run([kdb5_util, 'load', dumpfile])
out = realm.run([kadminl, 'getprincs'])
if realm.user_princ not in out or realm.host_princ not in out:
    fail('Missing principal after load')
out = realm.run([kadminl, 'getpols'])
if 'compat\n' not in out or 'fred\n' not in out or 'barney\n' not in out:
    fail('Missing policy after second load')

srcdumpdir = os.path.join(srctop, 'tests', 'dumpfiles')
srcdump = os.path.join(srcdumpdir, 'dump')
srcdump_r18 = os.path.join(srcdumpdir, 'dump.r18')
srcdump_r13 = os.path.join(srcdumpdir, 'dump.r13')
srcdump_b7 = os.path.join(srcdumpdir, 'dump.b7')
srcdump_ov = os.path.join(srcdumpdir, 'dump.ov')

# Load a dump file from the source directory.
realm.run([kdb5_util, 'destroy', '-f'])
realm.run([kdb5_util, 'load', srcdump])
realm.run([kdb5_util, 'stash', '-P', 'master'])

def dump_compare(realm, opt, srcfile):
    realm.run([kdb5_util, 'dump'] + opt + [dumpfile])
    if not cmp(srcfile, dumpfile, False):
        fail('Dump output does not match %s' % srcfile)

# Dump the resulting DB in each non-iprop format and compare with
# expected outputs.
dump_compare(realm, [], srcdump)
dump_compare(realm, ['-r18'], srcdump_r18)
dump_compare(realm, ['-r13'], srcdump_r13)
dump_compare(realm, ['-b7'], srcdump_b7)
dump_compare(realm, ['-ov'], srcdump_ov)

def load_dump_check_compare(realm, opt, srcfile):
    realm.run([kdb5_util, 'destroy', '-f'])
    realm.run([kdb5_util, 'load'] + opt + [srcfile])
    out = realm.run([kadminl, 'getprincs'])
    if 'user@' not in out:
        fail('Loaded dumpfile missing user principal')
    out = realm.run([kadminl, 'getprinc', 'nokeys'])
    if 'Number of keys: 0' not in out:
        fail('Loading dumpfile did not process zero-key principal')
    out = realm.run([kadminl, 'getpols'])
    if 'testpol' not in out:
        fail('Loaded dumpfile missing test policy')
    dump_compare(realm, opt, srcfile)

# Load each format of dump, check it, re-dump it, and compare.
load_dump_check_compare(realm, ['-r18'], srcdump_r18)
load_dump_check_compare(realm, ['-r13'], srcdump_r13)
load_dump_check_compare(realm, ['-b7'], srcdump_b7)

# Loading the last (-b7 format) dump won't have loaded the
# per-principal kadm data.  Load that incrementally with -ov.
out = realm.run([kadminl, 'getprinc', 'user'])
if 'Policy: [none]' not in out:
    fail('Loaded b7 dump unexpectedly contains user policy reference')
realm.run([kdb5_util, 'load', '-update', '-ov', srcdump_ov])
out = realm.run([kadminl, 'getprinc', 'user'])
if 'Policy: testpol' not in out:
    fail('Loading ov dump did not add user policy reference')

success('Dump/load tests')
