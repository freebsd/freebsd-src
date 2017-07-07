#!/usr/bin/python
from k5test import *

conf_slave = {'dbmodules': {'db': {'database_name': '$testdir/db.slave'}}}

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
    slave = realm.special_env('slave', True, kdc_conf=conf_slave)

    # Set up the kpropd acl file.
    setup_acl(realm)

    # Create the slave db.
    dumpfile = os.path.join(realm.testdir, 'dump')
    realm.run([kdb5_util, 'dump', dumpfile])
    realm.run([kdb5_util, 'load', dumpfile], slave)
    realm.run([kdb5_util, 'stash', '-P', 'master'], slave)

    # Make some changes to the master db.
    realm.addprinc('wakawaka')

    # Start kpropd.
    kpropd = realm.start_kpropd(slave, ['-d'])

    realm.run([kdb5_util, 'dump', dumpfile])
    realm.run([kprop, '-f', dumpfile, '-P', str(realm.kprop_port()), hostname])
    check_output(kpropd)

    out = realm.run([kadminl, 'listprincs'], slave)
    if 'wakawaka' not in out:
        fail('Slave does not have all principals from master')

# default_realm tests follow.
# default_realm and domain_realm different than realm.realm (test -r argument).
conf_slave2 = {'dbmodules': {'db': {'database_name': '$testdir/db.slave2'}}}
krb5_conf_slave2 = {'libdefaults': {'default_realm': 'FOO'},
                    'domain_realm': {hostname: 'FOO'}}
# default_realm and domain_realm map differ.
conf_slave3 = {'dbmodules': {'db': {'database_name': '$testdir/db.slave3'}}}
krb5_conf_slave3 = {'domain_realm':  {hostname: 'BAR'}}

realm = K5Realm(create_user=False)
slave2 = realm.special_env('slave2', True, kdc_conf=conf_slave2,
                           krb5_conf=krb5_conf_slave2)
slave3 = realm.special_env('slave3', True, kdc_conf=conf_slave3,
                           krb5_conf=krb5_conf_slave3)

setup_acl(realm)

# Create the slave db.
dumpfile = os.path.join(realm.testdir, 'dump')
realm.run([kdb5_util, 'dump', dumpfile])
realm.run([kdb5_util, '-r', realm.realm, 'load', dumpfile], slave2)
realm.run([kdb5_util, 'load', dumpfile], slave3)

# Make some changes to the master db.
realm.addprinc('wakawaka')

# Test override of default_realm with -r realm argument.
kpropd = realm.start_kpropd(slave2, ['-r', realm.realm, '-d'])
realm.run([kdb5_util, 'dump', dumpfile])
realm.run([kprop, '-r', realm.realm, '-f', dumpfile, '-P',
           str(realm.kprop_port()), hostname])
check_output(kpropd)
out = realm.run([kadminl, '-r', realm.realm, 'listprincs'], slave2)
if 'wakawaka' not in out:
    fail('Slave does not have all principals from master')

stop_daemon(kpropd)

# Test default_realm and domain_realm mismatch.
kpropd = realm.start_kpropd(slave3, ['-d'])
realm.run([kdb5_util, 'dump', dumpfile])
realm.run([kprop, '-f', dumpfile, '-P', str(realm.kprop_port()), hostname])
check_output(kpropd)
out = realm.run([kadminl, 'listprincs'], slave3)
if 'wakawaka' not in out:
    fail('Slave does not have all principals from master')

success('kprop tests')
