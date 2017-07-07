#!/usr/bin/python
from k5test import *

realm = K5Realm(create_kdb=False)

# Test err_fmt, using klist -c to induce errors.
fmt1 = 'FOO Error: %M (see http://localhost:1234/%C for more information)'
conf1 = {'libdefaults': {'err_fmt': fmt1}}
e1 = realm.special_env('fmt1', False, krb5_conf=conf1)
out = realm.run([klist, '-c', 'testdir/xx/yy'], env=e1, expected_code=1)
if out != ('klist: FOO Error: No credentials cache found (filename: '
           'testdir/xx/yy) (see http://localhost:1234/-1765328189 for more '
           'information)\n'):
    fail('err_fmt expansion failed')
conf2 = {'libdefaults': {'err_fmt': '%M - %C'}}
e2 = realm.special_env('fmt2', False, krb5_conf=conf2)
out = realm.run([klist, '-c', 'testdir/xx/yy'], env=e2, expected_code=1)
if out != ('klist: No credentials cache found (filename: testdir/xx/yy) - '
           '-1765328189\n'):
    fail('err_fmt expansion failed')
conf3 = {'libdefaults': {'err_fmt': '%%%M %-% %C%'}}
e3 = realm.special_env('fmt3', False, krb5_conf=conf3)
out = realm.run([klist, '-c', 'testdir/xx/yy'], env=e3, expected_code=1)
if out != ('klist: %No credentials cache found (filename: testdir/xx/yy) %-% '
           '-1765328189%\n'):
    fail('err_fmt expansion failed')

success('error message tests')
