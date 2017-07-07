#!/usr/bin/python

from k5test import *
from subprocess import *

realm = K5Realm(create_kdb=False)

def compare(s, expected, msg):
    if s == expected:
        return
    print 'expected:', repr(expected)
    print 'got:', repr(s)
    fail(msg)

out = realm.run(['./t_tdumputil', '2', 'field1', 'field2',
                 'value1', 'value2'])
expected = 'field1\tfield2\nvalue1\tvalue2\n'
compare(out, expected, 'tab-separated values')

out = realm.run(['./t_tdumputil', '-c', '2', 'field1', 'field2',
                 'space value', 'comma,value',
                 'quote"value', 'quotes""value'])
expected = 'field1,field2\nspace value,"comma,value"\n' \
    '"quote""value","quotes""""value"\n'
compare(out, expected, 'comma-separated values')

out = realm.run(['./t_tdumputil', '-T', 'rectype', '2', 'field1', 'field2',
                 'value1', 'value2'])
expected = 'rectype\tvalue1\tvalue2\n'
compare(out, expected, 'rectype prefixed')

success('tabdump utilities')
