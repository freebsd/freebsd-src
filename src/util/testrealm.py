# Copyright (C) 2010 by the Massachusetts Institute of Technology.
# All rights reserved.

# Export of this software from the United States of America may
#   require a specific license from the United States Government.
#   It is the responsibility of any person or organization contemplating
#   export to obtain such a license before exporting.
#
# WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
# distribute this software and its documentation for any purpose and
# without fee is hereby granted, provided that the above copyright
# notice appear in all copies and that both that copyright notice and
# this permission notice appear in supporting documentation, and that
# the name of M.I.T. not be used in advertising or publicity pertaining
# to distribution of the software without specific, written prior
# permission.  Furthermore if you modify this software you must label
# your software as modified software and not distribute it in such a
# fashion that it might be confused with the original M.I.T. software.
# M.I.T. makes no representations about the suitability of
# this software for any purpose.  It is provided "as is" without express
# or implied warranty.

# Invoked by the testrealm target in the top-level Makefile.  Creates
# a test realm and spawns a shell pointing at it, for convenience of
# manual testing.  If a numeric argument is present after options,
# creates that many fully connected test realms and point the shell at
# the first one.

from k5test import *

# A list of directories containing programs in the build tree.
progpaths = [
    'kdc',
    os.path.join('kadmin', 'server'),
    os.path.join('kadmin', 'cli'),
    os.path.join('kadmin', 'dbutil'),
    os.path.join('kadmin', 'ktutil'),
    os.path.join('clients', 'kdestroy'),
    os.path.join('clients', 'kinit'),
    os.path.join('clients', 'klist'),
    os.path.join('clients', 'kpasswd'),
    os.path.join('clients', 'ksu'),
    os.path.join('clients', 'kvno'),
    os.path.join('clients', 'kswitch'),
    'slave'
]

# Add program directories to the beginning of PATH.
def supplement_path(env):
    # Construct prefixes; these will end in a trailing separator.
    path_prefix = manpath_prefix = ''
    for dir in progpaths:
        path_prefix += os.path.join(buildtop, dir) + os.pathsep

    # Assume PATH exists in env for simplicity.
    env['PATH'] = path_prefix + env['PATH']

if args:
    realms = cross_realms(int(args[0]), start_kadmind=True)
    realm = realms[0]
else:
    realm = K5Realm(start_kadmind=True)
env = realm.env.copy()
supplement_path(env)

pwfilename = os.path.join('testdir', 'passwords')
pwfile = open(pwfilename, 'w')
pwfile.write('user: %s\nadmin: %s\n' % (password('user'), password('admin')))
pwfile.close()

print
print 'Realm files are in %s' % realm.testdir
print 'KRB5_CONFIG is %s' % env['KRB5_CONFIG']
print 'KRB5_KDC_PROFILE is %s' % env['KRB5_KDC_PROFILE']
print 'KRB5CCNAME is %s' % env['KRB5CCNAME']
print 'KRB5_KTNAME is %s' % env['KRB5_KTNAME']
print 'KRB5RCACHEDIR is %s' % env['KRB5RCACHEDIR']
print 'Password for user is %s (see also %s)' % (password('user'), pwfilename)
print 'Password for admin is %s' % password('admin')
print

subprocess.call([os.getenv('SHELL')], env=env)
success('Create test krb5 realm.')
