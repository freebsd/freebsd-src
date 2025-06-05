from k5test import *

entries = ('URI _kerberos.TEST krb5srv::kkdcp:https://kdc1 1 1\n',
           'URI _kerberos.TEST krb5srv::kkdcp:https://kdc3:300/path 3 1\n',
           'URI _kerberos.TEST krb5srv:m:kkdcp:https://kdc2/path 2 1\n',
           'URI _kerberos.TEST KRB5SRV:xMz:UDP:KDC4 4 1\n',
           'URI _kerberos.TEST krb5srv:xyz:tcp:192.168.1.6 6 1\n',
           'URI _kerberos.TEST krb5srv::tcp:kdc5:500 5 1\n',
           'URI _kerberos.TEST krb5srv::tcp:[dead:beef:cafe:7]:700 7 1\n',
           'URI _kerberos.TEST bogustag:m:kkdcp:https://bogus 8 1\n',
           'URI _kerberos.TEST krb5srv:m:bogustrans:https://bogus 10 1\n',
           'URI _kerberos.TEST krb5srv:m:kkdcp:bogus 11 1\n',
           'URI _kerberos.TEST krb5srv:m:bogusnotrans 12 1\n')

expected = ('7 servers:',
            '0: h:kdc1 t:https p:443 m:0 P:',
            '1: h:kdc2 t:https p:443 m:1 P:path',
            '2: h:kdc3 t:https p:300 m:0 P:path',
            '3: h:KDC4 t:udp p:88 m:1 P:',
            '4: h:kdc5 t:tcp p:500 m:0 P:',
            '5: h:192.168.1.6 t:tcp p:88 m:0 P:',
            '6: h:dead:beef:cafe:7 t:tcp p:700 m:0 P:')

conf = {'libdefaults': {'dns_lookup_kdc' : 'true'}}

realm = K5Realm(create_kdb=False, krb5_conf=conf)

hosts_filename = os.path.join(realm.testdir, 'resolv_hosts')
f = open(hosts_filename, 'w')
for line in entries:
    f.write(line)
f.close()

realm.env['LD_PRELOAD'] = 'libresolv_wrapper.so'
realm.env['RESOLV_WRAPPER_HOSTS'] = hosts_filename

out = realm.run(['./t_locate_kdc', 'TEST'], env=realm.env)
l = out.splitlines()

j = 0
for i in range(4, 12):
    if l[i].strip() != expected[j]:
        fail('URI answers do not match')
    j += 1

success('uri discovery tests')
