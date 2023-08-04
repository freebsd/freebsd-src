from k5test import *
from princflags import *
import re

realm = K5Realm(create_host=False, get_creds=False)

# Regex pattern to match an empty attribute line from kadmin getprinc
emptyattr = re.compile('^Attributes:$', re.MULTILINE)


# Regex pattern to match a kadmin getprinc output for a flag tuple
def attr_pat(ftuple):
    return re.compile('^Attributes: ' + ftuple.flagname() + '$',
                      re.MULTILINE)


# Test one flag tuple for kadmin ank.
def one_kadmin_flag(ftuple):
    pat = attr_pat(ftuple)
    realm.run([kadminl, 'ank', ftuple.setspec(),
               '-pw', 'password', 'test'])
    out = realm.run([kadminl, 'getprinc', 'test'])
    if not pat.search(out):
        fail('Failed to set flag ' + ftuple.flagname())

    realm.run([kadminl, 'modprinc', ftuple.clearspec(), 'test'])
    out = realm.run([kadminl, 'getprinc', 'test'])
    if not emptyattr.search(out):
        fail('Failed to clear flag ' + ftuple.flagname())
    realm.run([kadminl, 'delprinc', 'test'])


# Generate a custom kdc.conf with default_principal_flags set
# according to ftuple.
def genkdcconf(ftuple):
    d = { 'realms': { '$realm': {
                'default_principal_flags': ftuple.setspec()
                }}}
    return realm.special_env('tmp', True, kdc_conf=d)


# Test one ftuple for kdc.conf default_principal_flags.
def one_kdcconf(ftuple):
    e = genkdcconf(ftuple)
    pat = attr_pat(ftuple)
    realm.run([kadminl, 'ank', '-pw', 'password', 'test'], env=e)
    out = realm.run([kadminl, 'getprinc', 'test'])
    if not pat.search(out):
        fail('Failed to set flag ' + ftuple.flagname() + ' via kdc.conf')

    realm.run([kadminl, 'delprinc', 'test'])


# Principal name for kadm5.acl line
def ftuple2pname(ftuple, doset):
    pname = 'set_' if doset else 'clear_'
    return pname + ftuple.flagname()


# Translate a strconv ftuple to a spec string for kadmin.
def ftuple2kadm_spec(ftuple, doset):
    ktuple = kadmin_itable[ftuple.flag]
    if ktuple.invert != ftuple.invert:
        # Could do:
        # doset = not doset
        # but this shouldn't happen.
        raise ValueError
    return ktuple.spec(doset)


# Generate a line for kadm5.acl.
def acl_line(ftuple, doset):
    pname = ftuple2pname(ftuple, doset)
    spec = ftuple.spec(doset)
    return "%s * %s %s\n" % (realm.admin_princ, pname, spec)


# Test one kadm5.acl line for a ftuple.
def one_aclcheck(ftuple, doset):
    pname = ftuple2pname(ftuple, doset)
    pat = attr_pat(ftuple)
    outname = ftuple.flagname()
    # Create the principal and check that the flag is correctly set or
    # cleared.
    realm.run_kadmin(['ank', '-pw', 'password', pname])
    out = realm.run([kadminl, 'getprinc', pname])
    if doset:
        if not pat.search(out):
            fail('Failed to set flag ' + outname + ' via kadm5.acl')
    else:
        if not emptyattr.search(out):
            fail('Failed to clear flag ' + outname + ' via kadm5.acl')
    # If acl forces flag to be set, try to clear it, and vice versa.
    spec = ftuple2kadm_spec(ftuple, not doset)
    realm.run_kadmin(['modprinc', spec, pname])
    out = realm.run([kadminl, 'getprinc', pname])
    if doset:
        if not pat.search(out):
            fail('Failed to keep flag ' + outname + ' set')
    else:
        if not emptyattr.search(out):
            fail('Failed to keep flag ' + outname + ' clear')


# Set all flags simultaneously, even the ones that aren't defined yet.
def lamptest():
    pat = re.compile('^Attributes: ' +
                     ' '.join(flags2namelist(0xffffffff)) +
                     '$', re.MULTILINE)
    realm.run([kadminl, 'ank', '-pw', 'password', '+0xffffffff', 'test'])
    out = realm.run([kadminl, 'getprinc', 'test'])
    if not pat.search(out):
        fail('Failed to simultaenously set all flags')
    realm.run([kadminl, 'delprinc', 'test'])


for ftuple in kadmin_ftuples:
    one_kadmin_flag(ftuple)

for ftuple in strconv_ftuples:
    one_kdcconf(ftuple)

f = open(os.path.join(realm.testdir, 'acl'), 'w')
for ftuple in strconv_ftuples:
    f.write(acl_line(ftuple, True))
    f.write(acl_line(ftuple, False))
f.close()

realm.start_kadmind()
realm.prep_kadmin()

for ftuple in strconv_ftuples:
    one_aclcheck(ftuple, True)
    one_aclcheck(ftuple, False)

lamptest()

success('KDB principal flags')
