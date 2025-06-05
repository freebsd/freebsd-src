from k5test import *

realm = K5Realm()
server = 'p:' + realm.host_princ

mark('krb5 channel bindings')
realm.run(['./t_bindings', server, '-', '-'], expected_msg='no')
realm.run(['./t_bindings', server, 'a', '-'], expected_msg='no')
realm.run(['./t_bindings', server, 'a', 'a'], expected_msg='yes')
realm.run(['./t_bindings', server, '-', 'a'], expected_msg='no')
realm.run(['./t_bindings', server, 'a', 'x'],
          expected_code=1, expected_msg='Incorrect channel bindings')

mark('SPNEGO channel bindings')
realm.run(['./t_bindings', '-s', server, '-', '-'], expected_msg='no')
realm.run(['./t_bindings', '-s', server, 'a', '-'], expected_msg='no')
realm.run(['./t_bindings', '-s', server, 'a', 'a'], expected_msg='yes')
realm.run(['./t_bindings', '-s', server, '-', 'a'], expected_msg='no')
realm.run(['./t_bindings', '-s', server, 'a', 'x'],
          expected_code=1, expected_msg='Incorrect channel bindings')

client_aware_conf = {'libdefaults': {'client_aware_channel_bindings': 'true'}}
e = realm.special_env('cb_aware', False, krb5_conf=client_aware_conf)

mark('krb5 client_aware_channel_bindings')
realm.run(['./t_bindings', server, '-', '-'], env=e, expected_msg='no')
realm.run(['./t_bindings', server, 'a', '-'], env=e, expected_msg='no')
realm.run(['./t_bindings', server, 'a', 'a'], env=e, expected_msg='yes')
realm.run(['./t_bindings', server, '-', 'a'], env=e,
          expected_code=1, expected_msg='Incorrect channel bindings')
realm.run(['./t_bindings', server, 'a', 'x'], env=e,
          expected_code=1, expected_msg='Incorrect channel bindings')

mark('SPNEGO client_aware_channel_bindings')
realm.run(['./t_bindings', '-s', server, '-', '-'], env=e, expected_msg='no')
realm.run(['./t_bindings', '-s', server, 'a', '-'], env=e, expected_msg='no')
realm.run(['./t_bindings', '-s', server, 'a', 'a'], env=e, expected_msg='yes')
realm.run(['./t_bindings', '-s', server, '-', 'a'], env=e,
          expected_code=1, expected_msg='Incorrect channel bindings')
realm.run(['./t_bindings', '-s', server, 'a', 'x'], env=e,
          expected_code=1, expected_msg='Incorrect channel bindings')

success('channel bindings tests')
