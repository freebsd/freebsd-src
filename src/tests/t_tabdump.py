#!/usr/bin/python
from k5test import *

import csv
import StringIO

def tab_csv(s):
    io = StringIO.StringIO(s)
    return list(csv.DictReader(io, dialect=csv.excel_tab))


def getrows(dumptype):
    out = realm.run([kdb5_util, 'tabdump', dumptype])
    return tab_csv(out)


def checkkeys(rows, dumptype, names):
    if sorted(rows[0].keys()) != sorted(names):
        fail('tabdump %s field names' % dumptype)


realm = K5Realm(start_kdc=False, get_creds=False)


rows = getrows('keyinfo')
checkkeys(rows, 'keyinfo',
          ["name", "keyindex", "kvno", "enctype", "salttype", "salt"])

userrows = [x for x in rows if x['name'].startswith('user@')]
userrows.sort(key=lambda x: x['keyindex'])

if (userrows[0]['enctype'] != 'aes256-cts-hmac-sha1-96' or
    userrows[1]['enctype'] != 'aes128-cts-hmac-sha1-96'):
    fail('tabdump keyinfo enctypes')

success('tabdump keyinfo')


rows = getrows('keydata')
checkkeys(rows, 'keydata',
          ["name", "keyindex", "kvno", "enctype", "key", "salttype", "salt"])


rows = getrows('princ_flags')
checkkeys(rows, 'princ_flags', ["name", "flag", "value"])


rows = getrows('princ_lockout')
checkkeys(rows, 'princ_lockout', ["name", "last_success", "last_failed",
                                  "fail_count"])


realm.run([kadminl, 'addpol', '-history', '3', 'testpol'])
realm.run([kadminl, 'modprinc', '-policy', 'testpol', 'user'])

rows = getrows('princ_meta')
checkkeys(rows, 'princ_meta', ["name", "modby", "modtime", "lastpwd",
                               "policy", "mkvno", "hist_kvno"])

userrows = [x for x in rows if x['name'].startswith('user@')]

if userrows[0]['policy'] != 'testpol':
    fail('tabdump princ_meta policy name')


realm.run([kadminl, 'set_string', 'user', 'foo', 'bar'])

rows = getrows('princ_stringattrs')
checkkeys(rows, 'princ_stringattrs', ["name", "key", "value"])

userrows = [x for x in rows if x['name'].startswith('user@')]
if (len(userrows) != 1 or userrows[0]['key'] != 'foo' or
    userrows[0]['value'] != 'bar'):
    fail('tabdump princ_stringattrs key/value')


rows = getrows('princ_tktpolicy')
checkkeys(rows, 'princ_tktpolicy', ["name", "expiration", "pw_expiration",
                                    "max_life", "max_renew_life"])

success('tabdump')
