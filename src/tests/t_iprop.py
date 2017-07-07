#!/usr/bin/python

import os
import re

from k5test import *

# Read lines from kpropd output until we are synchronized.  Error if
# full_expected is true and we didn't see a full propagation or vice
# versa.
def wait_for_prop(kpropd, full_expected, expected_old, expected_new):
    output('*** Waiting for sync from kpropd\n')
    full_seen = sleep_seen = False
    old_sno = new_sno = -1
    while True:
        line = kpropd.stdout.readline()
        if line == '':
            fail('kpropd process exited unexpectedly')
        output('kpropd: ' + line)

        m = re.match(r'Calling iprop_get_updates_1 \(sno=(\d+) ', line)
        if m:
            if not full_seen:
                old_sno = int(m.group(1))
            # Also record this as the new sno, in case we get back
            # UPDATE_NIL.
            new_sno = int(m.group(1))

        m = re.match(r'Got incremental updates \(sno=(\d+) ', line)
        if m:
            new_sno = int(m.group(1))

        if 'KDC is synchronized' in line or 'Incremental updates:' in line:
            break

        # After a full resync request, these lines could appear in
        # either order.
        if 'Waiting for' in line:
            sleep_seen = True
        if 'load process for full propagation completed' in line:
            full_seen = True

        # Detect some failure conditions.
        if 'Still waiting for full resync' in line:
            fail('kadmind gave consecutive full resyncs')
        if 'Rejected connection' in line:
            fail('kpropd rejected kprop connection')
        if 'get updates failed' in line:
            fail('iprop_get_updates failed')
        if 'permission denied' in line:
            fail('kadmind denied update')
        if 'error from master' in line or 'error returned from master' in line:
            fail('kadmind reported error')
        if 'invalid return' in line:
            fail('kadmind returned invalid result')

    if full_expected and not full_seen:
        fail('Expected full dump but saw only incremental')
    if full_seen and not full_expected:
        fail('Expected incremental prop but saw full dump')
    if old_sno != expected_old:
         fail('Expected old serial %d from kpropd sync' % expected_old)
    if new_sno != expected_new:
         fail('Expected new serial %d from kpropd sync' % expected_new)

    # Wait until kpropd is sleeping before continuing, to avoid races.
    # (This is imperfect since there's there is a short window between
    # the fprintf and the sleep; kpropd will need design changes to
    # fix that.)
    while True:
        line = kpropd.stdout.readline()
        output('kpropd: ' + line)
        if 'Waiting for' in line:
            break
    output('*** Sync complete\n')

# Verify the output of kproplog against the expected number of
# entries, first and last serial number, and a list of principal names
# for the update entrires.
def check_ulog(num, first, last, entries, env=None):
    out = realm.run([kproplog], env=env)
    if 'Number of entries : ' + str(num) + '\n' not in out:
        fail('Expected %d entries' % num)
    if last:
        firststr = first and str(first) or 'None'
        if 'First serial # : ' + firststr + '\n' not in out:
            fail('Expected first serial number %d' % first)
    laststr = last and str(last) or 'None'
    if 'Last serial # : ' + laststr + '\n' not in out:
        fail('Expected last serial number %d' % last)
    assert(len(entries) == num)
    ser = first - 1
    entindex = 0
    for line in out.splitlines():
        m = re.match(r'\tUpdate serial # : (\d+)$', line)
        if m:
            ser = ser + 1
            if m.group(1) != str(ser):
                fail('Expected serial number %d in update entry' % ser)
        m = re.match(r'\tUpdate principal : (.*)$', line)
        if m:
            eprinc = entries[ser - first]
            if eprinc == None:
                fail('Expected dummy update entry %d' % ser)
            elif m.group(1) != eprinc:
                fail('Expected princ %s in update entry %d' % (eprinc, ser))
        if line == '\tDummy entry':
            eprinc = entries[ser - first]
            if eprinc != None:
                fail('Expected princ %s in update entry %d' % (eprinc, ser))

# slave1 will receive updates from master, and slave2 will receive
# updates from slave1.  Because of the awkward way iprop and kprop
# port configuration currently works, we need separate config files
# for the slave and master sides of slave1, but they use the same DB
# and ulog file.
conf = {'realms': {'$realm': {'iprop_enable': 'true',
                              'iprop_logfile': '$testdir/db.ulog'}}}
conf_slave1 = {'realms': {'$realm': {'iprop_slave_poll': '600',
                                     'iprop_logfile': '$testdir/ulog.slave1'}},
               'dbmodules': {'db': {'database_name': '$testdir/db.slave1'}}}
conf_slave1m = {'realms': {'$realm': {'iprop_logfile': '$testdir/ulog.slave1',
                                      'iprop_port': '$port8'}},
               'dbmodules': {'db': {'database_name': '$testdir/db.slave1'}}}
conf_slave2 = {'realms': {'$realm': {'iprop_slave_poll': '600',
                                     'iprop_logfile': '$testdir/ulog.slave2',
                                     'iprop_port': '$port8'}},
               'dbmodules': {'db': {'database_name': '$testdir/db.slave2'}}}

conf_foo = {'libdefaults': {'default_realm': 'FOO'},
            'domain_realm': {hostname: 'FOO'}}

realm = K5Realm(kdc_conf=conf, create_user=False, start_kadmind=True)
slave1 = realm.special_env('slave1', True, kdc_conf=conf_slave1)
slave1m = realm.special_env('slave1m', True, krb5_conf=conf_foo,
                            kdc_conf=conf_slave1m)
slave2 = realm.special_env('slave2', True, kdc_conf=conf_slave2)

# A default_realm and domain_realm that do not match the KDC's realm.
# The FOO realm iprop_logfile setting is needed to run kproplog during
# a slave3 test, since kproplog has no realm option.
conf_slave3 = {'realms': {'$realm': {'iprop_slave_poll': '600',
                                     'iprop_logfile': '$testdir/ulog.slave3',
                                     'iprop_port': '$port8'},
                          'FOO': {'iprop_logfile': '$testdir/ulog.slave3'}},
               'dbmodules': {'db': {'database_name': '$testdir/db.slave3'}}}
slave3 = realm.special_env('slave3', True, krb5_conf=conf_foo,
                           kdc_conf=conf_slave3)

# A default realm and a domain realm map that differ.
krb5_conf_slave4 = {'domain_realm': {hostname: 'FOO'}}
conf_slave4 = {'realms': {'$realm': {'iprop_slave_poll': '600',
                                     'iprop_logfile': '$testdir/ulog.slave4',
                                     'iprop_port': '$port8'}},
               'dbmodules': {'db': {'database_name': '$testdir/db.slave4'}}}
slave4 = realm.special_env('slave4', True, krb5_conf=krb5_conf_slave4,
                            kdc_conf=conf_slave4)

# Define some principal names.  pr3 is long enough to cause internal
# reallocs, but not long enough to grow the basic ulog entry size.
pr1 = 'wakawaka@' + realm.realm
pr2 = 'w@' + realm.realm
c = 'chocolate-flavored-school-bus'
cs = c + '/'
pr3 = (cs + cs + cs + cs + cs + cs + cs + cs + cs + cs + cs + cs + c +
       '@' + realm.realm)

# Create the kpropd ACL file.
acl_file = os.path.join(realm.testdir, 'kpropd-acl')
acl = open(acl_file, 'w')
acl.write(realm.host_princ + '\n')
acl.close()

ulog = os.path.join(realm.testdir, 'db.ulog')
if not os.path.exists(ulog):
    fail('update log not created: ' + ulog)

# Create the principal used to authenticate kpropd to kadmind.
kiprop_princ = 'kiprop/' + hostname
realm.extract_keytab(kiprop_princ, realm.keytab)

# Create the initial slave databases.
dumpfile = os.path.join(realm.testdir, 'dump')
realm.run([kdb5_util, 'dump', dumpfile])
realm.run([kdb5_util, 'load', dumpfile], slave1)
realm.run([kdb5_util, 'load', dumpfile], slave2)
realm.run([kdb5_util, '-r', realm.realm, 'load', dumpfile], slave3)
realm.run([kdb5_util, 'load', dumpfile], slave4)

# Reinitialize the master ulog so we know exactly what to expect in
# it.
realm.run([kproplog, '-R'])
check_ulog(1, 1, 1, [None])

# Make some changes to the master DB.
realm.addprinc(pr1)
realm.addprinc(pr3)
realm.addprinc(pr2)
realm.run([kadminl, 'modprinc', '-allow_tix', pr2])
realm.run([kadminl, 'modprinc', '+allow_tix', pr2])
check_ulog(6, 1, 6, [None, pr1, pr3, pr2, pr2, pr2])

# Start kpropd for slave1 and get a full dump from master.
kpropd1 = realm.start_kpropd(slave1, ['-d'])
wait_for_prop(kpropd1, True, 1, 6)
out = realm.run([kadminl, 'listprincs'], env=slave1)
if pr1 not in out or pr2 not in out or pr3 not in out:
    fail('slave1 does not have all principals from master')
check_ulog(1, 6, 6, [None], slave1)

# Make a change and check that it propagates incrementally.
realm.run([kadminl, 'modprinc', '-allow_tix', pr2])
check_ulog(7, 1, 7, [None, pr1, pr3, pr2, pr2, pr2, pr2])
kpropd1.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd1, False, 6, 7)
check_ulog(2, 6, 7, [None, pr2], slave1)
out = realm.run([kadminl, 'getprinc', pr2], env=slave1)
if 'Attributes: DISALLOW_ALL_TIX' not in out:
    fail('slave1 does not have modification from master')

# Start kadmind -proponly for slave1.  (Use the slave1m environment
# which defines iprop_port to $port8.)
slave1_out_dump_path = os.path.join(realm.testdir, 'dump.slave1.out')
slave2_in_dump_path = os.path.join(realm.testdir, 'dump.slave2.in')
slave2_kprop_port = str(realm.portbase + 9)
realm.start_server([kadmind, '-r', realm.realm, '-nofork', '-proponly', '-W',
                    '-p', kdb5_util, '-K', kprop, '-k', slave2_kprop_port,
                    '-F', slave1_out_dump_path], 'starting...', slave1m)

# Test similar default_realm and domain_realm map settings with -r realm.
slave3_in_dump_path = os.path.join(realm.testdir, 'dump.slave3.in')
kpropd3 = realm.start_server([kpropd, '-d', '-D', '-r', realm.realm, '-P',
                              slave2_kprop_port, '-f', slave3_in_dump_path,
                              '-p', kdb5_util, '-a', acl_file, '-A', hostname],
                             'ready', slave3)
wait_for_prop(kpropd3, True, 1, 7)
out = realm.run([kadminl, '-r', realm.realm, 'listprincs'], env=slave3)
if pr1 not in out or pr2 not in out or pr3 not in out:
    fail('slave3 does not have all principals from slave1')
check_ulog(1, 7, 7, [None], env=slave3)

# Test an incremental propagation for the kpropd -r case.
realm.run([kadminl, 'modprinc', '-maxlife', '20 minutes', pr1])
check_ulog(8, 1, 8, [None, pr1, pr3, pr2, pr2, pr2, pr2, pr1])
kpropd1.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd1, False, 7, 8)
check_ulog(3, 6, 8, [None, pr2, pr1], slave1)
out = realm.run([kadminl, 'getprinc', pr1], env=slave1)
if 'Maximum ticket life: 0 days 00:20:00' not in out:
    fail('slave1 does not have modification from master')
kpropd3.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd3, False, 7, 8)
check_ulog(2, 7, 8, [None, pr1], slave3)
out = realm.run([kadminl, '-r', realm.realm, 'getprinc', pr1], env=slave3)
if 'Maximum ticket life: 0 days 00:20:00' not in out:
    fail('slave3 does not have modification from slave1')
stop_daemon(kpropd3)

# Test dissimilar default_realm and domain_realm map settings (no -r realm).
slave4_in_dump_path = os.path.join(realm.testdir, 'dump.slave4.in')
kpropd4 = realm.start_server([kpropd, '-d', '-D', '-P', slave2_kprop_port,
                              '-f', slave4_in_dump_path, '-p', kdb5_util,
                              '-a', acl_file, '-A', hostname], 'ready', slave4)
wait_for_prop(kpropd4, True, 1, 8)
out = realm.run([kadminl, 'listprincs'], env=slave4)
if pr1 not in out or pr2 not in out or pr3 not in out:
    fail('slave4 does not have all principals from slave1')
stop_daemon(kpropd4)

# Start kpropd for slave2.  The -A option isn't needed since we're
# talking to the same host as master (we specify it anyway to exercise
# the code), but slave2 defines iprop_port to $port8 so it will talk
# to slave1.  Get a full dump from slave1.
kpropd2 = realm.start_server([kpropd, '-d', '-D', '-P', slave2_kprop_port,
                              '-f', slave2_in_dump_path, '-p', kdb5_util,
                              '-a', acl_file, '-A', hostname], 'ready', slave2)
wait_for_prop(kpropd2, True, 1, 8)
check_ulog(2, 7, 8, [None, pr1], slave2)
out = realm.run([kadminl, 'listprincs'], env=slave1)
if pr1 not in out or pr2 not in out or pr3 not in out:
    fail('slave2 does not have all principals from slave1')

# Make another change and check that it propagates incrementally to
# both slaves.
realm.run([kadminl, 'modprinc', '-maxrenewlife', '22 hours', pr1])
check_ulog(9, 1, 9, [None, pr1, pr3, pr2, pr2, pr2, pr2, pr1, pr1])
kpropd1.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd1, False, 8, 9)
check_ulog(4, 6, 9, [None, pr2, pr1, pr1], slave1)
out = realm.run([kadminl, 'getprinc', pr1], env=slave1)
if 'Maximum renewable life: 0 days 22:00:00\n' not in out:
    fail('slave1 does not have modification from master')
kpropd2.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd2, False, 8, 9)
check_ulog(3, 7, 9, [None, pr1, pr1], slave2)
out = realm.run([kadminl, 'getprinc', pr1], env=slave2)
if 'Maximum renewable life: 0 days 22:00:00\n' not in out:
    fail('slave2 does not have modification from slave1')

# Reset the ulog on slave1 to force a full resync from master.  The
# resync will use the old dump file and then propagate changes.
# slave2 should still be in sync with slave1 after the resync, so make
# sure it doesn't take a full resync.
realm.run([kproplog, '-R'], slave1)
check_ulog(1, 1, 1, [None], slave1)
kpropd1.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd1, True, 1, 9)
check_ulog(4, 6, 9, [None, pr2, pr1, pr1], slave1)
kpropd2.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd2, False, 9, 9)
check_ulog(3, 7, 9, [None, pr1, pr1], slave2)

# Make another change and check that it propagates incrementally to
# both slaves.
realm.run([kadminl, 'modprinc', '+allow_tix', pr2])
check_ulog(10, 1, 10, [None, pr1, pr3, pr2, pr2, pr2, pr2, pr1, pr1, pr2])
kpropd1.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd1, False, 9, 10)
check_ulog(5, 6, 10, [None, pr2, pr1, pr1, pr2], slave1)
out = realm.run([kadminl, 'getprinc', pr2], env=slave1)
if 'Attributes:\n' not in out:
    fail('slave1 does not have modification from master')
kpropd2.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd2, False, 9, 10)
check_ulog(4, 7, 10, [None, pr1, pr1, pr2], slave2)
out = realm.run([kadminl, 'getprinc', pr2], env=slave2)
if 'Attributes:\n' not in out:
    fail('slave2 does not have modification from slave1')

# Create a policy and check that it propagates via full resync.
realm.run([kadminl, 'addpol', '-minclasses', '2', 'testpol'])
check_ulog(1, 1, 1, [None])
kpropd1.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd1, True, 10, 1)
check_ulog(1, 1, 1, [None], slave1)
out = realm.run([kadminl, 'getpol', 'testpol'], env=slave1)
if 'Minimum number of password character classes: 2' not in out:
    fail('slave1 does not have policy from master')
kpropd2.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd2, True, 10, 1)
check_ulog(1, 1, 1, [None], slave2)
out = realm.run([kadminl, 'getpol', 'testpol'], env=slave2)
if 'Minimum number of password character classes: 2' not in out:
    fail('slave2 does not have policy from slave1')

# Modify the policy and test that it also propagates via full resync.
realm.run([kadminl, 'modpol', '-minlength', '17', 'testpol'])
check_ulog(1, 1, 1, [None])
kpropd1.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd1, True, 1, 1)
check_ulog(1, 1, 1, [None], slave1)
out = realm.run([kadminl, 'getpol', 'testpol'], env=slave1)
if 'Minimum password length: 17' not in out:
    fail('slave1 does not have policy change from master')
kpropd2.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd2, True, 1, 1)
check_ulog(1, 1, 1, [None], slave2)
out = realm.run([kadminl, 'getpol', 'testpol'], env=slave2)
if 'Minimum password length: 17' not in out:
    fail('slave2 does not have policy change from slave1')

# Delete the policy and test that it propagates via full resync.
realm.run([kadminl, 'delpol', 'testpol'])
check_ulog(1, 1, 1, [None])
kpropd1.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd1, True, 1, 1)
check_ulog(1, 1, 1, [None], slave1)
out = realm.run([kadminl, 'getpol', 'testpol'], env=slave1, expected_code=1)
if 'Policy does not exist' not in out:
    fail('slave1 did not get policy deletion from master')
kpropd2.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd2, True, 1, 1)
check_ulog(1, 1, 1, [None], slave2)
out = realm.run([kadminl, 'getpol', 'testpol'], env=slave2, expected_code=1)
if 'Policy does not exist' not in out:
    fail('slave2 did not get policy deletion from slave1')

# Modify a principal on the master and test that it propagates incrementally.
realm.run([kadminl, 'modprinc', '-maxlife', '10 minutes', pr1])
check_ulog(2, 1, 2, [None, pr1])
kpropd1.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd1, False, 1, 2)
check_ulog(2, 1, 2, [None, pr1], slave1)
out = realm.run([kadminl, 'getprinc', pr1], env=slave1)
if 'Maximum ticket life: 0 days 00:10:00' not in out:
    fail('slave1 does not have modification from master')
kpropd2.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd2, False, 1, 2)
check_ulog(2, 1, 2, [None, pr1], slave2)
out = realm.run([kadminl, 'getprinc', pr1], env=slave2)
if 'Maximum ticket life: 0 days 00:10:00' not in out:
    fail('slave2 does not have modification from slave1')

# Delete a principal and test that it propagates incrementally.
realm.run([kadminl, 'delprinc', pr3])
check_ulog(3, 1, 3, [None, pr1, pr3])
kpropd1.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd1, False, 2, 3)
check_ulog(3, 1, 3, [None, pr1, pr3], slave1)
out = realm.run([kadminl, 'getprinc', pr3], env=slave1, expected_code=1)
if 'Principal does not exist' not in out:
    fail('slave1 does not have principal deletion from master')
kpropd2.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd2, False, 2, 3)
check_ulog(3, 1, 3, [None, pr1, pr3], slave2)
out = realm.run([kadminl, 'getprinc', pr3], env=slave2, expected_code=1)
if 'Principal does not exist' not in out:
    fail('slave2 does not have principal deletion from slave1')

# Rename a principal and test that it propagates incrementally.
renpr = "quacked@" + realm.realm
realm.run([kadminl, 'renprinc', pr1, renpr])
check_ulog(6, 1, 6, [None, pr1, pr3, renpr, pr1, renpr])
kpropd1.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd1, False, 3, 6)
check_ulog(6, 1, 6, [None, pr1, pr3, renpr, pr1, renpr], slave1)
out = realm.run([kadminl, 'getprinc', pr1], env=slave1, expected_code=1)
if 'Principal does not exist' not in out:
    fail('slave1 does not have principal deletion from master')
realm.run([kadminl, 'getprinc', renpr], env=slave1)
kpropd2.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd2, False, 3, 6)
check_ulog(6, 1, 6, [None, pr1, pr3, renpr, pr1, renpr], slave2)
out = realm.run([kadminl, 'getprinc', pr1], env=slave2, expected_code=1)
if 'Principal does not exist' not in out:
    fail('slave2 does not have principal deletion from master')
realm.run([kadminl, 'getprinc', renpr], env=slave2)

pr1 = renpr

# Reset the ulog on the master to force a full resync.
realm.run([kproplog, '-R'])
check_ulog(1, 1, 1, [None])
kpropd1.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd1, True, 6, 1)
check_ulog(1, 1, 1, [None], slave1)
kpropd2.send_signal(signal.SIGUSR1)
wait_for_prop(kpropd2, True, 6, 1)
check_ulog(1, 1, 1, [None], slave2)

# Stop the kprop daemons so we can test kpropd -t.
stop_daemon(kpropd1)
stop_daemon(kpropd2)

# Test the case where no updates are needed.
out = realm.run_kpropd_once(slave1, ['-d'])
if 'KDC is synchronized' not in out:
    fail('Expected synchronized from kpropd -t')
check_ulog(1, 1, 1, [None], slave1)

# Make a change on the master and fetch it incrementally.
realm.run([kadminl, 'modprinc', '-maxlife', '5 minutes', pr1])
check_ulog(2, 1, 2, [None, pr1])
out = realm.run_kpropd_once(slave1, ['-d'])
if 'Got incremental updates (sno=2 ' not in out:
    fail('Expected full dump and synchronized from kpropd -t')
check_ulog(2, 1, 2, [None, pr1], slave1)
out = realm.run([kadminl, 'getprinc', pr1], env=slave1)
if 'Maximum ticket life: 0 days 00:05:00' not in out:
    fail('slave1 does not have modification from master after kpropd -t')

# Propagate a policy change via full resync.
realm.run([kadminl, 'addpol', '-minclasses', '3', 'testpol'])
check_ulog(1, 1, 1, [None])
out = realm.run_kpropd_once(slave1, ['-d'])
if ('Full propagation transfer finished' not in out or
    'KDC is synchronized' not in out):
    fail('Expected full dump and synchronized from kpropd -t')
check_ulog(1, 1, 1, [None], slave1)
out = realm.run([kadminl, 'getpol', 'testpol'], env=slave1)
if 'Minimum number of password character classes: 3' not in out:
    fail('slave1 does not have policy from master after kpropd -t')

success('iprop tests')
