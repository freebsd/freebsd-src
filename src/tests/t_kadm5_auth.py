from k5test import *

# Create a realm with the welcomer and bouncer kadm5_auth test modules
# in place of the builtin modules.
modpath = os.path.join(buildtop, 'plugins', 'kadm5_auth', 'test',
                       'kadm5_auth_test.so')
conf = {'plugins': {'kadm5_auth': {'module': ['welcomer:' + modpath,
                                              'bouncer:' + modpath],
                                   'enable_only': ['welcomer', 'bouncer']}}}
realm = K5Realm(krb5_conf=conf, create_host=False)
realm.start_kadmind()
realm.prep_kadmin()

# addprinc: welcomer accepts with policy VIP, bouncer denies maxlife.
realm.run_kadmin(['addprinc', '-randkey', 'princ'], expected_code=1)
realm.run_kadmin(['addprinc', '-randkey', '-policy', 'VIP', 'princ'])
realm.run_kadmin(['addprinc', '-randkey', '-policy', 'VIP', '-maxlife', '3',
                  'princ'], expected_code=1)

# modprinc: welcomer accepts with only maxrenewlife, bouncer denies
# with even-component target principal.
realm.run_kadmin(['modprinc', '-maxlife', '3', 'princ'], expected_code=1)
realm.run_kadmin(['modprinc', '-maxrenewlife', '3', 'princ'])
realm.run_kadmin(['modprinc', '-maxrenewlife', '3', 'user/admin'],
                 expected_code=1)

# setstr: welcomer accepts with key 'note', bouncer denies with value
# length > 10.
realm.run_kadmin(['setstr', 'princ', 'somekey', 'someval'], expected_code=1)
realm.run_kadmin(['setstr', 'princ', 'note', 'abc'])
realm.run_kadmin(['setstr', 'princ', 'note', 'abcdefghijkl'], expected_code=1)

# delprinc: welcomer accepts with target principal beginning with 'd',
# bouncer denies with "nodelete" string attribute.
realm.run_kadmin(['delprinc', 'user'], expected_code=1)
realm.run([kadminl, 'addprinc', '-randkey', 'deltest'])
realm.run_kadmin(['delprinc', 'deltest'])
realm.run([kadminl, 'addprinc', '-randkey', 'deltest'])
realm.run([kadminl, 'setstr', 'deltest', 'nodelete', 'yes'])
realm.run_kadmin(['delprinc', 'deltest'], expected_code=1)

# renprinc: welcomer accepts with same-length first components, bouncer
# refuses with source principal beginning with 'a'.
realm.run_kadmin(['renprinc', 'princ', 'xyz'], expected_code=1)
realm.run_kadmin(['renprinc', 'princ', 'abcde'])
realm.run_kadmin(['renprinc', 'abcde', 'fghij'], expected_code=1)

# addpol: welcomer accepts with minlength 3, bouncer denies with name
# length <= 3.
realm.run_kadmin(['addpol', 'testpol'], expected_code=1)
realm.run_kadmin(['addpol', '-minlength', '3', 'testpol'])
realm.run_kadmin(['addpol', '-minlength', '3', 'abc'], expected_code=1)

# modpol: welcomer accepts changes to minlife, bouncer denies with
# minlife > 10.
realm.run_kadmin(['modpol', '-minlength', '4', 'testpol'], expected_code=1)
realm.run_kadmin(['modpol', '-minlife', '8', 'testpol'])
realm.run_kadmin(['modpol', '-minlife', '11', 'testpol'], expected_code=1)

# getpol: welcomer accepts if policy and client policy have same length,
# bouncer denies if policy name begins with 'x'.
realm.run([kadminl, 'addpol', 'aaaa'])
realm.run([kadminl, 'addpol', 'bbbb'])
realm.run([kadminl, 'addpol', 'xxxx'])
realm.run([kadminl, 'modprinc', '-policy', 'aaaa', 'user/admin'])
realm.run_kadmin(['getpol', 'testpol'], expected_code=1)
realm.run_kadmin(['getpol', 'bbbb'])
realm.run_kadmin(['getpol', 'xxxx'], expected_code=1)

# end: welcomer counts operations using "ends" string attribute on
# "opcount" principal.  kadmind is dumb and invokes the end method for
# every RPC operation including init, so we expect four calls to the
# end operation.
realm.run([kadminl, 'addprinc', '-nokey', 'opcount'])
realm.run([kadminl, 'setstr', 'opcount', 'ends', '0'])
realm.run_kadmin(['getprinc', 'user'])
realm.run_kadmin(['getpol', 'bbbb'])
realm.run([kadminl, 'getstrs', 'opcount'], expected_msg='ends: 4')

success('kadm5_auth pluggable interface tests')
