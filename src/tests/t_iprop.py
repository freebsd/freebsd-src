import os
import re

from k5test import *

# On macOS with System Integrity Protection enabled, this script hangs
# in the wait_for_prop() call after starting the first kpropd process,
# most likely due to signal restrictions preventing the listening
# child from informing the parent that a full resync was processed.
if which('csrutil'):
    out = subprocess.check_output(['csrutil', 'status'],
                                  universal_newlines=True)
    if 'status: enabled' in out:
        skip_rest('iprop tests', 'System Integrity Protection is enabled')

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
        if ('error from primary' in line or
            'error returned from primary' in line):
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

# replica1 will receive updates from primary, and replica2 will
# receive updates from replica1.  Because of the awkward way iprop and
# kprop port configuration currently works, we need separate config
# files for the replica and primary sides of replica1, but they use
# the same DB and ulog file.
conf = {'realms': {'$realm': {'iprop_enable': 'true',
                              'iprop_logfile': '$testdir/db.ulog'}}}
conf_rep1 = {'realms': {'$realm': {'iprop_replica_poll': '600',
                                   'iprop_logfile': '$testdir/ulog.replica1'}},
             'dbmodules': {'db': {'database_name': '$testdir/db.replica1'}}}
conf_rep1m = {'realms': {'$realm': {'iprop_logfile': '$testdir/ulog.replica1',
                                    'iprop_port': '$port8'}},
              'dbmodules': {'db': {'database_name': '$testdir/db.replica1'}}}
conf_rep2 = {'realms': {'$realm': {'iprop_replica_poll': '600',
                                   'iprop_logfile': '$testdir/ulog.replica2',
                                   'iprop_port': '$port8'}},
             'dbmodules': {'db': {'database_name': '$testdir/db.replica2'}}}

conf_foo = {'libdefaults': {'default_realm': 'FOO'},
            'domain_realm': {hostname: 'FOO'}}
conf_rep3 = {'realms': {'$realm': {'iprop_replica_poll': '600',
                                   'iprop_logfile': '$testdir/ulog.replica3',
                                   'iprop_port': '$port8'},
                        'FOO': {'iprop_logfile': '$testdir/ulog.replica3'}},
            'dbmodules': {'db': {'database_name': '$testdir/db.replica3'}}}

krb5_conf_rep4 = {'domain_realm': {hostname: 'FOO'}}
conf_rep4 = {'realms': {'$realm': {'iprop_replica_poll': '600',
                                   'iprop_logfile': '$testdir/ulog.replica4',
                                   'iprop_port': '$port8'}},
             'dbmodules': {'db': {'database_name': '$testdir/db.replica4'}}}

for realm in multidb_realms(kdc_conf=conf, create_user=False,
                            start_kadmind=True):
    replica1 = realm.special_env('replica1', True, kdc_conf=conf_rep1)
    replica1m = realm.special_env('replica1m', True, krb5_conf=conf_foo,
                                  kdc_conf=conf_rep1m)
    replica2 = realm.special_env('replica2', True, kdc_conf=conf_rep2)

    # A default_realm and domain_realm that do not match the KDC's
    # realm.  The FOO realm iprop_logfile setting is needed to run
    # kproplog during a replica3 test, since kproplog has no realm
    # option.
    replica3 = realm.special_env('replica3', True, krb5_conf=conf_foo,
                                 kdc_conf=conf_rep3)

    # A default realm and a domain realm map that differ.
    replica4 = realm.special_env('replica4', True, krb5_conf=krb5_conf_rep4,
                                 kdc_conf=conf_rep4)

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
    realm.addprinc(kiprop_princ)
    realm.extract_keytab(kiprop_princ, realm.keytab)

    # Create the initial replica databases.
    dumpfile = os.path.join(realm.testdir, 'dump')
    realm.run([kdb5_util, 'dump', dumpfile])
    realm.run([kdb5_util, 'load', dumpfile], replica1)
    realm.run([kdb5_util, 'load', dumpfile], replica2)
    realm.run([kdb5_util, '-r', realm.realm, 'load', dumpfile], replica3)
    realm.run([kdb5_util, 'load', dumpfile], replica4)

    # Reinitialize the primary ulog so we know exactly what to expect
    # in it.
    realm.run([kproplog, '-R'])
    check_ulog(1, 1, 1, [None])

    # Make some changes to the primary DB.
    realm.addprinc(pr1)
    realm.addprinc(pr3)
    realm.addprinc(pr2)
    realm.run([kadminl, 'modprinc', '-allow_tix', pr2])
    realm.run([kadminl, 'modprinc', '+allow_tix', pr2])
    check_ulog(6, 1, 6, [None, pr1, pr3, pr2, pr2, pr2])

    # Start kpropd for replica1 and get a full dump from primary.
    mark('propagate M->1 full')
    kpropd1 = realm.start_kpropd(replica1, ['-d'])
    wait_for_prop(kpropd1, True, 1, 6)
    out = realm.run([kadminl, 'listprincs'], env=replica1)
    if pr1 not in out or pr2 not in out or pr3 not in out:
        fail('replica1 does not have all principals from primary')
    check_ulog(1, 6, 6, [None], replica1)

    # Make a change and check that it propagates incrementally.
    mark('propagate M->1 incremental')
    realm.run([kadminl, 'modprinc', '-allow_tix', pr2])
    check_ulog(7, 1, 7, [None, pr1, pr3, pr2, pr2, pr2, pr2])
    kpropd1.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd1, False, 6, 7)
    check_ulog(2, 6, 7, [None, pr2], replica1)
    realm.run([kadminl, 'getprinc', pr2], env=replica1,
              expected_msg='Attributes: DISALLOW_ALL_TIX')

    # Start kadmind -proponly for replica1.  (Use the replica1m
    # environment which defines iprop_port to $port8.)
    replica1_out_dump_path = os.path.join(realm.testdir, 'dump.replica1.out')
    replica2_in_dump_path = os.path.join(realm.testdir, 'dump.replica2.in')
    replica2_kprop_port = str(realm.portbase + 9)
    kadmind_proponly = realm.start_server([kadmind, '-r', realm.realm,
                                           '-nofork', '-proponly',
                                           '-p', kdb5_util,
                                           '-K', kprop, '-k',
                                           replica2_kprop_port,
                                           '-F', replica1_out_dump_path],
                                          'starting...', replica1m)

    # Test similar default_realm and domain_realm map settings with -r realm.
    mark('propagate 1->3 full')
    replica3_in_dump_path = os.path.join(realm.testdir, 'dump.replica3.in')
    kpropd3 = realm.start_server([kpropd, '-d', '-D', '-r', realm.realm, '-P',
                                  replica2_kprop_port, '-f',
                                  replica3_in_dump_path, '-p', kdb5_util, '-a',
                                  acl_file, '-A', hostname], 'ready', replica3)
    wait_for_prop(kpropd3, True, 1, 7)
    out = realm.run([kadminl, '-r', realm.realm, 'listprincs'], env=replica3)
    if pr1 not in out or pr2 not in out or pr3 not in out:
        fail('replica3 does not have all principals from replica1')
    check_ulog(1, 7, 7, [None], env=replica3)

    # Test an incremental propagation for the kpropd -r case.
    mark('propagate M->1->3 incremental')
    realm.run([kadminl, 'modprinc', '-maxlife', '20 minutes', pr1])
    check_ulog(8, 1, 8, [None, pr1, pr3, pr2, pr2, pr2, pr2, pr1])
    kpropd1.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd1, False, 7, 8)
    check_ulog(3, 6, 8, [None, pr2, pr1], replica1)
    realm.run([kadminl, 'getprinc', pr1], env=replica1,
              expected_msg='Maximum ticket life: 0 days 00:20:00')
    kpropd3.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd3, False, 7, 8)
    check_ulog(2, 7, 8, [None, pr1], replica3)
    realm.run([kadminl, '-r', realm.realm, 'getprinc', pr1], env=replica3,
              expected_msg='Maximum ticket life: 0 days 00:20:00')
    stop_daemon(kpropd3)

    # Test dissimilar default_realm and domain_realm map settings (no
    # -r realm).
    mark('propagate 1->4 full')
    replica4_in_dump_path = os.path.join(realm.testdir, 'dump.replica4.in')
    kpropd4 = realm.start_server([kpropd, '-d', '-D', '-P',
                                  replica2_kprop_port, '-f',
                                  replica4_in_dump_path, '-p', kdb5_util,
                                  '-a', acl_file, '-A', hostname], 'ready',
                                 replica4)
    wait_for_prop(kpropd4, True, 1, 8)
    out = realm.run([kadminl, 'listprincs'], env=replica4)
    if pr1 not in out or pr2 not in out or pr3 not in out:
        fail('replica4 does not have all principals from replica1')
    stop_daemon(kpropd4)

    # Start kpropd for replica2.  The -A option isn't needed since
    # we're talking to the same host as primary (we specify it anyway
    # to exercise the code), but replica2 defines iprop_port to $port8
    # so it will talk to replica1.  Get a full dump from replica1.
    mark('propagate 1->2 full')
    kpropd2 = realm.start_server([kpropd, '-d', '-D', '-P',
                                  replica2_kprop_port, '-f',
                                  replica2_in_dump_path, '-p', kdb5_util,
                                  '-a', acl_file, '-A', hostname], 'ready',
                                 replica2)
    wait_for_prop(kpropd2, True, 1, 8)
    check_ulog(2, 7, 8, [None, pr1], replica2)
    out = realm.run([kadminl, 'listprincs'], env=replica1)
    if pr1 not in out or pr2 not in out or pr3 not in out:
        fail('replica2 does not have all principals from replica1')

    # Make another change and check that it propagates incrementally
    # to both replicas.
    mark('propagate M->1->2 incremental')
    realm.run([kadminl, 'modprinc', '-maxrenewlife', '22 hours', pr1])
    check_ulog(9, 1, 9, [None, pr1, pr3, pr2, pr2, pr2, pr2, pr1, pr1])
    kpropd1.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd1, False, 8, 9)
    check_ulog(4, 6, 9, [None, pr2, pr1, pr1], replica1)
    realm.run([kadminl, 'getprinc', pr1], env=replica1,
              expected_msg='Maximum renewable life: 0 days 22:00:00\n')
    kpropd2.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd2, False, 8, 9)
    check_ulog(3, 7, 9, [None, pr1, pr1], replica2)
    realm.run([kadminl, 'getprinc', pr1], env=replica2,
              expected_msg='Maximum renewable life: 0 days 22:00:00\n')

    # Reset the ulog on replica1 to force a full resync from primary.
    # The resync will use the old dump file and then propagate
    # changes.  replica2 should still be in sync with replica1 after
    # the resync, so make sure it doesn't take a full resync.
    mark('propagate M->1->2 full')
    realm.run([kproplog, '-R'], replica1)
    check_ulog(1, 1, 1, [None], replica1)
    kpropd1.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd1, True, 1, 9)
    check_ulog(4, 6, 9, [None, pr2, pr1, pr1], replica1)
    kpropd2.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd2, False, 9, 9)
    check_ulog(3, 7, 9, [None, pr1, pr1], replica2)

    # Make another change and check that it propagates incrementally to
    # both replicas.
    mark('propagate M->1->2 incremental (after reset)')
    realm.run([kadminl, 'modprinc', '+allow_tix', pr2])
    check_ulog(10, 1, 10, [None, pr1, pr3, pr2, pr2, pr2, pr2, pr1, pr1, pr2])
    kpropd1.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd1, False, 9, 10)
    check_ulog(5, 6, 10, [None, pr2, pr1, pr1, pr2], replica1)
    realm.run([kadminl, 'getprinc', pr2], env=replica1,
              expected_msg='Attributes:\n')
    kpropd2.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd2, False, 9, 10)
    check_ulog(4, 7, 10, [None, pr1, pr1, pr2], replica2)
    realm.run([kadminl, 'getprinc', pr2], env=replica2,
              expected_msg='Attributes:\n')

    # Create a policy and check that it propagates via full resync.
    mark('propagate M->1->2 full (new policy)')
    realm.run([kadminl, 'addpol', '-minclasses', '2', 'testpol'])
    check_ulog(1, 1, 1, [None])
    kpropd1.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd1, True, 10, 1)
    check_ulog(1, 1, 1, [None], replica1)
    realm.run([kadminl, 'getpol', 'testpol'], env=replica1,
              expected_msg='Minimum number of password character classes: 2')
    kpropd2.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd2, True, 10, 1)
    check_ulog(1, 1, 1, [None], replica2)
    realm.run([kadminl, 'getpol', 'testpol'], env=replica2,
              expected_msg='Minimum number of password character classes: 2')

    # Modify the policy and test that it also propagates via full resync.
    mark('propagate M->1->2 full (policy change)')
    realm.run([kadminl, 'modpol', '-minlength', '17', 'testpol'])
    check_ulog(1, 1, 1, [None])
    kpropd1.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd1, True, 1, 1)
    check_ulog(1, 1, 1, [None], replica1)
    realm.run([kadminl, 'getpol', 'testpol'], env=replica1,
              expected_msg='Minimum password length: 17')
    kpropd2.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd2, True, 1, 1)
    check_ulog(1, 1, 1, [None], replica2)
    realm.run([kadminl, 'getpol', 'testpol'], env=replica2,
              expected_msg='Minimum password length: 17')

    # Delete the policy and test that it propagates via full resync.
    mark('propgate M->1->2 full (policy delete)')
    realm.run([kadminl, 'delpol', 'testpol'])
    check_ulog(1, 1, 1, [None])
    kpropd1.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd1, True, 1, 1)
    check_ulog(1, 1, 1, [None], replica1)
    realm.run([kadminl, 'getpol', 'testpol'], env=replica1, expected_code=1,
              expected_msg='Policy does not exist')
    kpropd2.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd2, True, 1, 1)
    check_ulog(1, 1, 1, [None], replica2)
    realm.run([kadminl, 'getpol', 'testpol'], env=replica2, expected_code=1,
              expected_msg='Policy does not exist')

    # Modify a principal on the primary and test that it propagates
    # incrementally.
    mark('propagate M->1->2 incremental (after policy changes)')
    realm.run([kadminl, 'modprinc', '-maxlife', '10 minutes', pr1])
    check_ulog(2, 1, 2, [None, pr1])
    kpropd1.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd1, False, 1, 2)
    check_ulog(2, 1, 2, [None, pr1], replica1)
    realm.run([kadminl, 'getprinc', pr1], env=replica1,
              expected_msg='Maximum ticket life: 0 days 00:10:00')
    kpropd2.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd2, False, 1, 2)
    check_ulog(2, 1, 2, [None, pr1], replica2)
    realm.run([kadminl, 'getprinc', pr1], env=replica2,
              expected_msg='Maximum ticket life: 0 days 00:10:00')

    # Delete a principal and test that it propagates incrementally.
    mark('propagate M->1->2 incremental (princ delete)')
    realm.run([kadminl, 'delprinc', pr3])
    check_ulog(3, 1, 3, [None, pr1, pr3])
    kpropd1.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd1, False, 2, 3)
    check_ulog(3, 1, 3, [None, pr1, pr3], replica1)
    realm.run([kadminl, 'getprinc', pr3], env=replica1, expected_code=1,
              expected_msg='Principal does not exist')
    kpropd2.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd2, False, 2, 3)
    check_ulog(3, 1, 3, [None, pr1, pr3], replica2)
    realm.run([kadminl, 'getprinc', pr3], env=replica2, expected_code=1,
              expected_msg='Principal does not exist')

    # Rename a principal and test that it propagates incrementally.
    mark('propagate M->1->2 incremental (princ rename)')
    renpr = "quacked@" + realm.realm
    realm.run([kadminl, 'renprinc', pr1, renpr])
    check_ulog(6, 1, 6, [None, pr1, pr3, renpr, pr1, renpr])
    kpropd1.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd1, False, 3, 6)
    check_ulog(6, 1, 6, [None, pr1, pr3, renpr, pr1, renpr], replica1)
    realm.run([kadminl, 'getprinc', pr1], env=replica1, expected_code=1,
              expected_msg='Principal does not exist')
    realm.run([kadminl, 'getprinc', renpr], env=replica1)
    kpropd2.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd2, False, 3, 6)
    check_ulog(6, 1, 6, [None, pr1, pr3, renpr, pr1, renpr], replica2)
    realm.run([kadminl, 'getprinc', pr1], env=replica2, expected_code=1,
              expected_msg='Principal does not exist')
    realm.run([kadminl, 'getprinc', renpr], env=replica2)

    pr1 = renpr

    # Reset the ulog on the primary to force a full resync.
    mark('propagate M->1->2 full (ulog reset)')
    realm.run([kproplog, '-R'])
    check_ulog(1, 1, 1, [None])
    kpropd1.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd1, True, 6, 1)
    check_ulog(1, 1, 1, [None], replica1)
    kpropd2.send_signal(signal.SIGUSR1)
    wait_for_prop(kpropd2, True, 6, 1)
    check_ulog(1, 1, 1, [None], replica2)

    # Stop the kprop daemons so we can test kpropd -t.
    realm.stop_kpropd(kpropd1)
    stop_daemon(kpropd2)
    stop_daemon(kadmind_proponly)
    mark('kpropd -t')

    # Test the case where no updates are needed.
    out = realm.run_kpropd_once(replica1, ['-d'])
    if 'KDC is synchronized' not in out:
        fail('Expected synchronized from kpropd -t')
    check_ulog(1, 1, 1, [None], replica1)

    # Make a change on the primary and fetch it incrementally.
    realm.run([kadminl, 'modprinc', '-maxlife', '5 minutes', pr1])
    check_ulog(2, 1, 2, [None, pr1])
    out = realm.run_kpropd_once(replica1, ['-d'])
    if 'Got incremental updates (sno=2 ' not in out:
        fail('Expected full dump and synchronized from kpropd -t')
    check_ulog(2, 1, 2, [None, pr1], replica1)
    realm.run([kadminl, 'getprinc', pr1], env=replica1,
              expected_msg='Maximum ticket life: 0 days 00:05:00')

    # Propagate a policy change via full resync.
    realm.run([kadminl, 'addpol', '-minclasses', '3', 'testpol'])
    check_ulog(1, 1, 1, [None])
    out = realm.run_kpropd_once(replica1, ['-d'])
    if ('Full propagation transfer finished' not in out or
        'KDC is synchronized' not in out):
        fail('Expected full dump and synchronized from kpropd -t')
    check_ulog(1, 1, 1, [None], replica1)
    realm.run([kadminl, 'getpol', 'testpol'], env=replica1,
              expected_msg='Minimum number of password character classes: 3')

success('iprop tests')
