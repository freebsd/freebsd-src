from k5test import *

# This file contains tests for kadmin command parsing.  Principal
# flags (which can also be used in kadm5.acl or krb5.conf) are tested
# in t_princflags.py.

# kadmin recognizes time intervals using either the
# krb5_string_to_deltat() formats or the relative getdate.y formats.
# (Absolute getdate.y formats also work with the current time
# subtracted; this isn't very useful and we won't test it here.)
intervals = (
    # krb5_string_to_deltat() formats.  Whitespace ( \t\n) is allowed
    # before or between most elements or at the end, but not after
    # 's'.  Negative or oversized numbers are allowed in most places,
    # but not after the first number in an HH:MM:SS form.
    ('28s', '0 days 00:00:28'),
    ('7m ', '0 days 00:07:00'),
    ('6m 9s', '0 days 00:06:09'),
    ('2h', '0 days 02:00:00'),
    ('2h-5s', '0 days 01:59:55'),
    ('2h3m', '0 days 02:03:00'),
    ('2h3m5s', '0 days 02:03:05'),
    ('5d ', '5 days 00:00:00'),
    ('5d-48s', '4 days 23:59:12'),
    ('5d18m', '5 days 00:18:00'),
    ('5d -6m56s', '4 days 23:54:56'),
    ('5d4h', '5 days 04:00:00'),
    ('5d4h 1s', '5 days 04:00:01'),
    ('5d4h3m', '5 days 04:03:00'),
    (' \t 15d \n 4h  3m  2s', '15 days 04:03:02'),
    ('10-8:45:0', '10 days 08:45:00'),
    ('1000:67:99', '41 days 17:08:39'),
    ('999:11', '41 days 15:11:00'),
    ('382512', '4 days 10:15:12'),

    # getdate.y relative formats (and "never", which is handled
    # specially as a zero interval).  Any number of relative forms can
    # be specified in any order.  Whitespace is ignored before or
    # after any token.  "month" and "year" are allowed as units but
    # depend on the current time, so we won't test them.  Plural unit
    # names are treated identically to singular unit names.  Numbers
    # before unit names are optional and may be signed; there are also
    # aliases for some numbers.  "ago" inverts the interval up to the
    # point where it appears.
    ('never', '0 days 00:00:00'),
    ('fortnight', '14 days 00:00:00'),
    ('3 day ago 4 weeks 8 hours', '25 days 08:00:00'),
    ('8 second -3 secs 5 minute ago 63 min', '0 days 00:57:55'),
    ('min mins min mins min', '0 days 00:05:00'),
    ('tomorrow tomorrow today yesterday now last minute', '0 days 23:59:00'),
    ('this second next minute first hour third fortnight fourth day '
     'fifth weeks sixth sec seventh secs eighth second ninth mins tenth '
     'day eleventh min twelfth sec', '91 days 01:22:34'))

realm = K5Realm(create_host=False, get_creds=False)
realm.run([kadminl, 'addpol', 'pol'])
for instr, outstr in intervals:
    realm.run([kadminl, 'modprinc', '-maxlife', instr, realm.user_princ])
    msg = 'Maximum ticket life: ' + outstr + '\n'
    realm.run([kadminl, 'getprinc', realm.user_princ], expected_msg=msg)

    realm.run([kadminl, 'modprinc', '-maxrenewlife', instr, realm.user_princ])
    msg = 'Maximum renewable life: ' + outstr + '\n'
    realm.run([kadminl, 'getprinc', realm.user_princ], expected_msg=msg)

    realm.run([kadminl, 'modpol', '-maxlife', instr, 'pol'])
    msg = 'Maximum password life: ' + outstr + '\n'
    realm.run([kadminl, 'getpol', 'pol'], expected_msg=msg)

    realm.run([kadminl, 'modpol', '-minlife', instr, 'pol'])
    msg = 'Minimum password life: ' + outstr + '\n'
    realm.run([kadminl, 'getpol', 'pol'], expected_msg=msg)

    realm.run([kadminl, 'modpol', '-failurecountinterval', instr, 'pol'])
    msg = 'Password failure count reset interval: ' + outstr + '\n'
    realm.run([kadminl, 'getpol', 'pol'], expected_msg=msg)

    realm.run([kadminl, 'modpol', '-lockoutduration', instr, 'pol'])
    msg = 'Password lockout duration: ' + outstr + '\n'
    realm.run([kadminl, 'getpol', 'pol'], expected_msg=msg)

success('kadmin command parsing tests')
