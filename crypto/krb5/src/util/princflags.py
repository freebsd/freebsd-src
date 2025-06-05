import re

# Module for translating KDB principal flags between string and
# integer forms.
#
# When run as a standalone script, print out C tables to insert into
# lib/kadm5/str_conv.c.

# KDB principal flag definitions copied from kdb.h

KRB5_KDB_DISALLOW_POSTDATED     = 0x00000001
KRB5_KDB_DISALLOW_FORWARDABLE   = 0x00000002
KRB5_KDB_DISALLOW_TGT_BASED     = 0x00000004
KRB5_KDB_DISALLOW_RENEWABLE     = 0x00000008
KRB5_KDB_DISALLOW_PROXIABLE     = 0x00000010
KRB5_KDB_DISALLOW_DUP_SKEY      = 0x00000020
KRB5_KDB_DISALLOW_ALL_TIX       = 0x00000040
KRB5_KDB_REQUIRES_PRE_AUTH      = 0x00000080
KRB5_KDB_REQUIRES_HW_AUTH       = 0x00000100
KRB5_KDB_REQUIRES_PWCHANGE      = 0x00000200
KRB5_KDB_DISALLOW_SVR           = 0x00001000
KRB5_KDB_PWCHANGE_SERVICE       = 0x00002000
KRB5_KDB_SUPPORT_DESMD5         = 0x00004000
KRB5_KDB_NEW_PRINC              = 0x00008000
KRB5_KDB_OK_AS_DELEGATE         = 0x00100000
KRB5_KDB_OK_TO_AUTH_AS_DELEGATE = 0x00200000
KRB5_KDB_NO_AUTH_DATA_REQUIRED  = 0x00400000
KRB5_KDB_LOCKDOWN_KEYS          = 0x00800000

# Input tables -- list of tuples of the form (name, flag, invert)

# Input forms from kadmin.c
_kadmin_pflags = [
    ("allow_postdated",         KRB5_KDB_DISALLOW_POSTDATED,    True),
    ("allow_forwardable",       KRB5_KDB_DISALLOW_FORWARDABLE,  True),
    ("allow_tgs_req",           KRB5_KDB_DISALLOW_TGT_BASED,    True),
    ("allow_renewable",         KRB5_KDB_DISALLOW_RENEWABLE,    True),
    ("allow_proxiable",         KRB5_KDB_DISALLOW_PROXIABLE,    True),
    ("allow_dup_skey",          KRB5_KDB_DISALLOW_DUP_SKEY,     True),
    ("allow_tix",               KRB5_KDB_DISALLOW_ALL_TIX,      True),
    ("requires_preauth",        KRB5_KDB_REQUIRES_PRE_AUTH,     False),
    ("requires_hwauth",         KRB5_KDB_REQUIRES_HW_AUTH,      False),
    ("needchange",              KRB5_KDB_REQUIRES_PWCHANGE,     False),
    ("allow_svr",               KRB5_KDB_DISALLOW_SVR,          True),
    ("password_changing_service", KRB5_KDB_PWCHANGE_SERVICE,    False),
    ("support_desmd5",          KRB5_KDB_SUPPORT_DESMD5,        False),
    ("ok_as_delegate",          KRB5_KDB_OK_AS_DELEGATE,        False),
    ("ok_to_auth_as_delegate",  KRB5_KDB_OK_TO_AUTH_AS_DELEGATE, False),
    ("no_auth_data_required",   KRB5_KDB_NO_AUTH_DATA_REQUIRED, False),
    ("lockdown_keys",           KRB5_KDB_LOCKDOWN_KEYS,         False),
]

# Input forms from lib/kadm5/str_conv.c
_strconv_pflags = [
    ("postdateable",            KRB5_KDB_DISALLOW_POSTDATED,    True),
    ("forwardable",             KRB5_KDB_DISALLOW_FORWARDABLE,  True),
    ("tgt-based",               KRB5_KDB_DISALLOW_TGT_BASED,    True),
    ("renewable",               KRB5_KDB_DISALLOW_RENEWABLE,    True),
    ("proxiable",               KRB5_KDB_DISALLOW_PROXIABLE,    True),
    ("dup-skey",                KRB5_KDB_DISALLOW_DUP_SKEY,     True),
    ("allow-tickets",           KRB5_KDB_DISALLOW_ALL_TIX,      True),
    ("preauth",                 KRB5_KDB_REQUIRES_PRE_AUTH,     False),
    ("hwauth",                  KRB5_KDB_REQUIRES_HW_AUTH,      False),
    ("ok-as-delegate",          KRB5_KDB_OK_AS_DELEGATE,        False),
    ("pwchange",                KRB5_KDB_REQUIRES_PWCHANGE,     False),
    ("service",                 KRB5_KDB_DISALLOW_SVR,          True),
    ("pwservice",               KRB5_KDB_PWCHANGE_SERVICE,      False),
    ("md5",                     KRB5_KDB_SUPPORT_DESMD5,        False),
    ("ok-to-auth-as-delegate",  KRB5_KDB_OK_TO_AUTH_AS_DELEGATE, False),
    ("no-auth-data-required",   KRB5_KDB_NO_AUTH_DATA_REQUIRED, False),
    ("lockdown-keys",           KRB5_KDB_LOCKDOWN_KEYS,         False),
]

# kdb.h symbol prefix
_prefix = 'KRB5_KDB_'
_prefixlen = len(_prefix)

# Names of flags, as printed by kadmin (derived from kdb.h symbols).
# To be filled in by _setup_tables().
_flagnames = {}

# Translation table to map hyphens to underscores
_squash = str.maketrans('-', '_')

# Combined input-to-flag lookup table, to be filled in by
# _setup_tables()
pflags = {}

# Tables of ftuples, to be filled in by _setup_tables()
kadmin_ftuples = []
strconv_ftuples = []
sym_ftuples = []
all_ftuples = []

# Inverted table to look up ftuples by flag value, to be filled in by
# _setup_tables()
kadmin_itable = {}
strconv_itable = {}
sym_itable = {}


# Bundle some methods that are useful for writing tests.
class Ftuple(object):
    def __init__(self, name, flag, invert):
        self.name = name
        self.flag = flag
        self.invert = invert

    def __repr__(self):
        return "Ftuple" + str((self.name, self.flag, self.invert))

    def flagname(self):
        return _flagnames[self.flag]

    def setspec(self):
        return ('-' if self.invert else '+') + self.name

    def clearspec(self):
        return ('+' if self.invert else '-') + self.name

    def spec(self, doset):
        return self.setspec() if doset else self.clearspec()


def _setup_tables():
    # Filter globals for 'KRB5_KDB_' prefix to create lookup tables.
    # Make the reasonable assumption that the Python runtime doesn't
    # define any names with that prefix by default.
    global _flagnames
    for k, v in globals().items():
        if k.startswith(_prefix):
            _flagnames[v] = k[_prefixlen:]

    # Construct an input table based on kdb.h constant names by
    # truncating the "KRB5_KDB_" prefix and downcasing.
    sym_pflags = []
    for v, k in sorted(_flagnames.items()):
        sym_pflags.append((k.lower(), v, False))

    global kadmin_ftuples, strconv_ftuples, sym_ftuples, all_ftuples
    for x in _kadmin_pflags:
        kadmin_ftuples.append(Ftuple(*x))
    for x in _strconv_pflags:
        strconv_ftuples.append(Ftuple(*x))
    for x in sym_pflags:
        sym_ftuples.append(Ftuple(*x))
    all_ftuples = kadmin_ftuples + strconv_ftuples + sym_ftuples

    # Populate combined input-to-flag lookup table.  This will
    # eliminate some duplicates.
    global pflags
    for x in all_ftuples:
        name = x.name.translate(_squash)
        pflags[name] = x

    global kadmin_itable, strconv_itable, sym_itable
    for x in kadmin_ftuples:
        kadmin_itable[x.flag] = x
    for x in strconv_ftuples:
        strconv_itable[x.flag] = x
    for x in sym_ftuples:
        sym_itable[x.flag] = x


# Convert the bit number of a flag to a string.  Remove the
# 'KRB5_KDB_' prefix.  Give an 8-digit hexadecimal number if the flag
# is unknown.
def flagnum2str(n):
    s = _flagnames.get(1 << n)
    if s is None:
        return "0x%08x" % ((1 << n) & 0xffffffff)
    return s


# Return a list of flag names from a flag word.
def flags2namelist(flags):
    a = []
    for n in range(32):
        if flags & (1 << n):
            a.append(flagnum2str(n))
    return a


# Given a single specifier in the form {+|-}flagname, return a tuple
# of the form (flagstoset, flagstoclear).
def flagspec2mask(s):
    req_neg = False
    if s[0] == '-':
        req_neg = True
        s = s[1:]
    elif s[0] == '+':
        s = s[1:]

    s = s.lower().translate(_squash)
    x = pflags.get(s)
    if x is not None:
        flag, invert = x.flag, x.invert
    else:
        # Maybe it's a hex number.
        if not s.startswith('0x'):
            raise ValueError
        flag, invert = int(s, 16), False

    if req_neg:
        invert = not invert
    return (0, ~flag) if invert else (flag, ~0)


# Given a string containing a space/comma separated list of specifiers
# of the form {+|-}flagname, return a tuple of the form (flagstoset,
# flagstoclear).  This shares the same limitation as
# kadm5int_acl_parse_restrictions() of losing the distinction between
# orderings when the same flag bit appears in both the positive and
# the negative sense.
def speclist2mask(s):
    toset, toclear = (0, ~0)
    for x in re.split('[\t, ]+', s):
        fset, fclear = flagspec2mask(x)
        toset |= fset
        toclear &= fclear

    return toset, toclear


# Print C table of input flag specifiers for lib/kadm5/str_conv.c.
def _print_ftbl():
    print('static const struct flag_table_row ftbl[] = {')
    a = sorted(pflags.items(), key=lambda k, v: (v.flag, -v.invert, k))
    for k, v in a:
        s1 = '    {"%s",' % k
        s2 = '%-31s KRB5_KDB_%s,' % (s1, v.flagname())
        print('%-63s %d},' % (s2, 1 if v.invert else 0))

    print('};')
    print('#define NFTBL (sizeof(ftbl) / sizeof(ftbl[0]))')


# Print C table of output flag names for lib/kadm5/str_conv.c.
def _print_outflags():
    print('static const char *outflags[] = {')
    for i in range(32):
        flag = 1 << i
        if flag > max(_flagnames.keys()):
            break
        try:
            s = '    "%s",' % _flagnames[flag]
        except KeyError:
            s = '    NULL,'
        print('%-32s/* 0x%08x */' % (s, flag))

    print('};')
    print('#define NOUTFLAGS (sizeof(outflags) / sizeof(outflags[0]))')


# Print out C tables to insert into lib/kadm5/str_conv.c.
def _main():
    _print_ftbl()
    print
    _print_outflags()


_setup_tables()


if __name__ == '__main__':
    _main()
