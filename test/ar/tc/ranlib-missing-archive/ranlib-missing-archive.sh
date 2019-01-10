# $Id$
#
# Verify that 'ranlib' exits with an error if asked to operate on a
# non-existent archive.
inittest ranlib-missing-archive tc/ranlib-missing-archive
runcmd "${RANLIB} nonexistent.a" work true
rundiff true
