#!/usr/bin/python
from k5test import *

realm = K5Realm(create_kdb=False)
realm.run(['./t_stringattr'])
success('String attribute unit tests')
