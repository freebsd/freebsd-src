from k5test import *

conf_replica = {'dbmodules': {'db': {'database_name': '$testdir/db.replica'}}}

def setup_acl(realm):
    acl_file = os.path.join(realm.testdir, 'kpropd-acl')
    acl = open(acl_file, 'w')
    acl.write(realm.host_princ + '\n')
    acl.close()

def check_output(kpropd):
    output('*** kpropd output follows\n')
    while True:
        line = kpropd.stdout.readline()
        if 'Database load process for full propagation completed' in line:
            break
        output('kpropd: ' + line)
        if 'Rejected connection' in line:
            fail('kpropd rejected connection from kprop')

# kprop/kpropd are the only users of krb5_auth_con_initivector, so run
# this test over all enctypes to exercise mkpriv cipher state.
for realm in multipass_realms(create_user=False):
    replica = realm.special_env('replica', True, kdc_conf=conf_replica)

    # Set up the kpropd acl file.
    setup_acl(realm)

    # Create the replica db.
    dumpfile = os.path.join(realm.testdir, 'dump')
    realm.run([kdb5_util, 'dump', dumpfile])
    realm.run([kdb5_util, 'load', dumpfile], replica)
    realm.run([kdb5_util, 'stash', '-P', 'master'], replica)

    # Make some changes to the primary db.
    realm.addprinc('wakawaka')

    # Start kpropd.
    kpropd = realm.start_kpropd(replica, ['-d'])

    realm.run([kdb5_util, 'dump', dumpfile])
    realm.run([kprop, '-f', dumpfile, '-P', str(realm.kprop_port()), hostname])
    check_output(kpropd)

    realm.run([kadminl, 'listprincs'], replica, expected_msg='wakawaka')

# default_realm tests follow.
# default_realm and domain_realm different than realm.realm (test -r argument).
conf_rep2 = {'dbmodules': {'db': {'database_name': '$testdir/db.replica2'}}}
krb5_conf_rep2 = {'libdefaults': {'default_realm': 'FOO'},
                  'domain_realm': {hostname: 'FOO'}}
# default_realm and domain_realm map differ.
conf_rep3 = {'dbmodules': {'db': {'database_name': '$testdir/db.replica3'}}}
krb5_conf_rep3 = {'domain_realm':  {hostname: 'BAR'}}

realm = K5Realm(create_user=False)
replica2 = realm.special_env('replica2', True, kdc_conf=conf_rep2,
                             krb5_conf=krb5_conf_rep2)
replica3 = realm.special_env('replica3', True, kdc_conf=conf_rep3,
                             krb5_conf=krb5_conf_rep3)

setup_acl(realm)

# Create the replica db.
dumpfile = os.path.join(realm.testdir, 'dump')
realm.run([kdb5_util, 'dump', dumpfile])
realm.run([kdb5_util, '-r', realm.realm, 'load', dumpfile], replica2)
realm.run([kdb5_util, 'load', dumpfile], replica3)

# Make some changes to the primary db.
realm.addprinc('wakawaka')

# Test override of default_realm with -r realm argument.
kpropd = realm.start_kpropd(replica2, ['-r', realm.realm, '-d'])
realm.run([kdb5_util, 'dump', dumpfile])
realm.run([kprop, '-r', realm.realm, '-f', dumpfile, '-P',
           str(realm.kprop_port()), hostname])
check_output(kpropd)
realm.run([kadminl, '-r', realm.realm, 'listprincs'], replica2,
          expected_msg='wakawaka')

stop_daemon(kpropd)

# Test default_realm and domain_realm mismatch.
kpropd = realm.start_kpropd(replica3, ['-d'])
realm.run([kdb5_util, 'dump', dumpfile])
realm.run([kprop, '-f', dumpfile, '-P', str(realm.kprop_port()), hostname])
check_output(kpropd)
realm.run([kadminl, 'listprincs'], replica3, expected_msg='wakawaka')
stop_daemon(kpropd)

# This test is too resource-intensive to be included in "make check"
# by default, but it can be enabled in the environment to test the
# propagation of databases large enough to require a 12-byte encoding
# of the database size.
if 'KPROP_LARGE_DB_TEST' in os.environ:
    output('Generating >4GB dumpfile\n')
    with open(dumpfile, 'w') as f:
        f.write('kdb5_util load_dump version 6\n')
        f.write('princ\t38\t15\t3\t1\t0\tK/M@KRBTEST.COM\t64\t86400\t0\t0\t0'
                '\t0\t0\t0\t8\t2\t0100\t9\t8\t0100010000000000\t2\t28'
                '\tb93e105164625f6372656174696f6e404b5242544553542e434f4d00'
                '\t1\t1\t18\t62\t2000408c027c250e8cc3b81476414f2214d57c1ce'
                '38891e29792e87258247c73547df4d5756266931dd6686b62270e6568'
                '95a31ec66bfe913b4f15226227\t-1;\n')
        for i in range(1, 20000000):
            f.write('princ\t38\t21\t1\t1\t0\tp%08d@KRBTEST.COM' % i)
            f.write('\t0\t86400\t0\t0\t0\t0\t0\t0\t2\t27'
                    '\td73e1051757365722f61646d696e404b5242544553542e434f4d00'
                    '\t1\t1\t17\t46'
                    '\t10009c8ab7b3f89ccf3ca3ad98352a461b7f4f1b0c49'
                    '5605117591d9ad52ba4da0adef7a902126973ed2bdc3ffbf\t-1;\n')
    assert os.path.getsize(dumpfile) > 4 * 1024 * 1024 * 1024
    with open(dumpfile + '.dump_ok', 'w') as f:
        f.write('\0')
    conf_large = {'dbmodules': {'db': {'database_name': '$testdir/db.large'}},
                  'realms': {'$realm': {'iprop_resync_timeout': '3600'}}}
    large = realm.special_env('large', True, kdc_conf=conf_large)
    kpropd = realm.start_kpropd(large, ['-d'])
    realm.run([kprop, '-f', dumpfile, '-P', str(realm.kprop_port()), hostname])
    check_output(kpropd)
    realm.run([kadminl, 'getprinc', 'p19999999'], env=large,
              expected_msg='Principal: p19999999')

success('kprop tests')
